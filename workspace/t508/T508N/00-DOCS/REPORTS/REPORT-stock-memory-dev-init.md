# REPORT：原厂 5.4 vs mainline 6.12 — memory_dev_init / device_register（device_add）区域对比

> 任务：`prompt-stock-memory-dev-init.md`（2026-08-19）
> 材料：`linux-6.12.103/drivers/base/{memory,core,bus,devtmpfs}.c`、`lib/kobject_uevent.c`、
> `fs/kernfs/dir.c`（当前树 HEAD `cb8e725f3` + 16 文件探针/修复）；
> `work/linux-5.4.210/`（v5.4.210 完整源码）；
> `work/stock_kallsyms.map` + `work/stock_Image_full.bin`（原厂反汇编）；
> `work/kernel.config`（原厂 config）。
> 生成：2026-08-19。只读既有 REPORT/SUMMARY；未重复验证已排除项（见 §8 引用）。

---

## 0. TL;DR（结论摘要）

1. **`device_add` 本体不是卡点来源**：5.4 与 6.12 的 `device_register → device_add` 结构逐段等价
   （`device_private_init → dev_set_name → get_device_parent → kobject_add(kernfs) →
   device_platform_notify → uevent 属性 → device_add_attrs → bus_add_device → dpm_sysfs_add →
   device_pm_add → (devt==0 跳过 dev/ devtmpfs) → bus_notify → kobject_uevent → bus_probe_device →
   klist_add_tail(parent) → class`）。memory 块 devt=0、fwnode=NULL、class=NULL、parent=NULL，
   6.12 新增的 `fw_devlink_link_device` / `dev_set_ready_to_probe` / `fw_devlink_unblock_consumers`
   全部**不执行**。`device_add` 路径**没有任何 completion / synchronize_rcu / 跨核等待原语**，
   只会被"某把锁被别的核持有不释放 / 分配器损坏"卡住（与 REPORT-CURRENT-RACE-ANALYSIS §3.4 一致）。
2. **"devtmpfs 线程协作创建节点"对 memory 块是伪命题**：`devtmpfs_create_node(dev)` 只在
   `if (MAJOR(dev->devt))` 分支内调用（6.12 core.c:3659-3669、5.4 core.c:2196-2206），
   memory 块 devt=0 ⇒ **永不调用**（两版一致）。devtmpfs 对 memory 路径唯一的真实影响 =
   `devtmpfs_init()` 在 `driver_init` 最前面**阻塞等待 kdevtmpfs 跨簇完成 mount**（竞态 A 区域），
   它发生在 memory_dev_init 之前，且原厂 **`CONFIG_DEVTMPFS` 未开**（见 §4）。
3. **真正的结构性差异在 memory_dev_init 的"周边"而非其内部**（按可疑度排序）：
   - **D1（最关键）**：6.12 把**跨簇唤醒空闲核**与 nohz idle-balance kick 全部改为
     **CSD + IPI_CALL_FUNC**（`__ttwu_queue_wakelist` / `kick_ilb`），5.4 全走
     **直接入队 + IPI_RESCHEDULE**（SGI#0，T770 上被证明可靠）。竞态 A（devtmpfs
     setup_done 卡）是这条链路失败的**直接观测**；竞态 B（memory device_register 卡）是
     **同链路的延迟表现**：更早某次跨簇唤醒/ kick 失败 → 某核/任务进入坏状态
     （持锁任务未醒 / slab 锁被楔住核持有 / 软中断堆积）→ CPU0 的 device_add 在
     分配/锁点卡住。卡点漂移 = "哪次唤醒丢失"漂移。
   - **D2**：6.12 `driver_init` 顺序 = `... of_core_init → platform_bus_init →
     auxiliary_bus_init → memory_dev_init → node_dev_init → cpu_dev_init → ...`；
     5.4 = `... of_core_init → platform_bus_init → cpu_dev_init → memory_dev_init → ...`
     ——**memory 从"第二个 device_register 风暴"变成"第一个"**（cpu_dev_init 被挪到后面，
     且 6.12 新增 bdi/devtmpfs/auxiliary/node 四个成员）。memory_dev_init 成为 8 核环境下
     **首次大规模 device_register + 首次密集 slab/kernfs 分配**的窗口。
   - **D3**：6.12 删除 5.4 的 `mem_sysfs_mutex`（boot 期无并发，非卡因，仅差异）。
   - **D4**：kernfs 锁 5.4 全局 `kernfs_mutex` → 6.12 per-root `kernfs_rwsem` +
     `kernfs_iattr_rwsem` + `kernfs_idr_lock`（每次节点创建 2 次 down_write + idr_alloc +
     ktime_get_real_ts64；仅 CPU0 使用，非卡因）。
   - **D5**：SLUB 6.12 `try_cmpxchg128`（LSE `casp`）vs 5.4 `cmpxchg_double`（LL/SC）——
     当前树已强制 LL/SC（`system_uses_lse_atomics()=false`，D2 实验）**仍卡** ⇒ 说明
     "casp 损坏内存"不是当前卡点的独立根因（或损坏源在仍被 patch 的 ~12K LSE 站点）。
4. **修复建议（按优先级）**：
   - P1：`kernel/sched/core.c` 把 `ttwu_queue_cond()` 恒改 false（所有唤醒直接入队 +
     resched_curr + IPI_RESCHEDULE，完全回到 5.4 语义，只用可靠的 SGI#0）。**当前树未含该补丁。**
   - P2：诊断隔离 `nohz=off` / `cpuidle.off=1` / `maxcpus=4`（仅 A55）/
     boot 期把 kdevtmpfs 等早 kthread pin 到 CPU0。
   - P3：开 `CONFIG_SOFTLOCKUP_DETECTOR`（stock 开、mainline 关 = 卡死无诊断）。
   - P4：探针钉死 device_add 内卡点（kernfs_add_one / klist_add_tail / kobject_uevent_env /
     kzalloc 各处 + y=355 block 计数已存在），一次刷机读全 y=340/355/380。
   - P5（对齐原厂，成本低）：恢复 `mem_sysfs_mutex`（无害）；`CONFIG_NR_CPUS=32`。

---

## 1. 探针语义与当前观测（卡点定位）

当前 memory.c 探针（git diff 确认）：

| 探针 | 位置 | 语义 |
|---|---|---|
| y=340 x=188 | `memory_dev_init` block_sz 计算后 | block_sz 有效（过 power-of-2 检查） |
| y=340 x=224 | 循环体首行（每 block 一次） | subsys_system_register 已返回，进入循环 |
| y=355 x=44+10n (n=0..8) | 循环体，每 4 个 block 一格 | **block 计数**（48 block ⇒ 最多 9 格） |
| y=340 x=296 | `__add_memory_block` device_register **前**（每 block 一次） | 已进入 device_register 路径 |
| y=380 x=8 | `__add_memory_block` device_register **返回后**（每 block 一次） | 部分 block 的 device_register 已返回 |
| y=340 x=260 | memory_dev_init 循环后 | 全部 block 完成 |

观测：x=296 亮、x=380 **部分亮**、x=260 不亮 ⇒ **循环已进入、若干 block 的
device_register 成功返回、随后某 block 卡在 device_register（device_add）内部**。
y=355 的格数（未提供）可直接给出"卡在第几个 block"。单点语义：`__add_memory_block`
（6.12 memory.c:679）在 device_register 之前不做任何锁操作（只填 dev 字段），
故 x=296 亮 ⇒ 卡点在 device_add 内的某一子步骤（见 §3/§6 逐段清单）。

---

## 2. memory.c 层对比（5.4 vs 6.12）

### 2.1 memory_dev_init（源码）

| 项 | 5.4 (work/linux-5.4.210/drivers/base/memory.c:785) | 6.12 (linux-6.12.103/drivers/base/memory.c:970) |
|---|---|---|
| block_sz 检查 | `is_power_of_2 && >= MIN_MEMORY_BLOCK_SIZE`，否则 panic | 同（探针插在中间） |
| 子系统注册 | `subsys_system_register(&memory_subsys, memory_root_attr_groups)` | 同 |
| **循环锁** | **`mutex_lock(&mem_sysfs_mutex)` 包住整个循环** | **无锁**（注释：boot 期无并发创建，不需要 device_hotplug_lock） |
| 循环范围 | `nr = 0; nr <= __highest_present_section_nr; nr += sections_per_block` | 同 |
| 每 block 调用 | `add_memory_block(nr)`（静态，被 LTO 内联进 memory_dev_init） | `add_boot_memory_block(nr)` |
| 错误处理 | 累计 ret，循环后统一 panic | 首个错误即 panic |
| 收尾 | `mutex_unlock(&mem_sysfs_mutex)`；`out:` ret 检查 panic | 无 |

### 2.2 add_boot_memory_block → init_memory_block 链

| 项 | 5.4 | 6.12 |
|---|---|---|
| section 计数 | `add_memory_block` 内联计数（present_section_nr 逐 section），0 则跳过 | `add_boot_memory_block` 同样计数（memory.c:831-845） |
| block 查重 | `find_memory_block_by_id` → `subsys_find_device_by_id`（**klist 线性扫描** + get_device） | `find_memory_block_by_id` → **`xa_load(&memory_blocks)`**（xarray，无锁） |
| 分配 | `kzalloc(sizeof(*mem), GFP_KERNEL)` | 同 |
| 字段 | `start_section_nr / state / phys_device=arch_get_memory_phys_device(start_pfn) / nid` | `start_section_nr / state / nid` + `INIT_LIST_HEAD(group_next)` +（CONFIG_NUMA 关时）`zone=early_node_zone_for_memory_block()`（纯计算） |
| 注册 | `register_memory(mem)`（静态，内联）→ **device_initialize + device_add**（LTO 拆开 device_register） | `__add_memory_block(mem)` → **`device_register(&memory->dev)`** |
| 注册后 | `mem->section_count = count` | **`xa_store(&memory_blocks, dev.id, mem, GFP_KERNEL)`**（xarray 登记） |

差异点（均非卡因，但构成"区域差异清单"）：
- 5.4 的查重走 klist（每次 O(n) 扫描 + klist 自旋锁）；6.12 走 xarray（O(1) 无锁）。方向相反但都琐碎。
- 6.12 在 device_register 之后才 xa_store；5.4 无独立登记表（设备即登记）。
- 6.12 的 `early_node_zone_for_memory_block` 在注册前遍历 `pgdat->node_zones`（MAX_NR_ZONES 小循环），
  无锁、不触物理内存。
- **`mem_sysfs_mutex` 删除**：6.12 memory_dev_init 无任何锁。boot 期单 CPU0 创建 ⇒ 无并发，非卡因。
  但 5.4 的锁使整个循环成为"原子区间"，6.12 则没有——若存在其它线程在循环期间
  sysfs/kernfs 活动（6.12 有 kdevtmpfs 线程常驻、migration/kworker 等），差异放大（见 §8 D2）。

### 2.3 反汇编验证（stock_Image_full.bin，objdump --adjust-vma=0xffffffc010080000）

**stock memory_dev_init = 0xffffffc01218ec8c**（.init.text，file_off=0x210ec8c）：
```
bl  0xffffffc010bc0e4c            ; memory_block_size_bytes
lsr x8, x0, #27 ...  popcount      ; is_power_of_2
str w8, [x20,#1848]                ; sections_per_block
bl  0xffffffc010b7d1bc            ; subsys_register (subsys_system_register 内联)
bl  0xffffffc0116a336c            ; mutex_lock          ← mem_sysfs_mutex
loop: 计数 present_section_nr（ldr + tbz bit0）
  udiv x1, x21, x8                 ; block_id = nr / sections_per_block
  mov w2, #1                       ; MEM_ONLINE
  bl  0xffffffc010bc1610          ; init_memory_block
  str w24, [x8, #16]               ; mem->section_count = count
bl  0xffffffc0116a2548            ; mutex_unlock
```
与 5.4 源码逐行对应；**确认 stock 在循环外加 mem_sysfs_mutex**。

**stock init_memory_block = 0xffffffc010bc1610**（register_memory 已 LTO 内联）：
```
bl  0xffffffc011680fbc            ; subsys_find_device_by_id (查重, 线性扫描)
bl  0xffffffc0104b5d40            ; kzalloc(size 0xdc0=sizeof(memory_block), GFP_KERNEL=0x3e0)
bl  0xffffffc010bc0fc8            ; arch_get_memory_phys_device(start_pfn)  ← 5.4 独有
...  填 dev.bus/&memory_subsys、dev.id、release、groups、offline
bl  0xffffffc010b75b2c            ; device_initialize   ← device_register 被 LTO 内联拆开
bl  0xffffffc010b75d04            ; device_add
```
**stock 的 register 路径 = device_initialize + device_add，与 6.12 的 device_register 等价。**

**stock device_add = 0xffffffc010b75d04**（调用清单，全部 bl 目标已解析）：
```
get_device → device_private_init(kzalloc) → dev_set_name → get_device_parent
→ kobject_add(0xffffffc011683da4)
→ software_node_notify(0xffffffc010b9586c)          ; device_platform_notify 内联
→ sysfs_add_file_mode_ns(0xffffffc010639008)        ; device_create_file(uevent attr)
→ sysfs_do_create_link_sd ×(0xffffffc01063a584)     ; device_add_class_symlinks（memory 无 class → 跳过）
→ internal_create_group(0xffffffc01063ab48)         ; device_add_attrs
→ bus_add_device(0xffffffc010b7f58c)
→ dpm_sysfs_add(0xffffffc010b959e8)
→ device_pm_add(0xffffffc010bae5ec)
→ (devt=0 → 无 dev 属性 / 无 devtmpfs)
→ blr x21 (KCFI 间接调用)                            ; blocking_notifier_call_chain(BUS_NOTIFY_ADD_DEVICE)
→ kobject_uevent_env(0xffffffc011684c2c)            ; kobject_uevent(KOBJ_ADD)
→ bus_probe_device(0xffffffc010b7f784)
→ klist_add_tail(0xffffffc01168161c)                ; knode_parent（memory parent=NULL → 跳过）
→ __mutex_lock_slowpath / __mutex_unlock_slowpath   ; class mutex（memory 无 class → 跳过）
```
**stock bus_add_device = 0xffffffc010b7f58c**：
```
kobject_get(bus_get) → internal_create_group(dev_groups) → sysfs_do_create_link_sd ×2
→ klist_add_tail(knode_bus) → (error: sysfs_remove_group ×2)
```
与 6.12 的 bus_add_device（bus.c:504-563）**逐段相同**（6.12 多 driver_override 分支，memory bus 未设 ⇒ 跳过）。

---

## 3. device_add 路径对比（core.c）

6.12 device_add = core.c:3576，5.4 = core.c:2113。逐段对比：

| 段 | 5.4 | 6.12 | 差异影响 |
|---|---|---|---|
| get_device / device_private_init | 同 | 同 | 无 |
| dev_set_name | `dev->init_name` / `bus->dev_name + id` | 同（memory 走 `%s%u`="memory%d"） | 无 |
| get_device_parent | 同（memory → devices_kset） | 同（class_to_subsys(NULL)→NULL 快速返回） | 无 |
| kobject_add | kernfs_create_dir_ns | 同 | 见 §6 kernfs |
| device_platform_notify | `(dev, KOBJ_ADD)` → software_node_notify | `(dev)` → software_node_notify | memory 无 fwnode/swnode ⇒ 空操作 |
| device_create_file(uevent) | sysfs_create_file | 同 | 无 |
| device_add_class_symlinks | memory 无 class 跳过 | 同 | 无 |
| device_add_attrs | 4~5 个文件（phys_index/state/phys_device/removable[/valid_zones]） | 同（groups 相同） | 无 |
| bus_add_device | 同 | 同（driver_override 跳过） | 无 |
| dpm_sysfs_add / device_pm_add | 同 | 同（diff 为空） | 无 |
| devt 分支 | `MAJOR(dev->devt)`=0 ⇒ 跳过 dev 属性 / sys_dev_entry / **devtmpfs_create_node** | 同 | 无（§4） |
| bus_notify | `blocking_notifier_call_chain(&bus->p->bus_notifier, ...)` | `bus_notify()` → 同链（bus_to_subsys kref + 同链） | memory 无订阅者 ⇒ 空操作 |
| kobject_uevent(KOBJ_ADD) | kobject_uevent_env | 同 | 见 §5 |
| **fw_devlink_link_device** | **无** | 有，但 `dev->fwnode==NULL` ⇒ **跳过** | 无 |
| **ready_to_probe** | **无** | `device_lock(dev); dev_set_ready_to_probe(dev); device_unlock(dev);` | dev->mutex 短临界，无并发持有者 |
| bus_probe_device | device_initial_probe（memory bus 无驱动 ⇒ 空转）+ interface 遍历（空） | 同（多了 sp kref） | 无 |
| fw_devlink_unblock_consumers | 无 | `dev->fwnode==NULL` ⇒ 跳过 | 无 |
| klist_add_tail(knode_parent) | parent=NULL ⇒ 跳过 | 同 | 无 |
| class 处理 | `dev->class` NULL ⇒ 跳过 | class_to_subsys(NULL) ⇒ 跳过 | 无 |

**结论：对 memory 块，6.12 与 5.4 的 device_add 可执行路径逐段等价；新增代码全部被
fwnode=NULL/class=NULL/devt=0 短路。device_add 内部没有任何"等待另一个核"的原语。**

---

## 4. devtmpfs 专题（任务重点方向）

### 4.1 memory 块不触发 devtmpfs（两版一致，伪命题坐实）
- `devtmpfs_create_node(dev)` 只存在于 `if (MAJOR(dev->devt))`（6.12 core.c:3659-3669；
  5.4 core.c:2196-2206）。memory 块 `dev->devt=0`（kzalloc 清零，`__add_memory_block` 不设 devt）⇒
  永不调用。两版一致。
- 即便被调用：`device_get_devnode()` 需要 `dev->type->devnode`；memory 块 `dev->type=NULL` ⇒ 返回 NULL ⇒
  直接 return（6.12 devtmpfs.c:114-139；5.4 同）。**双重短路。**

### 4.2 devtmpfs 线程的真实存在差异（竞态 A 区域）
- **原厂 5.4：`# CONFIG_DEVTMPFS is not set`**（work/kernel.config）——原厂**根本没有 kdevtmpfs 线程**，
  `driver_init` 第一个成员也不是 devtmpfs_init（原厂 driver_init 8 调用 = devices_init → buses_init →
  classes/firmware/hypervisor(内联) → of_core_init → platform_bus_init → cpu_dev_init →
  memory_dev_init → container_dev_init，见 SUMMARY §1.4/§2.14）。
- **6.12：`CONFIG_DEVTMPFS=y + CONFIG_DEVTMPFS_MOUNT=y`**。`driver_init`（init.c，当前树）第一个成员
  `devtmpfs_init()`：
  - `kthread_run(devtmpfsd, ...)`（devtmpfs.c:465）→ **跨簇唤醒** kdevtmpfs 到某空闲 A76 核；
  - CPU0 `wait_for_completion(&setup_done)`（devtmpfs.c:467）——**CPU0 阻塞等 kdevtmpfs 在别的核
    完成 devtmpfs mount**。这正是竞态 A（y=270 x=80 亮、x=116 不亮）的挂点。
  - 6.12 devtmpfs 请求通道 = `devtmpfs_submit_req`（devtmpfs.c:97-112）：`spin_lock(req_lock) → 入链 →
    wake_up_process(thread) → **wait_for_completion(&req->done)`**——任何带 devt 的 device_add 都会
    走这条"唤醒 kdevtmpfs + 等它"的跨核链路。**memory 块不触发，但 do_initcalls 之后第一个带 devt 的
    设备（e.g. tty/block）会触发**（SUMMARY §2.14 H6）。5.4 完全无此机制。
- **对 memory_dev_init 的影响**：间接。kdevtmpfs 线程在 devtmpfs_init 里已被唤醒并 mount 完成
  （本次观测已过该阶段），随后常驻 devtmpfsd 循环睡在某核。它不触碰 kernfs/sysfs/slab 之外的
  memory 路径。**"kdevtmpfs 线程协作"不是 memory device_register 卡点的直接机制。**

### 4.3 但 devtmpfs_init 是"跨簇唤醒链路"的第一个大规模实例
竞态 A（devtmpfs setup_done）与竞态 B（memory device_register）在先前会话被判定"大概率同源"
（SUMMARY §2.15）：6.12 把跨簇唤醒空闲核改为 CSD+IPI_CALL_FUNC（`__ttwu_queue_wakelist`），
5.4 用 `rq->wake_list + IPI_RESCHEDULE`。**A 是这条链路失败的可见实例；B 是同一链路
"某次更早失败"的延迟后果**（持锁任务未醒 / slab 锁被楔住核持有 / nohz kick 停滞 ⇒ CPU0 的
device_add 在分配/锁点卡住）。因此"devtmpfs"与"memory"两个卡点应合并为同一根因（§8 D1）。

---

## 5. uevent 专题

| 项 | 5.4 | 6.12 |
|---|---|---|
| 入口 | device_add → `kobject_uevent(&dev->kobj, KOBJ_ADD)` → kobject_uevent_env | 同 |
| kset 解析 | dev->kobj.kset = devices_kset（device_initialize 设置）→ dev_uevent_ops | 同（6.12 device_initialize core.c:3161） |
| filter | dev_uevent_filter：`dev->bus || dev->class` ⇒ memory 通过 | 同 |
| env | SUBSYSTEM=memory、DEVPATH=/devices/memory/memoryN（kernfs_path_from_node，kernfs_path_lock）、SEQNUM、ACTION=add | 同 |
| **seqnum** | `u64 uevent_seqnum`，`++uevent_seqnum` **持 uevent_sock_mutex**（kobject_uevent.c:596-599） | `atomic64_inc_return(&uevent_seqnum)`（无锁）；**uevent_sock_mutex 移到广播内**（kobject_uevent.c:314-337） |
| 广播 | `uevent_net_broadcast_untagged`（持 mutex 遍历 uevent_sock_list，netlink send） | 同结构（mutex 在函数内） |
| helper | `CONFIG_UEVENT_HELPER` 未开 ⇒ 无 usermodehelper | 同（.config `# CONFIG_UEVENT_HELPER is not set`） |

- 两版对每个 memory 块都发一次 netlink uevent（若无监听者则丢包），都持同一个全局
  `uevent_sock_mutex`。**差异仅 seqnum 的原子化 + 互斥锁位置**，行为等价。
- 6.12 `kobject_uevent_net_broadcast` 在 `net->uevent_sock == NULL` 时直接返回（不建 socket）⇒
  即使 netns 未初始化也无等待。**uevent 不是卡点。**

---

## 6. kernfs / klist 锁专题

### 6.1 kernfs（sysfs 后端）

| 项 | 5.4 | 6.12 |
|---|---|---|
| 树结构锁 | **全局 `DEFINE_MUTEX(kernfs_mutex)`**（dir.c:35） | **per-root `kernfs_rwsem`**（down_write）+ `kernfs_iattr_rwsem` |
| kernfs_add_one | `mutex_lock(kernfs_mutex)` → link_sibling → iattr 时间戳 → unlock | `down_write(root->kernfs_rwsem)` → link_sibling（**`spin_lock(kernfs_idr_lock)` + `idr_alloc(...,GFP_ATOMIC)`**）→ `down_write(root->kernfs_iattr_rwsem)` → `ktime_get_real_ts64` → 双 unlock → kernfs_activate |
| 每 memory 块节点数 | 1 dir + 5~6 文件 + 2 链接 ≈ 8-9 次 kernfs_add_one | 同（约 10 次，含 dpm power 组） |
| 总规模 | 24~48 块 × ~9 ≈ 220-430 次 | 同量级 |

- 两版都在每次节点创建时 `ktime_get_real_ts64`（timekeeper 已就绪，无锁阻塞点）。
- 6.12 每节点多 1 次 rwsem 写锁 + 1 次 idr 自旋锁 + 1 次 iattr rwsem；**boot 期只有 CPU0
  触碰 sysfs ⇒ 无竞争，非卡因**。但如果另一核被楔住且持有其中任一把（per-root rwsem 的
  wait_list / 自旋锁），CPU0 会死等——这仍是"楔住核"模型的表达，不是 kernfs 自身缺陷。
- kobject_add 之后的 `device_create_file(uevent)`、`device_add_attrs`、bus_add_device 的两个
  `sysfs_create_link`、dpm_sysfs_add 全部走同一 kernfs 路径。

### 6.2 klist（bus 设备链表）

- `bus_add_device` 末尾 `klist_add_tail(&dev->p->knode_bus, &sp->klist_devices)`：
  5.4 bus.c:462 / 6.12 bus.c:550；实现（lib/klist.c）两版同构：`klist->k_lock` 自旋锁 +
  `klist->get` 回调（klist_devices_get → get_device）。临界区极短，无跨核依赖。
- 5.4 的 `find_memory_block_by_id → subsys_find_device_by_id` 每 block 扫一次 klist（持 klist 锁 +
  get_device）；6.12 用 xarray。均为微操作。

### 6.3 device_add 内全部锁的"无等待"核实
device_add 路径内所有阻塞原语 = kernfs rwsem / klist 自旋锁 / dpm_list_mtx /
uevent_sock_mutex / dev->mutex / SLUB node 锁 / page allocator zone 锁 + GFP_KERNEL 分配。
**无 completion、无 synchronize_rcu/synchronize_srcu、无等待队列**（与 REPORT-CURRENT-RACE-ANALYSIS
§3.4 结论一致）。⇒ **卡死只可能来自"另一核持锁不释放（被楔住）"或"分配器/对象损坏"。**

---

## 7. 原厂反汇编验证汇总（任务 4）

| 原厂符号 | 地址 | 验证结论 |
|---|---|---|
| memory_dev_init | fffffffc01218ec8c | subsys_register → mutex_lock(mem_sysfs_mutex) → 循环（present 计数 + init_memory_block + section_count 写）→ mutex_unlock；**与 5.4 源码逐行一致** |
| init_memory_block | fffffffc010bc1610 | subsys_find_device_by_id 查重 → kzalloc(0xdc0) → arch_get_memory_phys_device → 填 dev 字段 → device_initialize(0xffffffc010b75b2c) → device_add(0xffffffc010b75d04)；**register_memory/device_register 被 LTO 内联拆开** |
| device_add | fffffffc010b75d04 | 调用序列与 5.4 core.c:2113 完全对应（§3 表）；无 devtmpfs、无 fw_devlink、无 ready_to_probe、无 CASP 特判；**无 dsb/dmb/isb/dc/ic/tlbi 痕迹**（与 SUMMARY §2.1 结论 A 一致） |
| bus_add_device | fffffffc010b7f58c | kobject_get → internal_create_group → sysfs_do_create_link_sd ×2 → klist_add_tail → 错误路径 sysfs_remove_group ×2；与 6.12 bus.c:504 逐段相同 |
| 原厂构建特征 | — | **CONFIG_LTO_CLANG=y + CONFIG_CFI_CLANG=y**（kernel.config）——解释了 LTO 内联（register_memory 进 init_memory_block、subsys_system_register 进 memory_dev_init）与 device_add 内 `blr x21`（KCFI）间接调用 |

---

## 8. 综合：最可疑差异排序（为什么 6.12 卡、原厂能过）

> 依据：本报告 §2-§7 + SUMMARY-ALL-ANALYSIS §2.8/§2.14/§2.15 + REPORT-CURRENT-RACE-ANALYSIS §2/§4。

| 序 | 差异 | 位置 | 为什么它能解释"间歇卡在 device_register" |
|---|---|---|---|
| **D1** | **跨簇唤醒/通知传输：6.12 = CSD+IPI_CALL_FUNC（`__ttwu_queue_wakelist`/`kick_ilb`），5.4 = 直接入队+IPI_RESCHEDULE（SGI#0）** | kernel/sched/core.c:3884-3915、fair.c kick_ilb | 竞态 A（devtmpfs setup_done）已直接观测到"跨簇 CSD+IPI_CALL_FUNC 唤醒失败"；B 是同一链路的延迟表现：**更早某次跨簇唤醒/nohz-kick 失败 → 某 A76 核或任务进入坏状态（持 slab/zone 锁不释放 / kthread 未醒 / idle-balance 停滞）→ CPU0 的 device_add 在 kzalloc / kernfs_rwsem / klist / uevent 锁上死等**。卡点漂移 = 每次"哪次唤醒丢失"不同。5.4 用 SGI#0 直接入队，T770 上被证明可靠。 |
| **D2** | **driver_init 成员与顺序**：6.12 = bdi→**devtmpfs**→…→of_core_init→platform→auxiliary→**memory_dev_init→node→cpu**；5.4 = …→of_core_init→platform→**cpu_dev_init→memory_dev_init**（无 bdi/devtmpfs/auxiliary/node） | drivers/base/init.c | memory_dev_init 从 5.4 的"第二个 device_register 风暴"变成 6.12 的"**第一个**"：8 核环境首次大规模 slab/kernfs/uevent 分配发生在"跨簇唤醒链路最脆弱"的时点；且 6.12 的 memory 循环无 mem_sysfs_mutex（D3）包护。 |
| D3 | mem_sysfs_mutex 删除（6.12 memory_dev_init 无锁） | memory.c | boot 期无并发，非独立卡因；但使整个循环失去"原子区间"语义，与 D2 叠加放大 |
| D4 | kernfs 全局 mutex → per-root rwsem + idr 自旋锁 + iattr rwsem | fs/kernfs/dir.c | 锁数量/重量增加；boot 期单 CPU0 使用，非独立卡因；若楔住核持锁则表现为死等（归入 D1 后果） |
| D5 | SLUB：6.12 try_cmpxchg128（LSE casp）vs 5.4 cmpxchg_double（LL/SC） | mm/slub.c | 当前树已强制 LL/SC（`system_uses_lse_atomics()=false`）**仍卡** ⇒ 排除"casp 损坏"为当前独立根因（残余 LSE 站点见 §8 注） |
| D6 | 查重/登记：klist 扫描 vs xarray；devtmpfs 有无；uevent seqnum 原子化 | memory.c/core.c | 微差异，非卡因（已逐项核实） |

**注（D5 残余）**：即使 `system_uses_lse_atomics()=false`，SM#2 的 apply_alternatives_all 仍把
~12K 个 LSE 替代站点（CB 型 10,901 + 经典 1,175，见 SUMMARY §2.10）patch 成 LSE 指令——
这些站点（uaccess、设备访问等）在 8 核上仍执行 LSE。故"强制 LL/SC"实验只能排除
"SLUB 的 casp 损坏"这一支，不能排除"其它 LSE 指令/混合指令流"问题。若要彻底 A/B，
需同时跳过 apply_alternatives_all 的 LSE 类（代价：回到全 LL/SC，性能可接受）。

**一句话结论**：memory_dev_init/device_add 区域本身 5.4 与 6.12 等价且无跨核等待原语；
6.12 间歇卡在 device_register 的最可疑差异 = **D1（跨簇唤醒/通知从 SGI#0+直接入队改为
CSD+IPI_CALL_FUNC）导致更早的跨簇唤醒失败残留坏状态**，其次是 D2（memory_dev_init 成为
8 核下第一个 device_register 风暴 + devtmpfs 线程常驻 + 无 mem_sysfs_mutex）。

---

## 9. 修复建议（按优先级）

1. **P1（首选，一行级改动，完全回到 5.4 语义）**：`kernel/sched/core.c` 的 `ttwu_queue_cond()`
   恒返回 false ⇒ 所有唤醒（含空闲核唤醒、nohz idle balance kick 走 `smp_send_reschedule`）
   直接入队 + resched_curr + **IPI_RESCHEDULE（SGI#0）**，弃用 CSD+IPI_CALL_FUNC 承载唤醒。
   **当前树未含该补丁（git status 无 sched 改动，P1 实验未落盘）**。
2. **P2（诊断隔离，一次一个变量）**：`nohz=off`（A76 持续 tick）；`cpuidle.off=1`（WFI-only）；
   `maxcpus=4`（仅 A55）；boot 期 `set_cpus_allowed_ptr(kdevtmpfs, cpumask_of(0))` 及早期 kthread
   pin A55——任何一项消除竞态 ⇒ 坐实"目标核/唤醒链路"。
3. **P3（诊断盲区）**：开 `CONFIG_SOFTLOCKUP_DETECTOR=y`（stock 开、mainline 关），区分"慢 vs 死"；
   y=355 block 计数 + device_add 子步骤探针（kernfs_add_one / klist_add_tail / kobject_uevent_env /
   kzalloc）一次刷机定位精确子步骤与 block 序号。
4. **P4（内存损坏验证）**：`slub_debug=U`（或 FZPU）一轮；`CONFIG_DEBUG_LIST/OBJECTS/SPINLOCK` 一轮；
   若坐实损坏，再对"残余 LSE 站点"做 A/B（跳过 alternatives 的 LSE 类，全 LL/SC）。
5. **P5（config/时序对齐原厂，低成本）**：`CONFIG_NR_CPUS=32`（stock）；恢复 `mem_sysfs_mutex`
   包循环（无害）；`CONFIG_DEVTMPFS` 可选关闭（与 stock 一致，消除 kdevtmpfs 线程与后续
   带 devt 设备的跨核等待；代价 = initramfs 无 /dev 自动创建，本阶段 initramfs 已自行 mknod
   则无影响）；开 SOFTLOCKUP。

---

## 10. 附录：符号/地址表

### 原厂（stock_kallsyms.map / stock_Image_full.bin）
| 符号 | 地址 |
|---|---|
| memory_dev_init | fffffffc01218ec8c |
| init_memory_block | fffffffc010bc1610 |
| memory_block_size_bytes | fffffffc010bc0e4c |
| subsys_register / subsys_system_register | fffffffc010b7d1bc / fffffffc010b7df88 |
| device_register / device_initialize / device_add | fffffffc010b75afc / fffffffc010b75b2c / fffffffc010b75d04 |
| dev_set_name / get_device_parent | fffffffc010b77180 / fffffffc010b771f8 |
| bus_add_device / bus_probe_device | fffffffc010b7f58c / fffffffc010b7f784 |
| dpm_sysfs_add / device_pm_add | fffffffc010b959e8 / fffffffc010bae5ec |
| kobject_add / kobject_uevent_env / kobject_uevent | fffffffc011683da4 / fffffffc011684c2c / fffffffc011685890 |
| klist_add_tail | fffffffc01168161c |
| subsys_find_device_by_id | fffffffc010b7e3dc |
| kernfs_add_one / kernfs_create_dir_ns | fffffffc010631f84 / fffffffc010633150 |
| mutex_lock / mutex_unlock | fffffffc0116a336c / fffffffc0116a2548 |
| memory_subsys（数据） | fffffffc0123f0fa0 |

### mainline 6.12（当前树源码位置，供探针参考）
- memory.c: `memory_dev_init` 970 / `add_boot_memory_block` 831 / `add_memory_block` 785 /
  `__add_memory_block` 679（device_register 前探针 691 / 返回探针 696）
- core.c: `device_add` 3576 / `device_register` 3788 / `device_initialize` 3160
- bus.c: `bus_add_device` 504 / `bus_notify` 1039
- devtmpfs.c: `devtmpfs_create_node` 114 / `devtmpfs_submit_req` 97 / `devtmpfs_init` 449
- init.c: `driver_init`（memory 在 auxiliary_bus_init 之后、node_dev_init 之前）
- lib/kobject_uevent.c: `kobject_uevent_env` 473
- fs/kernfs/dir.c: `kernfs_add_one` 770
