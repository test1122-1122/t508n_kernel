# PATCH：恢复 mem_sysfs_mutex + memory_dev_init 挪回 cpu_dev_init 之后（T508N 未验证项 2+3）

> 任务：`prompt-verify-mem-order.md`（2026-08-19）。材料：`work/linux-5.4.210/drivers/base/memory.c`（5.4 上游）、
> `linux-6.12.103/drivers/base/{memory.c,init.c}`（当前 bring-up 树，含 T508N 探针）、
> `work/DISASM-E-devm-驱动模型sysfs.md`（原厂反汇编，mem_sysfs_mutex@0x123f1070）、
> `work/REPORT-stock-memory-dev-init.md`、`work/UNISOC-MODS-AND-CONFIG.md`（§4 未验证项 2/3）。
> 本文件 = 分析 + 可编译补丁 + 依赖检查 + 死锁风险评估。**补丁已用 `git apply --check` 对当前树验证通过**
> （memory.c、init.c、二者合并均 OK），未改动源码树（只写本文件）。

---

## 0. TL;DR（结论摘要）

1. **5.4.210 的 `mem_sysfs_mutex`**：`static DEFINE_MUTEX`（memory.c:29），在 3 处持有 —— `memory_dev_init`
   （memory.c:805-812，**全程持有整个注册循环**，stock 二进制实锤：0x1218ec8c 内 mutex_lock(0x123f1070)→循环→mutex_unlock）、
   `create_memory_block_devices`（704-720，含错误回滚）、`remove_memory_block_devices`（740-749）。
   `init_memory_block`/`register_memory` **内部无锁**，锁由调用者持有。
2. **该锁保护什么（精确语义）**：不是"注册 vs /sys 访问"的完整互斥（5.4 的 sysfs state_store/state_show 路径
   根本不取它，只取 `device_hotplug_lock` + `dev->mutex` + 引用计数）。它串行化的是 **memory block 设备生命周期
   的复合操作序列**：`device_register`/`device_unregister` + `mem->section_count` 更新（memory.h 注释
   "serialized by mem_sysfs_mutex"）——即"创建/删除 block 的整段操作"彼此互斥（如并发 add/remove_memory、
   probe 属性并发触发创建），并保证"find→modify→unregister"不被并发 remove 打断。boot 期（memory_dev_init）
   无任何竞争者，锁是**纯结构对齐物**。
3. **6.12 用什么替代**：`device_hotplug_lock`（= `mem_hotplug_lock` percpu_rwsem，mm/memory_hotplug.c:208；
   add/remove_memory 持 write 侧，sysfs state_store 持 read 侧）+ xarray `memory_blocks`（RCU 安全查表）+
   device 引用计数。6.12 memory_dev_init 注释明说"boot 期无并发创建/删除，不需要 device_hotplug_lock"。
4. **补丁一（必做，对齐 stock 二进制）**：6.12 memory.c 加回 `static DEFINE_MUTEX(mem_sysfs_mutex)`，
   `memory_dev_init` 对**整个循环**加锁（非每 block）。**补丁二（可选，完整对齐 5.4 源码）**：
   `create/remove_memory_block_devices` 也加锁（相对 6.12 现有 device_hotplug_lock 冗余但无害，锁序单向无环）。
5. **补丁三（顺序）**：init.c 把 `memory_dev_init()` 挪到 `cpu_dev_init()` 之后（原厂 platform→cpu→memory→container；
   新序 auxiliary→node→cpu→**memory**→container）。依赖检查结论：**memory/node/cpu 三者 boot 期无相互硬依赖**
   （memory 只用 NODE_DATA/pgdat 与 devices_kset/system_kset；node_dev_init 不遍历 memory blocks；
   register_cpu_under_node 对缺失侧静默返回），挪移安全。
6. **死锁风险**：boot 路径 = 0（单 CPU0、唯一持有者、无反向锁序）；hotplug 扩展路径 = 新增唯一锁序边
   `mem_hotplug_lock(write) → mem_sysfs_mutex`，不存在任何 `mem_sysfs_mutex → mem_hotplug_lock` 路径 ⇒ 无环，
   与 5.4 证明过的设计一致。
7. **重要**：此补丁是**结构对齐（无害对齐）**，不改变 device_add 内部行为，**预计不消除** memory_dev_init
   device_register 间歇卡（根因仍在跨簇唤醒链路，见 §5）；它消除"6.12 独有无锁循环"这一结构差异，供后续 A/B 归因。

---

## 1. 5.4.210 vs 6.12.103 memory.c 对比（锁的获取/释放点、保护范围）

### 1.1 5.4.210（work/linux-5.4.210/drivers/base/memory.c）

| 函数 | 行 | 锁动作 | 临界区内容 |
|---|---|---|---|
| `memory_dev_init` | 805-812 | `mutex_lock` → 循环 → `mutex_unlock` | `add_memory_block(nr)` × N（每 block 一次 `init_memory_block` → `device_register`）；错误累计，循环后 panic |
| `create_memory_block_devices` | 704-720 | 同上 | `init_memory_block(block_id, MEM_OFFLINE)` × N + 失败时 `find_memory_block_by_id`+`unregister_memory` 回滚 |
| `remove_memory_block_devices` | 740-749 | 同上 | `find_memory_block_by_id`+`unregister_memory_block_under_nodes`+`unregister_memory` × N |
| `init_memory_block` | 628-654 | **无锁** | 查重（klist 扫描）→ kzalloc → 填字段 → `register_memory`（=device_initialize+device_add） |
| `register_memory` | ~606-621 | **无锁** | device_register（内部） |

- 定义：memory.c:29 `static DEFINE_MUTEX(mem_sysfs_mutex);`；include/linux/memory.h:29 注释
  `int section_count; /* serialized by mem_sysfs_mutex */`。
- **stock 二进制实锤**（DISASM-E §2.5 / REPORT-stock §2.3）：memory_dev_init（0xffffffc01218ec8c）=
  `subsys_register` → `mutex_lock(0x123f1070)` → 循环（present 计数 + `init_memory_block` 0xbc1610 + section_count 写）
  → `mutex_unlock`。create_memory_block_devices（0xbc134c）同样 `mutex_lock(0x123f1070)` 包循环。
  ⇒ **原厂 = 全程持有（一次 lock/unlock 包住整个循环），不是每 block 一锁**。

### 1.2 6.12.103（当前树）

| 函数 | 行 | 现状 |
|---|---|---|
| `memory_dev_init` | 974-1018 | **无锁**。注释（969-973）："boot 期无并发创建/删除，device_hotplug_lock 不需要" |
| `create_memory_block_devices` | 882-912 | **无锁**。注释（878-880）："Called under device_hotplug_lock" |
| `remove_memory_block_devices` | 921-940 | **无锁**。同上 |
| `add_memory_block`/`__add_memory_block` | 789-833 / 679-711 | 无锁；查重 = `xa_load(&memory_blocks)`（xarray），注册 = `device_register`，之后 `xa_store` |
| 定义 | — | `static DEFINE_XARRAY(memory_blocks)`（line 83）；全树无 `mem_sysfs_mutex`（grep 确认） |

6.12 删除该锁是上游有意为之（"不需要 device_hotplug_lock"），替代机制见 §1.4。

### 1.3 mem_sysfs_mutex 到底保护什么（回答任务关键问题）

精确语义（5.4 源码为准）：
- **创建/删除 block 的复合操作序列彼此互斥**：`create_memory_block_devices` / `remove_memory_block_devices`
  是"查重→注册/注销→登记/清计"的多步复合操作；锁保证并发 add_memory 与 remove_memory（或两个重叠 add）
  不会交错执行同 block 的 register/unregister（device_register 与 device_unregister 交错会破坏 kobject/kernfs 树）。
- **`mem->section_count` 的写写互斥**（5.4 才有该字段；memory.h 注释明示）。
- **"find→modify→unregister"原子性**：回滚路径（create 失败）与并发 remove 之间。
- **对 /sys 访问**：5.4 的 sysfs `state_show`/`state_store` **不取**此锁（state_store 只取
  `lock_device_hotplug_sysfs()`，memory.c:305）。/sys 侧防悬垂靠的是 device 引用计数 + klist/xarray 自身锁，
  online/offline 状态变化靠 `dev->mutex`。⇒ "memory block 注册与 /sys 访问的互斥"这一概括**不准确**，
  准确说法：**锁串行化"block 设备注册/注销复合操作"，/sys 状态访问由 device_hotplug_lock + dev->mutex + 引用计数保护**。
- boot 期（memory_dev_init）：无并发者，锁为空转——这就是"无害对齐"的依据。

### 1.4 6.12 的替代机制

| 5.4 mem_sysfs_mutex 的职责 | 6.12 替代 |
|---|---|
| 创建/删除复合操作互斥 | `device_hotplug_lock`（percpu_rwsem `mem_hotplug_lock`，mm/memory_hotplug.c:208）：`mem_hotplug_begin()`（cpus_read_lock + percpu_down_write）包住 add_memory/remove_memory ⇒ create/remove_memory_block_devices 天然互斥；sysfs state_store 走 `lock_device_hotplug_sysfs()`（read 侧） |
| 查表安全 | xarray `memory_blocks`（RCU + 内部锁），`xa_load` 无锁可读；`find_memory_block_by_id` 注释 "Called under device_hotplug_lock" |
| 生命周期 | device 引用计数（get_device/put_device）+ kref |
| boot 循环 | 无并发 ⇒ 显式注释"不需要锁" |

---

## 2. 补丁一：恢复 mem_sysfs_mutex（drivers/base/memory.c）

### 2.1 设计决策

- **定义**：`static DEFINE_MUTEX(mem_sysfs_mutex);` 放在 includes 之后、`MEMORY_CLASS_NAME` 之前
  （与 5.4 memory.c:29 同位置）。`DEFINE_MUTEX` 宏可用（device.h:24 已 include mutex.h；再显式加
  `#include <linux/mutex.h>` 与 5.4 对齐，无害）。
- **memory_dev_init：全程持有（一次 lock/unlock 包整个循环），不是每 block** —— 与 stock 二进制一致
  （DISASM-E §2.5 实锤）。每 block 一锁在 boot 语义等价，但与原厂指令结构不符，不采用。
- **hotplug 两处（create/remove_memory_block_devices）**：**默认建议一并加上**（完整对齐 5.4 源码 +
  stock create_memory_block_devices 反汇编 0xbc134c 也是持锁的）；但**标记为可选**——6.12 里这两处已被
  device_hotplug_lock（write 侧）完全串行化，加 mem_sysfs_mutex 是冗余（双重串行化），纯对齐目的。
  若只想做最小改动（只对齐 stock 二进制实锤的 memory_dev_init），可只打 hunk 1+2，跳过 hunk 3/4。
- 已用 `git apply --check` 验证：以下全部 hunk 单独与合并均能干净应用。

### 2.2 补丁（git apply 可直接应用；`---`/`+++` 头已归一化为 a/ b/）

```diff
--- a/drivers/base/memory.c
+++ b/drivers/base/memory.c
@@ -21,11 +21,14 @@
 #include <linux/mm.h>
 #include <linux/stat.h>
 #include <linux/slab.h>
+#include <linux/mutex.h>
 #include <linux/xarray.h>
 
 #include <linux/atomic.h>
 #include <linux/uaccess.h>
 
+static DEFINE_MUTEX(mem_sysfs_mutex);
+
 #define MEMORY_CLASS_NAME	"memory"
 
 static const char *const online_type_to_str[] = {
@@ -893,6 +896,8 @@
 			 !IS_ALIGNED(size, memory_block_size_bytes())))
 		return -EINVAL;
 
+	/* T508N: restore stock 5.4.210 locking (5.4 memory.c:704-720) */
+	mutex_lock(&mem_sysfs_mutex);
 	for (block_id = start_block_id; block_id != end_block_id; block_id++) {
 		ret = add_hotplug_memory_block(block_id, altmap, group);
 		if (ret)
@@ -908,6 +913,7 @@
 			remove_memory_block(mem);
 		}
 	}
+	mutex_unlock(&mem_sysfs_mutex);
 	return ret;
 }
 
@@ -929,6 +935,8 @@
 			 !IS_ALIGNED(size, memory_block_size_bytes())))
 		return;
 
+	/* T508N: restore stock 5.4.210 locking (5.4 memory.c:740-749) */
+	mutex_lock(&mem_sysfs_mutex);
 	for (block_id = start_block_id; block_id != end_block_id; block_id++) {
 		mem = find_memory_block_by_id(block_id);
 		if (WARN_ON_ONCE(!mem))
@@ -937,6 +945,7 @@
 		unregister_memory_block_under_nodes(mem);
 		remove_memory_block(mem);
 	}
+	mutex_unlock(&mem_sysfs_mutex);
 }
 
 static struct attribute *memory_root_attrs[] = {
@@ -993,7 +1002,13 @@
 	/*
 	 * Create entries for memory sections that were found
 	 * during boot and have been initialized
+	 *
+	 * T508N: restore stock 5.4.210 semantics - hold mem_sysfs_mutex
+	 * for the whole registration loop (stock binary: mutex_lock
+	 * before the loop, mutex_unlock after it; see
+	 * work/PATCH-mem-sysfs-order.md).
 	 */
+	mutex_lock(&mem_sysfs_mutex);
 	for (nr = 0; nr <= __highest_present_section_nr;
 	     nr += sections_per_block) {
 		extern void t508n_paint_bar(int x, int y, int w, int h);
@@ -1010,6 +1025,7 @@
 			panic("%s() failed to add memory block: %d\n", __func__,
 			      ret);
 	}
+	mutex_unlock(&mem_sysfs_mutex);
 
 	{
 		extern void t508n_paint_bar(int x, int y, int w, int h);
```

> 行号映射（当前树 / patched 后）：`memory_dev_init` 974（原循环 997-1012 → patched：mutex_lock 1011 紧邻 `for` 前、
> mutex_unlock 1028 紧邻循环 `}` 后）；`create_memory_block_devices` 882（patched：lock 900 / unlock 916）；
> `remove_memory_block_devices` 921（patched：lock 939 / unlock 948）。
> hunk 3/4（create/remove）为**可选**；hunk 1/2（定义 + memory_dev_init 全程持有）为**必做**。

---

## 3. 补丁二：memory_dev_init 挪回 cpu_dev_init 之后（drivers/base/init.c）

### 3.1 顺序对比

| | driver_init 后段顺序（of_core_init 之后） |
|---|---|
| 原厂 stock / 5.4.210（DISASM-E §1.5，8 成员） | platform → **cpu → memory** → container（无 auxiliary/node） |
| 6.12 当前树（14 成员） | platform → auxiliary → **memory → node → cpu** → container |
| **本补丁（6.12 保留新增成员，仅对 memory 对齐原厂）** | platform → auxiliary → node → cpu → **memory** → container |

### 3.2 补丁

```diff
--- a/drivers/base/init.c
+++ b/drivers/base/init.c
@@ -43,12 +43,12 @@
 	platform_bus_init();
 	auxiliary_bus_init();
 	t508n_paint_bar(8, 300, 24, 10);	/* auxiliary_bus_init 后 */
-	memory_dev_init();
-	t508n_paint_bar(44, 300, 24, 10);	/* memory_dev_init 后 */
 	node_dev_init();
 	t508n_paint_bar(80, 300, 24, 10);	/* node_dev_init 后 */
 	cpu_dev_init();
 	t508n_paint_bar(116, 300, 24, 10);	/* cpu_dev_init 后 */
+	memory_dev_init();
+	t508n_paint_bar(44, 300, 24, 10);	/* memory_dev_init 后 (原厂顺序: cpu→memory) */
 	container_dev_init();
 	t508n_paint_bar(152, 300, 24, 10);	/* container_dev_init 后 */
 	t508n_mark_row(260, 1);	/* 行号 1: driver_init 前段 */
```

> 探针说明：y=300 行的 bar 是"函数完成"标记，随函数移动（x=44 仍指 memory_dev_init，保持与旧日志可比）。
> 重排后屏幕点亮顺序变为 8(aux)→80(node)→116(cpu)→44(memory)→152(container)，仅显示顺序变化，探针语义不变。
> 若在意显示单调性，可把 memory 的 bar x 改为 128；本补丁保持 x=44 以最小化 diff。

### 3.3 依赖检查（挪移安全性）

**memory_dev_init 依赖清单**（挪到 cpu_dev_init 之后是否满足）：

| 依赖 | 提供者 | 就绪时点 | 满足 |
|---|---|---|---|
| devices_kset | devices_init()（init.c:29） | driver_init 前段 | ✅（本就靠前） |
| system_kset（memory_subsys 注册的 parent） | buses_init()（6.12 bus.c:1439，`kset_create_and_add("system",…,&devices_kset->kobj)`） | driver_init 前段 | ✅ |
| NODE_DATA(0)（=contig_page_data，NUMA=n） | setup_arch/bootmem | 远早于 driver_init | ✅ |
| early_node_zone_for_memory_block → pgdat->node_zones | 同上 | 同上 | ✅ |
| sparsemem sections / __highest_present_section_nr | mm 早期初始化 | 远早于 driver_init | ✅ |
| memory_block_size_bytes()（arch） | arch/arm64 | 同上 | ✅ |
| **cpu 设备 / node 设备** | cpu_dev_init / node_dev_init | — | **不依赖**（boot 路径零引用 cpu/node device；无 sysfs 链接创建） |

**node_dev_init / cpu_dev_init 是否依赖 memory 先建**（被"跨越"的两个成员）：

| 成员 | boot 期行为 | 对 memory blocks 的依赖 |
|---|---|---|
| node_dev_init（6.12 node.c:963-983） | subsys_system_register(&node_subsys) + `for_each_online_node → register_one_node`（register_node = device_register node 设备 + register_cpu_under_node 链接 cpu + node_init_caches） | **无**：`register_memory_blocks_under_node`（唯一遍历 memory blocks 的入口）只被 mm/memory_hotplug.c:1554（hotplug add_memory，MEMINIT_HOTPLUG）调用；MEMINIT_EARLY 变体（register_mem_block_under_node_early）在 6.12.103 无 boot 调用者。node_dev_init 自身不遍历 memory blocks |
| cpu_dev_init（6.12 cpu.c:665-672） | subsys_system_register(&cpu_subsys) + cpu_dev_register_generic()（arch_register_cpu 每核）+ vulnerabilities 组 | **无**：register_cpu → register_cpu_under_node 只碰 node 设备与 cpu 设备 |

**交叉链接的容错性**（boot 期唯一双向链接是 cpu↔node）：
- `register_cpu_under_node`（node.c:655-671）：`if (!node_online(nid)) return 0; obj = get_cpu_device(cpu); if (!obj) return 0;`
  —— 任一侧缺失都**静默返回 0**，两个方向都 order-tolerant。本补丁保持 node→cpu 相对顺序不变
  （node 先建，cpu_dev_init 注册 cpu 时建链接），与 6.12 上游一致，无行为变化。
- container_dev_init：纯 of 设备（arch_topology），不依赖 memory/cpu/node。原厂也是 container 最后，位置不变。

**结论**：新顺序 `...platform → auxiliary → node → cpu → memory → container` 的所有相对约束均满足；
memory_dev_init 后移跨越的 node/cpu 都不依赖 memory 先建，memory 也不依赖它们。**无破坏**。
（6.12 上游把 memory 排 node/cpu 之前属历史演进而非硬依赖——上游 register_one_node 注释虽提
"link the node to … memory block devices"，但 6.12.103 代码里 boot 路径并不做 memory 遍历。）

---

## 4. 死锁风险评估

### 4.1 boot 路径（memory_dev_init 全程持有 mem_sysfs_mutex）—— 风险 0

- driver_init 单线程（仅 CPU0）；mem_sysfs_mutex 是 memory.c 私有静态锁，**全系统唯一持有者 = memory.c 三处**，
  boot 期只有 memory_dev_init 会取 → 无竞争、无等待。
- 临界区内 `device_register → device_add` 会取用 kernfs per-root rwsem / klist 自旋锁 / uevent_sock_mutex /
  dev->mutex / SLUB·zone 锁，全部"获取→短临界→释放"，**这些锁的持有路径没有任何一处再回头取 mem_sysfs_mutex**
  （mem_sysfs_mutex 只被 memory.c 内部取用）⇒ 不存在反向边，无环。
- 持普通 mutex 跨 `device_register`（可睡路径：分配、mutex 等）合法；临界区内无 IRQ/原子上下文取锁点
  （`__add_memory_block` 的 xa_store 用 GFP_KERNEL 可睡，OK）。
- boot 期若有其它线程：DEVTMPFS=n ⇒ 无 kdevtmpfs 线程；migration/ksoftirqd/rcu 等 per-cpu kthread 均不取
  mem_sysfs_mutex。⇒ **死锁概率 0**。

### 4.2 hotplug 扩展路径（可选 hunk 3/4：create/remove 加锁）—— 风险 0（锁序单向）

新增唯一锁序边：
```
add_memory/remove_memory（mm/memory_hotplug.c）
  → mem_hotplug_begin(): cpus_read_lock → percpu_down_write(mem_hotplug_lock)   [write 侧]
  → create/remove_memory_block_devices → mutex_lock(mem_sysfs_mutex)             [新增边]
```
- sysfs 路径：`state_store → lock_device_hotplug_sysfs()`（mem_hotplug_lock **read** 侧）→ device_online/offline
  → memory_subsys_online/offline → memory_block_change_state → online_pages/offline_pages —— **全程不取 mem_sysfs_mutex**。
- 关键：**不存在任何"先持 mem_sysfs_mutex、再取 mem_hotplug_lock / cpus_read_lock"的路径**（memory.c 临界区内
  只有 device_register/unregister 及其子锁，不含 hotplug 锁）⇒ 锁序单向，无环。
- 与 5.4 的关系：5.4 同样是在 device_hotplug_lock（5.4 同名机制，memory_hotplug.c:57）之下再取 mem_sysfs_mutex，
  该组合在原厂/上游服役多年，无锁序告警记录 ⇒ 本补丁只是复刻已被证明的设计。
- 双重串行化说明：6.12 里 create/remove 已被 mem_hotplug_lock(write) 完全互斥，mem_sysfs_mutex 在此冗余；
  lockdep（若开启 CONFIG_PROVE_LOCKING）对"单向新类"无倒序告警。

### 4.3 与 6.12 特有锁的交互小结

| 锁 | 交互 | 风险 |
|---|---|---|
| mem_hotplug_lock（percpu_rwsem） | hotplug: write → mem_sysfs_mutex；sysfs: read 单独持有 | 无（单向） |
| kernfs per-root rwsem / iattr rwsem | 在 mem_sysfs_mutex 临界区**之内**取用并释放 | 无（内层短临界） |
| klist 自旋锁 / xarray 内部锁 | 同上 | 无 |
| dev->mutex（device_online 等） | sysfs 路径取；与 mem_sysfs_mutex 无交叠持有 | 无 |
| cpus_read_lock（mem_hotplug_begin 内） | 在 mem_sysfs_mutex 之前获取（hotplug） | 无（顺序固定） |

---

## 5. 对当前竞态（memory_dev_init device_register 间歇卡）的影响 —— 重要说明

- 该竞态观测为：循环已进入、部分 block 的 device_register 成功返回、随后某 block 卡在 device_add 内部
  （y=296 亮 / y=380 部分亮 / y=260 不亮）。device_add 内部无 completion/synchronize/跨核等待原语，
  卡死只可能来自"另一核持锁不释放 / 分配器被破坏"（REPORT-stock-memory-dev-init §0/§6.3）。
- mem_sysfs_mutex 只串行化 memory.c 内部操作（boot 期无竞争者），**不触碰** device_add 内部任何锁点 ⇒
  **恢复它不会改变竞态行为**。它与 memory_dev_init 顺序挪移同属"消除 6.12 独有结构差异"的**无害对齐**
  （UNISOC-MODS §4 未验证项 2/3；DISASM-E §4.6"复刻原厂时序实验：恢复 mem_sysfs_mutex（无害）+ 挪移顺序"）。
- 竞态根因仍指向 D1/D2（跨簇唤醒链路 CSD+IPI_CALL_FUNC 失败残留坏状态 → CPU0 在锁/分配点死等），
  与此补丁正交。做此对齐后，可把"memory 注册窗口"从根因排查中彻底排除。

---

## 6. 编译与验证建议

1. **应用方式**：本文件 §2.2 + §3.2 的 diff 已通过 `git apply --check`（memory.c / init.c / 合并三项均 OK），
   可直接 `git apply` 或手工落盘。只打必做部分 = §2.2 的 hunk 1+2。
2. **编译**：`make drivers/base/memory.o drivers/base/init.o`（增量，快）；全量 `make -j$(nproc)`。
   无新符号/新依赖；`mem_sysfs_mutex` 三处使用 + 一处定义，无未使用告警。
3. **boot 观测**（依赖既有 framebuffer 探针）：
   - y=300 行点亮顺序应变为 8(auxiliary)→80(node)→116(cpu)→44(memory)→152(container)；
   - y=340 x=260（memory_dev_init 完成）仍在循环后点亮；y=355 block 计数、y=296/380 device_register 探针不变。
   - 若开 CONFIG_DEBUG_MUTEXES/CONFIG_PROVE_LOCKING 跑一轮：不应有 lockdep 倒序告警。
4. **A/B 归因**：此补丁预期不改变竞态现象（§5）。若刷机后竞态仍现 ⇒ 与既有 D1（跨簇唤醒）结论一致；
   若消失 ⇒ 需重新评估（理论上不该，因为锁在 boot 无竞争者）。

---

## 7. 关键行号/地址速查

| 项 | 位置 |
|---|---|
| 5.4 mem_sysfs_mutex 定义 | work/linux-5.4.210/drivers/base/memory.c:29（memory.h:29 注释） |
| 5.4 持有点 | memory.c:704-720（create）、740-749（remove）、805-812（memory_dev_init 全程） |
| stock 二进制 | memory_dev_init 0xffffffc01218ec8c；mem_sysfs_mutex 数据 0xffffffc0123f1070；create_memory_block_devices 0xbc134c（均持锁） |
| 6.12 memory_dev_init | linux-6.12.103/drivers/base/memory.c:974（循环 997-1012） |
| 6.12 create/remove | memory.c:882-912 / 921-940 |
| 6.12 driver_init | linux-6.12.103/drivers/base/init.c:21-56（memory_dev_init 调用原 46-47，补丁后移至 cpu_dev_init 之后） |
| 替代锁 | mm/memory_hotplug.c:208 `DEFINE_STATIC_PERCPU_RWSEM(mem_hotplug_lock)`（device_hotplug_lock） |
