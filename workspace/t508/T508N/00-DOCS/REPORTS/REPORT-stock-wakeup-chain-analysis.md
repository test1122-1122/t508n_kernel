# T508N：原厂 5.4 完整唤醒链反汇编 vs mainline 6.12（为什么原厂 SM#2 后 CPU0 能调度 waiter）

> 范围：只读反汇编 + 源码对比。符号表一律 `/home/test/t508n/work/stock_kallsyms.map`。
> 反汇编：`aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000 --start-address=<sym> --stop-address=<sym+n> work/stock_Image_full.bin`
> 5.4.210 上游源码已拉取到 `/home/test/t508n/work/stock-5.4.210/`（git.kernel.org v5.4.210），
> mainline 6.12.103 源码 = `/home/test/t508n/linux-6.12.103/`（**该树已埋 t508n 探针**，探针语义见 §0）。

---

## 0. 探针语义（先钉死观测坐标系，来自 mainline 树内探针）

| 探针 | 位置 | 语义 |
|---|---|---|
| y=440 x=8 | `kernel/stop_machine.c:441 __stop_cpus` | `wait_for_completion` 前（入 wait） |
| y=440 x=44+10*i | `stop_machine.c:446` | wait 返回后的完成计数（不亮 = 没返回） |
| y=410/430 (8 格) | `stop_machine.c:530/534 cpu_stopper_thread` | 每核 fn 返回后 / signal_done 后 |
| y=450 | `stop_machine.c:537` | `cpu_stop_signal_done` 把 nr_todo 减到 0（= complete 已调用） |
| y=460 x=44 / x=116 | `arch/arm64/kernel/smp.c:960 do_handle_IPI` | SGI 到达，IPI_CALL_FUNC / 其他 |
| y=460 x=152 / **x=188** | `smp.c:964-966` | SM#2 阶段 IPI_CALL_FUNC / **IPI_RESCHEDULE 到达** |
| y=460 x=296 | `kernel/sched/core.c:3984 ttwu_queue` | SM#2 阶段**直接入队**（非 wakelist 分支） |
| y=460 x=260 | `kernel/sched/idle.c:280 do_idle` | SM#2 阶段 CPU0 进入 idle 循环 |
| y=470 x=8/44/116 | `core.c:3784/3796 sched_ttwu_pending` | wakelist 待处理 / 任务激活 / SM#2 阶段 |
| y=470 x=44 | `stop_machine.c:546` | CPU0 stopper 内 DAIF.I=1 |

**已实测事实**：y=450 亮（complete 已调）；x=296 亮（直接入队）；**x=188 亮（IPI_RESCHEDULE 已到 CPU0 do_handle_IPI）**；x=44/x=152 不亮（无 IPI_CALL_FUNC/wakelist）；y=440 计数不亮（wait 未返回）；timer/PPI 探针亮、CPU0 DAIF 正常。

**即：mainline 卡点在「do_handle_IPI 收到 IPI_RESCHEDULE 之后」——之后只有 scheduler_ipi（fold）→ 中断返回 → idle 循环 need_resched 检查 → schedule_idle() → kernel_init。**

---

## 1. 任务 1：原厂 cpu_stopper_thread 退出路径 + smpboot_thread_fn（preempt_count 处理）

### 1.1 原厂 `cpu_stopper_thread`（0xffffffc01030adc8）关键片段

```
30ade8: cmp  w0, #0x20 ; b.cs 30b0b0        ; BUG_ON(cpu >= 32)
30ae00: msr  daifset, #0x2                   ; local_irq_disable()
30ae08: ldr  w9, [x24, #24] ; add w9, w9,#1 ; str w9,[x24,#24]
                                              ; [current+24]++  ← preempt_count++（函数入口）
30ae24: b    30b0b8                          ; → raw_spin_lock(&stopper->lock)（CAS/LLSC 循环）
30ae4c: ldr  x21, [x25, #16]!                ; work = works.next
30ae54: b.eq 30b024                          ; 队空 → unlock 路径
30ae8c: stlrb w8, [x19]                      ; raw_spin_unlock
30ae90: msr  daifclr, #0x2                   ; local_irq_enable()
30ae98: ldr  x8,[x24,#24]; subs x8,x8,#1; str w8,[x24,#24]  ; preempt_count--
30aeb0: cbz  x21, 30b050                     ; work==NULL → 收尾/返回
30aec8: add  w8, w8, #1 ; str w8, [x24, #24] ; ★ preempt_count_inc()   ← 调 fn 前
30aedc: blr  x22                             ; ret = fn(arg)
30aef4: mov  w8,#1 ; neg w8,w8
30aefc: ldaddal w8, w9, [x21]                ; atomic_dec_and_test(&done->nr_todo)
30af04: cbnz w8, 30af10                      ; 非 0 → 跳过
30af08: add  x0, x21, #8 ; bl 0x227a70       ; ★ complete(&done->completion)（cpu_stop_signal_done 内联）
30af10: ldr  w8,[x24,#24]; sub w8,w8,#1; str w8,[x24,#24]  ; preempt_count_dec()
30af1c: ldr  w8,[x24,#24]; cbz w8, 30af2c    ; WARN_ONCE(preempt_count()!=0, ...) 检查
30af2c: msr  daifset, #0x2 ; [x24,#24]++     ; local_irq_disable + preempt_count_inc
30af3c: b    30aff8                          ; ★ goto repeat（重新抢 stopper->lock 取下一个 work）
30b024: stlrb w8,[x19]; msr daifclr,#0x2     ; 队空: unlock + local_irq_enable
30b038: [x24,#24]--                          ; preempt_count--
30b044: b.eq 30b070
30b070: ldr w8,[x24,#24]; cbnz w8, 30b050    ; if (preempt_count()!=0) → 直接返回
30b078: mrs x8, daif; and w9,w8,#0x80
30b080: cbnz w9, 30b050                      ; if (irqs_disabled()) → 直接返回
30b084: [x24,#24]++; mov w0,#1; bl __schedule ; ★ preempt_schedule_common 内联（__schedule(true)）
30b098: [x24,#24]--; ldr x8,[x24]; tbnz w8,#1, 30b084  ; while (TIF_NEED_RESCHED)
30b0ac: b 30b050                             ; → epilogue ret（返回 smpboot_thread_fn）
```

**对照**：上游 v5.4.210 `kernel/stop_machine.c:497-531`（已拉取）与 6.12 `stop_machine.c:501-560` 结构**完全相同**：
`repeat: raw_spin_lock_irq(lock) → 取 work → unlock_irq → if(work){ preempt_count_inc(); ret=fn(arg); if(done){done->ret; cpu_stop_signal_done(done);} preempt_count_dec(); WARN_ONCE(...); goto repeat; }`。

**差异结论（preempt_count）**：
- **原厂在调 `fn(arg)` 前同样有 `preempt_count_inc()`（0x30aec8-0x30aed0），与 mainline 完全一致**。任务假设"原厂没有 preempt_count_inc"**不成立**——两版 multi_cpu_stop 都在 in_atomic()==T 下执行。
- 原厂独有（上游 5.4 与 6.12 都没有）：入口 `preempt_disable()`（ae08）+ 队空收尾时 **`preempt_schedule()` 内联（b070-b0ac，`__schedule(true)`）**——即 work 队列清空后先显式让出 CPU 再返回；6.12 直接 `return` 给 smpboot_thread_fn 的 `schedule()`。两者最终都会让出 CPU，语义等价。

### 1.2 原厂 `smpboot_thread_fn`（0xffffffc0101e3dc8）

```
1e3df4: str x8(=1), [x22, #40]; dmb ish       ; set_current_state(TASK_INTERRUPTIBLE)
1e3e04: ldr w8,[x22,#24]; add w8,w8,#1; str w8,[x22,#24]  ; preempt_disable()
1e3e18: tbz w8, #5, ...                       ; kthread_should_stop / park 检查
...
（thread_should_run 分支）__set_current_state(TASK_RUNNING); preempt_enable(); ht->thread_fn(cpu);
（无 work 分支）preempt_enable_no_resched(); schedule();
```

与 v5.4.210 `kernel/smpboot.c:115-167` 及 6.12 `kernel/smpboot.c smpboot_thread_fn` **逐行一致**：`thread_fn(cpu)` 在 `preempt_enable()` 之后调用（preempt_count==0）；无 work 时 `preempt_enable_no_resched(); schedule();`（普通 schedule，非 schedule_idle/schedule_preempt_disabled）。

**任务 1 结论**：stopper 线程退出路径（signal_done → goto repeat → 队空 → 返回 smpboot → schedule）与 preempt_count 处理在 stock/mainline **等价**；原厂唯一多的是队空时显式 `preempt_schedule()`，不影响 waiter 侧唤醒。

---

## 2. 任务 2：原厂 resched_curr（0xffffffc0101f3b30）

```
1f3b3c: ldr  x8, [x0, #2416]                 ; rq->curr（rq+0x970）
1f3b40: ldr  x9, [x8] ; tbnz w9, #1, 1f3bb4  ; if (test_tsk_need_resched(curr)) return
1f3b4c: ldr  w9, [x0, #2632]                 ; cpu_of(rq)（rq+0xa48）
1f3b50: mrs  x10, tpidr_el1 ; ldr w10, [...] ; smp_processor_id()
1f3b5c: cmp  w9, w10 ; b.ne 1f3b80           ; 跨核 → 1f3b80
; ---- 同核分支 ----
1f3b6c: mov  w9, #2 ; stset x9, [x8]         ; ★ set_tsk_need_resched(curr)（LSE 原子置位 TIF_NEED_RESCHED=bit1）
1f3b74: mrs  x8, sp_el0 ; str wzr, [x8, #28] ; ★ set_preempt_need_resched()（Unisoc 表示：清 [current+28]，见下）
1f3b7c: b    1f3bb4                          ; return
; ---- 跨核分支 ----
1f3b88: mov  w10, #2 ; stset x10, [x8]       ; ★ set_nr_and_not_polling(curr)：原子置 curr->flags bit1（TIF_NEED_RESCHED）
1f3b90: ... and x8,x9,#0x3f ; ...            ; cpumask_of(cpu)
1f3bac: mov  w1, wzr                         ; ★ IPI 号 = 0 = IPI_RESCHEDULE
1f3bb0: bl   0x1169b4                        ; ★ smp_send_reschedule 被内联 → smp_cross_call(cpumask_of(cpu), 0)
```

（LSE `stset` 与 LL/SC `ldxr/orr/stxr`（1f3bd0/1f3be8）是同一替代站点 patch 前后两副本。）

**对照 mainline 6.12** `kernel/sched/core.c:1083-1102 resched_curr`：
```c
	if (test_tsk_need_resched(curr)) return;
	cpu = cpu_of(rq);
	if (cpu == smp_processor_id()) {
		set_tsk_need_resched(curr);
		set_preempt_need_resched();
		return;
	}
	if (set_nr_and_not_polling(curr))       // core.c:981-986（无 TIF_POLLING_NRFLAG → set_tsk_need_resched + return true）
		smp_send_reschedule(cpu);           // → arch_smp_send_reschedule → IPI_RESCHEDULE
	else
		trace_sched_wake_idle_without_ipi(cpu);
```

**差异结论**：
- 逻辑逐行相同。arm64 **5.4 与 6.12 都不定义 `TIF_POLLING_NRFLAG`**（已核对两版 `arch/arm64/include/asm/thread_info.h`）→ `set_nr_and_not_polling` 恒 true、`set_nr_if_polling` 恒 false → **跨核 resched 必发 IPI**（无 polling 免 IPI 优化），两版一致。
- idle 用 wfi：原厂 5.4 `arch/arm64/kernel/process.c:71-75 __cpu_do_idle(){ dsb(sy); wfi(); }`；6.12 同（idle.c:30 wfi）。两版都无 polling 兜底。
- 原厂同核分支的 `set_preempt_need_resched()` 编译成「写 0 到 [current+28]」（Unisoc 定制 thread_info 布局：preempt_count 在 +24、+28 是折叠标志字，语义上等价于 OR 0x80000000）；6.12 是标准 `preempt_count |= PREEMPT_NEED_RESCHED`。**语义等价**（且本场景走跨核分支，根本不执行同核分支）。

---

## 3. 任务 3：原厂 smp_send_reschedule（0x117470）+ smp_cross_call（0x1169b4）

```
11747c: and  w8, w0, #0x3f ; add w8, w8, #1 ; lsr w9, w0, #6   ; cpumask_of(cpu)（cpu_bit_bitmap 惯用法）
117490: ... sub x0, x8, w9, uxtw #3
117498: mov  w1, wzr                           ; IPI_RESCHEDULE = 0
11749c: bl   0x1169b4                           ; smp_cross_call(cpumask_of(cpu), 0)
1169d8: ldr  x9, [x9, w1, uxtw #3]             ; ipi_types[ipinr]（trace_ipi_raise 内联）
1169f0: ldr  x0, [x9, x19, lsl #3]             ; ipi_desc[ipinr]
1169f8: bl   0x278694                          ; ★ __ipi_send_mask(ipi_desc[ipinr], target)
                                               ;   → gic irqchip → gic_write_sgi1r(ICC_SGI1R_EL1)
```

**对照 mainline 6.12** `arch/arm64/kernel/smp.c`：
- `arch_smp_send_reschedule(cpu)`（:1120）= `smp_cross_call(cpumask_of(cpu), IPI_RESCHEDULE)`；
- `smp_cross_call`（:1031）= `trace_ipi_raise(...)` + `__ipi_send_mask(ipi_desc[ipinr], target)`。

**差异结论**：**完全一致**。IPI_RESCHEDULE=0、IPI_CALL_FUNC=1（两版 enum 相同）；SGI 都经 `__ipi_send_mask` → GICv3 `ICC_SGI1R_EL1` 写入。mainline 泛型 `smp_send_reschedule(cpu)`（include/linux/smp.h:141 内联宏）→ `arch_smp_send_reschedule`，链路同。

---

## 4. 任务 4：原厂 ipi_handler（0x116cac）→ IPI_RESCHEDULE 处理 + scheduler_ipi（0x1fc658）

```
116d38: adr x10, 116d38 ; ldrsw x11,[x9,x8,lsl#2] ; add x10,x10,x11 ; br x10   ; jump table（switch(ipinr)）
...
116d54: b.ne 116dfc                       ; 非 crash-stop 条件
116dfc: bl   0xffffffc0101fc658            ; ★ case IPI_RESCHEDULE: scheduler_ipi()
```

**原厂 scheduler_ipi（0x1fc658）是真实函数**：
```
1fc684: ldr x8,[x19] ; tbz w8,#1, 1fc690   ; if (tif_need_resched())
1fc68c: str wzr,[x19,#28]                  ;   ★ preempt_fold_need_resched()（Unisoc 表示：清 [current+28]）
1fc6a0: ldr x8,[x8,#3120]                  ; this_rq()->wake_list.first（rq+0xc30）
1fc6ac: cbz x8, 1fc778                     ; 空 → 直接返回（无 irq_enter）
1fc6b0: bl 0x28c58c                        ; irq_enter()
1fc6bc: [x19,#24]+=0x200                   ; preempt_active 置位
1fc6d4: bl 0x2c4680                        ; ★ sched_ttwu_pending()（旧式 rq->wake_list 唤醒）
1fc6f0: [x19,#24]-=0x200                   ; preempt_active 复位
1fc6fc: bl 0x203810                        ; nohz kick / irq_exit 路径
```

**对照 mainline 6.12**：`include/linux/sched.h:1968-1977` —— **scheduler_ipi 是 inline，只有 `preempt_fold_need_resched()`**（`if (tif_need_resched()) set_preempt_need_resched();`）；wakelist 处理整体搬到 **IPI_CALL_FUNC → CSD**（kernel/smp.c:588 `csd_do_func(sched_ttwu_pending, ...)` → `sched_ttwu_pending(arg)`）。

**差异结论**：
- **对 SM#2 直接入队场景（wake_list 空、无 nohz kick）**：原厂 scheduler_ipi 走到 `cbz x8, 1fc778` 直接返回，效果与 6.12 inline 的纯 fold **完全等价**。差异只影响「旧式 wakelist 唤醒」（5.4 用 rq->wake_list+IPI_RESCHEDULE；6.12 用 CSD+IPI_CALL_FUNC，本场景 x=44 不亮已证明没走）。
- 6.12 的 `preempt_fold_need_resched` 只在 CONFIG_PREEMPT 下把 TIF_NEED_RESCHED 折进 preempt_count；**idle 任务的唤醒判定靠的是 TIF_NEED_RESCHED 本身（waker 已在 resched_curr 里直接置位），fold 只是辅助** —— 两版一致。

---

## 5. 任务 5：原厂 try_to_wake_up（0x1f6084）跨核路径

```
1f60a4: [x23,#24]++                            ; preempt_disable()
1f60b0: cmp x23, x0 ; b.eq 1f61a4              ; if (p == current)
1f60c4: mrs x24, daif ; ... ; msr daifset,#2   ; raw_spin_lock_irqsave(&p->pi_lock) 前半
1f60cc: add x21, x20, #0x81c                   ; &p->pi_lock（task+0x81c）
1f6100: casa w8, w2, [x21]                     ; raw_spin_lock(&p->pi_lock)（LSE CAS；LL/SC 回退在 6504）
1f6114: ldr x8,[x20,#40] ; tst x8, x9(w22)     ; if (p->state & state)   ← state 在 task+40
1f6150: ldar w8,[x20,#0x50] ... wfe 循环       ; smp_cond_load_acquire(&p->on_cpu, !VAL)
1f6218: str x10(#0x200), [x20,#40]             ; p->state = TASK_WAKING(0x200)
1f6224: ldr w8,[x20,#84] ... per_cpu rq        ; 迁移/目标 rq 处理（stadd -1 = nr_running--）
1f6270: ... ldr x22,[x8,#88]                   ; select_task_rq（sched_class 回调）区域
（中部 0x1f6294 区域）ttwu_queue 内联：
  sched_feat(TTWU_QUEUE) && !cpus_share_cache(waker,target)
    → ttwu_queue_remote：llist_add(&p->wake_entry, &cpu_rq(cpu)->wake_list) + smp_send_reschedule(cpu)=IPI_RESCHEDULE
    （v5.4.210 core.c `ttwu_queue` / `ttwu_queue_remote` 已核对，见下）
  else → rq_lock(cpu_rq(target)) + ttwu_do_activate → wakeup_preempt → resched_curr（§2）
```

v5.4.210 源码（已拉取 `stock-5.4.210/kernel/sched/core.c`）：
```c
static void ttwu_queue(struct task_struct *p, int cpu, int wake_flags) {
	...
	if (sched_feat(TTWU_QUEUE) && !cpus_share_cache(smp_processor_id(), cpu)) {
		sched_clock_cpu(cpu); ttwu_queue_remote(p, cpu, wake_flags); return; }
	rq_lock(rq, &rf); update_rq_clock(rq); ttwu_do_activate(rq, p, wake_flags, &rf); rq_unlock(rq, &rf);
}
static void ttwu_queue_remote(...) {
	p->sched_remote_wakeup = !!(wake_flags & WF_MIGRATED);
	if (llist_add(&p->wake_entry, &cpu_rq(cpu)->wake_list)) {
		if (!set_nr_if_polling(rq->idle))   /* arm64 恒 false → 必发 */
			smp_send_reschedule(cpu);       /* IPI_RESCHEDULE */
		else
			trace_sched_wake_idle_without_ipi(cpu); } }
```

**对照 mainline 6.12** `core.c:3898 ttwu_queue_cond / 3952 ttwu_queue_wakelist / 3845 __ttwu_queue_wakelist`：
```c
static inline bool ttwu_queue_cond(struct task_struct *p, int cpu) {
	if (task_on_scx(p)) return false;
	if (p->sched_class == &stop_sched_class) return false;
	if (!cpu_active(cpu)) return false;
	if (!cpumask_test_cpu(cpu, p->cpus_ptr)) return false;
	if (!cpus_share_cache(smp_processor_id(), cpu)) return true;   /* 异簇 → wakelist */
	if (cpu == smp_processor_id()) return false;
	if (!cpu_rq(cpu)->nr_running) return true;                    /* ★ 新：目标 rq 空(空闲) → 也走 wakelist */
	return false; }
/* wakelist 传输: WRITE_ONCE(rq->ttwu_pending,1); __smp_call_single_queue(cpu, &p->wake_entry.llist); → IPI_CALL_FUNC */
```

**差异结论**：
- **同簇（共享 L2）唤醒：两版都走直接入队 + resched_curr**。SM#2 场景 waker 与 CPU0 同 A55 簇、CPU0 rq 非空（stopper 还在跑）→ 6.12 `ttwu_queue_cond` 返回 false → 直接入队（x=296 亮 ✓），与 5.4 行为一致。
- 唯一新增分支：**6.12 对「目标 rq 为空（CPU 空闲）」的同簇唤醒也走 wakelist（IPI_CALL_FUNC）**；5.4 同簇一律直接入队。若 complete() 发生在 CPU0 已回到 idle 之后（stopper 已 schedule 出去、rq->nr_running==0），6.12 会改走 IPI_CALL_FUNC 而非 IPI_RESCHEDULE —— **但实测 x=188（RESCHEDULE）亮、x=44/152（CALL_FUNC）不亮，说明 complete 时 CPU0 rq 非空，未触发该分支**。
- wakelist 传输通道：5.4 = `rq->wake_list` + **IPI_RESCHEDULE**；6.12 = CSD + **IPI_CALL_FUNC**（smp.c:588）。这是 5.4→6.12 的真实演进，但本场景不走 wakelist。

---

## 6. 任务 6：综合回答 —— 原厂能跑、mainline 卡，差异在哪？

### 6.1 唤醒链本身：**两版逐环节等价**（本报告核心结论）

| 环节 | 原厂 5.4 | mainline 6.12 | 差异 |
|---|---|---|---|
| stopper 调 fn | preempt_count_inc 包裹（0x30aec8） | preempt_count_inc 包裹（stop_machine.c:519） | **无**（任务假设"原厂没有"不成立） |
| complete() 调用点 | stopper 内 cpu_stop_signal_done（0x30af0c） | 同（stop_machine.c:524 + 探针 y=450） | 无 |
| stopper 队空退出 | 多一次 `preempt_schedule()`（Unisoc 定制）→ 返回 smpboot → schedule() | 直接返回 smpboot → schedule() | 都会让出 CPU，等价 |
| ttwu 同簇跨核 | 直接入队 → resched_curr → IPI_RESCHEDULE | 同（x=296/x=188 探针证实） | 无（本场景） |
| resched_curr | set_tsk_need_resched + smp_cross_call(0) | 同（core.c:1083） | 无 |
| 无 TIF_POLLING_NRFLAG / idle wfi | 两版都是 | 两版都是 | **无**（跨核必发 IPI，无 polling 兜底） |
| SGI 发送 | __ipi_send_mask → ICC_SGI1R_EL1 | 同（smp.c:1031） | 无 |
| IPI_RESCHEDULE 处理 | scheduler_ipi 函数：fold + wake_list 检查 | scheduler_ipi inline：仅 fold | 本场景等价（wake_list 空） |
| idle 唤醒 | wfi 被 SGI 唤醒 → need_resched → schedule_idle | 同 | 无 |

**→ 「原厂 SM#2 后 CPU0 能正常调度 waiter」与 mainline 的差异不在唤醒链代码本身**。complete→ttwu→resched_curr→SGI→do_handle_IPI→scheduler_ipi→idle→schedule_idle 这条链，原厂没有任何 mainline 缺失的"保证 waiter 被调度"机制。

### 6.2 真实差异（代码演进层面，均不影响本场景语义）

1. **scheduler_ipi：函数 vs inline**（5.4 core.c:2407 带 wake_list/nohz-kick 处理；6.12 sched.h:1968 纯 fold）。wakelist 处理随 5.5+ 重构迁移到 IPI_CALL_FUNC/CSD。
2. **wakelist 传输通道**：5.4 `rq->wake_list`+IPI_RESCHEDULE ↔ 6.12 CSD+IPI_CALL_FUNC（smp.c:588）。
3. **ttwu_queue_cond 新增「目标 rq 空 → wakelist」分支**（6.12 core.c:3922）——同簇唤醒到**空闲** CPU 时 6.12 走 IPI_CALL_FUNC、5.4 走直接入队+IPI_RESCHEDULE。本场景未触发（CPU0 rq 非空），但**是两版在"同簇唤醒"上的唯一行为分叉点**。
4. 原厂 stopper 队空尾部多一次显式 `preempt_schedule()`（Unisoc 定制）。
5. thread_info 布局定制（preempt_count@+24、折叠标志@+28、state@+40），语义等价。

### 6.3 最可能解释卡点的差异（按可能性排序）

- **R1（环境，头号嫌疑）：mainline 独有的「8 核晚期 apply_alternatives_all」I-cache 一致性问题**。探针已把卡点压缩到「do_handle_IPI 收到 IPI_RESCHEDULE（x=188）之后」：之后只有 scheduler_ipi（fold）→ 中断返回 → idle 的 need_resched 检查 → schedule_idle。原厂 5.4 同等场景（它也走 stop_machine 应用 alternatives，见 REPORT-stock-sync-mechanism.md）没有这一卡点。**若 CPU0 的中断返回路径/idle 循环/调度代码在 8 核 patch 后取到旧指令（DIC=1 时整轮无 `ic` 失效，见 REPORT-mainline-completion-wakeup-analysis.md §2.2 M2）**，则 SGI 能进 do_handle_IPI（探针亮）但返回路径或随后的 need_resched 检查执行"半新半旧"代码 → kernel_init 永远不被调度、y=440 计数不亮、tick 同源停摆。**这条链路在所有探针事实下自洽**，且 stock 没有对应的"晚期 8 核补丁"事件。
- **R2（环境，次席）：tick 停摆（PPI 死）**。与 R1 同源（同一中断投递/代码故障），不是独立代码差异。
- **R3（代码差异，低）：6.12 `ttwu_queue_cond` 的 `!nr_running → wakelist` 分支**。若后续把 complete() 时机后移（CPU0 已回 idle 才 complete），6.12 会发 IPI_CALL_FUNC 而 5.4 发 IPI_RESCHEDULE —— 但实测 x=188 已证明本场景走的是直接入队+RESCHEDULE，未触发。
- **排除**：preempt_count 处理（两版一致）；completion 原语（前报告已排除）；丢唤醒/内存序（前报告已排除）；DAIF 状态（探针正常）；SGI 投递（x=188 亮）。

### 6.4 可操作的验证/修复建议

1. **加探针钉死"do_handle_IPI 之后"的断点**（单次刷机，多观测）：
   - `scheduler_ipi()` 返回处（6.12 是 inline，在 smp.c `case IPI_RESCHEDULE` 后）画点 → 确认 fold 执行完；
   - `do_idle` 的 `while (!need_resched())` 循环体每次迭代入口（idle.c）画点 → 确认 CPU0 idle 是否在 wfi 后醒来并重新检查；
   - `schedule_idle()` / `__schedule` 入口（core.c）画点 → 确认是否进入调度；
   - 若 do_handle_IPI 后无任何点亮 → 中断返回路径/后续代码执行异常，直接指向 R1（I-cache）。
2. **读 CTR_EL0.DIC 并做对照**：若 DIC=1 → 强制 `icache_inval_all_pou()` 不做提前 return（或改按 VA `ic ivau`）重刷，验证 R1/M2。
3. **对照实验（排除 R3）**：若想验证同簇唤醒分叉，可临时把 6.12 `ttwu_queue_cond` 的 `!cpu_rq(cpu)->nr_running` 分支改回 5.4 行为（同簇一律直接入队）——但按探针时序本场景不触发，优先级最低。
4. **优先单变量验证 errata 覆盖差异**（承接 REPORT-stock-vs-mainline-completion.md §7）：mainline 新增的 A55 `1530923`（speculative-AT）在页错误/AT 路径首次 patch，若伴随 I-cache 不一致会污染中断路径。

---

## 附：符号速查（stock_kallsyms.map）

| 符号 | 地址 | 说明 |
|---|---|---|
| cpu_stopper_thread | 0xffffffc01030adc8 | §1.1 |
| smpboot_thread_fn | 0xffffffc0101e3dc8 | §1.2 |
| resched_curr | 0xffffffc0101f3b30 | §2 |
| smp_send_reschedule | 0xffffffc010117470 | §3（resched_curr 内联后仅 ftrace/其他调用者用） |
| smp_cross_call | 0xffffffc0101169b4 | §3 |
| ipi_handler | 0xffffffc010116cac | §4（do_handle_IPI 在其内，jump table 分发） |
| scheduler_ipi | 0xffffffc0101fc658 | §4（真实函数） |
| try_to_wake_up | 0xffffffc0101f6084 | §5 |
| complete | 0xffffffc010227a70 | cpu_stop_signal_done 内联调用 |
| __schedule | 0xffffffc01169e8e0 | stopper/preempt_schedule 尾部调用 |
| preempt_schedule | 0xffffffc01169e87c | §2 附：preempt_count@+24、preemptible()=cnt==0&&!I |
| wait_for_completion | 0xffffffc0116a0d30 | waiter 侧（y=440 探针处） |

## 附：5.4.210 源码拉取（本报告对照用）

`/home/test/t508n/work/stock-5.4.210/`（kernel/stop_machine.c、kernel/smpboot.c、kernel/sched/core.c、arch/arm64/kernel/smp.c、arch/arm64/include/asm/thread_info.h、arch/arm64/kernel/process.c 等，git.kernel.org v5.4.210）
