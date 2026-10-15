# driver_init 链后续卡点分析（devices_init 及之后，对比原厂 5.4）

> 任务文件：`prompt-stock-driver-init.md`
> 前置阅读：`work/REPORT-stock-sync-mechanism.md`（stop_machine/alternatives 同步）、
> `work/REPORT-stock-zone-memory.md`（NOMAP/set_zone_contiguous）、
> `work/SESSION-CONTEXT-COMPACT.md`（当前进度）。
> 源码：`linux-6.12.103/`（6.12.103 实测树，含 t508n 探针）、
> `work/linux-5.4.210/`（本次新拉取的 v5.4.210 上游源码）、
> `work/stock_Image_full.bin` + `work/stock_kallsyms.map`（原厂 5.4.210 二进制）。

---

## 0. 结论先行（TL;DR）

1. **devices_init 自身没有可卡住的逻辑**：它只是 4 个 kernfs 目录 + 1 个 workqueue，
   与 5.4 逐行一致（仅多 `alloc_workqueue("device_link_wq")`），锁路径都是
   "只有 CPU0 参与"的单方持有。若实测真的卡在 `y=260 x=44`，**卡点几乎不可能在
   devices_init 的代码里**，只能是更底层的原语（原子/LSE、SLUB、页分配器、锁）或
   "探针误导"（实际卡在链内更靠后的成员）。
2. **原厂 5.4.210 二进制的 driver_init 链**（反汇编确认，8 个调用）：
   `devices_init → buses_init → classes_init → firmware_init → hypervisor_init →
   of_core_init → platform_bus_init → cpu_dev_init → memory_dev_init →
   container_dev_init`。**没有** bdi_init、devtmpfs_init（原厂 config 关 DEVTMPFS）、
   auxiliary_bus_init、node_dev_init。
3. **6.12 链与 5.4 的差异**：多了 `bdi_init`、`devtmpfs_init`（含 kdevtmpfsd 内核线程）、
   `auxiliary_bus_init`；`node_dev_init` 从 5.4 的 postcore_initcall 移入链内；
   顺序变为 memory→node→cpu（5.4 是 cpu→memory）。这些差异本身都不产生
   "能卡住"的锁依赖，但改变了分配/时序分布。
4. **最可疑机制**（按证据强度排序，详见第 5 节）：
   - H1：实测卡点不是 devices_init，而是链内更重的成员
     （of_core_init 的 sysfs attach / memory_dev_init 的 device_register）——探针
     x=44/y=260 尚未确认点亮，历史卡点正是这两个位置，且卡点逐次漂移。
   - H2：8 核并发下低层原语的间歇性原子/锁活锁（LSE 修复后大幅减弱但未排除的残余
     面：cmpxchg128/LSE `casp` 的 SLUB 路径、rwsem 乐观自旋、跨 cluster 的
     qspinlock MCS 交接）——与"加探针能过、不加就卡"的时序敏感特征吻合。
   - H3：of_core_init 的 sysfs attach 量级（数千节点 × 每节点属性文件 =
     上万次 kernfs 创建/分配），是最重的分配阶段，也是历史卡点 #1。
5. **验证建议**：一次刷机读全 y=260/300/340/380 全部探针 → 精确定位卡点成员；
   然后 maxcpus=1 隔离 + 强制 LL/SC（不 enable LSE static_key）做 A/B。

---

## 1. devices_init 分析（6.12 实现 + 多核交互 + 潜在卡点）

### 1.1 位置与实现

6.12 中 `devices_init()` 位于 `drivers/base/core.c:4192`（5.4 在 `core.c:2621`，
**没有** `drivers/base/base.c`，base.c 在 6.12 已不存在）。实现：

```c
int __init devices_init(void)
{
	devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);
	dev_kobj = kobject_create_and_add("dev", NULL);
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	device_link_wq = alloc_workqueue("device_link_wq", 0, 0);
	return 0;
}
```

做的事：
- 在 sysfs 根（`sysfs_root_kn`）下创建 `/sys/devices` kset、`/sys/dev` kobject，
  再在 dev 下创建 `block`、`char` 两个 kobject；
- 创建 `device_link_wq` workqueue（供 device links 使用，此处不跑任何 work）。

### 1.2 调用路径与锁（多核交互点）

`kset_create_and_add/kobject_create_and_add` →
`kobject_add_internal`（lib/kobject.c:210）→ `create_dir` →
`sysfs_create_dir_ns`（fs/sysfs/dir.c:40）→ `kernfs_create_dir_ns` →
`kernfs_add_one`（fs/kernfs/dir.c:770）→ `down_write(&root->kernfs_rwsem)`。

锁/同步清单（全部是"此刻只有 CPU0 触碰"的锁）：
- **kernfs 每根 rwsem**（`kernfs_rwsem`，6.12 为每 root 一个；5.4 是全局
  `kernfs_mutex`）：`down_write` 无竞争时走 rwsem 快速路径（一次原子
  cmpxchg），不睡眠；
- **kset list_lock**（`kobj_kset_join` 里的 spinlock，kobject.c:180）：短暂；
- **SLUB 分配**：`kernfs_new_node` 首次分配 kernfs_node_cache、kobject 名
  `kvasprintf_const`（"devices"/"dev"/"block"/"char" 无 `%`，直接返回 rodata
  指针，**不分配**）；
- **页分配器**：首次 slab 建页时 pcp 快速路径 → 偶尔 zone->lock；
- `kernfs_add_one` 内 `ktime_get_real_ts64`（读 arch timer，timekeeping 正常）；
- `alloc_workqueue`：workqueue_init 已完成，仅分配 per-CPU pwq。

**此刻其它 7 个 CPU 在做什么**：idle 循环 + 定时器/RCU softirq + kthreadd/
worker 线程（workqueue_init 已跑）。它们不碰 kernfs、不碰 sysfs、不碰 kset 锁；
只与 CPU0 共享：SLUB 节点 partial list_lock、页分配器 zone->lock、调度 rq 锁
（CPU0 分配时可能短暂竞争）。**不存在跨 CPU 的锁依赖环，也没有等待其它 CPU
的 wait_for_completion**（devices_init 不 sleep 等待任何人）。

### 1.3 潜在卡点评估

- **devices_init 的代码逻辑本身：没有卡点**。与 5.4 逐行一致，5.4 在同样的
  8 核硬件上（原厂机）跑通到 Android。
- 若真卡在这里，只可能是：低层原语故障（见 H2）、分配到的物理页异常（见 2.5
  的 NOMAP/buddy 结论——**buddy 不含 NOMAP 页，排除"分到坏页"**）、或探针
  x=44 未点亮只是因为根本没刷到该探针版本（H1）。

---

## 2. driver_init 链后续逐个分析（6.12 实现 + 多核 + 卡点嫌疑）

> 6.12 `driver_init()`（drivers/base/init.c）调用序：
> `bdi_init → devtmpfs_init → devices_init → buses_init → classes_init →
> firmware_init → hypervisor_init → of_core_init → platform_bus_init →
> auxiliary_bus_init → memory_dev_init → node_dev_init → cpu_dev_init →
> container_dev_init`（14 个调用，全部在 CPU0 顺序执行）。

### 2.1 buses_init（bus.c:1433）
`bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL)` +
`system_kset = kset_create_and_add("system", NULL, &devices_kset->kobj)`。
与 5.4（bus.c:1189）逐行一致（仅错误处理方式不同）。2 个 kernfs 目录。
**嫌疑：低**（与 5.4 相同，无新锁）。

### 2.2 classes_init（class.c:673）
`class_kset = kset_create_and_add("class", NULL, NULL)`。与 5.4（class.c:572）
一致。1 个 kernfs 目录。**嫌疑：低**。

### 2.3 firmware_init / hypervisor_init（firmware.c:20 / hypervisor.c:18）
各 1 个 `kobject_create_and_add("firmware"/"hypervisor", NULL)`。与 5.4 一致。
**嫌疑：低**。

### 2.4 of_core_init（drivers/of/base.c:167）——**历史卡点 #1，重**
```c
void __init of_core_init(void)
{
	of_platform_register_reconfig_notifier();          // 6.12 新增（5.4 在 platform_bus_init 末尾）
	mutex_lock(&of_mutex);
	of_kset = kset_create_and_add("devicetree", NULL, firmware_kobj);
	for_each_of_allnodes(np) {
		__of_attach_node_sysfs(np);               // ← 逐节点 sysfs attach
		if (np->phandle && !phandle_cache[...])
			phandle_cache[...] = np;          // 6.12 内联填 phandle cache（5.4 用 of_populate_phandle_cache）
	}
	mutex_unlock(&of_mutex);
	if (of_root)
		proc_symlink("device-tree", NULL, "/sys/firmware/devicetree/base");
}
```
`__of_attach_node_sysfs`（drivers/of/kobj.c:115）：每节点 1 次 `kobject_add`
（kernfs 目录）+ 每属性 1 次 `__of_add_property_sysfs` → `sysfs_create_bin_file`
（kernfs 文件）。**T770 的完整 DTS 有数千节点、每节点多个属性 → 上万次
kernfs 创建 + 上万次 SLUB 分配**——是 driver_init 链中**最重的分配阶段**。
- 锁：`of_mutex`（仅 CPU0）；kernfs rwsem（仅 CPU0）；无跨 CPU 等待。
- 6.12 vs 5.4 差异：notifier 注册提前；phandle cache 填充位置不同；sysfs attach
  循环本身一致。
- **嫌疑：高（量级 + 历史卡点）**。若 H2 的"低层原语间歇故障"成立，
  of_core_init 是最可能被命中的成员。

### 2.5 platform_bus_init（platform.c:1485）——首次完整 device_add
```c
int __init platform_bus_init(void)
{
	early_platform_cleanup();
	error = device_register(&platform_bus);
	error = bus_register(&platform_bus_type);
	return error;
}
```
- `device_register(&platform_bus)` → `device_add`：`device_add_class_symlinks`
  （class=NULL 跳过）→ `device_add_attrs`（kernfs 属性文件）→ `bus_add_device`
  （`klist_add_tail(&dev->p->knode_bus, &sp->klist_devices)` spinlock +
  sysfs_create_link ×2）→ `dpm_sysfs_add`（power/ 目录 + 文件）→
  `kobject_uevent(KOBJ_ADD)`（**空操作**：uevent_sock_list 为空——netlink 未
  初始化；`uevent_helper[0]=='\0'`——CONFIG_UEVENT_HELPER 关）→
  `bus_probe_device`（无已注册 driver → 空操作）。
- `bus_register(&platform_bus_type)` → `subsys_register` 类似流程。
- 6.12 vs 5.4：`of_platform_register_reconfig_notifier` 移到 of_core_init；其余一致。
- **嫌疑：中**（首个 device_add，但 devt=0，无 devtmpfs 交互）。

### 2.6 auxiliary_bus_init（auxiliary.c:417）——**6.12 新增**
`WARN_ON(bus_register(&auxiliary_bus_type))`。5.4 无此函数（auxiliary bus
5.11 才引入）。1 次 bus_register。**嫌疑：低-中**（新代码，但路径与 bus_register
一致）。

### 2.7 memory_dev_init（memory.c:971）——**历史卡点 #2，重**
```c
void __init memory_dev_init(void)
{
	block_sz = memory_block_size_bytes();        // 探针 188,340
	if (!is_power_of_2(block_sz) || block_sz < MIN_MEMORY_BLOCK_SIZE)
		panic(...);
	sections_per_block = block_sz / MIN_MEMORY_BLOCK_SIZE;
	ret = subsys_system_register(&memory_subsys, memory_root_attr_groups);
	if (ret) panic(...);
	for (nr = 0; nr <= __highest_present_section_nr; nr += sections_per_block) {
		ret = add_boot_memory_block(nr);     // → init_memory_block → device_register
		if (ret) panic(...);
	}
}
```
- `subsys_system_register`（bus.c:1326）→ `subsys_register`：`bus_register` +
  `device_register(dev_root)`（system_kset 下，带 memory_root_attr_groups）。
- 循环：`add_boot_memory_block` → `add_memory_block` → `init_memory_block` →
  `__add_memory_block` → **`device_register(&memory->dev)`**（memory.c:693，
  探针 296,340 前 / 8,380 后）。每次 device_register 都是完整 device_add：
  kernfs 目录 + 属性组（memory_memblk_attr_groups）+ klist + power/ + uevent(空)。
- 块数：arm64 `memory_block_size_bytes()` 默认 128MB，6GB RAM ≈ **48 次
  device_register**。
- 6.12 vs 5.4 差异：
  1. **5.4 循环持 `mem_sysfs_mutex`**（memory.c:812 `mutex_lock(&mem_sysfs_mutex)`），
     **6.12 删掉了**（注释：boot 时无并发，device_hotplug_lock 不需要）——单核
     顺序执行下无影响；
  2. 5.4 用 `add_memory_block(nr)`，6.12 用 `add_boot_memory_block`（先算
     section_count，为 0 跳过）；
  3. 错误处理都 panic。
- **嫌疑：高（量级 + 历史卡点 + 48 次完整 device_add）**。

### 2.8 node_dev_init（node.c:963）——**6.12 移入链内（5.4 是 postcore_initcall）**
```c
void __init node_dev_init(void)
{
	ret = subsys_system_register(&node_subsys, cpu_root_attr_groups);
	if (ret) panic(...);
	for_each_online_node(i)
		register_one_node(i);
}
```
- `register_one_node`（node.c:~870）：kzalloc node + `register_node`（
  device_register）→ **`for_each_present_cpu` 且 `cpu_to_node(cpu)==nid` 时
  `register_cpu_under_node(cpu, nid)`**（sysfs_create_link）→ `node_init_caches`。
- **重要差异**：5.4 的 node 子系统由 `register_node_type()`（postcore_initcall，
  node.c:1033）注册，**在 driver_init 之后**；6.12 把它移进 driver_init 链
  （memory_dev_init 之后）。6.12 里 node_dev_init 先于 cpu_dev_init → 此时
  `register_cpu_under_node` 链接的 cpu 设备还不存在（cpu_dev_init 在后）——
  语义由 cpu 注册时反向链接补偿，不是卡点。
- 单节点系统：1 次 device_register + 每 present CPU 一次 sysfs link。
- **嫌疑：中**。

### 2.9 cpu_dev_init（cpu.c:665）
```c
void __init cpu_dev_init(void)
{
	if (subsys_system_register(&cpu_subsys, cpu_root_attr_groups))
		panic(...);
	cpu_dev_register_generic();   // for_each_possible_cpu → arch_register_cpu → register_cpu（8 次 device_register）
	cpu_register_vulnerabilities();
}
```
- 8 次 `register_cpu` → `device_register`，每次完整 device_add + 每 CPU 若干
  sysfs 文件（online/uevent 等）。
- 5.4（cpu.c:618）相同结构；**顺序差异**：5.4 链里 cpu_dev_init 在
  memory_dev_init **之前**，6.12 在 **之后**（node 之后）。
- **嫌疑：中**。

### 2.10 container_dev_init（container.c:34）
`subsys_system_register(&container_subsys, NULL)`。与 5.4 一致。**嫌疑：低**。

### 2.11 链内各成员小结

| 成员 | 工作量 | 6.12 vs 5.4 | 卡点嫌疑 |
|---|---|---|---|
| devices_init | 4 kernfs 目录 + 1 wq | +device_link_wq | 低（代码无卡点） |
| buses_init | 2 kernfs 目录 | 一致 | 低 |
| classes_init | 1 kernfs 目录 | 一致 | 低 |
| firmware/hypervisor_init | 各 1 kobject | 一致 | 低 |
| of_core_init | 数千节点 sysfs attach（最重） | notifier/cache 顺序变 | **高（历史#1）** |
| platform_bus_init | 2 次 device_add/bus_register | notifier 挪走 | 中 |
| auxiliary_bus_init | 1 bus_register | **新增** | 低-中 |
| memory_dev_init | ~48 次 device_register | 去 mem_sysfs_mutex；add_boot_memory_block | **高（历史#2）** |
| node_dev_init | 1 device_register + cpu links | **postcore→链内** | 中 |
| cpu_dev_init | 8 次 device_register | 顺序后移 | 中 |
| container_dev_init | 1 device_register | 一致 | 低 |

---

## 3. 原厂 5.4 的 driver_init 链（时机/顺序/实现差异）

### 3.1 反汇编证据（work/stock_Image_full.bin，vaddr 0xffffffc01218e794）

```
driver_init:
  bl   devices_init       (0xffffffc01218dcd8)
  bl   buses_init         (0xffffffc01218ddb0)
  bl   <kset_create_and_add "class">          → class_kset     (classes_init 内联展开)
  bl   <kobject_create_and_add "firmware">    → firmware_kobj  (firmware_init)
  bl   <kobject_create_and_add "hypervisor">  → hypervisor_kobj(hypervisor_init)
  bl   of_core_init       (0xffffffc0121a00a8)
  bl   platform_bus_init  (0xffffffc01218df80)
  bl   cpu_dev_init       (0xffffffc01218e6c4)
  bl   memory_dev_init    (0xffffffc01218ec8c)
  bl   container_dev_init (0xffffffc01218e814)
  ret
```

**结论**：
1. 原厂链 = `devices → buses → classes → firmware → hypervisor → of_core →
   platform_bus → cpu_dev → memory_dev → container`，与上游 v5.4.210
   `drivers/base/init.c` 一致；
2. **原厂没有** `bdi_init`、`devtmpfs_init` 调用——原厂 config 里
   `# CONFIG_DEVTMPFS is not set`（Android 用 ueventd 管 /dev），
   `devtmpfs_init()` 是空 stub；
3. **原厂没有** `auxiliary_bus_init`（5.11 才引入）、**没有** `node_dev_init`
   （node 子系统由 postcore_initcall `register_node_type` 注册，node.c:1033，
   在 do_initcalls 的 postcore 阶段执行）；
4. 顺序差异：原厂 `cpu_dev_init` 在 `memory_dev_init` **之前**；6.12 是
   memory→node→cpu。
5. 原厂这些函数**无任何 T770 特判**（与之前报告一致：反汇编无额外
   dsb/dmb/dc/ic/tlbi）。

### 3.2 原厂 config 关键差异（work/kernel.config vs linux-6.12.103/.config）

| 项 | 原厂 5.4 | mainline 6.12 | 影响 |
|---|---|---|---|
| CONFIG_DEVTMPFS | 关 | **开**（+MOUNT） | 6.12 多一个 kdevtmpfsd 内核线程在链期间运行（睡在 req 队列上，链内无 devt 设备，**不干扰**） |
| CONFIG_NUMA / NUMA_BALANCING | 关 | **开** | 6.12 单节点下 zonelist 走 node0 回退；NUMA_BALANCING 对 kthread（无用户 mm）基本空转 |
| CONFIG_NR_CPUS | 32 | **512** | per-cpu 区更大；possible=8（DTS），循环量同 |
| CONFIG_ARM64_WORKAROUND_SPECULATIVE_AT | 关 | **开** | 6.12 多 patch ~24 处 `at` 指令序列（alternative） |
| CONFIG_ARM64_WORKAROUND_REPEAT_TLBI | 开 | 开 | 同（alternative） |
| CONFIG_ARM64_ERRATUM_2054223/3117295/3194386 | 关 | **开** | 6.12 多若干 errata alternative |
| CONFIG_SOFTLOCKUP_DETECTOR | 开 | **关** | 6.12 卡住时**无 softlockup 打印**，原厂会 20s 报一次——诊断盲区 |
| CONFIG_JUMP_LABEL | 开 | 开 | LSE 选择机制（原厂式） |
| CONFIG_SLUB | 开 | 开 | 分配器一致（但内部原语不同，见 §4.3） |

### 3.3 原厂在 driver_init 时刻的并发环境
- 原厂 8 核上线后同样进入 `kernel_init_freeable → do_basic_setup → driver_init`，
  二级核 idle + kthreadd/worker。**与原厂相比，6.12 的差异不在并发模型，而在
  config（NUMA/errata/devtmpfs）与内核版本代码**。

---

## 4. 多核 sysfs/device model 的同步机制

### 4.1 kobject_add → sysfs 目录创建的锁路径（6.12）
```
kobject_add → kobject_add_internal
  ├─ kobject_get(parent)                    // kref（原子）
  ├─ kobj_kset_join: spin_lock(&kset->list_lock)  // 短暂
  └─ create_dir → sysfs_create_dir_ns → kernfs_create_dir_ns
       ├─ kernfs_new_node                   // kmem_cache_alloc（SLUB）
       └─ kernfs_add_one
            ├─ down_write(&root->kernfs_rwsem)    // 6.12: 每 kernfs root 一把 rwsem
            ├─ kernfs_link_sibling          // rb 树插入
            ├─ ktime_get_real_ts64          // arch timer
            └─ up_write(&root->kernfs_rwsem)
```
- **5.4 是全内核一把 `kernfs_mutex`**（fs/kernfs/dir.c:20）；6.12 改为每 root
  `kernfs_rwsem`。本链内两者都只有 CPU0 触碰 → 无差异、无竞争。
- kernfs 节点操作在 sysfs **未挂载**时只操作内存树（dentry/inode 在 mount 时才
  建立）——本阶段没有 VFS 层锁参与。

### 4.2 device_add 的锁路径（6.12）
```
device_register = device_initialize（kobject_init、dev->mutex init、klist init）
                + device_add:
  ├─ dev_set_name                 // kobject_set_name（kvasprintf_const）
  ├─ device_add_class_symlinks    // class==NULL 跳过
  ├─ device_add_attrs             // sysfs_create_groups（kernfs 文件，kernfs_rwsem）
  ├─ bus_add_device               // sysfs_create_link ×2 + spin_lock(bus->p->klist_devices)
  ├─ dpm_sysfs_add                // power/ 目录+文件（kernfs）
  ├─ if (MAJOR(dev->devt)) device_create_sys_dev_entry + devtmpfs_create_node
  ├─ kobject_uevent(KOBJ_ADD)     // 空操作（netlink 未起 + uevent_helper 关）
  └─ bus_probe_device             // 无 driver 注册 → 空操作
```
- `device_lock` = `dev->mutex`，由**调用方**持有；device_add 内部不再取它。
- 链内所有 device_add 的 dev->devt 均为 0 → **不触 devtmpfs_create_node**
  （不会向 kdevtmpfsd 发请求、不等待其完成）。devtmpfs 交互要等 do_initcalls
  里注册真正带 devt 的设备时才会出现。

### 4.3 分配器/锁原语（跨 CPU 共享面）
- **SLUB**：per-cpu freelist 快速路径无锁；跨 CPU slab 释放/取用走
  `try_cmpxchg128`（6.12，freelist+counter 128 位 ABA）或
  `cmpxchg_double`（5.4）——arm64 上都编译为 **LSE `casp`/`caspl` 或
  LL/SC `ldxp/stxp` 对**；节点 partial list_lock 是 spinlock。
  → **原厂 5.4 在同样硬件上跑同一类 128 位 compare-exchange，说明硬件原语
  本身可用**；6.12 的差异是调用频率/时机与 ABA 重试语义。
- **页分配器**：pcp 快速路径无锁；zone->lock 仅在 pcp 补货时取。
- **rwsem**：kernfs_rwsem 等，down_write 无竞争 = 一次原子；有竞争 = 乐观自旋 +
  睡眠。本链内无跨 CPU 竞争。
- **qspinlock**：MCS 交接 + 原子 cmpxchg（走 LSE static_key）。
- **原子选择机制（已修复项）**：6.12 全 C 原子（atomic/cmpxchg/xchg/
  cmpxchg128）统一走 `__lse_ll_sc_body` → `system_uses_lse_atomics()` =
  `static_branch_likely(&arm64_const_caps_ready) && static_branch_likely(
  &cpu_hwcap_keys[ARM64_HAS_LSE_ATOMICS])`（arch/arm64/include/asm/lse.h 已改）。
  static_key 在 `t508n_apply_lse_early()`（smp_prepare_boot_cpu，IRQ 关单核）里
  enable；jump_label patch 协议（arch_jump_label_transform_queue/apply →
  kick_all_cpus_sync）即便某核 I-cache 未同步，执行的也是**旧的 LL/SC 分支**
  （语义正确，仅慢）——**不存在指令级混用竞态**。这解释了为何 stop_machine
  已通过。

### 4.4 死锁/长等待可能性评估
- 链内**无跨 CPU 锁依赖环**、无"等待其它 CPU 的完成量"（devtmpfs_init 的
  wait_for_completion 已过，且其 kthread 是独立完成）。
- 唯一能造成"挂死"的机制：
  1. **原子/锁原语活锁**（某个 CPU 在自旋/重试循环里出不来）——时序敏感，
     与"加探针能过"吻合；
  2. **分配器状态损坏**（早前竞态污染 freelist/zone 状态 → 后续分配无限循环）；
  3. **物理访问 stall**（访问了不响应的内存）——**buddy 已排除 NOMAP 页**
     （见 4.5），此机制基本排除；
  4. **超长循环**（如 of_core_init 大 DT 遍历/名字扫描）——6.12 关掉了
     softlockup 检测，卡住时无诊断信息，**"慢"与"死"无法区分**。

### 4.5 NOMAP/buddy 的确认（排除"分到坏页"）
- DT `no-map` 区域在 `early_init_dt_reserve_memory` 里只 `memblock_mark_nomap`
  （drivers/of/of_reserved_mem.c:154），**仍留在 memblock.memory**；
- `map_mem` 用 `for_each_mem_range`（flags 不含 NOMAP）→ `should_skip_region`
  跳过 NOMAP → **线性映射不覆盖**；
- `free_low_memory_core_early` 用 `for_each_free_mem_range`（MEMBLOCK_NONE）→
  同样跳过 NOMAP → **NOMAP 页不进 buddy**；
- 但 `memmap_init` 用 `for_each_mem_pfn_range`（**无 NOMAP 过滤**）→ NOMAP 页的
  struct page 被初始化为 present → zone present_pages 虚高、set_zone_contiguous
  会走进空洞（**这就是上一轮 NOMAP 修复的由来**）。
- 结论：**分配器拿不到 NOMAP 页**；driver_init 链的 kmalloc 全部来自干净内存。

---

## 5. 综合：卡点假设排序 + 验证方案 + 修复建议

### 5.1 假设排序

**H1（最可能，先验证）——实测卡点不是 devices_init，而是链内更重的成员**
- 依据：x=44/y=260 探针"待确认"（当前固件可能根本没刷该探针版本）；devices_init
  与 5.4 逐行一致且逻辑无卡点；历史卡点就在 of_core_init sysfs attach 与
  memory_dev_init device_register；卡点逐次漂移。
- 若 x=44 未亮 → 卡在 devices_init..hypervisor_init 之间（这几个都极轻）；
  若 x=44 亮 → 用 y=300 行精确定位（x=8 auxiliary / x=44 memory / x=80 node /
  x=116 cpu / x=152 container）。

**H2（机制层）——8 核并发下低层原语的间歇性原子/锁活锁（LSE 修复后的残余面）**
- 依据：漂移 + "加探针能过、不加就卡"的强时序敏感特征；链内所有重成员都是
  分配密集（SLUB 跨 CPU 128 位 cmpxchg、页分配器 zone 锁、rwsem 自旋）。
- 残余面：SLUB `try_cmpxchg128`（LSE `casp`）、qspinlock 跨 cluster MCS 交接、
  rwsem 乐观自旋、`ic ialluis`+isb 序列（现为主线版，SM#2 已过 → 大概率无害）。
- 注意控制变量：原厂 5.4 在同一硬件上跑同样的原子/LSE/SLUB/锁 → **硬件原语
  并非坏**；若 H2 成立，说明是"主线的使用模式/时序"触发了 SoC 特定弱点。

**H3——of_core_init 的 sysfs attach 量级（历史卡点 #1 的最可能复现点）**
- 数千节点 × 属性 = 上万次 kernfs 创建/分配，链内最重；若 H2 机制存在，
  这里命中概率最高。

**H4——memory_dev_init 的 device_register（历史卡点 #2）**
- ~48 次完整 device_add；6.12 删了 5.4 的 mem_sysfs_mutex（boot 期无并发影响）；
  若 H2 机制存在，这里是次高命中点。

**H5——指令流状态（I-cache/alternatives）残余风险（低）**
- 现状：`icache_inval_all_pou()` 已是主线版（`ic ialluis` + dsb ish），且
  apply_boot_alternatives 与 apply_alternatives_all（SM#2）都带它跑通了 →
  之前"ic ialluis 在 T770 会 hang"的结论**很可能是 LSE 修复前的误归因**；
  广播 I-cache 失效在 EL1/MMU 开、SML 配好的前提下是工作的。
- 残余：后续任何 `kick_all_cpus_sync`/`ic ialluis`（模块加载、jump_label 再
  patch、cpu_enable）——但同路径已在 SM#2 验证。

**H6——devtmpfsd 线程干扰（本链低，链后高）**
- 链内无 devt 设备 → kdevtmpfsd 全程睡眠，不干扰；但 do_initcalls 之后首个
  带 devt 的 device_add 会 `devtmpfs_create_node` → 发请求 + `wait_for_completion`
  ——若 devtmpfsd 处理循环有问题，**卡点会出现在 driver_init 之后的 initcall
  阶段**（y=220 x=44 已亮、后面不亮），值得单独探针。

**H7——"慢"而非"死"（诊断盲区）**
- 6.12 config **关掉了 SOFTLOCKUP/HARDLOCKUP**（原厂开着）：如果内核卡在
  一个超长循环（of_core_init 大 DT、某 O(n²) 名字扫描）里缓慢推进，6.12 屏幕
  表现与死锁完全一样且无打印。**强烈建议打开 watchdog 系列 config**。

### 5.2 验证方案（一次刷机一个变量）

1. **第一步（必须）：刷当前带全套探针的固件，读全部坐标**
   - y=220（cpuset x=8 / driver_init 完成 x=44 / init_irq_proc x=80 / do_ctors x=116）
   - y=240（sched x=8 / workqueue_topology x=44 / async x=80 / padata x=116 / page_alloc_init_late x=152）
   - y=260（devtmpfs 后 x=8 / devices x=44 / classes x=80 / hypervisor x=116 / of_core x=152）
   - y=300（auxiliary x=8 / memory x=44 / node x=80 / cpu x=116 / container x=152）
   - y=340（block_sz x=188 / 循环内 x=224 / memory 完成 x=260）
   - y=380（device_register 返回 x=8）
   - 一次刷机即可把卡点精确到**链内某一个调用**。
2. **maxcpus=1 隔离**：bootargs 加 `maxcpus=1`（或注释掉 smp_init 后二级核启动）。
   - 单核过、8 核卡 → 跨核同步/调度窗口问题（H2 方向）；
   - 单核也卡 → 与 SMP 无关，查分配器/代码路径本身。
3. **LSE A/B**：临时把 `t508n_apply_lse_early` 里两个 `static_branch_enable` 注释
   掉（全系统强制 LL/SC）。
   - 卡点消失/移动 → 与 LSE 指令形式（尤其 `casp`）相关，进一步用微基准定位；
   - 不变 → LSE 不是残余因素，转查分配器/其它原语。
4. **of_core_init 内部探针**（若 y=260 x=152 未亮）：在
   `for_each_of_allnodes` 循环里每 128/256 个节点画一格，区分"卡在节点 N"
   （量级/时序）还是"根本没进循环"（of_mutex/notifier 前就挂）。
5. **打开诊断**：`CONFIG_SOFTLOCKUP_DETECTOR=y`、`CONFIG_HARDLOCKUP_DETECTOR=y`
   （或 watchdog），配合 `nmi_watchdog=1`——即使无串口，softlockup 触发时会
   打印栈到 console（如果 framebuffer console 已起）或至少改变行为可观察。
6. **链后对照**：若 y=220 x=44 亮（driver_init 完成）但后面不亮 → 卡点在
   init_irq_proc/do_ctors/do_initcalls；其中首个带 devt 设备的注册会走
   devtmpfs_create_node（H6），加对应探针。

### 5.3 修复建议（按命中假设）
- **H2/H3/H4（低层原语间歇 + 重分配阶段）**：
  - 先做 5.2-2/3 判别；若 LSE `casp` 命中，最省事的兜底是**全系统强制 LL/SC**
    （不 enable LSE static_key）——原厂 5.4 本身就是 LL/SC + LSE 混合（jump_label
    切换），性能损失在 boot 期可忽略；
  - 若指向分配器：对照 5.4 SLUB（cmpxchg_double）与 6.12（cmpxchg128 ABA）的
    差异，可在 config 层面试 `CONFIG_SLAB`（6.12 已移除 SLAB，不可行）或
    `slub_nomerge`/`slub_min_objects` 等参数改变分配模式做 A/B；
  - 若指向 qspinlock/rwsem 跨 cluster：可试 `qspinlock` 参数或临时把相关锁换成
    ticket 锁做对照（成本高，最后再试）。
- **H7（慢而非死）**：开 softlockup 拿到证据，同时给 of_core_init 循环加
  `cond_resched()`（当前 6.12 循环内无 cond_resched，虽然 PREEMPT=y 下可抢占，
  但长循环 + 无打印仍然不可观测）。
- **通用**：维持"一次刷机一个变量"；所有改动记录到 `linux-6.12.103` git diff。

### 5.4 原厂 vs mainline 关键差异（给后续工作的备忘）
1. devtmpfs：原厂关（无 kthread），mainline 开（kdevtmpfsd 常驻）——链内无影响，
   链后 device_add(devt) 会依赖它；
2. node 子系统：原厂 postcore_initcall（driver_init 后），mainline 移入链内
   memory 之后、cpu 之前；
3. auxiliary bus：mainline 新增；
4. cpu/memory 顺序：原厂 cpu→memory，mainline memory→node→cpu；
5. kernfs 锁：原厂全局 kernfs_mutex，mainline 每 root rwsem（本阶段无差异）；
6. SLUB 原语：原厂 cmpxchg_double，mainline cmpxchg128 ABA（同类硬件原语）；
7. config：NUMA/NUMA_BALANCING、NR_CPUS=512、SPECULATIVE_AT 与若干新 errata、
   软锁检测关闭——均为 mainline 独有变量；
8. LSE：两者都是 static_key/jump_label 选择，mainline 侧已改为原厂式并提前到
   单核阶段 enable（已修复 stop_machine）。

---

## 附：本次分析用到的关键源码位置（6.12）
- drivers/base/init.c（driver_init，含探针）
- drivers/base/core.c:4192（devices_init）、device_add
- drivers/base/bus.c:1433（buses_init）、1256（subsys_register）
- drivers/base/class.c:673（classes_init）
- drivers/base/firmware.c:20、hypervisor.c:18
- drivers/of/base.c:167（of_core_init）、drivers/of/kobj.c:115（__of_attach_node_sysfs）
- drivers/base/platform.c:1485、auxiliary.c:417
- drivers/base/memory.c:971（memory_dev_init）、679（__add_memory_block）
- drivers/base/node.c:963（node_dev_init）、cpu.c:665（cpu_dev_init）、container.c:34
- lib/kobject.c:210（kobject_add_internal）、fs/kernfs/dir.c:770（kernfs_add_one）
- fs/sysfs/dir.c:40（sysfs_create_dir_ns）
- arch/arm64/include/asm/lse.h（static_key LSE）、arch/arm64/kernel/alternative.c
- mm/mm_init.c（memmap_init 无 NOMAP 过滤）、mm/memblock.c:1290（__next_mem_pfn_range
  无 NOMAP 过滤）、mm/slub.c（try_cmpxchg128）、mm/slab.h（freelist ABA）
