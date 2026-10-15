# T508N：异簇唤醒/wakelist/NOHZ idle 唤醒 —— 原厂 5.4 vs mainline 6.12 全链对比与竞态根源

> 任务文件：`prompt-stock-wakeup-nohz.md`
> 源码：`linux-6.12.103/`（6.12.103 实测树，含 T508N 探针与 ttwu_queue_cond 修补）、
> `work/linux-5.4.210/`（v5.4.210 上游）、`work/stock_Image_full.bin` + `work/stock_kallsyms.map`（原厂二进制）。
> 前置报告：`work/REPORT-stock-wakeup-chain-analysis.md`（SM#2 唤醒链等价 + §6.2 wakelist 传输差异）、
> `work/REPORT-stock-driver-init.md`（driver_init 链）。

---

## 0. 结论先行（TL;DR）

1. **6.12 树当前的 wakelist 决策**（已被 T508N 修补过）：`ttwu_queue_cond` 只保留了
   **"异簇 → wakelist(IPI_CALL_FUNC)"** 一个分支；同簇一律直接入队 + resched_curr（IPI_RESCHEDULE）。
   **跨簇唤醒仍然走 CSD+IPI_CALL_FUNC** —— 这就是"同簇修补无效"的直接原因：竞态发生在跨簇路径。
2. **竞态 A 的完整链条**（kdevtmpfs 线程）：`kthreadd → kernel_thread → wake_up_new_task`
   两版**完全相同**（直接入队 + IPI_RESCHEDULE，不经过 wakelist）→ 新线程在空闲 A76 核跑
   `kthread()` 入口后睡下 → **`kthread_run` 的二次 `wake_up_process()`（A55→A76，跨簇现有任务唤醒）**：
   - 5.4 原厂：`ttwu_queue_remote` → `rq->wake_list` + **IPI_RESCHEDULE**（SGI#0）
   - 6.12：`__ttwu_queue_wakelist` → per-CPU `call_single_queue`(CSD) + **IPI_CALL_FUNC**（SGI#1）
   - **若该次跨簇 IPI_CALL_FUNC/CSD 处理间歇性失败 → kdevtmpfs 线程永远不被唤醒 →
     `complete(&setup_done)` 永不执行 → `wait_for_completion` 卡死。** 与探针证据
     （kthread_run 返回 = y=270 x=80 亮；线程未启动 = y=290 x=8 不亮）完全吻合。
3. **SGI 传输机制两版逐字节等价**（都是 SGI ID 0-7 → ICC_SGI1R_EL1 写入 → irq domain 分发 →
   ipi_handler(irq - ipi_irq_base) → do_handle_IPI switch），**T770 上不存在"5.4 用 SGI#0、
   6.12 用 SGI#17"的编号错位**。差异只在"哪个 SGI 承载唤醒工作 + 唤醒队列放哪里"。
4. **6.12 的 NOHZ idle balance kick（kick_ilb）也从 IPI_RESCHEDULE 改成了 CSD+IPI_CALL_FUNC**
   （`smp_call_function_single_async(ilb_cpu, &rq->nohz_csd)`）——与唤醒路径同类切换，
   同一条"CSD+IPI_CALL_FUNC"链路在 6.12 里承载了**所有**跨核异步通知（唤醒 + nohz kick），
   而原厂 5.4 全部走 IPI_RESCHEDULE。
5. **根源判断**：代码逻辑层面 6.12 的 CSD 路径与 5.4 的 wake_list 路径**都是无丢失唤醒的
   llist 模式（llist_add 首元素 → 发 IPI）**，没有能解释"间歇性丢失"的纯逻辑竞态；
   但 6.12 把跨簇唤醒从"IPI_RESCHEDULE（T770 上已被多轮实验证明可靠）"迁到
   "IPI_CALL_FUNC/CSD（T770 上无正面证据、且所有卡点都在这条链上）"。**综合证据
   （同簇修补无效 + kthread_run 返回但线程未运行 + 卡点漂移 + 5.4 同硬件可靠），
   跨簇 CSD+IPI_CALL_FUNC 传输/处理在 T770 上间歇性失效是最符合证据的根因。**
6. **修复方案（按优先级）**：
   - **P1（首选，最小改动，最强验证）：`ttwu_queue_cond` 恒 false → 所有唤醒直接入队 +
     resched_curr + IPI_RESCHEDULE**（跨簇也不再走 wakelist）。唤醒链完全回到 5.4 的
     "直接入队 + IPI_RESCHEDULE"，只使用已被证明可靠的 SGI#0。
   - **P2（恢复 5.4 式 wakelist 传输）：给 6.12 的 rq 加回 `wake_list` 字段 +
     IPI_RESCHEDULE 承载 wake_list 处理**（更接近原厂语义，改动较大）。
   - **P3（诊断隔离）：`nohz=off` / `cpuidle.off=1` / `maxcpus=4`（仅 A55）**，
     定位是"NOHZ idle 交互"还是"跨簇本身"。
   - **P4（探针钉死断点）**：目标 A76 核上加 do_handle_IPI(IPI_CALL_FUNC) 到达探针 +
     sched_ttwu_pending 执行探针，区分"SGI 未到" vs "CSD 未处理" vs "处理后未调度"。
7. 竞态 B（memory_dev_init device_register 前卡）与竞态 A **大概率同源**：memory_dev_init
   自身无跨簇唤醒依赖，但"更早某次跨簇唤醒失败 → 某核/某任务进入坏状态（如持有锁的
   任务未醒、软中断堆积、idle balance 停滞）→ CPU0 在其后的分配/锁/同步点卡住"——
   卡点随"哪次唤醒丢失"漂移，与观测一致。

---

## 1. 任务 1：异簇 wakelist 唤醒路径逐环节对比（5.4 vs 6.12）

### 1.1 决策点（waker 侧）

**5.4**（`kernel/sched/core.c:2492 ttwu_queue` + `:2445 ttwu_queue_remote`）：
```c
static void ttwu_queue(struct task_struct *p, int cpu, int wake_flags) {
	...
	if (sched_feat(TTWU_QUEUE) && !cpus_share_cache(smp_processor_id(), cpu)) {
		sched_clock_cpu(cpu);          /* Sync clocks across CPUs */
		ttwu_queue_remote(p, cpu, wake_flags);
		return;
	}
	rq_lock(rq, &rf); update_rq_clock(rq);
	ttwu_do_activate(rq, p, wake_flags, &rf); rq_unlock(rq, &rf);
}
static void ttwu_queue_remote(struct task_struct *p, int cpu, int wake_flags) {
	p->sched_remote_wakeup = !!(wake_flags & WF_MIGRATED);
	if (llist_add(&p->wake_entry, &cpu_rq(cpu)->wake_list)) {   /* rq->wake_list */
		if (!set_nr_if_polling(rq->idle))      /* arm64 恒 false */
			smp_send_reschedule(cpu);          /* ★ IPI_RESCHEDULE (SGI#0) */
		else
			trace_sched_wake_idle_without_ipi(cpu);
	}
}
```
- 决策：**异簇（不共享 LLC）→ wakelist；同簇 → 直接入队**。没有"目标 rq 空"分支。
- 唤醒队列：**`rq->wake_list`**（per-CPU rq 内嵌 llist）。
- IPI：**IPI_RESCHEDULE**。

**6.12**（`kernel/sched/core.c:3844 ttwu_queue_cond`（已修补）/ `:3926 ttwu_queue_wakelist` /
`:3831 __ttwu_queue_wakelist`）：
```c
static inline bool ttwu_queue_cond(struct task_struct *p, int cpu) {
	if (task_on_scx(p)) return false;
	if (p->sched_class == &stop_sched_class) return false;
	if (!cpu_active(cpu)) return false;
	if (!cpumask_test_cpu(cpu, p->cpus_ptr)) return false;
	if (!cpus_share_cache(smp_processor_id(), cpu)) return true;  /* ★ 异簇 → wakelist */
	/* [T508N 修补] 已删除 6.12 新增的 !cpu_rq(cpu)->nr_running → wakelist 分支 */
	return false;   /* 同簇一律直接入队 */
}
static void __ttwu_queue_wakelist(struct task_struct *p, int cpu, int wake_flags) {
	p->sched_remote_wakeup = !!(wake_flags & WF_MIGRATED);
	WRITE_ONCE(rq->ttwu_pending, 1);                    /* 6.12 新增标志 */
	__smp_call_single_queue(cpu, &p->wake_entry.llist); /* ★ per-CPU call_single_queue */
}
/* kernel/smp.c:379 __smp_call_single_queue:
 *   if (llist_add(node, &per_cpu(call_single_queue, cpu)))
 *       send_call_function_single_ipi(cpu);  → call_function_single_prep_ipi()
 *           → arch_send_call_function_single_ipi(cpu)  → ★ IPI_CALL_FUNC (SGI#1)   */
```
- 决策：**异簇 → wakelist（保留）**；同簇直接入队（已修补）。**跨簇唤醒在修补后仍是 wakelist。**
- 唤醒队列：**per-CPU `call_single_queue`**（CSD 队列，与 smp_call_function/irq_work 共享）。
- IPI：**IPI_CALL_FUNC**。

### 1.2 传输通道（llist 入队 + IPI 判定）

| | 5.4 | 6.12 |
|---|---|---|
| 入队目标 | `cpu_rq(cpu)->wake_list`（rq 内嵌） | `per_cpu(call_single_queue, cpu)`（CSD 队列） |
| 首元素判定 | `llist_add(...) == true → 发 IPI` | 同（`llist_add` 返回 true → 发 IPI） |
| IPI 类型 | **IPI_RESCHEDULE** | **IPI_CALL_FUNC** |
| 免 IPI 检查 | `set_nr_if_polling(rq->idle)` → arm64 恒 false → 必发 | `call_function_single_prep_ipi()` → `set_nr_if_polling(idle)` → 恒 false → 必发 |
| 额外状态 | 无 | `WRITE_ONCE(rq->ttwu_pending, 1)`（供 idle_cpu()/选择逻辑防假阴性） |

两版都是标准 llist 模式：**"队列从空变非空 → 恰发一个 IPI"**，逻辑上均无丢失唤醒窗口
（后到者依赖在途 IPI 一并冲刷，先到者负责发送）。

### 1.3 目标侧（接收与处理）

**5.4**（`arch/arm64/kernel/smp.c:889` + `kernel/sched/core.c:2407 scheduler_ipi`）：
```c
case IPI_RESCHEDULE:  scheduler_ipi();   /* 真实函数 */
void scheduler_ipi(void) {
	preempt_fold_need_resched();
	if (llist_empty(&this_rq()->wake_list) && !got_nohz_idle_kick())
		return;                            /* wake_list 空且无 nohz kick → 纯 fold 返回 */
	irq_enter();                           /* rcu_irq_enter + [idle&&!in_intr] tick_irq_enter + HARDIRQ */
	sched_ttwu_pending();                  /* llist_del_all(&rq->wake_list) → ttwu_do_activate 逐个 */
	if (unlikely(got_nohz_idle_kick())) { this_rq()->idle_balance = 1;
		raise_softirq_irqoff(SCHED_SOFTIRQ); }
	irq_exit();
}
```
**6.12**（`arch/arm64/kernel/smp.c:960` + `include/linux/sched.h:1968 scheduler_ipi` inline）：
```c
case IPI_CALL_FUNC:  generic_smp_call_function_interrupt();  /* = generic_smp_call_function_single_interrupt */
/* kernel/smp.c:456 → __flush_smp_call_function_queue(true):
 *   entry = llist_del_all(&call_single_queue); entry = llist_reverse_order(entry);
 *   第一遍: 跑 CSD_TYPE_SYNC（csd_lock_wait 语义）
 *   第二遍: 跑 CSD_TYPE_ASYNC / IRQ_WORK
 *   第三遍: 剩余全是 TTWU → csd_do_func(sched_ttwu_pending, entry, csd)
 * sched_ttwu_pending(arg): ttwu_do_activate 逐个; WRITE_ONCE(rq->ttwu_pending, 0)  */
static __always_inline void scheduler_ipi(void) {  /* IPI_RESCHEDULE 处理器 */
	preempt_fold_need_resched();                    /* 仅 fold，不处理任何 wake_list */
}
```

**逐环节差异表：**

| 环节 | 5.4 | 6.12 | 差异影响 |
|---|---|---|---|
| 唤醒队列位置 | `rq->wake_list`（独立队列） | `call_single_queue`（CSD 队列，与 SYNC/ASYNC/IRQ_WORK 共享） | 6.12 的 TTWU 条目排在 SYNC/ASYNC 之后处理；SYNC 卡住会延迟其后所有唤醒（5.4 无此依赖） |
| 承载 IPI | IPI_RESCHEDULE（SGI#0） | IPI_CALL_FUNC（SGI#1） | **唯一"线上"差异**：唤醒工作走哪个 SGI |
| RESCHEDULE 处理 | fold + 处理 wake_list + nohz kick | 仅 fold | 6.12 的 RESCHEDULE 不处理任何唤醒 |
| CALL_FUNC 处理 | irq_enter + flush call_single_queue（5.4 通用队列只服务 smp_call_function） | irq_enter（由 arch 入口做一次）+ flush call_single_queue（含 TTWU） | 6.12 把唤醒塞进了通用 CSD 队列 |
| irq_enter/exit | SGI 经 `__handle_domain_irq`（内含 irq_enter）+ 5.4 的 CALL_FUNC case 再显式 irq_enter（嵌套，合法） | arm64 entry 代码 `irqentry_enter` 一次 + `__handle_domain_irq`（6.12 的 `generic_handle_domain_irq` 内**无** irq_enter） | 语义等价（都恰好进出一次） |
| idle 循环出口 | `sched_ttwu_pending(); schedule_idle();`（5.4 idle.c） | `flush_smp_call_function_queue(); schedule_idle();`（6.12 idle.c） | 等价（各自冲刷自家队列） |
| on_cpu 窗口唤醒 | 无专门路径：`smp_cond_load_acquire(&p->on_cpu, !VAL)` 自旋等调度完成再入队 | **新增**：`if (smp_load_acquire(&p->on_cpu) && ttwu_queue_wakelist(p, task_cpu(p), ...))` —— 调度中任务直接挂 wakelist 靠 IPI（core.c:4254） | 6.12 在"任务正在被调度出"的窗口也依赖 IPI_CALL_FUNC/CSD；5.4 是自旋（不依赖 IPI） |

### 1.4 SGI 传输机制（两版逐字节等价，重要澄清）

- 发送：5.4 `smp_cross_call(target, ipinr)` → `__smp_cross_call`(=gic_raise_softirq) →
  `ICC_SGI1R_EL1.INTID = ipinr`；6.12 `smp_cross_call` → `__ipi_send_mask(ipi_desc[ipinr])` →
  `gic_ipi_send_mask` → `d->hwirq`(=ipinr) → `ICC_SGI1R_EL1.INTID = ipinr`。**都是 SGI ID 0-7。**
- 接收：IAR 返回 SGI ID（0-7）→ irq domain（SGI 以 hwirq 0-7 映射，`param_count==1 && param[0]<16`）
  → `ipi_handler(irq - ipi_irq_base)` → `do_handle_IPI(ipinr)` switch。
  **原厂二进制证实**：`gic_handle_irq`（0x081a4c）`irqnr > 15 → __handle_domain_irq`（0x81aa8）；
  `ipi_handler`（0x116cac）`w21 = irq - ipi_irq_base`（读全局 `ipi_irq_base` 0x...359ce8）+
  jump table switch(ipinr)；`gic_smp_init`（0x...184e90）`irq_domain_alloc_irqs(domain,-1,8,fwspec(param_count=1))`
  + `set_smp_ipi_range` —— **与 6.12 完全同构**（Unisoc 5.4 已移植新版 IPI 基础设施）。
- 因此任务描述中"5.4 走 IPI_RESCHEDULE、6.12 走 IPI_CALL_FUNC"指的是**内核 IPI 语义编号
  （ipinr=0 vs ipinr=1）**，线上 SGI 都是 SGI ID 0/1（物理 INTID 16/17）。T770 的 GIC 对
  两者投递机制完全相同；**不存在"6.12 用了 5.4 未用过的 SGI 编号"这种硬件级新依赖**。

### 1.5 结论（任务 1）

唤醒链 5.4→6.12 的**全部**行为差异收敛为两点：
1. 唤醒工作改由 **IPI_CALL_FUNC（SGI#1）** 承载（队列从 `rq->wake_list` 移到 `call_single_queue`）；
2. 唤醒条目与 SYNC/ASYNC/IRQ_WORK CSD **共享队列**，且多了一个 `rq->ttwu_pending` 标志。

两者都不是逻辑上的丢失唤醒竞态，但**把 T770 上所有跨核异步唤醒都押注在 SGI#1 的投递/CSD
处理上**——这是原厂（SGI#0 + 独立队列）没有的暴露面。

---

## 2. 任务 2：原厂 scheduler_ipi（0xffffffc0101fc658）完整反汇编

> 旧报告只摘了前半段；本次完整解码（含符号确认：0x28c58c=rcu_irq_enter、0x2c4680=tick_irq_enter、
> 0x203810=irqtime_account_irq、0x1f3d48=sched_ttwu_pending、0x19d724=irq_exit、
> 0x19d060=__raise_softirq_irqoff、0x1169e8e0=__schedule、0x3c7850=__ubsan...）。

```
scheduler_ipi:                       ; Unisoc 定制版（> 上游 5.4 的简化版）
  preempt_fold_need_resched()        ; 1fc684-1fc68c: if (TIF_NEED_RESCHED) [current+28]=0
  x8 = this_rq()->wake_list.first    ; 1fc6a0 (rq+0xc30)
  if (wake_list 非空) {              ; 1fc6ac cbz → 1fc778（空则跳过整个 irq_enter 块）
      rcu_irq_enter()                ; 1fc6b0 bl 0x28c58c
      if (is_idle_task(current) && !in_interrupt()) {   ; 1fc6b4/1fc6c4（[+60]bit1 + preempt&0x1fff00）
          preempt_count += 0x200     ; 1fc6c8  local_bh_disable()
          tick_irq_enter()           ; 1fc6d4 bl 0x2c4680
          preempt_count -= 0x200     ; 1fc6f0  local_bh_enable()
      }
      irqtime_account_irq(current)   ; 1fc6fc bl 0x203810
      preempt_count += 0x10000       ; 1fc700  __irq_enter()（HARDIRQ_OFFSET）
      sched_ttwu_pending()           ; 1fc70c bl 0x1f3d48  ★ rq->wake_list 逐个 ttwu_do_activate
  }
  ; ---- 统一尾部：idle balance 自触发 kick（上游 5.4 用 got_nohz_idle_kick()，原厂改为自检）----
  rq = cpu_rq(smp_processor_id())
  if (raw_spin_trylock 语义检查 rq->lock) { ... }   ; 1fc72c/1fc798（LSE stclr 解锁在 1fc76c/1fc8b8）
  if (rq->curr == rq->idle && rq->nr_running == 0 &&
      llist_empty(&rq->wake_list) && !need_resched()) {
      if (!cpu_isolated(smp_processor_id())) {       ; 1fc7f8: 读 __cpu_isolated_mask(0x...359d60)
          this_rq()->idle_balance = 1                ; rq+2568
          raise_softirq_irqoff(SCHED_SOFTIRQ)        ; 1fc830: bl __raise_softirq_irqoff(w0=7)
          if (!in_interrupt() && this_cpu(ksoftirqd) && ksoftirqd->[+40])
              wake_up_process(ksoftirqd, TASK_NORMAL, 0)  ; 1fc85c: bl try_to_wake_up(0x1f6084)
      }
  }
  irq_exit() 或直接返回
```

**对照：**
- **上游 v5.4.210 `scheduler_ipi`**（core.c:2407）：fold → `if (wake_list 空 && !got_nohz_idle_kick) return`
  → irq_enter + sched_ttwu_pending → `if (got_nohz_idle_kick()) { idle_balance=1; raise_softirq }` → irq_exit。
- **原厂 5.4（Unisoc）`scheduler_ipi`**：wake_list 处理同上游，但 nohz kick **改为自触发**
  （不查 `got_nohz_idle_kick()`，改为"空闲核收到任何 RESCHEDULE IPI 且无活可干 → 自己 kick
  idle balance + 唤醒本核 ksoftirqd"），且多了一个 `cpu_isolated` 豁免。
- **6.12 `scheduler_ipi`**（sched.h:1968 inline）：**只有 fold**；wake_list 处理整体迁到
  IPI_CALL_FUNC；nohz kick 迁到 `kick_ilb → smp_call_function_single_async(nohz_csd)`。

**任务 2 结论**：原厂 5.4 的 **IPI_RESCHEDULE 处理器自身就能完成"唤醒处理 + nohz idle balance
kick + ksoftirqd 唤醒"**，是一条自足、可靠（T770 实测）的链路；6.12 把这三件事全部拆到
IPI_CALL_FUNC/CSD，RESCHEDULE 退化为纯 fold。**若 6.12 的 IPI_CALL_FUNC 链路在 T770 失效，
原厂 5.4 依赖的 RESCHEDULE 兜底在 6.12 中不存在** —— 这正是任务假设的"差异致命点"。

---

## 3. 任务 3：NOHZ idle 唤醒对比

### 3.1 do_idle / CPUidle（两版等价）
- 5.4 `do_idle`（idle.c:225）与 6.12（idle.c:271）循环体**逐行一致**：
  `tick_nohz_idle_enter → while(!need_resched){ rmb; local_irq_disable; cpuidle_idle_call(wfi);
  ... } → preempt_set_need_resched; tick_nohz_idle_exit; __current_clr_polling; smp_mb__after_atomic;
  → [5.4: sched_ttwu_pending() | 6.12: flush_smp_call_function_queue()] → schedule_idle()`。
- arm64 `__cpu_do_idle = dsb(sy); wfi()` 两版相同；都**无 TIF_POLLING_NRFLAG** → 跨核唤醒必发 IPI。
- 6.12 入口多一次 `nohz_run_idle_balance(cpu)`（5.4 没有），只影响 idle balance 时机，不影响唤醒。

### 3.2 idle balance kick（关键差异）
| | 5.4 | 6.12 |
|---|---|---|
| `kick_ilb` | `smp_send_reschedule(ilb_cpu)` → **IPI_RESCHEDULE** | `smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd)` → **CSD + IPI_CALL_FUNC** |
| 目标侧 | scheduler_ipi → got_nohz_idle_kick → `idle_balance=1; raise_softirq(SCHED_SOFTIRQ)` | `__flush_smp_call_function_queue` → `nohz_csd_func` → nohz idle balance |
| 原厂额外兜底 | scheduler_ipi **自触发** kick（无 kick 标志也可，见 §2） | 无 |

**即：6.12 的 NOHZ idle balance 唤醒与任务唤醒共用同一条 CSD+IPI_CALL_FUNC 链路。**
若该链路失效，空闲核（A76）不仅收不到任务唤醒，连 idle balance 通知也收不到——空闲核
彻底"失联"。原厂 5.4 的 RESCHEDULE 链路承担了同样的职责且被证明可靠。

### 3.3 CPUidle 状态（两版相同 DTB）
`work/ot5.dtb`：A55 `cpu-pd-lit`（entry 1ms/exit 0.5ms/min 2.5ms）、A76 `cpu-pd-big`
（entry 4ms/exit 4ms/min 10ms），均 `local-timer-stop` + PSCI `arm,psci-suspend-param=0x10000`。
MENU governor 下 boot 期 A76 空闲多为 wfi（state0），长空闲才进 pd-big；两版面对相同状态集。
→ **不是"6.12 多了深睡眠"造成的差异**；但若 pd-big（经 ATF CPU_SUSPEND）的 SGI 唤醒在
特定时序下偶发失败，会以"间歇 + 漂移"呈现，且影响两类 SGI 等同——不能排除，需 P3 隔离。

### 3.4 任务 3 结论
"空闲核被跨核唤醒"的完整路径在两版机制等价（SGI 唤醒 wfi → 中断处理 → idle 循环检查
need_resched → schedule）；**差异仅在：谁负责把"唤醒事件"送到空闲核** —— 原厂是
IPI_RESCHEDULE（可靠），6.12 是 IPI_CALL_FUNC/CSD（无正面证据）。原厂 5.4 有而 6.12 没有的
"保证空闲核活跃"机制 = scheduler_ipi 的自触发 idle balance kick（§2）。

---

## 4. 任务 4：竞态根源假设验证 + 修复方案

### 4.1 竞态 A（devtmpfs setup_done 卡）链条重建

```
devtmpfs_init (CPU0/A55):
  kthread_run(devtmpfsd, ...)
    ├─ kthread_create_on_node: wake_up_process(kthreadd)  [同簇,直接入队,IPI_RESCHEDULE] ✓
    │     kthreadd(CPU0): kernel_thread → copy_process → wake_up_new_task(p)
    │          select_task_rq → 空闲 A76（异簇）
    │          activate_task(A76 rq) + wakeup_preempt → resched_curr → IPI_RESCHEDULE  ✓ 两版一致
    │     kdevtmpfs 线程在 A76 跑 kthread() 入口:
    │          __set_current_state(TASK_UNINTERRUPTIBLE); complete(create->done);
    │          schedule_preempt_disabled()   ← 睡在 A76 rq 上
    │     → kthread_create_on_node 返回（探针 y=270 x=80 亮 ✓）
    └─ wake_up_process(kdevtmpfs 线程)   ← CPU0(A55) → A76，【跨簇现有任务唤醒】
          6.12: ttwu_queue_cond → !cpus_share_cache → true
                → __ttwu_queue_wakelist → CSD + IPI_CALL_FUNC  ★★★ 唯一失败点
          5.4:  ttwu_queue_remote → rq->wake_list + IPI_RESCHEDULE（可靠）
  wait_for_completion(&setup_done)   [y=270 x=116 不亮 = 卡死]
```

**证据链吻合**：
- "kthread_run 返回" ⇒ create->done 已完成 ⇒ 线程在 wake_up_new_task 路径被正常激活并跑过
  kthread() 入口 ⇒ wake_up_new_task + IPI_RESCHEDULE 路径可靠；
- 线程随后睡在 A76 ⇒ 唯一剩余依赖 = kthread_run 的二次 `wake_up_process`（跨簇）⇒
  6.12 走 CSD+IPI_CALL_FUNC（5.4 走 IPI_RESCHEDULE）；
- "同簇修补（去掉 nr_running→wakelist 分支）无效" ⇒ 失败点不在同簇（同簇现在已直接入队），
  而在跨簇 —— 与上述链条完全一致。

### 4.2 为什么"代码逻辑上没有丢失唤醒竞态"却仍可能是根因
- CSD 路径的 llist 模式与 5.4 相同（首元素发 IPI），`ttwu_pending` 标志的读写窗口
  （waker 置 1 后、目标 `sched_ttwu_pending` 收尾清 0）不会丢条目（IPI 已在途）；
- 因此间歇性失败只能来自**投递/处理层的偶发故障**：SGI#1 偶发丢失、CSD 队列偶发未被冲刷、
  ATF CPU_SUSPEND 返回路径偶发丢失 SGI、或 8 核 I-cache/指令流问题（旧报告 R1，已基本排除）——
  无论哪一种，**都是 6.12 独有暴露面**（5.4 同一场景走 RESCHEDULE，T770 上被反复证明可靠）。
- 补充：`smp_call_function_single_async`（nohz_csd kick）若 CSD_FLAG_LOCK 未清会返回 -EBUSY
  静默放弃 —— 6.12 的 nohz kick 有"忙则丢"语义（5.4 的 smp_send_reschedule 无此语义），
  但这影响的是 idle balance，不是任务唤醒本身。

### 4.3 竞态 B（memory_dev_init device_register 前卡）
- memory_dev_init 链内（subsys_register/device_register/kernfs/klist）**无跨簇唤醒依赖**
  （见 REPORT-stock-driver-init.md），devt=0 不触 devtmpfs；
- 更合理解释：**与竞态 A 同源、异步恶化** —— 更早某次跨簇唤醒（kworker/ksoftirqd/其它
  kthread）失败后，某核/某任务进入坏状态（软中断堆积、持锁任务未醒、空闲核失联导致
  idle balance/RCU/页分配路径异常），CPU0 在 memory_dev_init 的重分配阶段（每块 device_register
  都有大量 kernfs/SLUB 分配）撞上该状态卡住；卡点随"哪次唤醒丢失"漂移 —— 与观测一致。

### 4.4 修复方案（按优先级，具体到代码）

**P1（首选 —— 全唤醒改直接入队，只用 IPI_RESCHEDULE）**
`kernel/sched/core.c ttwu_queue_cond` 改为恒 false：
```c
static inline bool ttwu_queue_cond(struct task_struct *p, int cpu)
{
	/* T508N: 全部唤醒直接入队 + resched_curr(IPI_RESCHEDULE)。
	 * T770 上 6.12 的跨簇 CSD+IPI_CALL_FUNC 间歇性失效（原厂 5.4 的
	 * rq->wake_list+IPI_RESCHEDULE 可靠），彻底绕开 wakelist/IPI_CALL_FUNC。 */
	return false;
}
```
- 效果：所有 try_to_wake_up 走 `ttwu_queue → rq_lock → ttwu_do_activate → wakeup_preempt →
  resched_curr → set_nr_and_not_polling + smp_send_reschedule(IPI_RESCHEDULE)`，与 5.4
  同簇路径/原厂唤醒链一致；代价仅是 waker 短暂取远程 rq->lock（跨簇缓存行访问），boot 期可忽略。
- 同时把 `sched_feat(TTWU_QUEUE)` 相关分支不会再有 wakelist 调用；`ttwu_pending` 恒 0，
  idle_cpu() 行为退化为 5.4 式。

**P2（恢复 5.4 式 wakelist 传输 —— 若 P1 后仍有竞态或想保留性能优化）**
1. `kernel/sched/sched.h struct rq` 加回 `struct llist_head wake_list;`（6.12 已删除该字段）；
2. `__ttwu_queue_wakelist` 改为：
```c
	p->sched_remote_wakeup = !!(wake_flags & WF_MIGRATED);
	if (llist_add(&p->wake_entry.llist, &cpu_rq(cpu)->wake_list))
		smp_send_reschedule(cpu);          /* 原厂 5.4 式: IPI_RESCHEDULE */
```
3. IPI_RESCHEDULE 处理（`arch/arm64/kernel/smp.c case IPI_RESCHEDULE`）在 `scheduler_ipi()`
   后追加 wake_list 冲刷（llist_del_all + sched_ttwu_pending 包装），等价于 5.4 scheduler_ipi。
- 改动面较大（涉及 rq 布局、arm64 smp.c、sched_ttwu_pending 签名），作为 P1 无效后的备选。

**P3（诊断隔离，一次一个变量）**
- `nohz=off`：关 NOHZ，验证是否 idle/tick 交互触发；
- `cpuidle.off=1`：禁用 CPUidle 深睡（只留 wfi），验证 ATF CPU_SUSPEND 交互；
- `maxcpus=4`：仅 A55 单簇，验证"跨簇"本身（若单簇稳定 → 跨簇路径坐实）；
- 或 `set_cpus_allowed_ptr(kdevtmpfs_task, cpumask_of(0))`（kthread_run 后立刻 pin 到 A55），
  若竞态消失 → 跨簇唤醒坐实。

**P4（探针钉死断点 —— 一次性刷机读全，区分投递/处理/调度）**
在目标核（A76）加：
- `do_handle_IPI case IPI_CALL_FUNC` 入口探针（SGI#1 是否到达）；
- `__flush_smp_call_function_queue` / `sched_ttwu_pending` 入口探针（CSD 是否被冲刷）；
- `ttwu_do_activate`（或 kdevtmpfs 线程实际运行点）探针。
判读：SGI 未到 → 投递层（GIC/ATF/硬件）；SGI 到而 CSD 未处理 → 队列处理层；
两者都亮而线程未运行 → 调度层。当前 y=290 x=8（devtmpfsd 启动）与 x=80/116 已可部分区分。

**P5（降低暴露面的 config 微调，成本最低）**
- 若证实是 SGI#1/CSD 链路：把 `CONFIG_IRQ_WORK`/`smp_call_function` 的使用面在 boot 期
  最小化不可行；退而求其次保持 P1 即可。
- `CONFIG_NR_CPUS=32`、`# CONFIG_NUMA`（对齐原厂）可顺带做，但不指望解决竞态。

### 4.5 任务 4 结论
竞态 A 的根因**最可能是 6.12 的跨簇 CSD+IPI_CALL_FUNC 唤醒在 T770 上间歇性失效**
（原厂 5.4 同场景用 IPI_RESCHEDULE，可靠）；竞态 B 与 A 大概率同源（更早丢失的跨簇唤醒
把系统带进坏状态，卡点漂移）。先做 P1（一行改动 + 最强验证），再按 P3/P4 精确定位机制层。

---

## 5. 任务 5：其他差异扫描（补充）

| 项 | 5.4 | 6.12 | 结论 |
|---|---|---|---|
| `wake_up_new_task`（kthread 首次激活） | 直接入队 + wakeup_preempt + resched_curr（core.c:3042） | 结构逐行一致（core.c:4810，仅 ENQUEUE_INITIAL 标志） | **无差异**（都不走 wakelist） |
| `kthread()` 入口（kthread.c） | `__set_current_state(UNINTERRUPTIBLE); complete(done); schedule_preempt_disabled()` | 同（kthread.c:376-384，多 sched_setscheduler + housekeeping affinity） | 无实质差异 |
| `kthread_create_on_node` / `kthread_run` | 同 | 同 | 无差异（kthreadd 唤醒同簇时直接入队） |
| complete/swait | —— | —— | 已排除（旧报告 REPORT-stock-vs-mainline-completion.md） |
| workqueue（devtmpfs_work_loop） | 原厂关 DEVTMPFS（无 kdevtmpfsd） | kdevtmpfsd 常驻，setup 后进 work loop | 卡点在 setup 前，与 work loop 无关 |
| config | PREEMPT=y, NO_HZ_IDLE=y, HZ=250, NR_CPUS=32, 无 NUMA/SMT | PREEMPT=y, NO_HZ_IDLE=y, HZ=250, NR_CPUS=512, NUMA=y, SCHED_SMT=y, 关 softlockup | NUMA/SMT/NR_CPUS 非根因；**softlockup 关闭 = 卡死无诊断**（建议开） |
| GIC/SGI 基础设施 | Unisoc 版已用 irq domain + ipi_irq_base（二进制证实） | 同构 | **无差异** |
| `kick_ilb`（nohz idle balance） | smp_send_reschedule（IPI_RESCHEDULE） | smp_call_function_single_async（CSD+IPI_CALL_FUNC） | **同类切换**，与唤醒同链 |

---

## 6. 附：关键源码/符号速查

- 6.12：`kernel/sched/core.c:3884 ttwu_queue_cond`（已修补）、`:3926 ttwu_queue_wakelist`、
  `:3831 __ttwu_queue_wakelist`、`:3772 sched_ttwu_pending`、`:4254` on_cpu wakelist 分支；
  `kernel/smp.c:379 __smp_call_single_queue`、`:475 __flush_smp_call_function_queue`（TTWU 第三遍）；
  `include/linux/sched.h:1968 scheduler_ipi`（inline fold）；`arch/arm64/kernel/smp.c:952 do_handle_IPI`、
  `:1017 smp_cross_call`；`kernel/sched/idle.c:271 do_idle`（:365 flush_smp_call_function_queue）；
  `kernel/sched/fair.c:12374 kick_ilb`（:12409 smp_call_function_single_async(nohz_csd)）；
  `drivers/irqchip/irq-gic-v3.c:867 __gic_handle_irq`、`:1428 gic_ipi_send_mask`、`:1453 gic_smp_init`、
  `:1638 gic_irq_domain_translate`（SGI: param[0]<16 → hwirq=param[0]）。
- 5.4 上游：`kernel/sched/core.c:2445 ttwu_queue_remote`、`:2407 scheduler_ipi`、`:2492 ttwu_queue`；
  `kernel/sched/sched.h:1000 rq->wake_list`；`arch/arm64/kernel/smp.c:878 handle_IPI`；
  `kernel/sched/fair.c:9425 kick_ilb`（smp_send_reschedule）。
- 原厂二进制：`scheduler_ipi` 0xffffffc0101fc658；`ipi_handler` 0xffffffc010116cac；
  `gic_handle_irq` 0xffffffc010081a4c；`gic_smp_init` 0xffffffc012184e90；
  `set_smp_ipi_range` 0xffffffc01214eaf0；`try_to_wake_up` 0xffffffc0101f6084；
  `sched_ttwu_pending` 0xffffffc0101f3d48；`__cpu_isolated_mask` 0xffffffc012359d60；
  `ipi_irq_base` 0xffffffc012359ce8；`ksoftirqd`(per-cpu) 0xffffffc012322080。

## 7. 建议下一步（一次一个变量）
1. **P1**（ttwu_queue_cond 恒 false）编译刷机 → 若 devtmpfs/memory 卡点消失 → 坐实跨簇
   CSD+IPI_CALL_FUNC 根因，完成修复；
2. 若 P1 无效 → **P3**（nohz=off / cpuidle.off=1 / maxcpus=4）定位机制层；
3. 同时刷 **P4** 探针（A76 的 CALL_FUNC 到达 + CSD 处理）精确定位投递/处理/调度；
4. 打开 `CONFIG_SOFTLOCKUP_DETECTOR` 消除"慢 vs 死"诊断盲区。
