# 任务：T508N 当前竞态根源分析（基于全部已有成果）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**先读** /home/test/t508n/work/SUMMARY-ALL-ANALYSIS.md（全部已确认事实/反汇编成果/已修复/已排除/当前竞态/探针布局——只读这一份就够，需要细节再查对应 REPORT）。

## 当前未解决问题（竞态，间歇漂移）

**竞态 A**：`devtmpfs_init` 的 `wait_for_completion(&setup_done)` 间歇卡——kdevtmpfs 线程被
kthread_run 创建（kthread_run 返回、y=270 x=80 亮）但 setup_done 未 complete（y=270 x=116
不亮）→ kdevtmpfs 线程未执行到 complete（或未运行）。
**竞态 B**：`memory_dev_init` → `__add_memory_block` → `device_register` 前间歇卡。
每次重启卡点漂移（A 或 B 随机出现，也有推进更远的）。

## 已试无效的修复（关键约束！）
1. **唤醒路径 A/B 对比**：6.12 原版（跨簇 CSD+IPI_CALL_FUNC、同簇空rq wakelist）vs
   P1 改版（所有唤醒直接入队+resched_curr+IPI_RESCHEDULE）——**两种唤醒路径竞态相同**！
2. **cpuidle 去 idle-states**（删 DTS 8 个 cpu-idle-states，A76 不再进 PD）——竞态 A 仍现。
3. 已修复（有效）：LSE static_key（stop_machine 过）、NOMAP 检查（set_zone_contiguous 过）。
4. 已排除 16 项假设（见 SUMMARY 第 4 节）。

## 关键观测
- y=325（per-CPU IPI 到达）**8 格**：A76 核能收到 IPI（GIC 投递正常）
- y=310（每核 tick）A55 前 4 格：A76 NOHZ 空闲停 tick（正常）
- SM#2（stop_machine）8 核全部正常完成（y=180 8 块）→ 8 核在线、调度、唤醒在 SM#2 时正常
- 竞态出现在 SM#2 之后（kernel_init_freeable 的后段）

## 任务：分析当前竞态的可能根源

**方向指引（用户怀疑"启动中有其他问题"，两种唤醒路径竞态相同）**：

### 1. 唤醒路径已排除 → 找共享失败点
- 两种唤醒路径都失败 → 失败点不在"wakelist vs 直接入队"，而在**共享环节**：
  - kdevtmpfs 线程"创建了但未运行到 complete"——即使 IPI 到达（y=325 亮），线程可能：
    a. 运行了但卡在 devtmpfs_setup（ksys_unshare(CLONE_NEWNS)/init_mount）
    b. 被调度但执行异常（kthread() 入口前）
    c. 唤醒后调度失败（调度器选核/运行队列问题）
  - **分析 ksys_unshare(CLONE_NEWNS) 在 8 核在线语境的可能卡点**（namespace 复制、锁、
    mnt 表复制、内存分配）
  - **分析 init_mount 在 devtmpfsd 线程里的卡点**（vfsmount 分配、锁、shmem）

### 2. 竞态 A/B 的共享特征
- 都发生在"创建新对象/新线程"（kthread 创建、device_register 的 kobject/kernfs 创建）
- 共享路径候选：
  - **SLUB 分配器**（task_struct/kthread 栈/kernfs_node/device 结构分配）——8 核并发 slab 分配
    的竞态？（注意：原厂同硬件用 SLUB 可跑）
  - **RCU**（新任务注册、8 核 quiescent state、NOHZ 核的 qs 上报）——**RCU 在 NOHZ idle
    核上的同步**（A76 核停 tick → RCU 依赖 IPI 或 qs 上报？）
  - **调度器**（新任务入队、负载均衡、sched_setscheduler）
  - **pid/栈分配**（fork/exec 路径）

### 3. 启动流程的特殊点
- kernel_init_freeable 在 SM#2 之后：sched_init_smp（调度域）→ workqueue_init_topology →
  async_init → padata_init → page_alloc_init_late → do_basic_setup
- **8 核的 idle 状态**：A76 核在 NOHZ idle（停 tick）→ **这些核的 RCU/sched 状态**
- **竞态是否与 A76 核的"长时间无 tick 空闲"相关**（比如 RCU 依赖它们上报 qs，但它们停
  tick → RCU 等待 → 卡？——但 RCU 有 IPI 机制（rcu_sched 的 IPI 化 qs））

### 4. 分析 kdevtmpfs 场景的完整生命周期
- kthread_run → kthread_create → kthreadd 创建线程 → wake_up_new_task → 线程运行
  kthread() → devtmpfsd → devtmpfs_setup → complete
- **每个环节在 8 核 + NOHZ 下的失败可能**（逐个分析）

### 5. 给出可操作的验证/修复方案
- 探针建议（加在哪、怎么判读）
- 修复建议（按优先级，具体到代码/config）
- **如果方向是"启动中其他问题"**：给出最可疑的 2-3 个方向（如 RCU、SLUB、namespace、
  调度域），每个方向的验证方法

## 输出
单文件：/home/test/t508n/work/REPORT-CURRENT-RACE-ANALYSIS.md
（不要写其他文件，避免与汇总文件冲突）

## 注意
- 只读 SUMMARY-ALL-ANALYSIS.md（+ 需要时查对应 REPORT-*.md）
- 不要重复已排除的假设（16 项）
- 结论落到"具体机制 + 验证方法"
