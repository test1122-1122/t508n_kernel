# 原厂 5.4 的 set_zone_contiguous / 内存布局分析（对比 mainline 6.12 卡点）

> 任务文件：`prompt-stock-zone-memory.md`
> 先读：`work/REPORT-stock-page-alloc-init.md`（上一轮 page_alloc_init_late 区域结论）
>
> **结论先行（最可疑差异）**：
> **原厂（SPRD 5.4.210 二进制）的 `pfn_valid()` / `pfn_to_online_page()` 对
> `MEMBLOCK_NOMAP`（DT `no-map`）区域返回 false/NULL —— 反汇编证实其 early-section
> 分支做 **memblock 二分查找 + NOMAP 标志拒绝**（`bic wN, wM, wN, lsr #2`）；
> mainline 6.12 arm64 用**通用 section 式 pfn_valid**（`early_section(ms) ||
> pfn_section_valid(ms, pfn)`），**完全不看 NOMAP**。
> 而我们的 DTS 恰好把 modem/安全世界保留区（sipc-mem@87800000、cp-modem@88000000、
> iq、tos、sml、audio…）全部标了 `no-map`（stock DTS 这些区域**都没有 no-map**，只是
> 普通 reserved）。于是 mainline 的 `set_zone_contiguous` 遍历**走进**了这些 no-map
> 空洞（stock 的 pfn_valid 会在第一个 no-map pageblock 处直接 return，根本不会走进去），
> 卡点恰好就在第一个 no-map 交界 —— sipc-mem / cp-modem（≈2.1–2.125GB，即“疑似
> 2GB 附近”），与探针 bar 位置完全吻合。**

---

## 1. 原厂 set_zone_contiguous + __pageblock_pfn_to_page 实现对比（任务 1）

### 1.1 5.4 源码（v5.4.210，mm/page_alloc.c:1521/1549，已从 stock-5.4.210 git 检出）

```c
struct page *__pageblock_pfn_to_page(unsigned long start_pfn,
				     unsigned long end_pfn, struct zone *zone)
{
	struct page *start_page, *end_page;

	end_pfn--;                                          // end_pfn 减一
	if (!pfn_valid(start_pfn) || !pfn_valid(end_pfn))   // ← 检查 start 和 end 两个 pfn
		return NULL;
	start_page = pfn_to_online_page(start_pfn);
	if (!start_page)
		return NULL;
	if (page_zone(start_page) != zone)
		return NULL;
	end_page = pfn_to_page(end_pfn);
	if (page_zone_id(start_page) != page_zone_id(end_page))
		return NULL;
	return start_page;
}

void set_zone_contiguous(struct zone *zone)
{
	unsigned long block_start_pfn = zone->zone_start_pfn;
	unsigned long block_end_pfn;

	block_end_pfn = ALIGN(block_start_pfn + 1, pageblock_nr_pages);
	for (; block_start_pfn < zone_end_pfn(zone);
	     block_start_pfn = block_end_pfn, block_end_pfn += pageblock_nr_pages) {
		block_end_pfn = min(block_end_pfn, zone_end_pfn(zone));
		if (!__pageblock_pfn_to_page(block_start_pfn, block_end_pfn, zone))
			return;
		cond_resched();
	}
	zone->contiguous = true;
}
```

### 1.2 6.12 源码（mm/page_alloc.c:1386 与 mm/mm_init.c:2268）

```c
struct page *__pageblock_pfn_to_page(unsigned long start_pfn,
				     unsigned long end_pfn, struct zone *zone)
{
	...
	end_pfn--;
	if (!pfn_valid(end_pfn))            // ← 6.12 只查 end，start 交给 pfn_to_online_page
		return NULL;
	start_page = pfn_to_online_page(start_pfn);
	...
	if (page_zone(start_page) != zone)
		return NULL;
	end_page = pfn_to_page(end_pfn);
	if (page_zone_id(start_page) != page_zone_id(end_page))
		return NULL;
	return start_page;
}
```

`set_zone_contiguous`（6.12）循环体与 5.4 同构，仅 `ALIGN(start+1, …)` → `pageblock_end_pfn()`
（语义相同）。**函数本身两版等价，循环都会正确终止（步进 = pageblock_nr_pages）。**

### 1.3 反汇编原厂 set_zone_contiguous（0xffffffc0104975ec）—— 关键证据

```
4975ec: str x30,[x18],#8            ; fentry
4975f0: stp x29,x30,[sp,#-32]!
4975fc: mov x19, x0                 ; x19 = zone
497600: ldr x0, [x19,#88]           ; block_start_pfn = zone->zone_start_pfn
497604: ldr x8, [x19,#104]          ; spanned_pages
497608: add x8, x8, x0              ; zone_end_pfn
497614: add x9, x0, #0x400          ; ALIGN(start+1, 1024)  ← pageblock = 1024 pages = 4MB
497618: and x9, x9, #0xfffffffffffffc00
49761c: cmp x9, x8
497620: csel x20, x9, x8, cc        ; block_end = min(…, zone_end)
497624: mov x1, x20                 ; arg2 = end_pfn
497628: mov x2, x19                 ; arg3 = zone
49762c: bl  0xffffffc010497338      ; __pageblock_pfn_to_page(start, end, zone)
497630: cbz x0, 497658              ; NULL → return（空洞 → 直接放弃该 zone）
497634: … reload zone_start/spanned …
497648: add x9, x20, #0x400         ; block_end += 1024
49764c: b.cc 49761c                 ; 循环
497650: mov w8, #1
497654: strb w8, [x19,#1469]        ; zone->contiguous = true
```

**反汇编证实：原厂 set_zone_contiguous 循环体里没有 cond_resched（紧凑循环）**——
与任务描述“去掉 cond_resched 仍卡”一致（原厂本来就没有；调度/软锁不是嫌疑）。

### 1.4 反汇编原厂 pfn_valid（0xffffffc010126b38，独立函数）—— 决定性证据

```
126b38: lsr x8, x0, #52 ; cbnz → return 0      ; pfn < 2^52
126b40: lsr x9, x0, #36 ; cbnz → return 0      ; pfn < 2^36
126b48: adrp x8, …61d000
126b4c: ldr x14, [x8, #3384]                   ; mem_section 根数组
126b50: cbz → return 0
126b54: lsr x9, x0, #20 ; and #0xffffffffff8   ; root 索引（128MB section, EXTREME, 256/root）
126b5c: ldr x8, [x14, x9]                      ; root 指针
126b60: cbz → return 0
126b64: lsr x9, x0, #15
126b68: adds x9, x8, w9, uxtb #4               ; &mem_section[pfn>>15 & 0xFF]
126b70: ldr x9, [x9]                           ; section_mem_map
126b74: tbnz w9, #1 → 126b80                   ; SECTION_HAS_MEM_MAP (bit1)
126b78: return 0
126b80: tbnz w9, #3 → 126ba4                   ; SECTION_IS_EARLY (bit3) → memblock 分支
126b84: … subsection_map 位测试 (pfn_section_valid) …
126ba4: adrp x8, …3a6000 ; add #0x3c0          ; &memblock.memory
126bac: ldr w12, [x8]                          ; memblock.memory.cnt
126bb0: ldr x8, [x8, #24]                      ; memblock.memory.regions
126bb8: lsl x9, x0, #12                        ; phys = pfn << 12
126bbc: … 二分查找 region->base/size …
126c14: ldr w8, [x8, #16]                      ; region->flags
126c18: mov w9, #1
126c1c: bic w0, w9, w8, lsr #2                 ; w0 = 1 & ~(flags >> 2)
                                               ;  ← flags 的 bit2 = MEMBLOCK_NOMAP(0x4)
                                               ;     NOMAP → w0 = 0 → return 0
126c20: ret
```

**原厂 pfn_valid 语义 = section 有效 && (early section → `memblock_is_map_memory(phys)`
（**拒绝 NOMAP**） | 非 early → subsection_map 位测试)。**
`__pageblock_pfn_to_page`（0x497338）内联了同一逻辑：start_pfn 走
`pfn_to_online_page`（0x4974f8 查 SECTION_IS_ONLINE + 0x4974ec 同样 NOMAP 拒绝），
end_pfn 走 pfn_valid（0x4974f0 前同样有 NOMAP 拒绝）。

→ **结论：只要 pageblock 落在 `no-map` 区域，原厂 `__pageblock_pfn_to_page` 必然返回
NULL，`set_zone_contiguous` 立刻 return（zone->contiguous 保持 false）。原厂遍历
“遇洞即停”，绝不过洞。**

### 1.5 mainline 6.12 的 pfn_valid —— 没有 NOMAP 检查

- arm64 6.12 **不定义自己的 pfn_valid、不选 HAVE_ARCH_PFN_VALID**（grep 证实）→ 用
  include/linux/mmzone.h:2024 的通用 SPARSEMEM 版：

```c
static inline int pfn_valid(unsigned long pfn)
{
	...
	if (pfn_to_section_nr(pfn) >= NR_MEM_SECTIONS)
		return 0;
	ms = __pfn_to_section(pfn);
	if (!valid_section(ms))            // 只查 SECTION_HAS_MEM_MAP
		return 0;
	ret = early_section(ms) || pfn_section_valid(ms, pfn);   // ← 无 memblock、无 NOMAP
	return ret;
}
```

- `pfn_to_online_page`（6.12 mmzone.h / 反汇编 0x247750）：查 SECTION_IS_ONLINE +
  subsection_map 位 —— 同样**不看 NOMAP**。
- 已反汇编当前 mainline_Image（0xffffffc08023ccfc）确认二进制与源码一致；整个函数只含
  vmemmap struct page 读、mem_section 表读、NODE_DATA 表读、per-CPU preempt_count 加减，
  **没有任何原子/LSE 指令**（排除原子类卡点）。

### 1.6 任务 1 小结

| | 原厂 5.4 二进制 | mainline 6.12 |
|---|---|---|
| pfn_valid 对 no-map pfn | **false**（memblock 查找 + NOMAP 拒绝） | **true**（early_section 即真） |
| pfn_to_online_page 对 no-map pfn | NULL（同样拒绝） | 返回 struct page |
| 遍历行为 | 第一个 no-map pageblock 即 return | **穿过所有 no-map 空洞继续走** |
| cond_resched | 无（二进制证实） | 探针已删 |
| pageblock | 1024 页 = 4MB | 512 页 = 2MB（仅影响循环次数） |

---

## 2. 原厂内存布局（任务 2，关键）

### 2.1 DTS memory 节点对比

| | 起始 | 大小 | 范围 |
|---|---|---|---|
| stock `stock_ums9620.dts` | 0x80000000 | **0x2_00000000 = 8GB（占位）** | 2GB–10GB，LK 裁剪到实际 |
| 我们 `ums9620-ot5.dts` | 0x80000000 | **0xc0000000 = 3GB** | **2GB–5GB 连续**（LK: cs0 1.5G + cs1 1.5G） |

3GB LPDDR4x @2GB 起、连续到 5GB；无 2GB 边界断档。

### 2.2 reserved-memory 对比（关键差异！）

位置相同，**属性不同**：

| 区域 | 地址 | stock | 我们 |
|---|---|---|---|
| ddrbist | 0x80000000 (4K) | 普通 reserved | **no-map** |
| sysdumpinfo/sysdump | 0x80001000 (4K) | 普通 reserved | **no-map** |
| sipc-mem | 0x87800000 (8M) | 普通 reserved | **no-map** |
| cp-modem | 0x88000000 (464M) | 普通 reserved | **no-map** |
| iq-mem | 0xaa000000 (64M) | 普通 reserved | **no-map** |
| ch-ddr | 0xae600000 (3M) | 普通 reserved | **no-map** |
| audio-mem | 0xaf700000 | **3M** 普通 reserved | **9M no-map** |
| audiodsp | 0xafa00000 (6M) | 普通 reserved | **no-map** |
| sysdump-uboot | 0xb5000000 (16M) | 普通 reserved | **no-map** |
| uboot-log | 0xbbe80000 (512K) | 普通 reserved | **no-map** |
| sml-mem | 0xbd000000 (256K) | 普通 reserved | **no-map** |
| tos-mem | 0xbd040000 (63M) | 普通 reserved | **no-map** |
| sh-mem | 0xc3000000 (1M) | 普通 reserved | **no-map** |
| dptx-hdcp | 0xfff10000 | **0x60000** 普通 reserved | **0x10000 no-map** |
| cma_share | 0xf0000000 (48M reusable CMA) | **有** | **无** |
| rebootescrow | 0xfff70000 (64K) | no-map | **无** |
| ramoops | 0xfff80000 (256K) | 有 | 有 |

**stock DTS 除 rebootescrow 外没有任何 no-map**；我们把全部 modem/安全区标了 no-map。
在 mainline 上 `no-map` → `memblock_mark_nomap()`（drivers/of/of_reserved_mem.c:154，
只打标志、**不 memblock_remove**），区域仍在 memblock.memory 里。

### 2.3 zone 布局（重点：两版遍历范围一致）

**stock（ZONE_DMA=n、ZONE_DMA32=y、NUMA=n、4K、FORCE_MAX_ZONEORDER=11）：**
- ZONE_DMA32：0x80000–0x100000（2–4GB）
- ZONE_NORMAL：0x100000–0x140000（4–5GB）

**mainline（ZONE_DMA=y + DMA32=y、NUMA=y、4K、ARCH_FORCE_MAX_ORDER=10）：**
- `zone_sizes_init`（arch/arm64/mm/init.c:122）：`zone_dma_limit = min(根节点
  dma-ranges(无 → PHYS_ADDR_MAX), ACPI IORT(无))` → 因 DRAM 起点 < 4GB 再
  `min(…, U32_MAX)` = 4GB → `arm64_dma_phys_limit = max_zone_phys(4GB) = 4GB`
- ZONE_DMA：0x80000–0x100000（2–4GB）
- ZONE_DMA32：空（DMA 已盖满 4GB 以下）
- ZONE_NORMAL：0x100000–0x140000（4–5GB）

**两版 set_zone_contiguous 遍历的物理范围完全相同（2–4GB + 4–5GB）。**
zone 边界本身不是差异；差异在于**过洞行为**（任务 1 的 pfn_valid）。

### 2.4 卡点位置推算（与探针 bar 吻合）

mainline 2MB/pageblock：ZONE_DMA 从 0x80000 起，第 i 块覆盖
[0x80000+(i-1)·512, 0x80000+i·512)：

| 迭代 | pfn | 物理 | 区域 |
|---|---|---|---|
| 1–60 | … | 2.000–2.109GB | 真实内存（ddrbist 4K no-map 在块 1 头部，mainline pfn_valid=TRUE 继续走） |
| **61** | 0x87800 | **2.109GB** | **sipc-mem no-map 起点** |
| 65 | 0x88000 | **2.125GB** | **cp-modem no-map 起点** |

探针 bar n（x=44+10n, y=230）在迭代 8(n+1)+1 绘制（bar0 迭代 9 … bar7 迭代 65）。
→ **“疑似 2GB 附近、遍历中途” = 第一个 no-map 交界（sipc-mem/cp-modem），
bar 停在 6–7 格。**

---

## 3. 原厂 vs mainline config 对比（任务 3）

| 配置项 | stock 5.4 | mainline 6.12 | 影响 |
|---|---|---|---|
| CONFIG_ZONE_DMA | 无 | **y** | zone 结构：DMA vs DMA32（遍历范围相同，见 §2.3） |
| CONFIG_ZONE_DMA32 | y | y | 相同 |
| CONFIG_NUMA | 无 | **y**（GENERIC_ARCH_NUMA/OF_NUMA，NODES_SHIFT=4） | page_to_nid 读 page->flags；单节点 dummy → nid=0，无实质影响 |
| CONFIG_SPARSEMEM / _VMEMMAP / _EXTREME | y / y / y | y / y / y | 相同 |
| CONFIG_FORCE_MAX_ZONEORDER | **11** | 无（ARCH_FORCE_MAX_ORDER=10） | pageblock 4MB vs 2MB（仅循环次数） |
| CONFIG_ARM64_4K_PAGES | y | y | 相同 |
| CONFIG_ARM64_VA_BITS / PA_BITS | 39 / 48 | 39 / 48 | 相同 |
| CONFIG_ARCH_KEEP_MEMBLOCK | **y** | **y** | **两版都保留 memblock → mainline 可随时用 memblock_is_map_memory/pfn_is_map_memory 复刻原厂 NOMAP 检查** |
| CONFIG_HUGETLB_PAGE | 无 | y | 无关本卡点 |
| CONFIG_SHUFFLE_PAGE_ALLOCATOR | y（static key 默认关） | 无 | 无关（boot 不 shuffle） |
| CONFIG_MEMORY_HOTPLUG / HOTREMOVE | y / 无 | y / y | 无关 |
| CONFIG_DEFERRED_STRUCT_PAGE_INIT | 无 | 无 | 相同，跳过 |

**config 层面唯一实质差异：mainline 的 ZONE_DMA=y 与 NUMA=y**（都不改变本卡点的遍历
物理范围，见 §2.3；不是根因，但建议对齐原厂，见 §5）。

---

## 4. vmemmap 映射（任务 4）

### 4.1 6.12 在 T770 3GB 布局下的 vmemmap 完整性 —— 分析结论：**是完整的**

- 6.12 `sparse_init`（mm/sparse.c:487）：`memblocks_present()` 用
  `for_each_mem_pfn_range`（mm/memblock.c `__next_mem_pfn_range`，**无 NOMAP 过滤**）
  遍历 memblock.memory（no-map 区域也在其中）→ `memory_present()` 把覆盖 2–5GB 的
  section 16–39 全部标 **present + SECTION_IS_ONLINE**；
- `sparse_init_nid` 对每个 present section 调 `__populate_section_memmap` →
  `vmemmap_populate`（4K 全 section 走 PMD 2MB 大页映射）→ `sparse_init_one_section`
  置 HAS_MEM_MAP；→ **no-map 区域的 section 一样有 vmemmap**；
- `memmap_init`（mm/mm_init.c:930）同样遍历 memblock.memory 对 2–5GB 每个 pfn
  （含 no-map）调 `__init_single_page` 初始化 struct page；
- memblock 分配（memblock.c:1082 `memblock_should_skip`）**跳过 NOMAP 区域** →
  vmemmap 后备页分配在低位真实 DRAM，不会落在 modem 区内；
- pfn_to_page = vmemmap + (pfn − PHYS_PFN_OFFSET)·64，VA_BITS=39 下 vmemmap 区 4GB
  （VMEMMAP_START=0xffffffc000000000 附近），2–5GB 全部落在已映射区。

→ **pfn_to_page / struct page 读取在软件层面全部有效，vmemmap 不缺映射。**
mainline 遍历 no-map 区域读到的 struct page 是**已初始化、zone/nid 正确**的
（nid=0、zone=DMA/NORMAL）。

### 4.2 那为什么还挂？—— 机制层面的诚实结论

静态分析穷尽了遍历路径上的所有访问（mem_section 表、vmemmap struct page、
NODE_DATA、preempt_count），**均有效且无原子指令**；循环步进有界。即：
**纯 MM 逻辑层面 mainline 遍历本应像原厂一样跑完。** 但实测它停在第一个 no-map
交界（sipc/cp-modem），而原厂（1）其 DTS 这些区域根本**不是 no-map**（普通 reserved，
遍历它们无害），（2）即便标了 no-map，原厂 pfn_valid 也会在第一个 no-map pageblock
直接 return，**根本不会过洞**。因此最合理的机制判断是：

- **主嫌疑（结构性、可操作）**：mainline 在 `set_zone_contiguous` 里**越过了 no-map
  空洞**，首次触碰 modem/安全世界区域边界时发生**同步异常（data abort）或总线级 stall**；
  本机无 UART/console，异常→oops/panic 路径不可见，表现为“卡死”（LCD 停在上一个 bar）。
  具体出错指令无法从静态分析唯一确定（可能与 no-map 区域在页表/预取/页表遍历上的
  特殊状态有关，或与 T770 的 modem 内存访问特性有关）。
- **次嫌疑（已被排除项）**：vmemmap 缺失 / struct page 未初始化 / 原子指令 /
  调度 / zone 边界计算错误 / 循环不终止 —— 均已在上述分析中排除。

---

## 5. 综合：最可疑差异 + 修复建议（任务 5）

### 5.1 最可疑差异（按嫌疑排序）

1. **pfn_valid/pfn_to_online_page 的 NOMAP 处理**：原厂二进制明确拒绝 no-map
   （memblock 查找 + `~(flags>>2)`），mainline 6.12 通用 pfn_valid 不看 NOMAP。
   → 结合我们 DTS 把全部 modem/安全区标 no-map，mainline 遍历**过洞**，卡在第一个洞
   （sipc-mem@0x87800000 / cp-modem@0x88000000，≈2.1GB，与“疑似 2GB 附近”完全吻合）。
2. **DTS 保留区属性与 stock 不一致**：stock 这些区域是普通 reserved（无 no-map），
   我们是 no-map；尺寸也有出入（audio 9M vs 3M、dptx-hdcp 64K vs 384K），且缺
   cma_share/rebootescrow。
3. （次要）CONFIG_ZONE_DMA=y、CONFIG_NUMA=y 与原厂不同 —— 不改变遍历范围，但建议对齐。

### 5.2 修复建议

**A. 复刻原厂“遇 no-map 即停”（推荐、治本）：**
在 6.12 `mm/page_alloc.c __pageblock_pfn_to_page()` 开头加 NOMAP 检查，语义与
原厂 pfn_valid 完全一致：

```c
	/* end_pfn is one past the range we are checking */
	end_pfn--;

	/* T508N: 复刻原厂 —— no-map 区域视为空洞，遇洞即停 */
	if (!pfn_is_map_memory(start_pfn) || !pfn_is_map_memory(end_pfn))
		return NULL;

	if (!pfn_valid(end_pfn))
		return NULL;
	...
```

- arm64 + CONFIG_ARCH_KEEP_MEMBLOCK=y（两版都 =y）时
  `pfn_is_map_memory(pfn)` = `memblock_is_map_memory(pfn<<12)`，**排除 NOMAP**（
  mm/memblock.c:1889-1895 `return !memblock_is_nomap(...)`）—— 与原厂反汇编行为逐位一致。
- 效果：ZONE_DMA 遍历在第一个 no-map pageblock（ddrbist 或 sipc，取决于从哪开始）
  直接 return，`zone->contiguous=false`（安全默认值，与原厂一致），**不再过洞**。
- 若不想动核心 mm，等价做法是在 `set_zone_contiguous` 循环里对每个 block 先做
  `memblock_is_nomap(PFN_PHYS(block_start_pfn))` 判断后 `return`。

**B. DTS 对齐 stock（配合 A，或单独先试）：**
把 `ums9620-ot5.dts` reserved-memory 各区域**去掉 `no-map`**（与 stock 同为普通
reserved），并补齐差异（audio-mem 改 3M、dptx-hdcp 改 0x60000、加
cma_share@f0000000 48M reusable CMA、加 rebootescrow@fff70000 no-map）。
注意：仅去 no-map 不会改变 mainline 的 section 式 pfn_valid（遍历仍会过洞），
所以单独用 B 可能仍卡；B 的价值是与 stock DTS 逐位对齐、消除线性映射空洞，并配合 A
达到与原厂完全一致的引导布局。

**C. config 对齐原厂（低风险、顺手做）：**
- `CONFIG_NUMA=n`（T770 单节点；去掉 dummy_numa/page_to_nid 路径，与原厂一致）；
- `CONFIG_ZONE_DMA=n`（保留 DMA32，zone 结构与原厂一致：DMA32 2–4GB + NORMAL 4–5GB）；
- `CONFIG_ARCH_FORCE_MAX_ORDER=11`（可选，pageblock 4MB，与原厂遍历粒度一致）。
- 保持 `CONFIG_ARCH_KEEP_MEMBLOCK=y`（A 方案依赖它）。

**D. 验证探针：**
- 加打印/探针确认每个 zone 的 `zone_start_pfn / zone_end_pfn / spanned_pages /
  present_pages`（预期 DMA: 0x80000/0x100000/0x80000/0x80000，NORMAL:
  0x100000/0x140000/0x40000/0x40000）；
- 记录卡死时 y=230 亮了几格（6 格 → sipc 边界，7 格 → cp-modem 边界），用于坐实
  卡点就是第一个 no-map 交界；
- 若 A 生效，应看到 set_zone_contiguous 很快返回、(8,230) bar 点亮、继续
  page_alloc_sysctl_init。

---

## 附：本轮关键材料索引

- 原厂反汇编：`set_zone_contiguous=0xffffffc0104975ec`、
  `__pageblock_pfn_to_page=0xffffffc010497338`、`pfn_valid=0xffffffc010126b38`、
  `zone_sizes_init=0xffffffc012150244`（`work/stock_kallsyms.map`，file_off = vaddr − 0xffffffc010080000）
- mainline 反汇编：`__pageblock_pfn_to_page=0xffffffc08023ccfc`、
  `pfn_to_online_page=0xffffffc080247750`、`set_zone_contiguous=0xffffffc0801ff160`
  （当前 System.map 与 `work/mainline_Image` 同批 21:46 构建）
- 源码：stock-5.4.210（mm/page_alloc.c、include/linux/mmzone.h）、
  linux-6.12.103（mm/page_alloc.c、mm/mm_init.c、mm/sparse.c、mm/memblock.c、
  arch/arm64/mm/init.c、arch/arm64/mm/mmu.c、drivers/of/of_reserved_mem.c、
  include/linux/mmzone.h、include/asm-generic/memory_model.h）
