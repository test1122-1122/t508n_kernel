# T508N 当前竞态（A: devtmpfs setup_done 卡 / B: memory_dev_init device_register 卡）根源分析

> 任务：`prompt-current-race-analysis.md`（2026-08-18 之后的新一轮，输入 = SUMMARY-ALL-ANALYSIS.md 全部成果
> + 新一轮关键约束：**两种唤醒路径（6.12 原版 CSD+IPI_CALL_FUNC vs P1 直接入队+RESCHEDULE）竞态相同**、
> **cpuidle 去 idle-states 竞态仍现**；LSE static_key / NOMAP 已修复；16 项假设已排除）。
> 源码：`linux-6.12.103/`（当前树 = HEAD cb8e725f3 + 16 文件探针/修复，**树内 ttwu_queue_cond 仍是原版**，P1 不在树中）。
> 只读 SUMMARY-ALL-ANALYSIS.md 与对应 REPORT-*.md；未重复已排除假设。
> 生成：2026-08-18。

---

## 0. 结论摘要（TL;DR）

1. **失败点被探针逻辑压缩到一个狭小区间**：y=270 x=80 亮（kthread_run 返回）⇒ 首次唤醒链
   （kthreadd→kernel_thread→wake_up_new_task→空闲 A76 首跑 kthread() 入口→complete(create->done)→睡下）
   完全可靠；y=290 x=8 不亮 ⇒ 二次 `wake_up_process(kdevtmpfs)` 之后、任务再次运行之前，**devtmpfsd
   从未执行**。卡点 = "二次唤醒 → 任务在目标核上再次被调度" 之间的共享环节。
2. **P1（所有唤醒直接入队+IPI_RESCHEDULE）下 x=80 仍亮 ⇒ 入队动作已完成**（CPU0 已完成 on_cpu 自旋 +
   远程 rq 加锁 + activate_task + resched_curr + IPI 发出），因此 **P1 下失败点必然在"目标核侧"**：
   IPI 未达 / IPI 到达但 idle 循环未重新调度 / 调度了但 pick 未选中 / 选中但未切换 / 切换后任务死在
   devtmpfsd 之前。6.12 原版下另多一个"CSD 丢失（入队未完成）"的可能——**两种路径"竞态相同"若为真，
   则原版下的失败点也应归到目标核侧；若原版是 CSD 丢失而 P1 是目标核侧，则"相同"只是表象**。
   这是本报告第一个必须用探针钉死的分叉点（见 §5 D0-1）。
3. **对"用户怀疑'启动中有其他问题'"的回应**：y=310（A76 无 tick）**无法区分"NOHZ 空闲"与
   "IRQ-off 被楔死"两种状态**——这是当前观测的最大盲点。若 A76 核在 SM#2 之后、driver_init 之前
   的早期跨核活动（migration/kworker 首跑、stopper 退出、广播 TLB/IC 维护）中被楔住（IRQs 关、
   持锁自旋），则：竞态 A = 目标核无法处理唤醒 IPI；竞态 B = CPU0 之后在该核持有的锁（slab
   node_lock / kernfs rwsem / rq lock）上自旋；卡点漂移 = 每次重启"哪个核、哪把锁先坏"不同。
   **最可疑方向排序：① SLUB/内存损坏（8 核并发 slab，casp 首次大规模使用）② 某核被楔住
   （源头在 SM#2 之后早期跨核活动）③ RCU（PREEMPT_RCU+NOHZ，A76 EQS）④（次）GIC/SGI 投递层
   ——y=325 是累积性证据，不能排除二次唤醒 IPI 丢失**。
4. devtmpfs_setup（ksys_unshare/init_mount）经分析**不是当前观测卡点**（x=8 不亮已排除"卡在 setup"）；
   其全局锁依赖只有 namespace_sem，且 boot 期无并发持有者；但列出其完整阻塞面供"线程运行但卡在 setup"
   变体使用（§3.3）。
5. **验证优先级**：先加 5 个探针钉死"IPI 是否到 / 目标核是否在 idle 循环 / 任务是否被 pick / 任务是否
   跑过 kthread() 续跑点"，再按 maxcpus=4 → nohz=off → cpuidle.off=1 → kdevtmpfs pin CPU0 二分，
   再上 slub_debug=U + 强制 LL/SC A/B 与 RCU 隔离；同时开 SOFTLOCKUP 消除"慢 vs 死"盲区（当前关）。

---

## 1. 证据重读：探针告诉了我们什么、没告诉我们什么

### 1.1 已确认（探针逻辑推导，无歧义）

| 探针 | 状态 | 结论 |
|---|---|---|
| y=270 x=80（kthread_run 后） | 亮 | `kthread_create_on_node` 的 `wait_for_completion_killable(&done)` 已返回 ⇒ kdevtmpfs 已在其**首跑**中执行 `complete(create->done)` ⇒ 首次激活链（kthreadd 唤醒[kthreadd 在 CPU0，同核直接入队] → kernel_thread → wake_up_new_task[select_task_rq → 空闲 A76，直接入队+IPI_RESCHEDULE，两版一致] → A76 首跑 kthread() 入口 → `__set_current_state(UNINTERRUPTIBLE)` + `schedule_preempt_disabled()` 睡下）**全部可靠** |
| y=290 x=8（devtmpfsd 入口） | 不亮 | 二次 `wake_up_process(kdevtmpfs)` 之后任务**从未再运行**（kthread() 续跑段 `preempt_enable → cgroup_kthread_ready → __kthread_parkme → threadfn` 全部无锁无阻塞，只要任务被调度 x=8 必亮——除非任务在该段崩溃） |
| y=270 x=116（wait_for_completion 返回） | 不亮 | setup_done 永不 complete，CPU0 永久阻塞（swait 队列项无超时） |
| y=325（do_handle_IPI 入口，累积） | 8 格亮 | **每个核都"曾经"收到过 IPI** —— 仅此而已；不能证明"二次唤醒的那次 IPI 到达目标核"（格子可能在更早的唤醒/维护 IPI 时点亮） |
| y=310（timer_handler，累积） | A55 前 4 格亮、A76 后 4 格不亮 | A76 各核的 tick 早已停。**歧义：NOHZ idle（tick_nohz_idle_stop_tick）与"IRQ-off 被楔死"在屏上不可区分**（前者是正常态，后者是死态） |
| y=150/160/180/200（SM#2） | 全亮 | stop_machine（apply_alternatives_all）8 核协议通过；LSE `ldaddal` 跨 8 核正确 ⇒ 低层原子在 SM#2 时点 OK |
| y=230 / y=240（page_alloc 段） | 过 | NOMAP 修复（set_zone_contiguous 遇洞即停）生效；page_alloc_init_late 段（含 buffer_init 首次 post-SMP cpuhp/slab）已通过 |

### 1.2 已知但无法定性的（探针盲区，§5 的验证对象）

- **B1**：二次唤醒的 IPI（无论 RESCHEDULE 还是 CALL_FUNC）是否**真的到达目标 A76 核**？y=325 是累积心跳，无法回答。目标核地址也未知（kdevtmpfs 睡在哪个 A76 核取决于 select_task_rq，逐次可能不同）。
- **B2**：目标 A76 核此刻**在不在 idle 循环里**？y=310 无法区分 NOHZ idle 与 IRQ-off 楔死。
- **B3**：任务是否被 enqueue 到目标 rq（6.12 原版下 CSD 丢失 ⇒ 入队未发生；P1 下 x=80 亮 ⇒ 入队必已发生）。
- **B4**："加探针能过、不加就卡"的强时序敏感性 ⇒ 任何 A/B 对照（P1 vs 原版）都必须**使用完全相同的探针布局**，否则结论可能因时序翻转而失真（P1 实验与 stock 实验若探针布局不同，"竞态相同"的判读需复核，见 §6.8）。

### 1.3 与 5.4 原厂的对照基线（SUMMARY §1.4/§2.15，未变）

- 5.4 同硬件可靠，其二次唤醒走 `rq->wake_list + IPI_RESCHEDULE`（SGI#0）；6.12 原版走
  `call_single_queue(CSD) + IPI_CALL_FUNC`（SGI#1）。
- **本板 DTS 拓扑特例（新确认）**：`ums9620.dtsi` 的 `cpu-map` 把 8 核全部放在**单一 cluster0**
  （无 thread 层、无 capacity-dmips-mhz）⇒ arm64 拓扑 = 单 MC 域（8 核），`sd_llc_id` 全同 ⇒
  `cpus_share_cache(任意两核)=true` ⇒ 6.12 原版 ttwu_queue_cond 的"异簇→wakelist"分支在本板**不触发**，
  触发的是 **"目标 rq 空 → wakelist"分支**（唤醒到空闲 A76 时）——即 6.12 原版实际走
  CSD+IPI_CALL_FUNC 的入口是"目标空闲"分支（与 SUMMARY §2.15"SM#2 时 CPU0 rq 非空未触发"一致）。
  P1（恒 false）则全走直接入队。两条路都失败 ⇒ 共享失败点排除"wakelist vs 直接入队"本身。

---

## 2. 失败点精确定位（从"唤醒路径已排除"出发的必然推论）

### 2.1 P1 路径下的逻辑压缩

P1 = `ttwu_queue_cond` 恒 false ⇒ `wake_up_process(kdevtmpfs)` 在 CPU0 上执行：

```
try_to_wake_up(p):
  pi_lock → state 匹配(UNINTERRUPTIBLE∈TASK_NORMAL ✓) → on_rq==0 ✓
  → [若 on_cpu==1: ttwu_queue_wakelist 恒 false ⇒ smp_cond_load_acquire(&p->on_cpu, !VAL) 自旋]
  → select_task_rq → set_task_cpu（若迁移）
  → ttwu_queue: rq_lock(目标 rq) → update_rq_clock → ttwu_do_activate(activate_task 入队)
                → ttwu_do_wakeup → check_preempt_curr → wakeup_preempt → resched_curr
                → set_nr_and_not_polling(目标 idle 任务 TIF_NEED_RESCHED=1) → smp_send_reschedule(IPI_RESCHEDULE)
  → rq_unlock
```

- **kthread_run 返回（x=80 亮）在 P1 下 ⇒ 上述全部完成**：任务已入队、TIF 已置、IPI_RESCHEDULE 已发出。
- 之后唯一能让 x=8 不亮的分支：
  - (a) IPI_RESCHEDULE 未达目标核（GIC/SGI 投递丢失）；
  - (b) 目标核收到 IPI 但未重新调度（不在 idle 循环 / idle 循环异常 / 中断返回路径异常）；
  - (c) 目标核调度了但 `pick_next_task_fair` 未选中该任务（cfs_rq 结构损坏 ⇒ 内存损坏）；
  - (d) 选中但上下文切换失败/任务切换后在 kthread() 续跑段崩溃（内存损坏 / 指令错误）。
- **5.4 同路径（直接入队+IPI_RESCHEDULE）在 T770 可靠 ⇒ (a)(b) 的硬件/协议基础 5.4 已证明可用**；
  6.12 与 5.4 在 (a)(b) 的差异只剩"IPI 类型编号、irq 处理代码版本、NOHZ 状态机版本"——均为全平台通用代码，
  无 T770 特判。这削弱 (a)(b) 而**增强 (c)(d)（内存/状态损坏）与"目标核被楔住"**的权重。

### 2.2 6.12 原版路径下的分叉

原版在 `on_cpu==1` 窗口与"目标 rq 空"窗口都走 `__ttwu_queue_wakelist`（CSD+IPI_CALL_FUNC）：
- 若 CSD 丢失 ⇒ 入队未发生 ⇒ 任务永久睡（x=8 不亮）——这是 6.12 原版独有的**入队前失败**可能；
- 若 CSD 到达但目标核侧失败 ⇒ 与 P1 共享目标核侧失败。

**若"两种路径竞态相同"是严格观测**（同探针布局下失败症状与频度一致），则原版也归到目标核侧（(a)-(d)），
CSD 丢失假设被排除；**若只是"症状相似"而未做严格同布局对照**，则 CSD 丢失仍在表内（§6.8 要求复核）。
无论哪种，**下一步的探针都应该同时覆盖两条路径**（P1 与原版都刷一遍），以把 (a)-(d) 逐项钉死。

### 2.3 竞态 A/B 的共享特征（SUMMARY §5.2 + 本报告）

- A：CPU0 阻塞在 `wait_for_completion(&setup_done)`，解除依赖"某任务（kdevtmpfs）在某个核上被调度"。
- B：CPU0 阻塞在 `device_register`（x=296 亮、x=380/260 不亮），device_add/kernfs 路径**无等待原语、
  无 RCU 同步、无 completion**（§3.4 核实），纯 = 锁自旋（kernfs_rwsem / klist / SLUB 锁）或分配。
  ⇒ **B 的阻塞只能来自"某把锁/某个 slab 分配路径被另一个核卡住"**。
- 共性：CPU0 的推进依赖"另一个核/子系统"；卡点逐次漂移 ⇒ "哪个核/哪个子系统先坏"逐次不同。
- **统一模型（候选 1）**：某 A76 核在 SM#2 之后被楔住（IRQs 关、持锁自旋）。A = 该核恰是 kdevtmpfs
  的目标核，无法处理唤醒；B = CPU0 之后撞上该核持有的锁。**该模型一次解释 A、B、漂移、探针时序敏感、
  y=310（楔死核无 tick）、y=325（楔死前点亮过）全部观测**。
- **统一模型（候选 2）**：SLUB 8 核并发下分配器状态损坏（casp/LL-SC 错误或 freelist 损坏）⇒ 某对象的
  内存被写坏（task_struct/se → "入队但永不选中"；kernfs_node/device → CPU0 的 device_add 内数据异常/
  死循环；freelist 本身 → 后续分配返回坏内存）。同样一次解释 A、B、漂移、探针时序敏感（分配时序不同
  ⇒ 损坏点不同）。

---

## 3. 共享失败点逐项分析（任务 1/2/4 要求）

### 3.1 二次唤醒链路的逐环节可靠性（kthread 生命周期，任务 4）

| 环节 | 执行者 | 6.12 行为 | 失败可能 |
|---|---|---|---|
| `kthread_create_on_node` → `wake_up_process(kthreadd)` | CPU0 | kthreadd 在 CPU0（rest_init 创建，无迁移理由）⇒ 同核直接入队 | 无（同核） |
| kthreadd → `create_kthread` → `kernel_thread` | CPU0 | copy_process 在 CPU0 分配 task_struct/栈（SLUB，CPU0 per-CPU slab） | **SLUB 损坏候选点①** |
| `wake_up_new_task(p)` | CPU0 | select_task_rq(WF_FORK) → 空闲 A76 → 直接入队 + IPI_RESCHEDULE（两版一致） | 已由 x=80 证明可靠 |
| kdevtmpfs 首跑（A76）：`sched_setscheduler_nocheck` + `set_cpus_allowed_ptr(current, HK_TYPE_KTHREAD)` + `complete(done)` + `schedule_preempt_disabled()` | A76 | 自改 affinity（housekeeping=全核，无迁移）；complete 后睡在 A76 | 已由 x=80 证明可靠 |
| 二次 `wake_up_process(kdevtmpfs)` | CPU0 | 原版：CSD+IPI_CALL_FUNC；P1：直接入队+IPI_RESCHEDULE | ★唯一不确定点（§2） |
| kthread() 续跑：`preempt_enable → cgroup_kthread_ready(仅置标志) → __kthread_parkme(置/清 TASK_PARKED) → threadfn=devtmpfsd` | 目标核 | **无锁、无阻塞**；x=8 在 devtmpfsd 第一行 | 若任务被调度则必亮；不亮 ⇒ 未被调度，或在此段崩溃（内存损坏） |

- `__kthread_parkme` 细节：`set_special_state(TASK_PARKED)` → 检查 `KTHREAD_SHOULD_PARK`（未置）→ break →
  `__set_current_state(TASK_RUNNING)` → 返回。无等待。
- **结论：只要任务在二次唤醒后被调度一次，x=8 必亮。** 失败 = "未被调度"或"调度前状态/内存已坏"。

### 3.2 目标核侧机制逐项（调度器/NOZ 在 8 核下的竞态，任务 2c）

**(i) resched_curr 的 `test_tsk_need_resched(curr)` 早退**：仅当目标 idle 任务 TIF 已置（瞬态"即将调度"），
  早退不丢唤醒（核反正要 schedule）⇒ 排除永久丢失。

**(ii) idle 循环 `while(!need_resched())` + WFI**：IPI 到达 → wfi 被唤醒（GIC 转发 pended IRQ，即使 PSTATE.I 掩蔽，
  wfi 仍因中断 pending 而返回，随后 `raw_local_irq_enable` 取中断）→ 返回循环再查 need_resched → schedule_idle。
  协议级无丢失窗口（arm64 全平台通用，5.4 同）。除非：(a) IPI 真没到；(b) 核不在 idle 循环（被楔住）；
  (c) 中断返回路径（irqentry_exit / irq_exit / tick_nohz_irq_exit / irqtime_account_irq）在 NOHZ idle 核上
  异常——6.12 全平台通用路径，无 T770 特判，可能性低但**静态不可排除**（无 console 时表现为静默卡死）。

**(iii) pick_next_task_fair 未选中已入队任务**：入队正确（P1 下 x=80 亮证明 activate_task 完成）但 CFS
  选不到 ⇒ cfs_rq 的 rb-tree/leftmost/`h_nr_running` 损坏 ⇒ 内存损坏（SLUB 双 free/越界写/写坏 se）。
  该症状对两种唤醒路径完全一致 ⇒ 与"竞态相同"吻合。

**(iv) 上下文切换后任务崩溃**：kthread() 续跑段无锁，崩溃 ⇒ 指令/数据损坏。指向同一内存损坏源。

**(v) 任务状态不一致（on_rq=1 且 state≠RUNNING 的死睡）**：ttwu 走 `ttwu_runnable` 返回 success 但不再入队；
  然而 CFS pick 不检查 state，死睡任务仍会被反复 pick 运行（运行后再次 schedule 自睡），不构成"永不运行"；
  且 P1 下 x=80 亮要求入队发生 ⇒ 排除。

### 3.3 devtmpfs_setup：ksys_unshare(CLONE_NEWNS) / init_mount 卡点（任务 1a —— 供"线程运行但卡在 setup"变体）

> 说明：当前观测（x=8 不亮）已**排除**"卡在 setup"（setup 在 x=8 之后）；本节为完整性 + 该变体做分析。
> 若未来观测变为"x=8 亮、x=44 不亮"则对照本节。

`devtmpfs_setup`（在 kdevtmpfs 线程 = 某 A76 核上执行）：

1. **`ksys_unshare(CLONE_NEWNS)`**（fork.c:3301）：
   - `unshare_fs` → `copy_fs_struct`：`kmem_cache_alloc(fs_cachep)`——A76 per-CPU slab；无锁阻塞（除非 slab node 锁被楔住核持有）。
   - `unshare_nsproxy_namespaces` → `create_new_namespaces` → `copy_mnt_ns`（fs/namespace.c）：
     - **`down_write(&namespace_sem)`**：全局 mount 命名空间锁。boot 期（driver_init 链内）唯一 namespace_sem 使用者 =
       init_mount 类调用（本线程自己、devtmpfs_mount 等）——**无并发持有者 ⇒ 正常立即获取**；若某处持有者被楔住
       （rwsem 慢路径会**睡眠等待**而非自旋 ⇒ A76 核随之 idle）⇒ 死等。这是 setup 路径唯一的全局锁依赖。
     - 遍历当前 ns 的 mount 表复制 vfsmount（boot 期仅 rootfs，量小）；`list_add_tail` + `mntget`（LSE ldaddal）。
   - `switch_task_namespaces`：`task_lock(current)` + rcu_assign_pointer，无阻塞。
2. **`init_mount("devtmpfs", "/", ...)`**：
   - `path_mount` → `do_new_mount` → `vfs_kern_mount`：**再取 `down_write(&namespace_sem)`**（与 1 不嵌套，先放后取）；
     `alloc_vfsmnt`（kmem_cache_alloc on A76）→ `shmem_get_tree` → `shmem_fill_super`（superblock/inode/page
     分配，均在 A76 的 slab）→ `do_add_mount` → `attach_recursive_mnt` → `mnt_set_mountpoint`/`commit_tree`。
   - 跨核依赖：全部为 A76 上的分配 + namespace_sem；无 completion、无 RCU 同步、无跨核等待。
3. `init_chdir("/..")` / `init_chroot(".")`：dcache/inode 锁，本地，量小。

**结论**：setup 的阻塞面 = namespace_sem（全局，boot 期无人竞争）+ SLUB 分配（依赖 slab 锁健康）。
若系统健康，setup 无卡点；若"线程运行了却卡在 setup"（x=8 亮 x=44 不亮），则几乎必然是
namespace_sem 持有者被楔住 或 SLUB 损坏 —— 与 §2.3 的统一模型同源。**当前探针已把这两种变体区分开
（x=8 是否亮），不要改动现有探针布局再引入歧义。**

### 3.4 竞态 B 的直接依赖核实（device_register 内为何能"无等待地卡住"）

- `__add_memory_block`（memory.c:679）：`device_register(&memory->dev)` → `device_add` → `kobject_add` →
  `kernfs_create_dir`/`sysfs_create_files`（**kernfs_rwsem 写锁** + 大量 `kmem_cache_alloc(kernfs_node)`）
  → `bus_add_device` → `klist_add_tail`（自旋锁）。
- **核实：device_add/kernfs/sysfs 路径无 completion、无 synchronize_rcu/synchronize_srcu**（core.c:268 的
  `synchronize_srcu(&device_links_srcu)` 在 device_del 路径，不在 device_add）。
- ⇒ B 的卡点只能是：① 某把锁（kernfs_rwsem / klist / SLUB node lock）被另一核持有且不释放（另一核被楔住）；
  ② 分配返回坏内存导致 kernfs 内部死循环/崩溃。两种都指向"另一核/分配器"状态 —— 与 A 的统一模型一致。
- 附加：`init_memory_block` 只做逻辑判断（zone_intersects 等），不触碰物理内存；`memory_block` 经 kzalloc 分配
  ⇒ 不涉及 no-map 空洞触碰（NOMAP 修复后 set_zone_contiguous 已不再过洞，且 memory_dev_init 不遍历物理页）。

### 3.5 SLUB 分析（任务 2a —— 统一模型候选 2 的机理）

- 6.12 SLUB freelist 更新用 `try_cmpxchg128`/`cmpxchg128`：LSE 路径 = `casp`/`caspl`，LL/SC 路径 = `ldxp/stxp`；
  原子选择 = `system_uses_lse_atomics()`（**本 bring-up 定制为 static_key，已在单核早期 enable**）。
- 5.4 用 `cmpxchg_double`（arm64 恒 LL/SC `ldxp/stxp` 对）。**两版机制不同：6.12 的 `casp` 是本 bring-up
  首次在 T770 上大规模使用**（SM#2 的 `ldaddal` 通过 ≠ 全部 LSE 指令正确；casp 的 128-bit 原子性、对齐、
  memory-model 语义与 ldaddal 不同）。
- 若 casp 在 A76/A55 某 revision 上存在缺陷（T770 的 A76 r0p0–r3p0 有 errata 背景）或被错误 patch/混用
  （LL/SC 与 LSE 指令流混用 ⇒ freelist 更新撕裂）⇒ freelist 损坏 ⇒ 分配返回重叠/已用对象 ⇒ 对象内容被
  并发写坏 ⇒ 症状 = "创建新对象"处的随机损坏（A：task_struct/se 坏 → 入队不选中；B：kernfs_node/device 坏
  → device_add 内异常）——**与"两种唤醒路径竞态相同"（损坏在目标核侧/分配器侧，与唤醒传输无关）、
  卡点漂移、探针时序敏感全部吻合**。
- 反方证据：原厂同硬件 SLUB 可跑（但用 LL/SC）；SM#2 8 核原子正确。⇒ 需 A/B 实验（强制 LL/SC）定案。
- **验证成本最低、信息量最大**：`slub_debug=U`（或 FZPU）开机参数 + `CONFIG_SLUB_DEBUG_ON` 一轮；再 A/B 强制 LL/SC。

### 3.6 RCU 分析（任务 2b —— PREEMPT_RCU + NOHZ + A76 EQS）

- config：`CONFIG_PREEMPT_RCU=y`、`CONFIG_NO_HZ_IDLE=y`、`CONFIG_RCU_STALL_COMMON=y`（stall 检测编译在，
  但无 console ⇒ 静默）。A76 核 NOHZ idle ⇒ 进入 EQS（dynticks 计数），GP kthread 依赖其退出 EQS 或
  FQS 循环 `resched_cpu` IPI 来收集 qs。
- **直接路径核实**：A 的 wait_for_completion 路径、B 的 device_add/kernfs 路径**均无 synchronize_rcu/call_rcu/
  synchronize_srcu 依赖** ⇒ RCU 不能直接卡死 A/B。RCU 只能通过间接途径（某持锁任务在 RCU 等待中未醒 /
  rcu_nocb 卸载路径 / grace period 卡住导致某 boot 路径阻塞）参与——均需先有"另一异常"。
- 结论：RCU 不是首要嫌疑，但 **A76 核的 EQS 状态是"楔住核"模型的重要旁证/干扰项**（EQS 下的核同样
  表现为无 tick），且 PREEMPT_RCU + NOHZ 组合的 dynticks/irqtime 交互是 6.12 全平台通用路径，静态排除不了。
  验证：`rcu_nocbs=4-7`（把 A76 的 RCU 回调卸载）、`rcupdate.rcu_cpu_stall_suppress=0`、`rcu_expedited` 观察。

### 3.7 "启动中有其他问题"：SM#2 之后到 driver_init 的特殊点（任务 3）

SM#2 之后的序列（kernel_init_freeable）：`sched_init_smp`（建调度域）→ `workqueue_init_topology` →
`async_init` → `padata_init` → `page_alloc_init_late` → `do_basic_setup`（cpuset_init_smp → driver_init → ...）。
其间**跨核活动清单**（A76 核的第一次"真实工作"窗口）：

1. **二级核退出 SM#2**：stopper 线程 `local_irq_restore` 后回 smpboot 循环 → schedule → idle。
   （若该退出路径异常 ⇒ 核被楔住，见 §2.3 候选 1。）
2. **migration/4-7 线程**（`migration_init` 是 early_initcall，在 do_pre_smp_initcalls = SM#2 之前已创建，
   当时 CPU4-7 未上线 ⇒ 这些任务挂在 CPU0 rq 上、cpus_ptr={4..7}；CPU4-7 上线后经负载均衡迁往各自核）。
3. **kworker/4-7**（`workqueue_init` 在 smp_init 之前为全部可能 CPU 建 per-CPU worker，同理先挂 CPU0 rq）。
4. **sched_init_smp**：CPU0 建调度域（单 MC 域 8 核 + SMT 退化 + NUMA 单节点），`migration_init` 的
   `smpboot_register_percpu_thread` 等在 CPU0 完成。
5. **workqueue_init_topology**（NUMA=y）：`wq_update_unbound_numa` 重建 unbound 池，可能新创 worker（跨核唤醒）。
6. **async_init / padata_init**：创建 async/padata kthread（跨核唤醒到空闲 A76）。
7. **page_alloc_init_late**：buffer_init（首个 post-SMP `__cpuhp_setup_state` + slab 创建）——已通过探针。

**特殊风险点**：
- 2/3 的"任务创建时目标核未上线/刚上线"窗口：`ttwu_queue_cond` 对 `!cpu_active(cpu)` 返回 false ⇒ 直接入队，
  任务留在 CPU0 rq ⇒ 依赖负载均衡迁往本核。若均衡器（NOHZ idle balance，6.12 = CSD+IPI_CALL_FUNC kick）
  在 A76 上失效 ⇒ migration/kworker 长期滞留 CPU0 rq，A76 核上无任务 ⇒ 不影响唤醒本身，但**是 6.12
  CSD 链路失效的另一观察点**（与唤醒同链，见 REPORT-stock-wakeup-nohz §3.2）。
- **NOHZ idle 的"失联"风险**：A76 核进入 NOHZ idle 后，唯一唤醒途径 = IPI（arm64 无 polling）；
  若 IPI 投递/处理在该核上失败 ⇒ 该核永久"失联"（既收不到任务唤醒也收不到 nohz kick）——y=310 外观
  与"失联"完全一致。**"失联"的成因仍是 IPI 链路本身（GIC/中断处理）或核被楔住**。

---

## 4. 假设排序（当前证据下的最可疑方向）

| 序 | 假设 | 与证据吻合度 | 关键验证 |
|---|---|---|---|
| H-A | **SLUB/内存损坏**：8 核并发 slab（casp 首次大规模使用 + 定制 LSE static_key）导致分配器/对象损坏 ⇒ "入队但永不选中"/device_add 内损坏 | 高：A/B 都是"创建新对象"；与唤醒传输无关（两种路径同坏）；漂移；探针时序敏感；无声 | `slub_debug=U`、强制 LL/SC A/B、DEBUG_LIST/OBJECTS 一轮 |
| H-B | **某核被楔住**（IRQs 关、持锁自旋），源头在 SM#2 之后早期跨核活动（stopper 退出 / migration/kworker 首跑 / 广播维护 / 锁竞争时序） | 高：一次解释 A、B、漂移、y=310 歧义、探针时序敏感 | idle 循环心跳探针（区分 NOHZ vs 楔死）+ maxcpus 二分 + lockup 检测 |
| H-C | **GIC/SGI 投递层对 A76 的 IPI 间歇丢失**（两类 SGI 同丢） | 中：y=325 无法排除；但 5.4 同硬件 SGI 可靠，6.12 无新 GIC 依赖（§1.4） | 按 IPI 类型的到达探针 + 目标核地址探针（P4 细化） |
| H-D | **RCU（PREEMPT_RCU+NOHZ）** A76 EQS/GP 卡 → 间接阻塞 boot | 低-中：A/B 直接路径无 RCU 等待；只能间接参与 | rcu_nocbs=4-7、stall 检测、rcu_expedited |
| H-E | 调度域/NUMA 拓扑配置差异（单簇 8 核 + SMT/MC + NUMA + NR_CPUS=512） | 低：不产生静默永卡；但影响唤醒路径选择与均衡时序 | maxcpus / numa=off / 双簇 cpu-map A/B |
| H-F | CSD 丢失（仅 6.12 原版入队前失败） | **与"两种路径竞态相同"矛盾**，仅在 P1 对照不严格时成立 | P4 探针（sched_ttwu_pending 执行）区分 |

---

## 5. 验证方案（按优先级，具体到探针/命令/判读）

### D0 —— 探针钉死失败点（一次刷机读全，优先做）

| # | 探针落点 | 判读 |
|---|---|---|
| D0-1 | `do_handle_IPI`：按 `ipinr` 分 x 位置画格（IPI_RESCHEDULE=0 / IPI_CALL_FUNC=1 / 其它），并记录**目标核** | A76 各核是否收到 RESCHEDULE/CALL_FUNC；二次唤醒的 IPI 是否到达（配合 D0-3 的时间窗） |
| D0-2 | **A76 各核 idle 循环心跳**：`do_idle` 入口（或 schedule_idle）per-cpu 画格 | 区分"NOHZ 空闲"（持续亮）vs"被楔住"（某核不再亮）——**当前最大盲点的直接解** |
| D0-3 | `try_to_wake_up` 内 `p == kdevtmpfs 相关`：wake_up_process(kdevtmpfs) 前后各画一格 + 目标 `task_cpu(p)` 值 | 确认二次唤醒发生时刻与目标核 |
| D0-4 | kthread() 续跑点：`preempt_enable()` 后立即画格 | 区分"未被调度"（不亮）vs"调度了但死在续跑段"（亮后崩溃） |
| D0-5 | （原版路径）`sched_ttwu_pending` 入口画格；`__flush_smp_call_function_queue` 入口画格 | 区分 CSD 未到 / 未处理 / 处理后未调度（H-F vs 目标核侧） |
| D0-6 | `pick_next_task_fair` 或 `schedule_idle` 入口 per-cpu 计数（画格即可） | 确认目标核是否进入调度 |

> 探针注意：所有新探针**必须与 P1/原版两次实验完全同布局**（B4 盲区）；优先复用现有画格行（如 y=325 行
> 加第二排、y=310 行加第二排），避免改动既有 y 行语义。

### D1 —— 隔离实验（一次一个变量）

1. `maxcpus=4`（仅 A55）：稳定 ⇒ 跨簇/多核交互坐实；再 `maxcpus=5..8` 二分找触发核数。
2. `nohz=off`：排除 NOHZ idle 交互（A76 持续 tick ⇒ y=310 全亮）。
3. `cpuidle.off=1`：排除 cpuidle 深睡（已删 idle-states，仍验 WFI-only 路径）。
4. `kthread_run` 后立即 `set_cpus_allowed_ptr(kdevtmpfs, cpumask_of(0))`（pin 到 A55）：A 消失 ⇒ 目标核侧坐实。
5. `numa=off` / `rcu_nocbs=4-7`：排除 NUMA/RCU 卸载交互。

### D2 —— SLUB/内存方向（H-A，成本最低信息量最大）

1. cmdline `slub_debug=U`（或 FZPU）刷一轮：若报 double-free/object-corruption 或卡点变化 ⇒ 坐实 SLUB。
2. A/B：`t508n_apply_lse_early` 里**不 enable LSE static_key**（强制全 LL/SC）刷一轮：若竞态消失 ⇒ casp/LSE 路径坐实。
3. `CONFIG_DEBUG_LIST=y` + `CONFIG_DEBUG_OBJECTS=y` + `CONFIG_DEBUG_MUTEXES=y` + `CONFIG_DEBUG_SPINLOCK=y` 一轮
   （廉价，抓链表/对象损坏的第一现场）。

### D3 —— 诊断盲区消除（无论哪个方向都应先做）

1. `CONFIG_SOFTLOCKUP_DETECTOR=y`（stock 开、mainline 关）——卡死时若 softlockup 打印（即便无 console，
   WDT 心跳 y=140 行为 + 屏上进度可间接观察）。
2. 观察 y=140 WDT 心跳：继续闪 = 整机活着、CPU0 单点卡（"慢"非"死"）；不闪 = tick 全死。
3. `rcupdate.rcu_cpu_stall_suppress=0` 备好（无 console 时收益有限，但配合 D3-1 可判 RCU stall）。

### D4 —— config/拓扑对齐原厂（一次性，排 H-E）

- `CONFIG_NR_CPUS=32`、`# CONFIG_NUMA`（+ NUMA_BALANCING 随之关）、`# CONFIG_SCHED_SMT/MC`、CPU_FREQ 关。
- DTS cpu-map 改双簇（cluster0=4×A55、cluster1=4×A76）+ `capacity-dmips-mhz`（A55≈400、A76≈1024）：
  **注意**：这会改变 `cpus_share_cache` 判定（跨簇 → true→false），6.12 原版下跨簇唤醒改走 wakelist 分支
  （当前单簇拓扑下走的是"目标 rq 空"分支）——与 P1 实验需一并评估，避免引入新变量。

---

## 6. 修复建议（按优先级）

1. **治标 + 诊断（立即做）**：开 `CONFIG_SOFTLOCKUP_DETECTOR`；boot 期把 kdevtmpfs 及其它早 kthread
   pin 到 A55（`set_cpus_allowed_ptr(..., cpumask_of(0))`），观察竞态是否只在"目标= A76"时出现。
2. **若 D2 坐实 SLUB/LSE（H-A）**：修复到具体指令/路径——强制 LL/SC（`t508n_apply_lse_early` 不 enable
   static_key）为最稳退路（代价 = 性能，boot 期无感）；或按损坏点修 6.12 SLUB 与 LSE 交互。
3. **若 D1-4 坐实目标核侧（H-B）**：二分定位楔住源头（stopper 退出 / migration/kworker 首跑 / 广播维护），
   修到具体锁/路径；临时方案 = `maxcpus=4` 或 A76 不参与早期唤醒（isolcpus 式）。
4. **若 D0-1 坐实 IPI 未达（H-C）**：对比 5.4/6.12 GIC 驱动差异（GICR_WAKER/优先级/EOI 模式、
   `CONFIG_IRQ_FASTEOI_HIERARCHY_HANDLERS=y` 影响）、与 ATF/SML 的 GIC 配置交互。
5. **若 D0-5 坐实 CSD 未处理（H-F，仅原版路径）**：采用 P1（ttwu_queue_cond 恒 false）作为修复——
   但需先按 §6.8 复核 P1 实验严格性。
6. **长期**：DTS 双簇拓扑 + capacity + config 对齐原厂（D4）；`t508n_sm2_done` 改 release/acquire
   （crosscluster 报告遗留项）。

### 6.8 需要先复核的事实（防误判）

1. **P1 实验的严格性**：P1（ttwu_queue_cond 恒 false）是否以**与 6.12 原版实验完全相同的探针布局/镜像**
   构建刷机？"竞态相同"是"症状相同"还是"同布局下频度/卡点分布相同"？探针布局不同 ⇒ 时序不同 ⇒
   "P1 无效"的结论可能被探针扰动掩盖（B4）。**在复核前，H-F（CSD 丢失）不能彻底排除。**
2. **y=325 的时间信息**：A76 各核的 IPI 格子是在二次唤醒前还是后点亮的？（D0-1 按类型+时间窗探针回答。）
3. 当前树 ttwu_queue_cond 为原版（SUMMARY 文件头提醒 2 已核实）；若要在原版上做 D0 探针，无需改 sched 代码。

---

## 7. 一句话结论

> **唤醒传输（CSD vs 直接入队）确实不是分叉点：P1 下 kthread_run 返回（x=80 亮）已经把失败点压缩到
> "任务已入队 → 目标核侧（IPI 未达 / 核被楔住 / pick 未选中 / 切换后崩溃）"。最可疑 = ①SLUB/内存损坏
> （casp 首次大规模使用，造成"入队但永不选中"与 device_add 内损坏，与唤醒路径无关）② 某 A76 核在
> SM#2 之后被楔住（y=310 无法区分 NOHZ 与楔死）③（次）GIC/SGI 对 A76 的 IPI 丢失（y=325 累积性证据
> 不能排除）。验证 = D0 五类探针（按 IPI 类型到达 + idle 循环心跳 + kthread 续跑点 + sched_ttwu_pending +
> pick_next）+ slub_debug=U 与强制 LL/SC A/B + maxcpus/nohz 二分 + 开 SOFTLOCKUP。**
