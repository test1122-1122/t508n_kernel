# T508N：原厂跨 cluster 可见性操作模式分析（普通 store vs 原子/dsb/锁）

> 范围：只读反汇编 + 源码对比。符号表一律 `/home/test/t508n/work/stock_kallsyms.map`。
> 反汇编：`aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000 --start-address=<sym> --stop-address=<sym+n> work/stock_Image_full.bin`
> 5.4.210 上游源码 = `/home/test/t508n/work/stock-5.4.210/`；mainline 6.12.103（已埋探针）= `/home/test/t508n/linux-6.12.103/`。
> 引用（不重复反汇编）：`REPORT-stock-sync-mechanism.md`（multi_stop 协议 + flag 置位）、
> `REPORT-stock-wakeup-chain-analysis.md`（唤醒链等价）、`REPORT-stock-vs-mainline-completion.md`（completion 对比，**§7 有一处被本报告 §3.3 修正**）。

---

## 0. TL;DR（结论摘要）

1. **原厂跨核写全部落在四种模式之一，没有"裸普通 store 单次读"模式**（§1 逐点证据）：
   - **原子 RMW**（LSE/LL-SC）：`thread_ack`（`ldaddal`/`ldxr-stlxr`+`dmb ish`）、`done->nr_todo`（`ldaddal`）、
     TIF_NEED_RESCHED 跨核置位（`stset`）、自旋锁（`casa`/`ldaxr-stxr`）。
   - **持锁临界区**：completion `done++`（`casa` 抢锁 → 普通 `str` → `stlrb` 释放）、`rq->nr_running` 等。
   - **显式 store barrier + 普通 store + 轮询读者**：`msdata->state`（`set_state` = 普通 `str` + `dmb ishst` + 普通 `str`；
     读者 `ldr` + `yield` 轮询）、`all_alternatives_applied`（前置 `dsb ish` → 普通 `str`；读者 `ldr`+`yield`+`isb`）。
   - **普通 store/原子位 + IPI，GIC 侧 `dmb ishst` 兜底**：`resched_curr`（`stset` → `smp_cross_call` →
     `gic_ipi_send_mask` 内 **`dmb ishst`** 后写 SGI1R）。
2. **"普通 store 跨 cluster 不可靠"的精确表述**：原厂在 T770 上**确实用普通 store 跨核通信且工作**
   （alternatives flag、`msdata->state`），证明 PoC 一致性可用；但这些普通 store 全部配了
   **前置 dsb/dmb + 容忍延迟的轮询读者**。ARMv8 内存模型本身就不给"无同步的普通 store → 单次 load"
   任何可见性保证——读旧值是**标准预期行为**，不是 T770 特有的硬件缺陷（§3.4）。
3. **探针 `t508n_sm2_done` 是内核里唯一"裸普通 store + 单次读"的跨核模式**（§4.3）——写侧
   `t508n_sm2_done = true`（alternative.c:237，无 barrier/原子/锁），读侧 `if (t508n_sm2_done)` 单次读
   （swait.c:30/33/37、core.c:3797/3985）。它在 waker 核读到 false、在另一些核读到 true，
   与"无同步的 store 按传播时延逐渐可见"完全自洽（§4.3）。
4. **mainline 内核自身的跨核同步与原厂同构，没有可解释卡点的"裸 store 依赖"**（§4）：
   SGI 路径两版都有 store barrier（stock `dmb ishst` + 内存映射 SGI1R 写；mainline `dsb(ishst)`+`isb` +
   `ICC_SGI1R_EL1` sysreg）；stop_machine 状态机两版逐行同构；completion 两版都持同一把 `wait.lock`。
5. **修正上一报告**（§3.3）：实际 mainline `.config` 中 **`CONFIG_ARM64_ERRATUM_4118414` 和
   `1530923` 都是 "is not set"**，与原厂 5.4 的 workaround 覆盖完全相同（都是 A76-only 的
   1165522/1286807）。`REPORT-stock-vs-mainline-completion.md §7` 关于"mainline 首次启用
   repeat-TLBI/speculative-AT"的假设**不成立**，其 §8 建议 1（关 4118414/1530923 做单变量）无从谈起。
6. **探针观测自洽性提醒**（§4.3 末）：当前探针代码里 `t508n_sm2_done = true` 位于 fn 入口、
   8 核都会执行；SM#2 的 complete 必然由最后一个执行完 fn 的 stopper 核调用（同核先写后读）。
   **同核自写自读在相干内存上不可能读到 false**——因此"waker 读 false"要么是发生在该核
   **进入 fn 之前**的另一次 swake_up_locked，要么需要主会话复核探针时序/写入位置。

---

## 1. 任务 1：原厂跨核置位/唤醒点的 barrier 结构（核心）

### 1.1 `all_alternatives_applied` 置位（CPU0 分支）

引用 `REPORT-stock-sync-mechanism.md`（已反汇编）：`__apply_alternatives`（`ffffffc010114cd8`）尾部
`dsb ish; [ic ialluis(条件)]; dsb ish; isb`（0x...14e94/14ea8/14eac），返回后 CPU0 分支
`mov w8,#1; str w8,[x19,#2584]`（0x...14c9c/14ca0，**普通 store，无 release、无 dcache clean**）。
二级核：`yield` + 普通 `ldr` 轮询 + 观察到 1 后 `isb`。

- 置位前的 barrier：**有**（`dsb ish`，且它排在 `__apply_alternatives` 的 dc/ic 维护之后）。
- 置位本身：**普通 store**（无 stlr/原子）。
- 读者：**轮询**（yield 循环容忍传播时延），非单次读。
- 对照 mainline 6.12 上游（本次抓取 v6.12.103 原文件）：`__apply_alternatives_multi_stop` 结构相同
  ——二级核 `while (!all_alternatives_applied) cpu_relax(); isb();`，CPU0 `__apply_alternatives(...)` 后
  `all_alternatives_applied = 1`（注释 "Barriers provided by the cache flushing"，即同样依赖
  `__apply_alternatives` 内部的 dsb）。**两版协议同构**。

### 1.2 TIF_NEED_RESCHED 跨核置位（resched_curr，`ffffffc0101f3b30`）

本次反汇编（与 `REPORT-stock-wakeup-chain-analysis.md §2` 一致，此处给出完整指令序列）：

```
1f3b40: ldr x9,[x8]; tbnz w9,#1, ret        ; test_tsk_need_resched
1f3b5c: cmp w9,w10; b.ne 1f3b80             ; 跨核 → 1f3b80
1f3b88: mov w10,#2; stset x10,[x8]          ; ★ 跨核分支: LSE 原子置 TIF_NEED_RESCHED (bit1)
1f3b90-1f3ba8: cpumask_of(cpu) 计算
1f3bac: mov w1,wzr                          ; IPI_RESCHEDULE=0
1f3bb0: bl 0x1169b4                         ; smp_cross_call(cpumask, 0)
```

- `stset`（LSE 原子位 RMW）与 LL/SC 替代副本（0x1f3bd0/0x1f3be8 `ldxr/orr/stxr`）同址。
- **`stset` 与 SGI 之间没有任何 dmb/dsb**——跨核可见性由**原子性 + GIC 驱动的 `dmb ishst`** 兜底（§1.5）。

### 1.3 stop_machine 的 `msdata->state` 置位（set_state / ack_state）

5.4.210 源码（`stock-5.4.210/kernel/stop_machine.c:165-179`）：

```c
struct multi_stop_data { fn@0; data@8; num_threads@16; active_cpus@24;
                         state@32; atomic_t thread_ack@36; };
static void set_state(struct multi_stop_data *msdata, enum multi_stop_state newstate)
{
	atomic_set(&msdata->thread_ack, msdata->num_threads);  /* = 普通 str（atomic_set 就是 WRITE_ONCE）*/
	smp_wmb();                                             /* arm64 = dmb ishst */
	WRITE_ONCE(msdata->state, newstate);                    /* = 普通 str */
}
static void ack_state(struct multi_stop_data *msdata)
{
	if (atomic_dec_and_test(&msdata->thread_ack))           /* ldaddal / ldxr-stlxr + dmb ish */
		set_state(msdata, msdata->state + 1);
}
```

本次反汇编 `multi_cpu_stop`（`ffffffc01030a420`）逐条对上：

```
30a4f0: ldr w27,[x19,#32]        ; 读者: READ_ONCE(state) —— 普通 ldr
30a4ec: bl 0x30ac68              ; stop_machine_yield() = yield; ret （纯 yield，无 wfe/sev/dsb）
30a55c-30a588 (LSE 路径):
  ldaddal w8,w9,[x24]            ; atomic_dec_and_test(thread_ack)  ← acquire+release 原子
  cbnz w8, loop                  ; 非 0 → 继续轮询
  ldr w8,[x19,#16]               ; num_threads
  ldr w9,[x19,#32]               ; state
  str w8,[x19,#36]               ; atomic_set(thread_ack, num_threads) —— 普通 str
  dmb ishst                      ; ★ smp_wmb()
  add w8,w9,#1; str w8,[x19,#32] ; WRITE_ONCE(state, state+1) —— 普通 str
30a5f0-30a608 (LL/SC 路径): ldxr/stlxr + dmb ish
```

- 结论：state 是**"前置 store-barrier（dmb ishst）的普通 store"** + **yield 轮询读者**，
  状态推进由**原子 ack 计数器**（`ldaddal`，release 语义）同步。**全程无锁、无 wfe/sev**。
- mainline 6.12 `kernel/stop_machine.c`（本次核对 165-199 行）**逐行同构**（`atomic_set`+`smp_wmb`+
  `WRITE_ONCE`；`atomic_dec_and_test`）。两版一致。

### 1.4 completion 的 `done++`（complete，`ffffffc010227a70`）

本次反汇编：

```
227a90-227aa8: local_irq_save/disable
227ad4: casa w8, w2, [x19]       ; ★ raw_spin_lock(&x->wait.lock)  LSE acquire CAS
227ae4: ldr w8,[x20]             ; done
227af4: str w8,[x20]             ; ★ done++ —— 普通 store，但在锁内
227af8+: __wake_up_common 内联    ; wait_queue 遍历, blr entry->func (autoremove_wake_function)
227b90: stlrb w8,[x19]           ; ★ raw_spin_unlock —— release 语义
227c18-227c4c: (LL/SC 锁副本) ldaxr/eor/stxr
```

waiter 侧 `wait_for_common`（`ffffffc0116a0610`）：

```
6a0680: casa w8, w2, [x20]       ; 同一把 wait.lock（x20 = x0+8）
6a0690: ldr w8,[x21]             ; ★ done 读也在锁内（x21 = x0 = &done）
```

- 跨核可见性由**自旋锁 release/acquire（stlrb / casa）**保证：`done++` 在锁内，waiter 持同锁读 done。
- LL/SC 锁副本（0x6a0610 区 ldaxr/stxr）同理。**无任何"锁外读/写 done"的路径**。

### 1.5 其他跨核通信点（快速核对）

| 点 | 原厂实现 | 模式 |
|---|---|---|
| `done->nr_todo`（cpu_stop_signal_done） | `ldaddal`（0x30a564）/ `ldxr-stlxr`+`dmb ish`（0x30a5f4/604） | 原子 RMW + barrier |
| `thread_ack`（ack_state） | `ldaddal` / `ldxr-stlxr`+`dmb ish` | 原子 RMW + barrier |
| `stop_cpus_mutex` | `casa`/`casl`，LL/SC `ldaxr/stxr`（0x30a660/724/7b0） | 锁 |
| stopper `works` 链表 | `list_add_tail` 在 `stopper->lock` 内 + `wake_q_add` | 锁 |
| SGI 发送（`gic_ipi_send_mask` `ffffffc0109aa6bc`） | **0x9aa770: `dmb ishst`** → 0x9aa788: `str w8,[x9]`（GICR_SGI1R，内存映射写） | **store barrier 在 SGI 写前** |
| IPI 接收（`ipi_handler` `ffffffc010116cac`） | jump table 分发，无额外 barrier（靠 IPI 投递序） | — |
| `rq->nr_running` | rq 锁内 `+=`（两版同） | 锁 |
| `stop_machine_yield`（0x30ac68） | `yield; ret`（无 wfe/sev） | — |

### 1.6 任务 1 结论

> **原厂跨核写全部是"原子 RMW / 持锁 / 前置 dmb-dsb + 轮询"之一；没有裸普通 store 被另一核单次读。**
> 原厂唯一两处"普通 store 跨核"（alternatives flag、`msdata->state`）都满足：
> （a）写侧前面有 `dsb ish` / `dmb ishst`；（b）读者是 yield 轮询（容忍传播时延），不是单次读。

---

## 2. 任务 2：原厂 complete 的 barrier 链 vs mainline swait

### 2.1 原厂（waitqueue）链

```
complete():   casa(acquire, &x->wait.lock) → done++(普通str) → __wake_up_common(wq, TASK_NORMAL,1)
              → default/autoremove_wake_function → try_to_wake_up → stlrb(release, &x->wait.lock)
waiter:       wait_for_common(): casa(acquire, &x->wait.lock) → ldr done → (0→prepare_to_wait+schedule)
              → 醒来后持锁再读 done
```

### 2.2 mainline（swait）链（`kernel/sched/completion.c` + `swait.c`，本次读取）

```
complete_with_flags(): raw_spin_lock_irqsave(&x->wait.lock) → x->done++ → swake_up_locked(&x->wait)
              → try_to_wake_up(队头) → list_del → raw_spin_unlock_irqrestore(&x->wait.lock)
waiter:       __wait_for_common(): raw_spin_lock_irq(&x->wait.lock) → do_wait_for_common:
              if(!x->done){ __prepare_to_swait; __set_current_state; unlock; schedule; lock }
              → while (!x->done && timeout)   ← 读 done 也在锁内
```

### 2.3 对比结论

- **两边 `done` 的读写都发生在同一把 `wait.lock` 临界区内**，release/acquire（arm64 `stlrb`/`casa`，
  或 `ldaxr/stxr`）就是可见性屏障。barrier 结构**等价**。
- **没有哪一边在锁外读写 done / 队列**。mainline 的 `swake_up_locked` 里唯一的"锁外"访问是
  t508n 探针对 `t508n_sm2_done` 的读（§4.3）——那是探针自己加的，与 swait 语义无关。
- 差异仅是 waitqueue→swait 的队列结构（`list` 无自旋、`task_list` 有锁），唤醒收敛点都是
  `try_to_wake_up`（引用 completion 报告 §1/§2/§6）。**completion 原语不是可见性差异点。**

---

## 3. 任务 3：T770 跨 cluster 可见性的事实确认

### 3.1 无 CCI/CCN/CMN/DSU / 无 cluster 特判

引用 `REPORT-stock-sync-mechanism.md` 任务 3：stock_kallsyms.map 无 `cci_*/ccn_*/cmn_*/dsu_*/scu_*`
（仅 USB/SCSI 无关符号）；SML 无 snoop/coherency 初始化字符串；核心 cache 维护（`__clean_dcache_area_poc`
`dc cvac`+`dsb sy`、`__flush_dcache_area` `dc civac`+`dsb sy`、`__clean_dcache_area_pou` `dc cvau`+`dsb ish`）
全部是标准 ARMv8 序列，**无 A55 cluster 特判**。本次未发现反例。

### 3.2 SGI 发送路径：两版都有 store barrier（但实现寄存器不同）

- **原厂**（本次反汇编 0x9aa770-0x9aa788）：`dmb ishst` → `str w8,[x9]`，x9 = per-cpu 指针 + 0xf00，
  SGI 经**内存映射寄存器**写入（偏移 0xf00，属 GIC Redistributor/Distributor 的 SGI 寄存器区，
  与上游用 `write_sysreg(ICC_SGI1R_EL1)` 不同——Unisoc/原厂驱动定制，具体寄存器名未做寄存器级
  核验，不影响结论）；该函数路径无尾部 `isb`，但 **barrier 在 SGI 写之前**，语义正确。
- **mainline**（本次读取 `irq-gic-v3.c:1428-1449`）：`dsb(ishst)` → `for_each_cpu: gic_send_sgi()
  → gic_write_sgi1r()`（`ICC_SGI1R_EL1` sysreg）→ 尾部 `isb()`。注释明示 "Ensure that stores to
  Normal memory are visible to the other CPUs before issuing the IPI"。
- 结论：**两版在"发 IPI 前先把普通 store 发布到 inner-shareable 域"上完全一致**（stock `dmb ishst`
  vs mainline `dsb ishst`；dsb 更强但同效）。这是 `resched_curr` 的 `stset` / wakelist 的
  `ttwu_pending=1` 等普通/原子 store 能被目标核看到的架构依据。

### 3.3 errata 覆盖：两版实际相同（修正 REPORT-stock-vs-mainline-completion.md §7）

本次核对两个配置文件（`work/kernel.config` vs `linux-6.12.103/.config`）：

| workaround | stock 5.4 | mainline 6.12.103（实际 .config） |
|---|---|---|
| 1286807 repeat-TLBI（A76 r0p0-r3p0） | y | y |
| 1165522 speculative-AT（A76 r0p0-r2p0） | y | y |
| 4118414 repeat-TLBI（全部 A76+...） | —（5.4 无此选项） | **# is not set** |
| 1530923 speculative-AT（A55 r0p0-r2p0） | —（5.4 无此选项） | **# is not set** |
| 1418040 / 1463225 / 1542419 | y / y / y | y / y / y |

> **修正**：`REPORT-stock-vs-mainline-completion.md §7.1` 声称 mainline 6.12 config 开启
> `CONFIG_ARM64_ERRATUM_4118414=y`、`1530923=y`——与实际 `.config` 不符（两处都是 "is not set"）。
> 因此其 §7.3 的"mainline 首次在 T770 上打 repeat-TLBI/speculative-AT"假设**不成立**，
> 其 §8 建议 1（关掉这两个选项做单变量）在当前 config 下是空操作。
> 两版在 T770 上实际打进的 errata patch 覆盖相同（仅 A76 r0p0-r3p0/r0p0-r2p0 两项）。

### 3.4 "普通 store 跨核"在 T770 上到底可不可用？

- **可用**：原厂 alternatives flag（普通 `str`，前置 `dsb ish`）和 `msdata->state`（普通 `str`，前置
  `dmb ishst`）都是跨 cluster 的普通 store，且原厂整套 stop_machine/调度/RCU/sysfs 都跑通
  （引用 sync 报告任务 3.3 / 5.3）。T770 的 PoC 一致性功能正常。
- **"不可靠"的准确含义**：对**无同步的普通 store → 另一核单次 load**，ARMv8 内存模型**不保证任何
  时间点的可见性**（可能很快、也可能很慢，取决于传播时延与缓存状态）。A55/A76 双簇的跨簇 snoop
  时延使该窗口可观（毫秒级/更多），表现为"有的核读到、有的核读不到"。这是**标准架构行为**，
  不是硬件相干性缺陷。
- **因此原厂的操作规则是"用同步换确定性"**：要么原子/锁（release-acquire 对），要么
  `dmb/dsb`+IPI，要么轮询（把"何时可见"变成"最终可见"）。

---

## 4. 任务 4：mainline 的裸普通 store 跨核点（对照）

### 4.1 结论先行

**mainline 6.12 内核自身没有"依赖普通 store 跨核即时可见"的地方**。逐点核对：

| mainline 候选点 | 写侧 | 读侧 | 保护 |
|---|---|---|---|
| `rq->ttwu_pending`（core.c:3851） | `WRITE_ONCE`（普通 store） | `sched_ttwu_pending`（core.c:3819 清 0） | 写后 `__smp_call_single_queue` → `llist_add`（原子）+ GIC `dsb ishst`+SGI —— **有序** |
| `all_alternatives_applied`（上游 6.12） | 普通 store | `while(...) cpu_relax()` 轮询 + `isb` | 前置 `__apply_alternatives` 内部 dsb + **轮询** |
| TIF_NEED_RESCHED（resched_curr） | `stset`（原子） | IPI 处理路径读 | 原子 + GIC `dsb ishst`+SGI（两版同，引用 wakeup-chain §2/§3） |
| `p->state`/`p->on_cpu` | 锁内 / `smp_cond_load_acquire` | 锁内 / `ldar` | 锁 / acquire |
| `rq->nr_running`（ttwu_queue_cond:3922 无锁读） | rq 锁内写 | **无锁读**（读侧！） | 读旧值只影响 wakelist 分支选择，语义仍正确（良性） |
| `cpu_online/active`、`nohz` 标志、`irq_work` | `set_bit`（原子）/ llist（原子） | — | 原子 |

### 4.2 与"原厂用原子/dsb 而 mainline 用裸 store"的对照

- SGI 路径：两版都 barrier（§3.2）。
- stop_machine：两版同构（§1.3）。
- completion：两版都持锁（§2）。
- **唯一裸普通 store 跨核 = 探针本身**，不在内核逻辑里。

### 4.3 探针 `t508n_sm2_done`（真正的问题点）

- 写：`arch/arm64/kernel/alternative.c:237` `t508n_sm2_done = true;` —— **volatile 普通 store，
  无 dsb/dmb/原子/锁**；位于 `__apply_alternatives_multi_stop` fn 入口、`if (cpu)` 之前，
  **8 个核都会执行**（符号为全局 BSS `bool`）。
- 读：`kernel/sched/swait.c:30/33/37`（swake_up_locked）、`kernel/sched/core.c:3797/3985`
  （sched_ttwu_pending/ttwu_queue）、`arch/arm64/kernel/smp.c:961` —— 全部为 **volatile 单次读**。
- 行为：CPU0（A55）先写，waker 核单次读 → 可能读到 false（store 尚未传播到该核簇）；
  另一些核（同簇或更晚读）→ true。**与"无同步 store 的传播时延"完全自洽**。
- **自洽性提醒（需主会话复核）**：既然 8 核都在 fn 入口写过该 flag，而 SM#2 的 `complete()` 必然由
  **最后一个执行完 fn 的 stopper 核**调用（`cpu_stop_signal_done` 同核 `atomic_dec_and_test` 归零才
  complete），那么该核在 `swake_up_locked` 里读的是**自己早先的写**——相干内存下同核自写自读
  **不可能返回 false**。因此"waker 读 false"只能是下面之一：
  1. 该次 swake_up_locked 不是 SM#2 complete，而是**该核进入 fn 之前**的另一次唤醒（y=355 行
     采样到的是 boot 早期的其他 completion）；
  2. 或实际刷机镜像的探针写入位置/时机与当前树不同（如只在 CPU0 写）；
  3. 或主会话对"waker 核"的判定有误。
  建议：把 y=355 x=8 的事件与各核 y=370（进入 fn）逐核比对——若某核 y=370 已亮而 y=355 x=8
  不亮，则矛盾成立（不可能）；若 y=370 未亮，则该读发生在 fn 之前，观测与跨簇传播无关。

---

## 5. 任务 5：综合

### 5.1 原厂跨核通信的操作模式（选用规则）

1. **需要排他/计数 → 原子 RMW**（LSE `ldaddal`/`stset`/`casa`，LL/SC `ldxr-stlxr`/`ldaxr`；必要时
   尾部 `dmb ish`/`dmb ishst`）：`thread_ack`、`nr_todo`、自旋锁、`stop_cpus_mutex`、TIF 位。
2. **共享数据 → 持锁临界区**（锁的 release/acquire 承担全部可见性）：completion `done++`、
   `rq->nr_running`、stopper works 链表。
3. **一次性状态翻转 + 不关心精确时刻 → 前置 store barrier + 普通 store + 轮询**：`msdata->state`
   （`dmb ishst`）、`all_alternatives_applied`（`dsb ish`）；读者 `yield` 轮询 + 必要处 `isb`。
4. **唤醒远核 → 原子/普通置位 + IPI**，可见性由 **GIC 驱动在 SGI 写前的 `dmb/dsb ishst`** 兜底。
5. **从不**：裸普通 store 后由另一核**单次读**并依赖其值。

### 5.2 mainline 与之一致的程度

**一致**。SGI barrier（§3.2）、stop_machine 状态机（§1.3）、completion 锁（§2）、resched_curr
`stset`+IPI（§1.2）逐点同构。mainline 内核逻辑里没有"裸普通 store 单次读"的跨核点（§4.1）。
**不一致的只有探针 instrumentation 本身**（§4.3）。

### 5.3 对探针门控的建议（按优先级）

1. **首选（最小改动，正确性优先）**：写侧改 release：
   `smp_store_release(&t508n_sm2_done, true);`（arm64 编译为 `stlr`）；
   读侧 `smp_load_acquire(&t508n_sm2_done)`（`ldar`）或至少保持 `READ_ONCE`。
   这样"读到 true ⇒ 写已发布"，探针语义可当可靠观测用。
2. **次选（原厂式，保持普通 store 但加 barrier）**：写侧
   `WRITE_ONCE(t508n_sm2_done, true); dsb(ish);`（原厂 alternatives flag 就是 `dsb ish` 后普通 store），
   读侧 `READ_ONCE`。注意这仍只保证"**最终**可见"，单次读仍可能旧——所以读侧要轮询或接受
   "false = 尚未发布"语义。
3. **若目的就是验证"T770 跨簇普通 store 传播时延"**：把写改成**仅 CPU0** 且写后 `dsb(ish)`，
   读侧在目标核轮询并记录首次看到 true 的耗时——这才是干净的单变量实验；当前"8 核都写 +
   单次读"的探针既不能证明也不能证伪跨簇传播问题（§4.3 自洽性提醒）。
4. **不要**：用 `dcache_clean/inval_poc` 处理这个 flag（引用 sync 报告任务 5：原厂证明普通
   cacheable + PoC 一致性足够，问题在同步模式不在 cache 维护）。
5. **顺带**：`REPORT-stock-vs-mainline-completion.md §8` 的 errata 单变量建议（关 4118414/1530923）
   按实际 `.config` 是空操作（两版都没开），不要再按该报告 §7 追这条线（§3.3）。

---

## 附：本次新增反汇编证据索引

| 符号 | 地址 | 证据点 |
|---|---|---|
| `complete` | `ffffffc010227a70` | 227ad4 `casa` 抢锁；227af4 done++（锁内普通 str）；227b90 `stlrb` 释放；227c18 ldaxr/stxr 锁副本 |
| `wait_for_common` | `ffffffc0116a0610` | 6a0680 `casa` 抢同锁；6a0690 `ldr done`（锁内读） |
| `resched_curr` | `ffffffc0101f3b30` | 1f3b8c `stset` 跨核置 TIF；1f3bb0 bl smp_cross_call；中间无 barrier |
| `multi_cpu_stop` | `ffffffc01030a420` | 30a4f0 `ldr state`（轮询读）；30a564 `ldaddal`；30a578-584 set_state（str+dmb ishst+str）；30a5f4/604 LL/SC+dmb ish |
| `stop_machine_yield` | `ffffffc01030ac68` | `yield; ret` |
| `__ipi_send_mask` | `ffffffc010278694` | 2786e8 blr chip->ipi_send_single |
| `gic_ipi_send_mask` | `ffffffc0109aa6bc` | **9aa770 `dmb ishst`**；9aa788 `str` SGI1R 内存映射写（per-cpu 指针 +0xf00） |
| `ipi_handler` | `ffffffc010116cac` | jump table 分发，接收侧无额外 barrier |
| mainline 上游 `alternative.c` | v6.12.103 原文件 | `__apply_alternatives_multi_stop` 与 stock 同构（"Barriers provided by the cache flushing"） |
| mainline `irq-gic-v3.c:1428` | 6.12.103 | `dsb(ishst)` → ICC_SGI1R_EL1 → `isb()` |

## 附：配置证据

- `work/kernel.config`（stock 5.4.210）：1418040/1165522/1286807/1463225/1542419 = y；无 4118414/1530923 选项。
- `linux-6.12.103/.config`：`# CONFIG_ARM64_ERRATUM_1530923 is not set`、`# CONFIG_ARM64_ERRATUM_4118414 is not set`、
  `# CONFIG_ARM64_ERRATUM_2441007 is not set`；1165522/1286807 = y。
