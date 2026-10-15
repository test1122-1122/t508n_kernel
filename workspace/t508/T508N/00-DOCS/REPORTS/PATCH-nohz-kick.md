# PATCH-nohz-kick：恢复 NOHZ + kick_ilb 改 smp_send_reschedule（IPI_RESCHEDULE）

> 验证子会话输出（prompt-verify-nohz-kick.md）。树：`linux-6.12.103`（HEAD `cb8e725f3` + T508N 工作树，含 P1 `ttwu_queue_cond` 恒 false 修补）。
> 参考：`work/UNISOC-MODS-AND-CONFIG.md` §4-1、`work/DISASM-B-sched-调度器.md`（B1/D4）、`work/REPORT-stock-wakeup-nohz.md`（§2/§3/§4.2）。
> 只改 3 个内核源文件 + 1 个 .config 项，**不触碰** P1 ttwu_queue_cond 修补。

---

## 0. 结论（TL;DR）

1. **改动**：`kernel/sched/fair.c` 的 `kick_ilb()` 把 nohz idle balance kick 的传输通道从
   `smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd)`（CSD + IPI_CALL_FUNC/SGI#1）
   改回原厂 5.4 式 `smp_send_reschedule(ilb_cpu)`（IPI_RESCHEDULE/SGI#0）。
2. **关键设计（谁触发 rebalance）**：6.12 的 `scheduler_ipi`（include/linux/sched.h:1968）是纯 fold inline，
   必须扩展为真实函数（放回 `kernel/sched/core.c`，5.4 布局）：fold 之后检测本核
   `nohz_flags & NOHZ_KICK_MASK`，命中即调用**现成的 `nohz_csd_func()`**（core.c:1236，原 CSD 回调）——
   它负责：释放 kick 所有权（`atomic_fetch_andnot(NOHZ_KICK_MASK|NOHZ_NEWILB_KICK)`）、
   `rq->idle_balance = idle_cpu(cpu)`、`rq->nohz_idle_balance = flags`、
   `__raise_softirq_irqoff(SCHED_SOFTIRQ)`；softirq 在 **IRQ 返回路径**（irq_exit→do_softirq）被
   `sched_balance_softirq()` → `nohz_idle_balance()` → `_nohz_idle_balance()` 消费，对
   `nohz.idle_cpus_mask` 全部空闲核跑 `sched_balance_domains(CPU_IDLE)`。
   **接收侧逻辑与旧 CSD 路径逐字节复用同一函数，只换传输通道。**
3. **等价性**：kick 所有权协议（atomic_fetch_or 旧值检查 + 接收方唯一清位）、
   idle_cpu 门控、"忙则丢"语义、STATS/NEXT 标志消费链全部与 CSD 路径一致；与 5.4 的
   `got_nohz_idle_kick()` 语义等价（6.12 的 has_blocked/next_blocked 更新在 `_nohz_idle_balance` 内完成）。
4. **不需补**：wake_list 处理（P1 后唤醒全走直接入队，6.12 无 `rq->wake_list` 字段）、
   irq_enter/irq_exit（本路径不做任务唤醒，无 irqtime 记账需求）、ksoftirqd 唤醒
   （6.12 的 irq_exit 会在最外层 IRQ 返回时**内联跑完 pending softirq**，与原厂 C3 定制 irq_exit 不同）。
   原厂式"自触发 idle balance + 唤醒 ksoftirqd"作为**可选加固 OPT** 给出（默认不启用）。
5. **风险**：低。补丁在 `CONFIG_NO_HZ_COMMON=n` 时编译为空（与现状完全一致）；`=y` 时功能与
   原厂 stock 对齐。WARN_ON 理论不可触发（有论证）。回退 = git revert 3 文件 + config 复原。

---

## 1. 背景与目标

- 当前树：`CONFIG_NO_HZ_IDLE=n`（诊断关闭），`ttwu_queue_cond` 恒 false（P1）后唤醒已全部
  走直接入队 + resched_curr + **IPI_RESCHEDULE**；但 `kick_ilb`（nohz idle balance kick）仍是
  **CSD + IPI_CALL_FUNC** —— 这是 P1 后调度侧残留的唯一 CSD+IPI_CALL_FUNC 暴露面
  （DISASM-B D4 / REPORT-stock-wakeup-nohz §3.2）。
- 原厂 5.4.210 stock：`CONFIG_NO_HZ_IDLE=y`，kick_ilb 走 `smp_send_reschedule`（SGI#0），
  `scheduler_ipi` 是真实函数（fold + wake_list + nohz kick + ksoftirqd 唤醒，DISASM-B B1）。
- T770 证据链（REPORT-stock-wakeup-nohz §4.2）：跨簇 CSD+IPI_CALL_FUNC 传输/处理在 T770
  间歇性失效是竞态 A/B 最符合证据的根因；SGI#0 通道被多轮实验证明可靠。
- **目标**：恢复 `CONFIG_NO_HZ_IDLE=y`（原厂对齐），同时把 nohz kick 迁回 RESCHEDULE，
  使**调度侧所有跨核异步通知都只走 SGI#0**，彻底消除 CSD 暴露面。

---

## 2. 现状链路分析（6.12 vs 5.4 vs 原厂）

### 2.1 6.12 现状（未修补）

**kick_ilb**（kernel/sched/fair.c:12374，CONFIG_NO_HZ_COMMON 内）：
```c
static void kick_ilb(unsigned int flags)
{
	int ilb_cpu;

	if (flags & NOHZ_BALANCE_KICK)
		nohz.next_balance = jiffies+1;

	ilb_cpu = find_new_ilb();
	if (ilb_cpu < 0)
		return;

	if ((atomic_read(nohz_flags(ilb_cpu)) & flags) == flags)
		return;

	/* Access to rq::nohz_csd is serialized by NOHZ_KICK_MASK; he who sets
	 * the first flag owns it; cleared by nohz_csd_func(). */
	flags = atomic_fetch_or(flags, nohz_flags(ilb_cpu));
	if (flags & NOHZ_KICK_MASK)
		return;

	smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd);  /* ★ CSD + SGI#1 */
}
```

**接收侧 nohz_csd_func**（kernel/sched/core.c:1236，CSD 回调，`INIT_CSD(&rq->nohz_csd, nohz_csd_func, rq)` @ core.c:8495）：
```c
static void nohz_csd_func(void *info)
{
	struct rq *rq = info;
	int cpu = cpu_of(rq);
	unsigned int flags;

	flags = atomic_fetch_andnot(NOHZ_KICK_MASK | NOHZ_NEWILB_KICK, nohz_flags(cpu));
	WARN_ON(!(flags & NOHZ_KICK_MASK));

	rq->idle_balance = idle_cpu(cpu);
	if (rq->idle_balance) {
		rq->nohz_idle_balance = flags;
		__raise_softirq_irqoff(SCHED_SOFTIRQ);
	}
}
```

**调度 softirq 消费链**（fair.c:13010 sched_balance_softirq ← SCHED_SOFTIRQ handler @ fair.c:13828）：
```c
static __latent_entropy void sched_balance_softirq(void)
{
	struct rq *this_rq = this_rq();
	enum cpu_idle_type idle = this_rq->idle_balance;   /* nohz_csd_func 置的 0/1 */

	if (nohz_idle_balance(this_rq, idle))              /* 读 this_rq->nohz_idle_balance */
		return;
	sched_balance_update_blocked_averages(this_rq->cpu);
	sched_balance_domains(this_rq, idle);
}
```
`nohz_idle_balance`（fair.c:12765）读 `rq->nohz_idle_balance` 为 flags，`idle != CPU_IDLE(=0)` 则跳过，
否则 `_nohz_idle_balance(rq, flags)`（fair.c:12662）遍历 `nohz.idle_cpus_mask` 做
`update_nohz_stats`（STATS）/ `sched_balance_domains(rq, CPU_IDLE)`（BALANCE）并更新 next_balance/next_blocked。

**scheduler_ipi（6.12）= 纯 fold inline**（include/linux/sched.h:1968，全 arch 共享）：
```c
static __always_inline void scheduler_ipi(void)
{
	preempt_fold_need_resched();
}
```

### 2.2 5.4.210 参考（kick 走 RESCHEDULE 的完整闭环）

**kick_ilb**（work/linux-5.4.210/kernel/sched/fair.c:9425）：结构相同，末尾
`flags = atomic_fetch_or(...); if (flags & NOHZ_KICK_MASK) return; smp_send_reschedule(ilb_cpu);`

**got_nohz_idle_kick**（5.4 core.c:632）：
```c
static inline bool got_nohz_idle_kick(void)
{
	int cpu = smp_processor_id();
	if (!(atomic_read(nohz_flags(cpu)) & NOHZ_KICK_MASK))
		return false;
	if (idle_cpu(cpu) && !need_resched())
		return true;
	atomic_andnot(NOHZ_KICK_MASK, nohz_flags(cpu));   /* 忙/有活 → 丢弃 kick */
	return false;
}
```

**scheduler_ipi**（5.4 core.c:2407，真实函数）：
```c
void scheduler_ipi(void)
{
	preempt_fold_need_resched();
	if (llist_empty(&this_rq()->wake_list) && !got_nohz_idle_kick())
		return;
	irq_enter();
	sched_ttwu_pending();
	if (unlikely(got_nohz_idle_kick())) {
		this_rq()->idle_balance = 1;
		raise_softirq_irqoff(SCHED_SOFTIRQ);
	}
	irq_exit();
}
```

### 2.3 原厂 Unisoc scheduler_ipi（DISASM-B B1，比 5.4 上游更强）

fold + **就地处理 `rq->wake_list`**（sched_ttwu_pending）+ **自触发 idle balance kick**
（空闲核收到任意 RESCHEDULE 且 `nr_running==0 && wake_list 空 && !need_resched` → `idle_balance=1` +
`raise_softirq(SCHED_SOFTIRQ)` + `wake_up_process(ksoftirqd)`，带 cpu_isolated 豁免）。
即原厂 SGI#0 一个中断承载"唤醒 + 平衡 kick + ksoftirqd 唤醒"三种职责。

### 2.4 关键设计问题与答案

> **RESCHEDULE 到达 ilb_cpu 后，谁触发 rebalance？**

6.12 中 RESCHEDULE 处理器（scheduler_ipi）只 fold，而 CSD 路径的 rebalance 触发逻辑全在
`nohz_csd_func` 里。答案：**把 scheduler_ipi 从 inline 恢复为 core.c 真实函数，在 fold 后增加
"检测本核 nohz_flags 的 NOHZ_KICK_MASK → 调 nohz_csd_func()"**。即把原 CSD 回调的执行点搬到
RESCHEDULE 处理器内，其余（flags 释放、idle_cpu 门控、raise SCHED_SOFTIRQ、softirq 消费链）
原封不动。这正对应原厂 5.4 scheduler_ipi 的"IPI 到达即干活"结构，只是 6.12 用现成的
`nohz_csd_func` 实现等价逻辑（无需重新发明 got_nohz_idle_kick）。

不需要的补充（论证）：
- **wake_list 处理**：P1（ttwu_queue_cond 恒 false）后唤醒永不入 wakelist；6.12 `struct rq` 已无
  `wake_list` 字段。RESCHEDULE 只需 fold（need_resched 已由 resched_curr 置位）。
- **irq_enter/irq_exit**：5.4 需要是因为 sched_ttwu_pending 做任务唤醒（irqtime 记账）。本路径只
  raise softirq，arm64 entry 已做 irqentry_enter/exit，softirq 在最外层 IRQ 返回时内联运行。
- **ksoftirqd 唤醒**：6.12 的 `irq_exit()`（kernel/softirq.c）在 `!in_interrupt() && local_softirq_pending()`
  时直接 `invoke_softirq()` 跑完 pending；原厂因为 C3 定制 irq_exit（pending&0x41 才内联、
  否则唤醒 ksoftirqd）才需要显式唤醒。6.12 语义下不需要（OPT 保留为兜底）。

---

## 3. 设计方案

### 3.1 设计 A（推荐，5.4/原厂对齐）：scheduler_ipi 恢复为 core.c 真实函数

| 文件 | 改动 |
|---|---|
| `kernel/sched/fair.c` `kick_ilb` | `smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd)` → `smp_send_reschedule(ilb_cpu)`（1 行 + 注释） |
| `include/linux/sched.h` `scheduler_ipi` | inline（12 行）→ `extern void scheduler_ipi(void);`（!CONFIG_SMP 空 inline 保留） |
| `kernel/sched/core.c` | 新增 `void scheduler_ipi(void)`：fold + `#ifdef CONFIG_NO_HZ_COMMON` 下 atomic_read 守卫 + `nohz_csd_func(this_rq())`；放在 sched_ttwu_pending() 之后（CONFIG_SMP 区内，nohz_csd_func 定义于其上 1236 行，可见） |
| `.config` | `CONFIG_NO_HZ_IDLE=y`（select NO_HZ_COMMON/NO_HZ） |

**运行闭环（补丁后）**：
```
忙核 scheduler_tick → sched_balance_trigger → nohz_balancer_kick → kick_ilb(flags)
  → atomic_fetch_or(flags, nohz_flags(ilb_cpu)) [旧值含 KICK_MASK → 早退，合并]
  → smp_send_reschedule(ilb_cpu)                        [SGI#0，一次]
空闲 ilb_cpu:
  do_handle_IPI(IPI_RESCHEDULE) → scheduler_ipi()
    → preempt_fold_need_resched()
    → atomic_read(nohz_flags) & NOHZ_KICK_MASK ?       [非 kick 的 RESCHEDULE 直接返回]
    → nohz_csd_func(this_rq())
        → atomic_fetch_andnot(KICK_MASK|NEWILB)        [释放所有权 → 下次 kick_ilb 可再 kick]
        → rq->idle_balance = idle_cpu(cpu)             [仍空闲才继续]
        → rq->nohz_idle_balance = flags
        → __raise_softirq_irqoff(SCHED_SOFTIRQ)
  IRQ 返回 → irq_exit → do_softirq → sched_balance_softirq
    → nohz_idle_balance(rq, CPU_IDLE) → _nohz_idle_balance(flags)
        → 遍历 nohz.idle_cpus_mask: update_nohz_stats / sched_balance_domains(CPU_IDLE)
```

### 3.2 设计 B（备选，保留 inline 性质）

sched.h 保留 inline，但内部调外部辅助函数：
```c
#ifdef CONFIG_SMP
extern void sched_ipi_nohz_kick(void);
static __always_inline void scheduler_ipi(void)
{
	preempt_fold_need_resched();
	sched_ipi_nohz_kick();
}
#else
static inline void scheduler_ipi(void) { }
#endif
```
core.c 定义 `void sched_ipi_nohz_kick(void)`（同 3.1 的 nohz 块）。效果等价，但多一层调用且
与 5.4 布局不一致——**不推荐**，仅当有强约束必须保持 scheduler_ipi inline 时使用。

### 3.3 为什么不改 arch/arm64/kernel/smp.c

`scheduler_ipi` 是 include/linux/sched.h 里的**全 arch 共享** inline，nohz kick 消费是通用语义
（任何用 smp_send_reschedule 承载 kick 的 arch 都需在 RESCHEDULE 处理器消费）；改 core.c 一处
全 arch 生效，且与 5.4 上游布局一致。arm64 smp.c:961 的调用点无需任何改动。

---

## 4. 具体补丁（git diff 形式，可直接 `git apply`）

### 4.1 kernel/sched/fair.c —— kick_ilb 传输通道（1 行实质改动）

```diff
--- a/kernel/sched/fair.c
+++ b/kernel/sched/fair.c
@@ -12394,8 +12394,9 @@
 		return;
 
 	/*
-	 * Access to rq::nohz_csd is serialized by NOHZ_KICK_MASK; he who sets
-	 * the first flag owns it; cleared by nohz_csd_func().
+	 * Access to nohz_flags() is serialized by NOHZ_KICK_MASK; he who sets
+	 * the first flag owns the kick; it is cleared by the receiver
+	 * (nohz_csd_func(), reached from scheduler_ipi() on IPI_RESCHEDULE).
 	 */
 	flags = atomic_fetch_or(flags, nohz_flags(ilb_cpu));
 	if (flags & NOHZ_KICK_MASK)
@@ -12405,8 +12406,14 @@
 	 * This way we generate an IPI on the target CPU which
 	 * is idle, and the softirq performing NOHZ idle load balancing
 	 * will be run before returning from the IPI.
+	 *
+	 * T508N: 原厂 5.4 式 —— 用 smp_send_reschedule() (IPI_RESCHEDULE/SGI#0)
+	 * 替代 smp_call_function_single_async() (CSD + IPI_CALL_FUNC/SGI#1)。
+	 * T770 上 6.12 的 CSD+IPI_CALL_FUNC 跨核通知间歇性失效
+	 * (REPORT-stock-wakeup-nohz.md §4.2), 5.4 的 RESCHEDULE 通道实测可靠。
+	 * 目标侧由扩展后的 scheduler_ipi() -> nohz_csd_func() 消费, 语义等价。
 	 */
-	smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd);
+	smp_send_reschedule(ilb_cpu);
 }
 
 /*
```

### 4.2 include/linux/sched.h —— scheduler_ipi inline → extern 声明

```diff
--- a/include/linux/sched.h
+++ b/include/linux/sched.h
@@ -1965,15 +1965,7 @@
 })
 
 #ifdef CONFIG_SMP
-static __always_inline void scheduler_ipi(void)
-{
-	/*
-	 * Fold TIF_NEED_RESCHED into the preempt_count; anybody setting
-	 * TIF_NEED_RESCHED remotely (for the first time) will also send
-	 * this IPI.
-	 */
-	preempt_fold_need_resched();
-}
+extern void scheduler_ipi(void);
 #else
 static inline void scheduler_ipi(void) { }
 #endif
```

### 4.3 kernel/sched/core.c —— 新增 scheduler_ipi 真实函数（插在 sched_ttwu_pending 之后，CONFIG_SMP 区内）

```diff
--- a/kernel/sched/core.c
+++ b/kernel/sched/core.c
@@ -3807,6 +3807,43 @@
 	WRITE_ONCE(rq->ttwu_pending, 0);
 	rq_unlock_irqrestore(rq, &rf);
 }
 
+/*
+ * T508N: IPI_RESCHEDULE (SGI#0) 处理器 —— 原厂 5.4 式扩展。
+ *
+ * 除 fold TIF_NEED_RESCHED 外, 还消费 kick_ilb() 经 smp_send_reschedule()
+ * 送达的 NOHZ idle balance kick。处理逻辑直接复用原 CSD 回调
+ * nohz_csd_func(): 释放 kick 所有权 + 置 rq->nohz_idle_balance +
+ * raise SCHED_SOFTIRQ; softirq 在 IRQ 返回路径由 sched_balance_softirq()
+ * -> nohz_idle_balance() -> _nohz_idle_balance() 消费 —— 与旧
+ * CSD+IPI_CALL_FUNC 路径唯一区别是传输通道 (SGI#0 vs SGI#1)。
+ */
+void scheduler_ipi(void)
+{
+	/*
+	 * Fold TIF_NEED_RESCHED into the preempt_count; anybody setting
+	 * TIF_NEED_RESCHED remotely (for the first time) will also send
+	 * this IPI.
+	 */
+	preempt_fold_need_resched();
+
+#ifdef CONFIG_NO_HZ_COMMON
+	/*
+	 * Someone kicked us for NOHZ idle load balancing: kick_ilb() set a
+	 * NOHZ_KICK_MASK bit on our nohz_flags and sent IPI_RESCHEDULE.
+	 *
+	 * Guard safety: NOHZ_KICK_MASK bits are only set by kick_ilb()
+	 * (remote atomic_fetch_or) and only cleared on this CPU (here, in
+	 * nohz_csd_func()), so a kick observed by the atomic_read() is still
+	 * present in nohz_csd_func()'s atomic_fetch_andnot() -- the
+	 * WARN_ON(!(flags & NOHZ_KICK_MASK)) there cannot fire.  RESCHEDULE
+	 * IPIs sent for other reasons (resched_curr) see no kick bits and
+	 * return after the fold above.
+	 */
+	if (unlikely(atomic_read(nohz_flags(smp_processor_id())) & NOHZ_KICK_MASK))
+		nohz_csd_func(this_rq());
+#endif
+}
+
 /*
  * Prepare the scene for sending an IPI for a remote smp_call
  *
```

### 4.4 .config —— 恢复 NO_HZ_IDLE

```
# 当前（linux-6.12.103/.config 第 87-89 行）:
# CONFIG_NO_HZ_IDLE is not set
# CONFIG_NO_HZ_FULL is not set
# CONFIG_NO_HZ is not set

# 改为:
CONFIG_NO_HZ_IDLE=y
# CONFIG_NO_HZ_FULL is not set
CONFIG_NO_HZ=y

# 执行:  ./scripts/config --enable NO_HZ_IDLE && make olddefconfig
# (NO_HZ_IDLE 会 select CONFIG_NO_HZ_COMMON / CONFIG_NO_HZ, olddefconfig 自动补齐;
#  NR_CPUS=32 / NUMA=n / SMT=n / DEVTMPFS=n 等既有对齐项不动)
```

### 4.5 OPT（可选加固，默认不启用）：原厂式自触发 + ksoftirqd 唤醒

仅当验证发现"空闲核 SCHED_SOFTIRQ 未及时处理"（如 6.12 irq_exit 路径在 T770 上有时序问题，
参考 DISASM-C C3 原厂定制 irq_exit）时，在 4.3 的 `scheduler_ipi()` 尾部追加（需 `#include <linux/interrupt.h>`，core.c 已有）：

```c
#ifdef CONFIG_NO_HZ_COMMON
	/*
	 * 原厂自触发 (DISASM-B B1): 空闲核收到任何 RESCHEDULE 且无活可干,
	 * 即使没有 kick 标志也顺手推进 idle balance + 唤醒本核 ksoftirqd。
	 * 6.12 的 irq_exit() 在 IRQ 返回时内联跑 softirq, 正常不需要; 仅作
	 * T770 "IPI 即干活"式兜底。启用会略微增加空闲核 softirq 频率。
	 */
	if (idle_cpu(smp_processor_id()) && !need_resched() &&
	    !this_rq()->nohz_idle_balance) {
		this_rq()->idle_balance = 1;
		__raise_softirq_irqoff(SCHED_SOFTIRQ);
		wake_up_process(this_cpu_ksoftirqd());
	}
#endif
```
（`this_cpu_ksoftirqd()` 为 include/linux/interrupt.h:621 的 static inline；`wake_up_process` 不睡眠，
硬中断上下文合法。）注意此段会使"任意 RESCHEDULE 到达空闲核"都触发一次 softirq——与原厂行为
一致但比 4.1-4.3 主补丁更激进，务必作为独立单变量验证。

---

## 5. 语义等价性与关键不变量论证

| 不变量 | 补丁后 | 与旧 CSD 路径 / 5.4 的关系 |
|---|---|---|
| kick 所有权 | `atomic_fetch_or` 旧值含 KICK_MASK → 早退（合并）；接收方唯一清位（fetch_andnot） | 与 CSD 路径完全一致（代码未动）；与 5.4 kick_ilb 一致 |
| kick 不丢失 | 队列空→发恰一个 SGI#0；后到 kick 与在途 IPI 合并（标志仍在） | 5.4 语义；CSD 路径有 -EBUSY 静默丢弃（CSD_FLAG_LOCK 忙），RESCHEDULE **无此语义**——反而更稳 |
| "忙则丢" | nohz_csd_func: `idle_balance = idle_cpu(cpu)`；非空闲 → 不 raise，kick 已消费 | 等价 5.4 got_nohz_idle_kick 的 `atomic_andnot + return false`；非空闲核靠自身 tick 做周期平衡 |
| WARN_ON 安全 | atomic_read 守卫后 KICK_MASK 位不可能被第三方清除（唯一清位者就是本核正在执行的 nohz_csd_func） | 不可触发；若仍有疑虑可改 WARN_ON_ONCE（见 §7 风险 2） |
| flags 消费 | `rq->nohz_idle_balance = flags`（含 STATS/NEXT/BALANCE/NEWILB）→ nohz_idle_balance → _nohz_idle_balance | 与 CSD 路径逐字节一致（同一函数同一数据流） |
| has_blocked/next_blocked | 由 `_nohz_idle_balance` 的 STATS 分支更新（fair.c:12751-12753） | 5.4 在 got_nohz_idle_kick 里做；6.12 本就在消费链里做，无缺失 |
| NOHZ_NEWILB_KICK | 仅此位置位时 atomic_read&KICK_MASK=0 → 守卫不触发 → 仍由 `nohz_run_idle_balance`（进入 idle 时）消费 | 与 CSD 路径行为一致（kick_ilb 只在置了 KICK_MASK 位时才发 IPI） |
| softirq 时序 | raise 于硬中断 → 最外层 IRQ 返回 irq_exit → do_softirq 内联执行 | 与 CSD 路径（IPI_CALL_FUNC 硬中断内 raise）完全一致 |
| 非 kick 的 RESCHEDULE | fold 后 atomic_read=0 → 直接返回 | 与现状 inline 完全一致（零开销路径只多一次原子读） |

---

## 6. 影响评估

### 6.1 功能
- **nohz idle balance 完整恢复**：`CONFIG_NO_HZ_IDLE=y` 后，空闲核 tick 停止、`nohz.idle_cpus_mask`
  维护、ILB kick（STATS/BALANCE/NEXT）与消费链全部工作，且 kick 传输与 T770 上被证明可靠的
  SGI#0 通道对齐。原厂 stock 即此设计。
- **与 P1 协同**：P1 后所有唤醒走直接入队 + resched_curr（SGI#0）；本补丁使 nohz kick 也走 SGI#0。
  **调度侧跨核异步通知 100% 收敛到 IPI_RESCHEDULE**，CSD+IPI_CALL_FUNC 不再承载任何调度工作。
- **残留 CSD 暴露面**：`cfsb_csd`（fair.c:6232，CFS bandwidth 异步 unthrottle）仍走
  CSD+IPI_CALL_FUNC——但 5.4 上游同样如此（非 6.12 回归），且 boot 期无 cgroup cpu.max 节流，
  影响面≈0；如需彻底清零可后续单独立项（超出本任务范围）。

### 6.2 性能
- kick 频率不变（tick 驱动，NOHZ_KICK_MASK 合并），只是 SGI#0 vs SGI#1——线上同为 GIC SGI，
  投递开销相同。
- ilb_cpu 深度空闲（A76 pd-big，entry 4ms）时 RESCHEDULE 唤醒延迟与 CSD 路径同量级（都是 SGI 唤醒
  wfi/psci），无回归；原厂同硬件已验证。
- scheduler_ipi 从 inline 变外部函数：IPI 路径多一次直接调用（+~5ns 级），5.4 多年来即此布局；
  非 kick 的 RESCHEDULE（高频路径）多一次本地原子读——可忽略。
- 无新增软中断频率（自触发 OPT 才增加，默认不启用）。

### 6.3 交互
- 与 `nohz=off` / `cpuidle.off` / `maxcpus=4` 等诊断手段正交：nohz=off 时 CONFIG 层仍在但
  nohz 状态机不启动，kick_ilb 空转（`atomic_read(&nohz.nr_cpus)` 检查早退），无副作用。
- 与 swait/stopper/KCFI 等其余差异无交互（本补丁只动 sched 传输层）。
- 多 arch 编译：scheduler_ipi extern 后所有调用它的 arch（arm/arm64/x86/mips/...，30+ 处）链接
  core.c 的实现；arm64 是唯一实际构建目标。!CONFIG_SMP 的 sched.h stub 保留。

---

## 7. 风险与回退

1. **假设不成立风险**：若竞态根因不是"CSD+IPI_CALL_FUNC 失效"（REPORT §4.2 的假设），本补丁
   不解决竞态，但功能等价、无回归（验证手段见 §8 探针 3/4）。
2. **WARN_ON**：理论上不可触发（§5 论证）。若实机出现，说明另有清位者（侵入性探针误写等），
   建议先抓现场再决定；稳妥起见可在 4.3 中把 `nohz_csd_func()` 的 `WARN_ON` 改为 `WARN_ON_ONCE`
   （一行，不影响语义）。
3. **重复/多余 RESCHEDULE**：resched_curr 与 kick 共用 SGI#0，空闲核可能收到两条；第二条经
   atomic_read 守卫自然过滤，无重复 rebalance。
4. **深空闲唤醒延迟**：与 CSD 路径同量级，无回归；若实测 ilb 响应慢（pd-big 长空闲），可加
   `sysctl_sched_migration_cost`/`nohz.next_balance` 调参，或启用 OPT 自触发。
5. **回退**：`git revert` 4.1-4.3 三个 hunk + `.config` 复原 NO_HZ_IDLE=n，即回到当前基线；
   补丁不触碰 P1、LSE、DEVTMPFS 等其它改动，回退无耦合。

---

## 8. 验证建议（一次一个变量）

1. **编译**：`make -j$(nproc) Image`（沿用现有构建/LLVM 工具链），确认无新 warning；
   `CONFIG_NO_HZ_COMMON=n`（即当前 NO_HZ_IDLE=n）下编译必须与现状等价（4.3 的 nohz 块被裁剪）。
2. **顺序**：先刷"3 文件补丁 + NO_HZ_IDLE 仍=n"（等价现状，回归为 0）→ 再刷 `NO_HZ_IDLE=y`。
3. **探针**（区分投递/处理/消费）：
   - kick_ilb 的 `smp_send_reschedule` 调用计数（发侧）；
   - `scheduler_ipi` 内 atomic_read 命中计数（收侧——证明 RESCHEDULE 到达并消费 kick）；
   - `do_handle_IPI(IPI_CALL_FUNC)` 计数应**显著下降**（nohz kick 不再走 SGI#1）；
   - `sched_balance_softirq` / `nohz_idle_balance` 执行计数（消费侧）。
4. **功能验证**：长时间空闲后 `cat /sys/kernel/debug/sched/debug`（或 trace `sched_load_balance`）
   确认 idle balance 仍在发生；`nohz.idle_cpus_mask` 正常维护。
5. **竞态对照**：memory_dev_init 卡点频率/位置与基线（NO_HZ_IDLE=n）对比；保留
   `nohz=off`、`maxcpus=4`、`cpuidle.off=1` 对照实验。
6. **长跑**：多轮 boot + 稳定运行，确认无 WARN、无 softlockup（建议开启
   `CONFIG_SOFTLOCKUP_DETECTOR`）。

---

## 9. 关键源码速查（6.12.103 实测行号）

| 符号 | 位置 | 备注 |
|---|---|---|
| `kick_ilb` | kernel/sched/fair.c:12374（改动点 :12409） | 传输通道替换 |
| `nohz_balancer_kick` | kernel/sched/fair.c:12416 | 决策点，不变 |
| `_nohz_idle_balance` | kernel/sched/fair.c:12662 | rebalance 执行体 |
| `nohz_idle_balance` | kernel/sched/fair.c:12765 | 消费 rq->nohz_idle_balance |
| `nohz_run_idle_balance` | kernel/sched/fair.c:12797 | NEWILB 消费 |
| `sched_balance_softirq` | kernel/sched/fair.c:13010 | SCHED_SOFTIRQ handler |
| `open_softirq(SCHED_SOFTIRQ,…)` | kernel/sched/fair.c:13828 | 注册 |
| `nohz_csd_func` | kernel/sched/core.c:1236 | **复用为 RESCHEDULE 消费函数** |
| `scheduler_ipi`（新） | kernel/sched/core.c（插入于 :3807 后） | 新增真实函数 |
| `scheduler_ipi`（旧 inline） | include/linux/sched.h:1968 | 改 extern |
| `INIT_CSD(&rq->nohz_csd,…)` | kernel/sched/core.c:8495 | 保留（无害，nohz_csd 字段不再被队列化） |
| `NOHZ_KICK_MASK` 等 | kernel/sched/sched.h:3186-3204 | 位定义 |
| `rq` 字段 nohz_csd/idle_balance/nohz_idle_balance/nohz_flags | kernel/sched/sched.h:1146-1227 | 不变 |
| `smp_send_reschedule` | include/linux/smp.h:139 / arch/arm64/kernel/smp.c:1106 | 发送端（SGI#0） |
| `do_handle_IPI` case IPI_RESCHEDULE | arch/arm64/kernel/smp.c:960-962 | 调用 scheduler_ipi，无需改动 |
| 5.4 参考 kick_ilb / scheduler_ipi / got_nohz_idle_kick | work/linux-5.4.210 fair.c:9425 / core.c:2407 / core.c:632 | 本设计蓝本 |
| 原厂 scheduler_ipi 反汇编 | work/DISASM-B-sched-调度器.md §2.3（0xffffffc0101fc658） | 自触发参考（OPT） |

---

## 10. 变更文件清单

- `kernel/sched/fair.c`（kick_ilb 传输通道 1 行 + 注释）
- `include/linux/sched.h`（scheduler_ipi 声明）
- `kernel/sched/core.c`（scheduler_ipi 真实函数）
- `.config`（CONFIG_NO_HZ_IDLE=y）
- （可选）`kernel/sched/core.c` OPT 自触发段，独立单变量验证后再并入
