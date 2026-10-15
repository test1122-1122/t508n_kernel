# 原厂 5.4 的 page_alloc_init_late 区域分析（对比 mainline 6.12 卡点）

> 任务文件：`prompt-stock-page-alloc-init.md`
> 结论先行：**原厂 5.4 在这个区域几乎"无事可做"**——`memblock_discard()` 在
> 两版都因 `CONFIG_ARCH_KEEP_MEMBLOCK=y` 被编译掉、`shuffle_free_memory()` 在
> mainline 因 `CONFIG_SHUFFLE_PAGE_ALLOCATOR` 未开被编译掉（原厂也默认不 shuffle，
> static key 默认关）。真正的新增执行量是 **6.12 把 `mem_init_print_info()` 和
> `buffer_init()` 搬进了 `page_alloc_init_late()`，从原厂的单核 pre-SMP 阶段
> 移到了 8 核全部在线的 kernel_init_freeable 阶段**；再加上 6.12 新增的
> `page_alloc_sysctl_init()`。卡点最可疑的就是这三个"post-SMP 首次执行"的步骤。

---

## 1. 原厂 config 对比（任务 1）

### 1.1 逐项对比

| 配置项 | 原厂 5.4 (`work/kernel.config`) | mainline 6.12 (`.config`) | 差异结论 |
|---|---|---|---|
| CONFIG_SHUFFLE_PAGE_ALLOCATOR | **y** | **n（未设置）** | mainline 的 `shuffle_free_memory()` 是空 inline，**编译掉**；原厂虽 =y，但由 `page_alloc_shuffle_key`（`DEFINE_STATIC_KEY_FALSE`）门控，**默认关**，仅 cmdline `shuffle` 参数可开。→ **两者 boot 时都不 shuffle，不是卡点** |
| CONFIG_DEFERRED_STRUCT_PAGE_INIT | n | n | 相同，deferred 块两版都跳过 |
| CONFIG_ARCH_KEEP_MEMBLOCK | **y** | **y** | **两版都 =y** → `memblock_discard()` 被 `#ifndef CONFIG_ARCH_KEEP_MEMBLOCK` 编译掉（mm/memblock.c:363 两版相同）。→ **不是卡点**（任务列出的"memblock_discard"步骤在两版都是空操作） |
| CONFIG_NUMA | **n** | **y**（OF_NUMA/GENERIC_ARCH_NUMA/NODES_SHIFT=4） | **mainline 独有**。DT 无 numa-map → `dummy_numa_init()` 建单 node 0 覆盖全内存，功能上 1 节点，但 NUMA 代码路径（`for_each_node_state`/node_zonelist/zonelist）与 stock 不同 |
| CONFIG_ZONE_DMA | n（仅 DMA32） | **y**（+DMA32） | mainline 多了 ZONE_DMA；DRAM 从 0x80000000 起，zone 布局与 stock 不同 |
| CONFIG_MEMORY_HOTPLUG | y（HOTPLUG_SPARSE） | y（+HOTREMOVE） | 都开，不影响本区域 |
| CONFIG_BLOCK / CONFIG_BLK_DEV_INITRD | y / y | y / y | 都开，`buffer_init()` 两版都编译 |
| CONFIG_FORCE_MAX_ZONEORDER | 11 | 未设（MAX_PAGE_ORDER=10 默认） | pageblock 大小不同（4MB vs 2MB），只影响 `set_zone_contiguous` 循环次数，不构成挂死 |
| CONFIG_ARM64_ERRATUM_2441007 | — | n | 未开（A55 repeat-TLBI），与本区域无直接关系 |

### 1.2 结论

- 任务中"待查"的 `CONFIG_SHUFFLE_PAGE_ALLOCATOR`：**mainline 未开 → shuffle 完全编译掉**，不可能卡在 shuffle。
- `memblock_discard`：**两版都被编译掉**（`CONFIG_ARCH_KEEP_MEMBLOCK=y` 两版一致），原厂反汇编证实原厂该函数体里没有 memblock_discard 调用（见 §4）。任务假设的"执行 memblock_discard"在两版都不成立。
- **唯一实质 config 偏离**：mainline 开了 `CONFIG_NUMA=y`（原厂关）与 `CONFIG_ZONE_DMA=y`（原厂关）。

---

## 2. 原厂 5.4 的 page_alloc_init_late 实现（任务 2）

### 2.1 5.4 源码（`mm/page_alloc.c:1929`，已从 v5.4.210 检出）

```c
void __init page_alloc_init_late(void)
{
	struct zone *zone;
	int nid;
#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT   // 两版都未开 → 整块跳过
	...
#endif
	/* Discard memblock private memory */
	memblock_discard();                  // ← CONFIG_ARCH_KEEP_MEMBLOCK=y → 编译掉

	for_each_node_state(nid, N_MEMORY)
		shuffle_free_memory(NODE_DATA(nid));  // ← static key 默认关 → boot 时跳过

	for_each_populated_zone(zone)
		set_zone_contiguous(zone);            // ← 唯一实际执行的工作
}
```

**5.4 的 `page_alloc_init_late()` 实际只做 `set_zone_contiguous()`。**

### 2.2 6.12 源码（`mm/mm_init.c:2291`，当前工作区含探针）

```c
void __init page_alloc_init_late(void)
{
	...
	mem_init_print_info();                // ← 6.12 新增（5.4 在 arch mem_init()，pre-SMP）
	buffer_init();                        // ← 6.12 新增（5.4 在 start_kernel()，pre-SMP！）
	memblock_discard();                   // ← 两版都被编译掉
	shuffle_free_memory(...);             // ← mainline 编译掉（SHUFFLE 未开）
	set_zone_contiguous(zone);            // ← 与 5.4 相同
	page_alloc_sysctl_init();             // ← 6.12 新增（5.4 的 vm sysctl 在 start_kernel 注册）
}
```

### 2.3 调用时序对比（关键）

| 函数 | 5.4 调用点 | 6.12 调用点 | 执行环境 |
|---|---|---|---|
| `buffer_init()` | `start_kernel()`（init/main.c:759） | **`page_alloc_init_late()` 内**（mm/mm_init.c:2321） | 5.4：CPU0 单核、pre-SMP；6.12：**8 核全在线 post-SMP** |
| `mem_init_print_info()` | arch mem_init()（arch/arm64/mm/init.c:552） | **`page_alloc_init_late()` 内**（mm/mm_init.c:2319） | 同上 |
| `page_alloc_sysctl_init()` | 无（sysctl 在 start_kernel 早期注册） | `page_alloc_init_late()` 内（mm/mm_init.c:2340） | 6.12 post-SMP 首次 |
| `set_zone_contiguous()` | `page_alloc_init_late()` | `page_alloc_init_late()` | 两版相同位置 |

原厂在 `kernel_init_freeable()` 里的顺序（5.4 init/main.c:1187-1192）：
`smp_init → sched_init_smp → page_alloc_init_late → page_ext_init → do_basic_setup`。
mainline 在 `page_alloc_init_late` 之前还插入了 `workqueue_init_topology → async_init →
padata_init`（这些探针都 ✓），然后才进 `page_alloc_init_late`。

**5.4 中 `buffer_init()` 在 start_kernel 阶段（单核、无二级核）执行；6.12 把它搬进
post-SMP 的 `page_alloc_init_late()`——这是两版在本区域最大的结构性差异。**

---

## 3. buffer_init / shuffle / set_zone_contiguous 逐函数对比（任务 3）

### 3.1 buffer_init（fs/buffer.c）

| | 5.4 (v5.4.210 fs/buffer.c:3430) | 6.12 (fs/buffer.c:3171) |
|---|---|---|
| 建 cache | `kmem_cache_create("buffer_head", ..., SLAB_RECLAIM_ACCOUNT\|SLAB_PANIC\|SLAB_MEM_SPREAD)` | `KMEM_CACHE(buffer_head, SLAB_RECLAIM_ACCOUNT\|SLAB_PANIC)` |
| 计数 | `nr_free_buffer_pages() * 10 / 100` | 相同 |
| 注册 cpuhp | `cpuhp_setup_state_nocalls(CPUHP_FS_BUFF_DEAD, ...)` | 相同 |

函数体几乎一致（差异仅是 SLAB_MEM_SPREAD 在 6.x 删除）。**代码相同，但执行时机不同**：
5.4 在 start_kernel（单核）执行；6.12 在 `page_alloc_init_late`（8 核在线）执行。
`cpuhp_setup_state_nocalls` → `__cpuhp_setup_state` → `cpus_read_lock()`（percpu rwsem
快路径 `this_cpu_inc`，无跨核流量）+ `mutex_lock(&cpuhp_state_mutex)` + 写 cpuhp 回调表。
这是 T770 上 **post-SMP 阶段首次做 CPU hotplug 状态注册 + 新建 slab cache** 的位置之一。

### 3.2 shuffle_free_memory（mm/shuffle.c）

- 5.4 与 6.12 的 `__shuffle_zone` / `__shuffle_free_memory` 算法**逐行相同**（Fisher-Yates，
  `spin_lock_irqsave(&z->lock)` 持锁 + `get_random_long` + `list_swap` + 每 100×order_pages
  `cond_resched`）。仅改名：`page_order()`→`buddy_order()`、`add_to_free_area_random`→`shuffle_pick_tail`。
- 6.12 因 `CONFIG_SHUFFLE_PAGE_ALLOCATOR` 未开，`shuffle_free_memory()` 是空 inline（mm/shuffle.h:40）。
- 原厂 =y 但 static key 默认关（`shuffle_show/store` 只在 cmdline/module 参数时开）。
- **结论：两版 boot 时都不 shuffle，不是卡点。**

### 3.3 set_zone_contiguous（5.4 mm/page_alloc.c:1549 vs 6.12 mm/mm_init.c:2268）

- 循环体相同：按 pageblock 步进，`__pageblock_pfn_to_page()` 遇到空洞 `return`，否则
  `cond_resched()`，全通后 `zone->contiguous = true`。
- 5.4 用 `ALIGN(block_start_pfn + 1, pageblock_nr_pages)`，6.12 用 `pageblock_end_pfn()`
  （语义相同）。
- `__pageblock_pfn_to_page`：5.4（page_alloc.c:1525）与 6.12（page_alloc.c:1388）仅
  `pfn_valid` 检查顺序略有差异，逻辑等价。
- **结论：函数本身与原厂同构。** 差异只在 zone 布局（mainline 多了 ZONE_DMA、NUMA=y），
  若卡在该函数内，需核对 zone_start_pfn/zone_end_pfn。

---

## 4. 反汇编原厂 page_alloc_init_late（任务 4）

符号：`stock_kallsyms.map` 行 90597 → `page_alloc_init_late = 0xffffffc01216ab1c`
（init 段内：`__init_begin=0xffffffc012140000`，`__init_end=0xffffffc012350000`）。

反汇编（`objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000`，
范围 ab1c..abb4，下一符号 `init_cma_reserved_pageblock=abb4`）：

```
ab1c:  str x30,[x18],#8          ; fentry
ab20:  stp x29,x30,[sp,#-48]!
ab30:  adrp x19, <data>          ; x19 = 0xffffffc01245a740 (node/zone 基址)
ab38:  nop                       ; ← jump label 占位（shuffle static key，默认 nop=关）
ab3c:  ldr x8,[x19,#112]; cbz x8 → ab4c
ab44:  mov x0,x19
ab48:  bl  0xffffffc0104975ec    ; ← set_zone_contiguous（kallsyms 行 8982 确认）
ab4c:  ... 循环推进 zone 指针（步长 0x680），cbnz 回 ab3c
ab74:  ret
ab78:  ...（冷路径）shuffle 循环：
abac:  bl  0xffffffc01169d8e4    ; ← __shuffle_zone（kallsyms 行 48901 确认）
```

**反汇编证实**：
1. 原厂 `page_alloc_init_late` 主循环**只调 `set_zone_contiguous`**（bl 0x4975ec），
   **没有 memblock_discard 调用**（`CONFIG_ARCH_KEEP_MEMBLOCK=y` → 编译掉，与 §1 一致）。
2. shuffle 循环存在但挂在 **jump label（ab38 的 nop）后面**——static key 默认关，
   boot 时不会执行。
3. 函数体极小（~0x98 字节），因为两版"重活"（buffer_init/mem_init_print_info/
   sysctl）在原厂根本不在这里。

原厂 `buffer_init = 0xffffffc0121717f4`：反汇编显示 `bl kmem_cache_create_usercopy`
(0x44e064) → `bl nr_free_buffer_pages` (0x4959dc) → `bl __cpuhp_setup_state`
(0x1939cc)，与 6.12 函数体一致，只是调用点不同（原厂在 start_kernel，见 §2.3）。

---

## 5. 综合：最可疑差异 + 修复建议（任务 5）

### 5.1 最可疑差异（按嫌疑排序）

1. **`buffer_init()` 从 pre-SMP `start_kernel()` 搬到 post-SMP `page_alloc_init_late()`**
   （6.12 上游改动）。这是原厂能过、mainline 卡的最直接解释：原厂在 CPU0 单核、
   二级核未上线时执行 kmem_cache_create + 首次 cpuhp 状态注册；mainline 在
   8 核全部在线、`sched_init_smp/workqueue_init_topology/async_init/padata_init`
   之后的 kernel_init_freeable 里执行同一段代码。T770 的跨 cluster 一致性/
   原子问题此前已在 stop_machine（post-SMP 多核同步）上暴露，`buffer_init` 的
   cpuhp 注册 + slab 新建正是 post-SMP 首次执行的新代码路径。
2. **`mem_init_print_info()` 从 arch mem_init()（pre-SMP）搬到 `page_alloc_init_late()`**
   （post-SMP）。函数本身只是 printk + 计数，风险低，但同样属于"post-SMP 首次"。
3. **`page_alloc_sysctl_init()`（6.12 新增）**：`register_sysctl_init("vm", ...)`
   post-SMP 首次做 sysctl 表分配注册（原厂在 start_kernel 早期静态注册）。
4. **config 偏离**：mainline `CONFIG_NUMA=y`（原厂关）。虽然 DT 无 numa-map 时
   `dummy_numa_init` 退化为单节点，但 `for_each_node_state`/zonelist 路径与
   原厂（NUMA 关）不同；`CONFIG_ZONE_DMA=y`（原厂关）改变 zone 布局，
   影响 `set_zone_contiguous` 的遍历范围。

### 5.2 已排除的选项（不要再查）

- `shuffle_free_memory`：mainline 编译掉（SHUFFLE 未开）；原厂 static key 默认关。
- `memblock_discard`：两版都因 `CONFIG_ARCH_KEEP_MEMBLOCK=y` 编译掉（原厂反汇编证实）。
- `CONFIG_DEFERRED_STRUCT_PAGE_INIT`：两版都未开。

### 5.3 精确定位（下一步先做）

当前 `mm/mm_init.c` 内已有逐步骤探针，直接读屏即可定位卡点：

| 最后点亮的 bar | 卡在哪一步 | 说明 |
|---|---|---|
| (188,240) 亮、224 不亮 | `mem_init_print_info` | printk 卡（低概率，查 console/printk） |
| (224,240) 亮、260 不亮 | `buffer_init` | **最高嫌疑**：cpuhp 注册 / kmem_cache_create post-SMP |
| (260,240) 亮、296 不亮 | `memblock_discard` | **不可能**（编译掉）——若发生说明配置被改过 |
| (296,240) 亮、(8,230) 不亮 | `set_zone_contiguous` | 查 zone 布局（ZONE_DMA/NUMA 引入的 zone 差异） |
| (8,230) 亮、152 不亮 | `page_alloc_sysctl_init` | register_sysctl post-SMP 卡 |

### 5.4 修复建议

**A. 若卡在 `buffer_init`（最可能）——原厂式搬回 pre-SMP：**
   在 6.12 `start_kernel()`（如 5.4 init/main.c:759 位置，`proc_caches_init()` 之后）
   加 `buffer_init();`，并从 `page_alloc_init_late()` 删除该调用（或临时用变量门控）。
   这完全复刻原厂时序：单核执行 kmem_cache_create + cpuhp 注册。若这样能过，
   即证实"post-SMP 执行 buffer_init"是卡点根因。

**B. 若卡在 `page_alloc_sysctl_init`：**
   仿 5.4，把 `register_sysctl_init("vm", page_alloc_sysctl_table)` 提前到
   start_kernel 早期（sysctl_init 阶段）执行，或先注释掉验证。

**C. config 对齐原厂（低风险、建议顺手做）：**
   - `CONFIG_NUMA=n`（原厂关；T770 是单节点 SoC，开 NUMA 只会引入原厂没有的代码路径）；
   - `CONFIG_ZONE_DMA=n`（原厂只有 DMA32，改回后 zone 布局与原厂一致，
     `set_zone_contiguous` 遍历范围一致）；
   - 保持 `CONFIG_ARCH_KEEP_MEMBLOCK=y`（两版一致，不要关）。
   - `CONFIG_SHUFFLE_PAGE_ALLOCATOR` 可开可不开（原厂开了但 boot 时 static key 关，
     开不开都不影响 boot 行为；若想与原厂二进制更接近可开）。

**D. 若卡在 `set_zone_contiguous`：**
   在 mainline 加探针打印每个 zone 的 `zone_start_pfn / zone_end_pfn / present_pages`，
   与 stock 对比（stock 无 ZONE_DMA、NUMA 关）。重点看 ZONE_DMA 是否为空 zone
   或存在巨大的 `spanned_pages` 导致遍历超长。

**E. 对照实验（单变量）**：bootargs 加 `maxcpus=1`，若单核能过 → 进一步坐实
   跨核/post-SMP 同步问题；若单核也卡 → 与 SMP 无关，查函数本身。
