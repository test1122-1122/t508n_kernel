# T508N mainline 6.12 completion 唤醒链路分析（stop_machine 卡点 · 第 2 版）

> 范围：只读 mainline 6.12.103（/home/test/t508n/linux-6.12.103/）。
> 版本：V2，按父会话已坐实的新事实（ic ialluis 假设弱化、tick 停摆升为核心）重写方向。
> 结论先行：**arm64 上 completion 唤醒没有 polling 兜底（无 TIF_POLLING_NRFLAG + idle 用 WFI），100% 依赖 SGI 投递**；「无心跳(PPI 死) + x=44 不亮(SGI 唤醒失效)」同源指向「stop_machine 之后中断投递整体停摆」。纯软件的 completion/swait/ttwu 链仍是自洽的。

---

## 0. 关键新事实（本轮坐实，直接改变方向）

| # | 事实 | 出处 | 影响 |
|---|---|---|---|
| F1 | **arm64 不定义 `TIF_POLLING_NRFLAG`** | 全 arch/arm64 无此宏（仅 x86/arm/powerpc 等有） | `set_nr_if_polling()` 恒返 false（core.c:981-986）→ 跨核唤醒**必发 IPI**，无 polling 免 IPI 优化 |
| F2 | **arm64 idle 用 `wfi()` 而非轮询** | arch/arm64/kernel/idle.c:30 | idle CPU 只能被中断唤醒；PPI/SGI 都不来 → 永不调度 waiter |
| F3 | **`CONFIG_ARM64_PSEUDO_NMI` 未开** | work/kernel.config:450 | 中断屏蔽走纯 DAIF（PSTATE.I），**无 PMR/优先级屏蔽复杂度**；entry.S 的 GIC_PRIO_MASKING 站点被 `#ifdef` 编译掉 |
| F4 | **`hard_irq_disable()` 在 arm64 是 no-op** | include/linux/interrupt.h:545-546 | multi_cpu_stop 的关中断只有 `local_irq_disable()`=daifset #3 一条有效 |
| F5 | **DIC 是 alternative 站点，且「早期未 patch、晚期已 patch」** | cacheflush.h:135 + alternative-macros.h:248-264 | 早期 LSE patch 时 `ic ialluis` 执行过且不挂；晚期（apply_alternatives_all）DIC 站点已被同轮 patch，**若 DIC=1 则 `ic ialluis` 被跳过 → 8 核轮完全不做 I-cache 失效** |
| F6 | **只开了 A76 errata（1165522/1286807/1463225），A55 的（1530923/2441007/4118414）未开** | work/kernel.config:344-346 | REPEAT_TLBI/SPECULATIVE_AT 只作用于 A76 大核 |
| F7 | **REPEAT_TLBI/SPECULATIVE_AT 的 patch 点都不在 tick/调度/中断投递热路径** | tlbflush.h:176-192、fault.c:279、KVM 各点 | 它们的「首次 patch」风险不在语义，而在「patch 后 I-cache 一致性不彻底」 |

---

## 1. 完整链路（函数+行号，含 arm64 特有结论）

### 1.1 complete 侧（CPU7，最后一个 signal_done）

```
kernel/stop_machine.c:78  cpu_stop_signal_done()
  ├─ :80  atomic_dec_and_test(&done->nr_todo)   // 8→0, 最后一个核 true（LSE ldaddal，已由 thread_ack 归零 3 次证明正确）
  └─ :81  complete(&done->completion)
kernel/sched/completion.c:45  complete() → :47 complete_with_flags(x, 0)
  ├─ :20  raw_spin_lock_irqsave(&x->wait.lock)
  ├─ :22  x->done++
  ├─ :24  swake_up_locked(&x->wait, 0)
  └─ :25  unlock
kernel/sched/swait.c:21  swake_up_locked()
  ├─ :25  list_empty() 检查（丢唤醒判据，§2.1 已排除）
  └─ :29  try_to_wake_up(curr->task, TASK_NORMAL, 0)
```

### 1.2 try_to_wake_up（跨核：CPU7 A76 → kernel_init@CPU0 A55）

```
kernel/sched/core.c:4142  try_to_wake_up(p, TASK_NORMAL, 0)
  ├─ :4182 ttwu_state_match → success=1（UNINTERRUPTIBLE ∈ TASK_NORMAL）
  ├─ :4245 p->__state = TASK_WAKING
  ├─ :4281 select_task_rq → 目标 CPU0
  └─ :4296 ttwu_queue(p, CPU0, ...)
kernel/sched/core.c:3958  ttwu_queue → :3963 ttwu_queue_wakelist()
kernel/sched/core.c:3884  ttwu_queue_cond()
  └─ :3915 !cpus_share_cache(CPU7, CPU0)   // A76 vs A55 不同 cluster → true → 走 wakelist
kernel/sched/core.c:3831  __ttwu_queue_wakelist()
  ├─ :3837 WRITE_ONCE(rq->ttwu_pending, 1)
  └─ :3838 __smp_call_single_queue(CPU0, &p->wake_entry.llist)
```

### 1.3 IPI 发送（arm64：**必发 IPI，无 polling 免 IPI**）

```
kernel/smp.c:379  __smp_call_single_queue()
  ├─ :411 llist_add(node, &per_cpu(call_single_queue, CPU0))  // 队空 → true
  └─ :412 send_call_function_single_ipi(CPU0)
kernel/smp.c:113  send_call_function_single_ipi()
  ├─ :116 call_function_single_prep_ipi(CPU0)
  └─ :119 arch_send_call_function_single_ipi(CPU0)
kernel/sched/core.c:3815  call_function_single_prep_ipi()
  └─ :3817 set_nr_if_polling(cpu_rq(CPU0)->idle)
       // ★ arm64 无 TIF_POLLING_NRFLAG → set_nr_if_polling 恒 false（core.c:981-986）
       //   → 恒 return true → 恒走 arch_send_call_function_single_ipi（真发 SGI）
arch/arm64/kernel/smp.c:857  arch_send_call_function_single_ipi()
  └─ :859 smp_cross_call(cpumask_of(CPU0), IPI_CALL_FUNC)
arch/arm64/kernel/smp.c:1017  smp_cross_call() → :1020 __ipi_send_mask(ipi_desc[IPI_CALL_FUNC], ...)
  └─ GICv3: gic_write_sgi1r()（arch_gicv3.h:107-110，写 ICC_SGI1R_EL1）
```

### 1.4 IPI 处理（CPU0 收到 SGI）

```
arch/arm64/kernel/smp.c:1011  ipi_handler → :952 do_handle_IPI(IPI_CALL_FUNC)
  └─ :964  generic_smp_call_function_interrupt()
kernel/smp.c:456  → :475 __flush_smp_call_function_queue()
  └─ :586  csd_do_func(sched_ttwu_pending, ...)      // CSD_TYPE_TTWU
kernel/sched/core.c:3772  sched_ttwu_pending()
  └─ :3792 ttwu_do_activate(rq, p, ...)
kernel/sched/core.c:3667  ttwu_do_activate()
  ├─ :3690 activate_task()              // kernel_init 入队 CPU0 rq
  ├─ :3691 wakeup_preempt() → resched_curr()   // 本核（IPI handler 就在 CPU0）→ set_tsk_need_resched
  └─ :3693 ttwu_do_wakeup()             // TASK_RUNNING
// irq_exit 时检查 need_resched → schedule() → kernel_init 运行 → wait_for_completion 返回 → x=44 亮
```

### 1.5 关键结论：**arm64 上这条链没有第二条路可走**

- `resched_curr()` 跨核时（core.c:1101）`set_nr_and_not_polling()` 在 arm64 恒返 true（core.c:975-979）→ **必发 reschedule SGI**。
- 本核时（IPI handler 内，core.c:1095-1098）直接 `set_tsk_need_resched`。
- idle 唤醒只能靠中断：`cpu_do_idle()` 的 `wfi()`（idle.c:30），无 TIF_POLLING 轮询，无 `flush_smp_call_function_queue` 的 polling 兜底（idle.c:374 那段只在发 IPI 的语义下有意义，但 arm64 根本不会走「不发 IPI」分支）。

**因此：`wait_for_completion` 返回 = 「CPU0 收到 SGI(IPI_CALL_FUNC) 并处理」。x=44 不亮 ⟺ SGI 没到/没处理。这与「无心跳 ⟺ PPI(timer) 也没到」构成同一中断投递故障的两个证据。**

---

## 2. 卡点分析（按新方向）

### 2.1 丢唤醒 / 内存序（原问题 1/4/5）—— 依旧排除

`complete_with_flags`（completion.c:20）与 `do_wait_for_common`（completion.c:84-97）持同一把 `x->wait.lock`，锁覆盖「入队+set_state」与「done+++查队唤醒」，经典丢唤醒不存在；`__set_current_state` 的 smp_store_mb 与 ttwu 的 `smp_mb__after_spinlock`（core.c:4181）配对，内存序无虞。stop_machine 场景 waiter 仅 1 个且必已睡眠。→ 排除。

### 2.2 环境差异：早期 LSE patch vs 晚期 apply_alternatives_all（重聚焦任务 1）

| 维度 | 早期 t508n_apply_lse_early | 晚期 apply_alternatives_all |
|---|---|---|
| CPU 数 | 1（CPU0） | 8（stop_machine，7 核关中断 spin） |
| IRQ 状态 | 全程关（early boot，中断尚未投递） | 进入前 tick 活着，DISABLE_IRQ 阶段关，返回后恢复 |
| patch 的 cap | 仅 LSE | remaining_capabilities = 全部非 boot cap（含 REPEAT_TLBI/SPECULATIVE_AT/PAN/DIC/IDC…） |
| DIC 站点状态 | **未 patch** → `alternative_has_cap_unlikely(DIC)`=false → `ic ialluis` **执行** | **本轮已 patch** → 若 DIC=1 → `ic ialluis` **被跳过** |
| 收尾 I-cache | `ic ialluis`（单核，无广播目标）+ 显式 `ic ivau`(caches_clean_inval_pou) | `dsb ish` + `icache_inval_all_pou()`(可能空) + `isb` |
| 结果 | boot 顺利到 smp_cpus_done | tick/IPI 停摆 |

**唯一能解释「早期没事、这里 tick 死」的收尾差异**：晚期这轮是 8 核、且 `ic ialluis` 可能因 DIC=1 被跳过（F5）。由此引出两条机制：

- **M1（若 DIC=0）**：8 核广播 `ic ialluis`（早期是单核没广播过）。T770 上广播到 7 个 `cpu_relax` spin 的核，SKILL 坑#4 的「hang」虽未当场发生（x=80 亮），但可能留下 snoop/L3/某核 I-cache 的不一致隐患。
- **M2（若 DIC=1，更值得怀疑）**：晚期整轮**完全没有 `ic` 失效**，依赖 DIC=1 的「I/D 硬件一致性」。若 T770 的 DIC 一致性在 A76→A55 跨 cluster 或 stop_machine 场景下不成立，则 7 个 spin 核（及随后跑中断路径的核）在 `isb()` 后仍可能取到**旧指令** → 部分核跑「半新半旧」代码。

### 2.3 REPEAT_TLBI / SPECULATIVE_AT 首次 patch（重聚焦任务 2）—— 语义不在 tick 路径，风险在 I-cache 一致性

实际启用（F6）：仅 A76 的 `1286807`(repeat-TLBI)、`1165522`(speculative-AT)、`1463225`(SVC-stepping，与本卡点无关)。

- **REPEAT_TLBI 实际 patch 指令**：`__repeat_tlbi_sync()`（tlbflush.h:176-182）→ 在 `__tlbi_sync_s1ish()`（:188-192）里追加一次 `tlbi vale1is, 0` + `dsb ish`。调用方全部是**广播型** flush：`flush_tlb_all`(:285)、`flush_tlb_mm`(:293)、`flush_tlb_range`(:495)、`flush_tlb_kernel_range`(:507)、`__flush_tlb_kernel_pgtable`(:530)。**这些都不在 tick/调度/中断投递路径上**；`switch_mm` 常规 flush 走 `local_flush_tlb_all()`（context.c:256，`tlbi vmalle1` 非广播），**不受影响**。
- **SPECULATIVE_AT 实际 patch 指令**：`ALTERNATIVE("nop","isb",...)` 在 AT 指令后插 ISB。非 KVM 主核仅 fault.c:279（`at s1e1r`，页错误路径）；其余全在 KVM/hyp（kvm/at.c:441 等）。**不碰 tick/中断路径**。

→ 结论：**这两个 workaround 的「首次启用」不会在语义上直接破坏 tick/IPI**。它们的真实风险是「作为 apply_alternatives_all 这一轮的 8 核 patch 内容，若 I-cache 一致性不彻底（M2），个别核会在 TLB/AT 内联路径执行旧码」，但这已归入 §2.2 的 M1/M2，而非独立根因。

### 2.4 中断屏蔽嵌套（重聚焦任务 2 的 tick 停摆机制）—— 代码干净，但需实测钉死

- `multi_cpu_stop`：stop_machine.c:217 `local_save_flags(flags)` → :236 `local_irq_disable()`（daifset #3，irqflags.h:52-57）→ :237 `hard_irq_disable()`（no-op）→ :258 `local_irq_restore(flags)`（write_sysreg daif，irqflags.h:172-177）。**save/restore 对称、无失衡**；stopper 线程在 `fn(arg)` 前已 `raw_spin_unlock_irq`（stop_machine.c:505），故 entry 时 IRQ 是开的、`flags` 里 I=0、restore 后 I=0。
- pseudo-NMI 关（F3）→ 无 PMR 写回、无 `system_uses_irq_prio_masking()` 分支，纯 DAIF。
- → 理论推断：stop_machine 返回后 8 核 PSTATE.I=0、PPI/SGI 应可投递。**「DAIF 卡住」只能靠探针实测排除**（见 §3.3），无法从源码判定为根因。

---

## 3. 根因假设排序 + 验证

### 总览

| 排名 | 假设 | 状态 |
|---|---|---|
| **R1** | **CPU0 收不到 SGI（跨核唤醒 IPI 不投递）**——arm64 无 polling 兜底，此点即卡点 | 核心（最可能） |
| **R2** | **晚期 patch 的 I-cache 一致性不彻底**（M2：DIC=1 跳 ic；或 M1：ic ialluis 广播隐患）→ 部分核在中断/分发路径执行旧码 → PPI/SGI 停摆 | 头号诱因 |
| **R3** | **DAIF.I 未恢复 / GIC 状态被 stop_machine 破坏** | 次要，需探针排除 |
| **R4** | REPEAT_TLBI/SPECULATIVE_AT 语义直接破坏 tick | 排除（§2.3：不在 tick 路径） |
| **R5** | completion 丢唤醒 / nr_todo 未归零 | 排除 |

### 3.1 R1：CPU0 收不到 SGI（核心）

**机制**：§1 证明 arm64 上 `wait_for_completion` 返回只依赖「CPU0 收到 IPI_CALL_FUNC SGI 并跑 `sched_ttwu_pending`」。idle 在 `wfi()`，SGI 不来就永远不调度 kernel_init。而「无心跳」说明 timer PPI 也没来。PPI 与 SGI 是两条独立中断线，同停 → 交集在 **PSTATE.I 屏蔽 / GIC CPU-interface(ICC_*) 状态 / 中断入口代码** 之一。

**需要钉死的三岔**：
1. SGI 到底发没发？（在 `smp_cross_call`/`__ipi_send_mask` 侧观察）
2. CPU0 的 `do_handle_IPI` 进没进？（SGI 是否到达 CPU0）
3. `sched_ttwu_pending` 跑没跑？（SGI 到达后是否被正确分发）

### 3.2 R2：I-cache 一致性不彻底（头号诱因，承接 ic ialluis 弱化后）

**机制（M2，重点）**：晚期 apply_alternatives_all 这一轮，`ARM64_HAS_CACHE_DIC` 站点已被同轮 patch（F5）。若 T770 `CTR_EL0.DIC=1`，则 `icache_inval_all_pou()`（cacheflush.h:135）提前 return，**8 核轮完全不做 `ic` 失效**，只靠 `dc civac`(alternative.c:139) + `dsb ish`(:188) + 各核 `isb`(:243)。这依赖 DIC=1 的硬件 I/D 一致性。T770 若在 A76/A55 跨 cluster 或 stop_machine 关中断 spin 场景下 DIC 一致性不成立，7 个 spin 核（及其后跑中断入口的核）会取到**旧指令**，而中断入口 `kernel_entry`/`kernel_exit`（entry.S）里恰有系统级站点（PAN=SYSTEM_FEATURE，cpufeature.c:2415；DIT、ADDRESS_AUTH 等）本轮才 patch → 中断路径「半新半旧」→ PPI/SGI 分发异常。

**M1（低权重，保留）**：若 DIC=0，`ic ialluis` 8 核广播对 7 个 spin 核留 snoop/L3/I-cache 隐患。早期单核跑过没挂，但没跑过「8 核广播」，不排除广播上下文特异性副作用。

**判定**：读 `CTR_EL0` bit29(DIC) 画屏（观测，零成本）。

### 3.3 R3：DAIF/GIC 状态 —— 次要但必须排除

**验证探针（集中在以下行，见 §3.4）**：multi_cpu_stop 返回后、`cpu_stop_signal_done` 前读 `daif`；`do_handle_IPI` 入口；`arch_timer_handler` 入口。

### 3.4 探针落点（不刷机先确认行号，刷机后单变量）

| 探针 | 位置（文件:行） | 判据 |
|---|---|---|
| SGI 是否发出 | arch/arm64/kernel/smp.c:1017 `smp_cross_call` 入口（或 :1020 `__ipi_send_mask` 前） | 亮=发 IPI |
| PPI 是否到达 | drivers/clocksource/arm_arch_timer.c:665 `timer_handler` 入口 | 亮=定时器中断还活着 |
| SGI 是否到达 CPU0 | arch/arm64/kernel/smp.c:952 `do_handle_IPI` 入口 | 亮=IPI 到了 |
| TTWU 是否处理 | kernel/sched/core.c:3772 `sched_ttwu_pending` 入口 | 亮=唤醒被分发 |
| 唤醒是否启动 | kernel/sched/core.c:4142 `try_to_wake_up` 入口（或 swait.c:29 之后） | 亮=complete 侧真的调了 ttwu |
| need_resched 是否置位 | kernel/sched/core.c:1083 `resched_curr` 入口 | 亮=入队+抢占成功 |
| DAIF 是否卡住 | stop_machine.c:524 `cpu_stop_signal_done` 前读 `daif`（画 I-bit） | I=1 → R3 坐实 |
| DIC 值 | 早期任意点读 `ctr_el0` bit29 画屏 | DIC=1 → 坐实 M2 前提 |

### 3.5 建议刷机顺序（单变量）

1. **Flash 1**：现有 y=440/450 探针 → 钉死「complete 调用 / waiter 未醒」。
2. **Flash 2**：加 §3.4 的 `do_handle_IPI` / `timer_handler` / `sched_ttwu_pending` 探针 → 定位中断链断在哪一环（SGI 没发？没到？没分发？）。
3. **Flash 3**：读 DIC（观测，并入任意一次）；若 DIC=1 → 强制 `icache_inval_all_pou()` 不做提前 return（或改为 `ic ivau` 按 VA）重刷 → 验证 M2。
4. **Flash 4**：若 DIC=0 且 M2 排除 → 关 `CONFIG_ARM64_ERRATUM_1286807`（repeat-TLBI）对照 → 验证「patch 内容」因素。

---

## 结论摘要（供父会话）

- **arm64 无 polling 兜底**：`TIF_POLLING_NRFLAG` 未定义 + idle 用 `wfi()`（idle.c:30）→ `set_nr_if_polling`/`set_nr_and_not_polling` 恒走「发 IPI」分支（core.c:981-986 / 975-979）→ **`wait_for_completion` 返回 = CPU0 收到 SGI(IPI_CALL_FUNC) 并跑 `sched_ttwu_pending`**。x=44 不亮 ⟺ SGI 未投递。
- **无心跳(PPI) + x=44(SGI) 同源 = 中断投递停摆**，不是 completion 原语问题（软件链自洽，丢唤醒/内存序已排除）。
- **根因排序**：R1=CPU0 收不到 SGI（核心）；R2=晚期 patch 的 I-cache 一致性不彻底（头号诱因，承接 ic ialluis 弱化：DIC 站点本轮才 patch，若 DIC=1 则 8 核轮**完全无 `ic` 失效**、靠硬件 DIC 一致性，T770 跨 cluster 场景不可靠）；R3=DAIF/GIC 状态（次要，需探针排除）；R4/R5 排除。
- **REPEAT_TLBI/SPECULATIVE_AT 语义不在 tick 路径**（tlbflush.h 广播 flush / fault.c+KVM 的 AT），其风险只在「8 核 patch 的 I-cache 一致性」，已并入 R2。
- **下一步**：先加 `do_handle_IPI`/`timer_handler`/`sched_ttwu_pending` 探针定位中断链断点，再读 DIC 判 M1/M2，单变量验证。
