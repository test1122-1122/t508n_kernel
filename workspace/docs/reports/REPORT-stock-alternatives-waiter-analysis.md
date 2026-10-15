# T508N：原厂 5.4 alternatives 与 waiter 唤醒交互深度分析（stock vs mainline 6.12 探针构建）

> 任务来源：`prompt-stock-alternatives-waiter.md`。本报告聚焦任务增量：
> **① 原厂 wait_for_completion/complete 精确序列；② 原厂 alternatives 规模；③ patch 期间核状态与 ic ialluis 后措施；④ mainline 12K 条替代点中落在调度/唤醒代码的数量；⑤ 综合归因**。
> 已覆盖内容（唤醒链逐环节等价、completion 原语、multi_stop 协议）直接引用
> `REPORT-stock-wakeup-chain-analysis.md` / `REPORT-stock-vs-mainline-completion.md` / `REPORT-stock-sync-mechanism.md`，不重复。
>
> 符号表：`work/stock_kallsyms.map`（原厂）与 **`linux-6.12.103/System.map`**（mainline 探针构建自身，Aug 17 13:22，
> 与 `work/mainline_Image` 同构建；`work/System.map` 为另一构建，**不可用**）。
> 反汇编：`aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=<base> work/stock_Image_full.bin`（stock base=0xffffffc010080000，mainline base=0xffffffc080000000，fileoff=vaddr−base）。

---

## 0. 结论摘要（TL;DR）

1. **原厂 alternatives 规模 ≈ 77,662 条（__alt_instructions 0x121cd988→0x122b11f0，0xE3868 字节），是 mainline 探针构建（30,477 槽 / 29,649 有效）的 2.5 倍以上**。其中最大类别是
   **cap 42（PAN/uaccess 类：`msr pan,#1` 替换 `mrs x8,daif` 序列）57,220 条**，其次 LSE（cap 11）11,331 条。
   **“mainline 12,000+ 条 patch 比原厂多、更容易出事”的前提不成立——原厂打得更多、照样能跑。**
2. **原厂 waiter 侧（wait_for_common @ 0x6a0610）与 waker 侧（complete @ 0x227a70）序列逐行钉死**：
   入队（wait.lock 持锁下 list_add_tail，WQ_FLAG_EXCLUSIVE）→ 置 __state（task+40，持锁下）→ 解锁 → schedule_timeout →
   唤醒后持锁重查 done → **waiter 自摘链**；waker 只 `done++ + __wake_up_common 唤醒 1 个 exclusive`，**不摘链**。
   与 mainline 6.12 swait 版（入队/置态/自摘顺序相同，唯一差异=**waker 在 swake_up_locked 内立即摘链**）——沿用 H3 结论：原语不是根因。
3. **patch 期间核状态与 cache 维护两边完全一致**：都经 stop_machine（multi_cpu_stop 的 `MULTI_STOP_DISABLE_IRQ` 状态
   `local_irq_disable()+hard_irq_disable()`，8 核关中断跑 fn）、二级核 `yield` 轮询 + `isb`、逐站点 `dc civac`、
   尾部 `dsb ish; ic ialluis; dsb ish; isb`（stock 0x114e68/114e98-114eac；mainline 0x209xx）。**原厂 ic ialluis 之后没有任何额外措施。**
4. **落在核心唤醒/调度函数内的替代点：原厂 320 条（complete 6、wait_for_common 8、try_to_wake_up 11、__schedule 16、
   do_idle 28、swake_up*/prepare_to_wait* ~38、resched_curr 1、scheduler_ipi 6、cpu_stopper_thread 8、multi_cpu_stop 5、stop_machine 4 等，
   绝大多数 cap42 PAN + 少量 LSE）；mainline 仅 47 条**（__schedule 18、resched_curr 2、try_to_wake_up 1、ttwu_do_activate 1、
   wake_up_new_task 1、do_idle 1、multi_cpu_stop 1、cpu_stop_signal_done 1 等），**mainline 的 complete/wait_for_completion/swait 内部替代点为 0**。
   “mainline 12K 更可能碰到调度器/唤醒代码”在数字上不成立（47 << 320）。
5. **对当前卡点（complete 被调 y=450 但 pid==1 永不 ttwu）的归因更新**：
   - 先前报告的“errata 覆盖差异”头号假设（4118414/1530923）**已被探针构建的 .config（Aug 17 12:44）证伪**：
     两者均为 `# CONFIG_ARM64_ERRATUM_4118414 is not set` / `# CONFIG_ARM64_ERRATUM_1530923 is not set`，
     与 stock 相同只开 1286807/1165522。表内 cap98（repeat-TLBI）=97 条、cap99（spec-AT）=24 条只是静态存在，是否 apply 取决于 A76 修订。
   - **新发现的最显著机制差异 = mainline LSE 打补方式**：6.12 原子操作改为 `alternative_has_cap_likely()`（asm-goto
     `b l_no` → **NOP 掉分支、执行流落入紧随其后的内联 LSE 序列**，经 `alt_cb_patch_nops` 回调，10,712 条）+
     经典替换（880 条）；原厂 5.4 是纯经典替换（11,331 条，memcpy 式覆盖）。这是唯一与“唤醒失效”相关的机制级差异候选。
   - 结合探针事实（SM#2 期间调度器活着、别的任务正常 ttwu），最一致的解释仍是 **CPU0 侧（kernel_init 所在核）在 8 核 patch
     风暴后执行了半新半旧指令，破坏中断返回/调度路径或 kernel_init 的 swait 队列项/__state**（与唤醒链报告的 R1 一致）；
     mainline 的 NOP-落入式 LSE 几何（分支原地变 NOP、LSE 序列紧跟其后）比原厂的 memcpy 覆盖更贴近“半新半旧”敏感面，是首选单变量。

---

## 1. 任务 1：原厂 wait_for_completion 完整反汇编（0xffffffc0116a0d30）

### 1.1 `wait_for_completion`（0x6a0d30）= 薄封装

```
6a0d30: str x30,[x18],#8            ; ftrace 入口
6a0d34: stp x29,x30,[sp,#-16]!
6a0d3c: mov x1, #0x7fffffffffffffff ; timeout = MAX_SCHEDULE_TIMEOUT
6a0d40: mov w2, #2                  ; state = TASK_UNINTERRUPTIBLE
6a0d44: bl  0xffffffc0116a0610      ; → wait_for_common()
6a0d50: ret
```

### 1.2 waiter 核心 `wait_for_common`（0x6a0610，do_wait_for_common 内联其中）

```
610:  sub sp,#0x90; ...; mrs x24, sp_el0          ; current
64c:  msr daifset,#2; [x24,#24]++                 ; local_irq_disable + preempt_disable
670:  casa w8,w2,[x20]                            ; ★ raw_spin_lock(&x->wait.lock)（LSE CAS 版；LL/SC 版在 6a6ce4）
68c:  cbnz → 8ec → bl 0x2452a8                    ;   争用慢路径
690:  ldr w8,[x21]        ; x21=&x->done
694:  cbnz w8 → 818       ; if (done) 直接走 done-- 路径（不排队）
; ---- 入队（持锁下） ----
698-6b0: 在栈上构造 wait_queue_entry{flags=1(WQ_FLAG_EXCLUSIVE), private=current,
         func=0xffffffc0116ce9ac(KCFI 桩→default_wake_function)}
6b4-6e8: list_add_tail(&entry, &x->wait.head)     ; ★ 入队（在 state 设置之前、持锁）
; ---- 信号检查（TASK_UNINTERRUPTIBLE 恒跳过）+ 置态 ----
6fc:  and x27, x23, #0x101          ; state & (INTERRUPTIBLE|WAKEKILL) == 0 → 跳过
71c:  str x23, [x24, #40]           ; ★ __set_current_state(state)（task+40，仍在持锁下）
720:  stlrb w8,[x20]; daifclr #2     ; unlock wait.lock + local_irq_enable
730-744: preempt_count-- ；可抢占则 79c-7d8: preempt_schedule 内联（__schedule(true)）
748:  bl 0x6a59ac                    ; ★ schedule_timeout(timeout) → schedule() 睡入
750-760: daifset#2 + preempt_count++ （为重新取锁做准备）
7dc-6a6ce4: raw_spin_lock(&x->wait.lock)（LL/SC 副本 ldaxr/eor/stxr）
78c:  cbz x19 → 7fc                    ; timeout==0 → 摘链
790:  ldr w8,[x21]                     ; 重查 done
794:  cbz w8 → 704                     ; ★ done==0 && timeout!=0 → 回到循环：置态→解锁→再睡
798:  → 7fc                            ; done!=0 → 摘链返回
7fc:  bl 0x952ab0 (list_del)           ; ★ waiter 自摘链（持锁下）
810:  ldr w8,[x21]; cbz → 830
818:  cmn w8,#1; b.eq 828; sub; str     ; done--（complete_all 的 UINT_MAX 除外）
828:  cmp x19,#0; csinc ...            ; return timeout ?: 1
830-83c: unlock + local_irq_restore
844+: epilogue（可抢占则 preempt_schedule）
```

**waiter 精确序列结论**：
1. 持 wait.lock（IRQ 关）→ 查 done → **入队**（list_add_tail，exclusive）→ **置 __state** → 解锁 → 睡。
2. 入队与置态的**顺序：先入队、后置态，且都在同一把锁内**（对应 waker 取同一把锁 → 不可能出现“waker 看到空队列/未睡态”丢唤醒）。
3. 唤醒后：重取锁 → 重查 done → 若 done==0 且 timeout 未到 → **重新置态再睡**（状态更新在持锁下，无窗口）。
4. **摘链时机 = waiter 自己被唤醒后、持锁自摘**（`__remove_wait_queue`），**不是 waker 摘**。

### 1.3 对照 mainline 6.12（kernel/sched/completion.c:80-116 `do_wait_for_common`，swait）

```c
if (!x->done) {
    DECLARE_SWAITQUEUE(wait);
    do {
        if (signal_pending_state(state, current)) { timeout = -ERESTARTSYS; break; }
        __prepare_to_swait(&x->wait, &wait);   /* 入队（持锁下） */
        __set_current_state(state);            /* 置态（持锁下） */
        raw_spin_unlock_irq(&x->wait.lock);
        timeout = action(timeout);             /* schedule_timeout */
        raw_spin_lock_irq(&x->wait.lock);
    } while (!x->done && timeout);
    __finish_swait(&x->wait, &wait);           /* waiter 自摘链（持锁下） */
    ...
}
```

**差异结论**：入队→置态→睡→重查→自摘的顺序**逐行相同**（先入队后置态、都在锁内、waiter 自摘）。
唯一结构差异：stock=传统 waitqueue（func 间接调用），mainline=swait；**不构成丢唤醒点（H3 已排除，直接引用）**。

---

## 2. 任务 2：原厂 complete 完整反汇编（0xffffffc010227a70）

```
7a90: mrs x22, daif; and w8, w22, #0x80; cbnz → 7aac     ; local_save_flags
7aa8: msr daifset, #2                                      ; local_irq_disable（未关时）
7ab0-7ab8: [current,#24]++                                  ; preempt_disable
7ac4-7adc: casa w8,w2,[x19]                                 ; ★ raw_spin_lock(&x->wait.lock)（x19=&x+8）
7ae4: ldr w8,[x20]                                          ; x->done
7ae8: cmn w8,#1; b.eq 7af8; add w8,#1; str                ; ★ if (done != UINT_MAX) done++
7af8: ldr x8,[x20,#16]!; cmp x20,x8; b.eq 7b90            ; list_empty → 直接返回
7b1c-7b4c: 遍历 waitqueue：
   ldr w25,[x0,#-24]     ; entry->flags（WQ_FLAG_WOKEN=bit2 跳过）
   ldur x8,[x8,#-8]      ; entry->func（KCFI 校验：0x6ce9a0 表内 → 直接 blr）
   mov w1,#3(TASK_NORMAL); mov w2,#0; mov x3,#0
   blr x8                ; ★ func(entry, TASK_NORMAL, 0, NULL)
                          ;  → default_wake_function（0x1fa220，经 KCFI 桩 0x6ce9ac）→ try_to_wake_up
7b50: tbnz w0,#31 → 7b90 ; ret<0 → 停
7b54: tbz w25,#0 → 7b10  ; 非 exclusive → 继续
7b58: cbz w0 → 7b10      ; exclusive 但唤醒失败 → 继续
7b5c: b 7b90             ; ★ exclusive 且成功 → 停（nr_exclusive=1 语义）
7b90: stlrb w8,[x19]; msr daif,x22                        ; unlock + local_irq_restore
```

**waker 侧结论**：
1. `done++` 与唤醒遍历在同一把 wait.lock 内；exclusive=1 语义 = 唤醒**第一个成功唤醒的 exclusive waiter 后停止**（7b58/7b5c）。
2. **waker 不摘链**——waiter 保持队列中直到自己醒来自摘（§1.2 的 7fc）。
3. 唤醒调 `func`（默认 `default_wake_function`）→ `try_to_wake_up`；KCFI 桩表 0x6ce9a0 内 `b 0x1fa220` 即 default_wake_function（0x6ce9ac）。

### 对照 mainline 6.12 swake_up_locked（kernel/sched/swait.c:21-31，实际代码）

```c
void swake_up_locked(struct swait_queue_head *q, int wake_flags)
{
	struct swait_queue *curr;

	if (list_empty(&q->task_list))
		return;                                        /* ★ 队列空 → 静默返回 */
	curr = list_first_entry(&q->task_list, typeof(*curr), task_list);
	try_to_wake_up(curr->task, TASK_NORMAL, wake_flags); /* ★ 唤醒第一个 waiter */
	list_del_init(&curr->task_list);                     /* ★ 无条件摘链（唤醒失败也摘！） */
}
```

**差异结论**：
- 两版 waker 都收敛到 try_to_wake_up + 状态检查；**若 waiter 不在队列或 __state 异常，两版都会“静默失败”**（complete 返回、waiter 永远睡）。
  探针事实（y=450 亮、pid==1 ttwu 不亮、y=440 计数不亮）与该静默失败模式**完全自洽**：要么 swait 队列在 complete 时为空
  （kernel_init 没入队/队列项被破坏），要么 `try_to_wake_up` 因 `__state` 不匹配返回 0。
- **结构性差异（放大因素）**：stock 的 complete 唤醒后由 waiter 事后自摘，**唤醒失败时条目留在队列**（可被后续 complete 重试）；
  mainline 的 swake_up_locked **无条件 `list_del_init`**——一旦 `try_to_wake_up` 因状态异常返回 0，kernel_init 的队列项被
  立即摘除且 task 指针清零，**后续任何 complete 都再也找不到它**。一旦 kernel_init 的 __state/队列项被 patch 风暴波及，
  mainline 的丢失是**不可逆**的，与“y=440 永不返回”完全吻合。

---

## 3. 任务 3：原厂 alternatives 规模（核心对比点）

### 3.1 段边界与条数（从二进制解析 struct alt_instr=12B：s32 orig, s32 alt, u16 feat, u8 olen, u8 alen）

| 项 | 原厂 5.4 | mainline 6.12 探针构建 |
|---|---|---|
| `__alt_instructions` | `0xffffffc0121cd988` | `0xffffffc081ce7514` |
| `__alt_instructions_end` | `0xffffffc0122b11f0` | `0xffffffc081d409b0` |
| 段大小 | **0xE3868 = 931,432 B** | 0x5949C = 365,724 B |
| 槽位 | **77,662** | 30,477 |
| 有效条目（cap<102、len 自洽） | ~77,116 | 29,649 |

### 3.2 按 cpucap 分布（有效条目）

**原厂 5.4（77,662 槽）**：
| cap | 条数 | 判定 |
|---|---|---|
| **42（PAN/uaccess 类）** | **57,220** | 替换内容 `msr pan,#1`（0xd500419f），orig 为 `mrs x8,daif` 开头的 irq/uaccess 序列；遍布全 text（0x10081b14–0x1215dfe4）。Unisoc 定制编号，语义=uaccess-disable 的 HW-PAN 捷径（stock config：CONFIG_ARM64_PAN=y + CONFIG_ARM64_SW_TTBR0_PAN=y） |
| **11（LSE）** | **11,331** | 已验证（resched_curr 内 LSE stset 站点，cap 11） |
| 10 | 3,822 | errata/feature（待定，多为 TLBI/barrier 类） |
| 9 | 1,724 | errata（repeat-TLBI 类候选） |
| 1 | 1,433 | errata（DEVICE_LOAD_ACQUIRE 类候选） |
| 5 | 1,027 | errata（spec-AT 类候选） |
| 17/36/4/… | 205/207/43/… | UAO 及其它 |
| 其余 | <20 每 cap | 杂项 |

**mainline 6.12 探针构建（30,477 槽，29,649 有效）**：
| cap | 条数 | 名称（6.12 cpucap-defs.h） |
|---|---|---|
| 95 | 13,066 | ARM64_WORKAROUND_DEVICE_LOAD_ACQUIRE（站点=__memcpy_fromio 等 ldr→ldarb；**config 未开，不会 apply**） |
| **37 CB（bit15 置位）** | **10,712** | **LSE 运行时检查**：`alternative_has_cap_likely(ARM64_HAS_LSE_ATOMICS)` 的 asm-goto `b l_no`，经 `alt_cb_patch_nops` 回调 **NOP 掉分支、落入紧随的内联 LSE 序列** |
| 37 普通 | 880 | LSE 经典替换（ARM64_LSE_ATOMIC_INSN / cmpxchg / percpu 等） |
| 51 | 2,385 | ARM64_HAS_VIRT_HOST_EXTN（KVM） |
| 1 CB | 1,137 | ARM64_ALWAYS_SYSTEM 运行时检查（同 NOP-落入模式） |
| 39 | 558 | ARM64_HAS_NESTED_VIRT（KVM） |
| 54 | 139 | ARM64_KVM_HVHE |
| **98** | **97** | **repeat-TLBI（1286807，A76 r0p0-r3p0）** |
| **68** | **67** | **KPTI（ARM64_UNMAP_KERNEL_AT_EL0）** |
| **99** | **24** | **spec-AT（1165522，A76 r0p0-r2p0）** |
| 42/36/45/25CB/… | 54/52/50/50/… | S1POE/LPA2/SB/FPSIMD 等 |
| 合计 LSE | **11,592** | 与任务所称“LSE 12,074”同量级（±差异=计数口径/坏槽） |

### 3.3 结论：量级对比

- **原厂 77,662 条 > mainline 30,477 条（2.55×）**；原厂最大类别 cap42 一条就 57,220 条（> mainline 全部）。
- 若 T770 上 PAN 能力被探测到，原厂 SM#2 实际 apply 量可达 ~70K 条（LSE 11,331 + PAN 57,220 + errata），
  **约为 mainline 实际 apply 量（LSE 11,592 + KPTI 67 + repeat-TLBI 97 + spec-AT 24 ≈ 11,800）的 6 倍**。
- **“原厂 alternatives 规模小所以不卡”的直觉被证伪——原厂规模大一个数量级照样跑通 SM#2。**

### 3.4 errata 配置对照（重要更新：先前报告的头号假设已被证伪）

| workaround | stock config | mainline 探针构建 .config（Aug 17 12:44） |
|---|---|---|
| repeat-TLBI 1286807 | y | y |
| **repeat-TLBI 4118414** | 无 | **# CONFIG_ARM64_ERRATUM_4118414 is not set** |
| spec-AT 1165522 | y | y |
| **spec-AT 1530923** | 无 | **# CONFIG_ARM64_ERRATUM_1530923 is not set** |
| LSE | y | y |

→ `REPORT-stock-vs-mainline-completion.md §7` 的“mainline 独有启用 4118414/1530923 首次在中断/TLB 路径打补丁”假设
**在当前探针构建上不再成立**（两者已关）。表内 cap98=97/cap99=24 只是静态条目，是否 apply 取决于 A76 修订（r0-r3/r0-r2 才命中）。

---

## 4. 任务 4：patch 期间核状态与 I-cache 一致性

### 4.1 8 核状态（stock == mainline）

- 两边 SM#2 都走 `stop_machine`（stock `apply_alternatives_all`@0x1214e354 → stop_machine；mainline `apply_alternatives_all`@0x81c555b8）。
- `multi_cpu_stop`（stock 0x30a420 / mainline 0x16d6e8）状态机在 RUN 前有 **`MULTI_STOP_DISABLE_IRQ: local_irq_disable(); hard_irq_disable()`**
  （5.4.210 与 6.12 源码逐字相同，均含 hard_irq_disable = DAIF 全掩）。→ **patch 执行期间 8 核全部关中断（含 debug/SError）**，
  只有 CPU0 跑 `__apply_alternatives`，其余 7 核在状态机循环里 `stop_machine_yield()` 自旋。
- 原厂二级核在 `__apply_alternatives_multi_stop`（0x114c0c）内：`yield` 轮询 `all_alternatives_applied` 标志（普通 ldr，无 wfe/dc/dsb），
  见标志后 `isb` 再返回（REPORT-stock-sync-mechanism.md §1，直接引用）。
- mainline 探针构建二级核（0x81c55458）：`ldr w0,[x2,#16]; cbnz → 跳出; yield; ldr; cbz 循环; isb`——**同一模式**。

### 4.2 ic ialluis 与收尾（两边完全一致，无额外措施）

- **逐站点**：原厂 patch 循环内 `dc civac, x10`（0x114e68）；mainline `clean_dcache_range_nopatch()`（`dc civac` 循环，0x8106ed80 起）。
- **收尾**：原厂 `dsb ish; ic ialluis(条件, DIC=0 则执行); dsb ish; isb`（0x114e98-114eac）；
  mainline 探针构建 `dsb ish; nop(DIC 检查点); ic ialluis; dsb ish; isb`（0x2093c-2094c，已实测 DIC=0 真实执行）。
- 原厂之后置 `all_alternatives_applied=1`（普通 str，无 dcache clean、无 sev）；mainline 同（0x55558 `dsb ish` 后 str）。
- **原厂没有任何“ic ialluis 之后的额外 per-core isb/sync/barrier”**（二级核的 isb 在看见标志后执行，恰好在 ialluis 广播之后，顺序正确）。

### 4.3 结论

patch 期间的核状态（8 核关中断自旋）、I-cache 广播（ic ialluis）、D-cache（dc civac）、序列化（isb）
在 stock 与 mainline **逐环节相同**。不存在“原厂独有的恢复措施”。（与唤醒链报告 §6.1 的等价表一致。）

---

## 5. 任务 5：替代点与调度器/唤醒代码的交集

统计方法：把 alt 表每条 orig 地址映射到符号表函数区间（[sym_i, sym_{i+1})），
按“核心唤醒/调度函数名单”匹配（complete*/wait_for_*/do_wait_for_common/swake_up*/__wake_up*/try_to_wake_up/
wake_up_*/ttwu*/resched_curr/scheduler_ipi/schedule*/do_idle/cpu_startup_entry/preempt_schedule*/
cpu_stopper_thread/multi_cpu_stop/cpu_stop_signal_done/stop_machine*/smpboot_thread_fn/…）。

| 函数 | 原厂 5.4 命中（cap） | mainline 6.12 命中（cap） |
|---|---|---|
| `__schedule` | 16（LSE 4 + PAN 12） | **18**（LSE 13 + VHE 5） |
| `do_idle` | 28（LSE 10 + PAN 18） | 1（VHE） |
| `try_to_wake_up` | **11（全 PAN）** | **1（LSE）** |
| `complete` / `complete_all` / `complete_io` | 6+6+28 | **0** |
| `wait_for_common` / `wait_for_common_io` | 8+8 | 0（swait 版函数名不同，见下） |
| `swake_up_one/all` | 12+8 | 0（mainline 本体 0 命中） |
| `prepare_to_wait*/prepare_to_swait*` | 18+12 | 0 |
| `resched_curr` / `resched_cpu` | 1 / 7 | 2 / 0 |
| `scheduler_ipi` | 6（LSE） | 0 |
| `cpu_stopper_thread` | 8 | 0（函数名不同） |
| `multi_cpu_stop` / `cpu_stop_signal_done` | 5 / — | 1 / 1 |
| `stop_machine` / `stop_machine_cpuslocked` | 4 / 4 | — |
| `wake_up_new_task` | 10（PAN） | 1（LSE） |
| `ttwu_queue/ttwu_remote/ttwu_stat/ttwu_do_activate` | 3+4+1+0 | 0+0+0+1 |
| `sched_ttwu_pending` | 8 | 1（VHE） |
| `enqueue/dequeue_task_*` | 7 | 4 |
| `preempt_schedule*` | 8 | 0 |
| **合计（核心名单）** | **~320** | **~47** |

（另：mainline 的 complete/wait_for_completion/swait 相关函数在 System.map 中为
`complete`@0x80e3a88、`wait_for_completion`@0x810737d4、`swake_up_locked`@0x80e40ac 等——**命中均为 0**。）

**结论**：
- **原厂在唤醒/调度代码里的替代点（320）是 mainline（47）的 6.8 倍**；原厂甚至在 `complete`、`wait_for_common`、
  `try_to_wake_up`、`prepare_to_wait*` 内部就打了几十处 PAN 类补丁且安然无恙。
- **“mainline 12K 条替代点更可能‘碰到’调度器/唤醒代码”在数量上不成立**；两者真正差别的不是“命中多少”，而是
  **命中的“补丁内容与方式”**（§6）。

---

## 6. 任务 6：综合回答——原厂为什么能跑、mainline 为什么卡

### 6.1 逐项排除（本轮新增证据）

| 假设 | 结论 |
|---|---|
| 原厂 alternatives 规模小（任务前提） | **证伪**：原厂 77,662 条 > mainline 30,477 条；实际 apply 量原厂 ~6× |
| mainline 12K 更常命中调度/唤醒代码 | **证伪**：核心命中 47 << 原厂 320 |
| errata 覆盖差异（4118414/1530923 首启） | **证伪（当前构建）**：探针构建 .config 已关闭两者，与 stock 同口径 |
| 唤醒链代码差异（ttwu/resched/IPI/idle） | 已排除（唤醒链报告逐环节等价） |
| completion 原语差异（waitqueue vs swait） | 已排除（H3），仅“waker 摘链 vs waiter 自摘”为放大因素（§2） |
| patch 期间核状态 / cache 维护 / ic ialluis 后措施 | **无差异**（§4） |

### 6.2 仍成立的差异（按可疑度排序）

1. **【机制级，本轮新发现】LSE 打补方式**
   - 原厂 5.4：11,331 条全部经典替换（编译期 `ALTERNATIVE(llsc, lse, LSE)`，替换代码在 altinstr 池，
     `memcpy`/`patch_alternative` 覆盖 orig 站点；orig 与替换是“两个独立副本”）。
   - mainline 6.12：11,592 条中 **10,712 条是 `alternative_has_cap_likely()` 运行时检查**——补丁动作 =
     `alt_cb_patch_nops` 把 `b l_no` 原地 NOP，**执行流落入紧跟 NOP 之后的内联 LSE 序列**（同一条 cache line 内
     “NOP + LSE 指令”成为一体）；880 条才是经典替换。
   - 后果：mainline 补丁后，被改站点附近的“指令几何”与 stock 不同——分支变 NOP、落入路径在相邻字节。
     若 8 核 `ic ialluis` 广播在 T770 双簇（A76+A55）上存在任何不完备（DIC=0 时 ialluis 是 Inner-shareable 域广播），
     半新半旧的最坏形态（某核取到“NOP 已写、LSE 序列未变”或反之）对落入式比覆盖式更敏感。
2. **【既有，R1 保持】CPU0 中断返回/调度路径执行半新半旧指令**（唤醒链报告 §6.3 R1）：
   探针已把卡点压缩到「do_handle_IPI 收到 IPI_RESCHEDULE（x=188 亮）之后」；若 CPU0 的 scheduler_ipi 折返/
   idle need_resched 检查/__schedule 取到旧指令 → kernel_init 永不被调度，且 y=440 计数不亮。
   §5 显示 mainline 在 `__schedule`（18 条）与 `resched_curr`（2 条）内有 LSE 站点——正是 CPU0 中断返回后必经路径。
3. **【放大因素，本轮坐实】swait 的无条件摘链 + 静默失败**（§2 对照，6.12 实际代码）：
   `swake_up_locked` 在队列空时静默返回；队列非空时只唤醒第一个 waiter 并**无条件 `list_del_init`（唤醒失败也摘）**。
   一旦 kernel_init 的 swait 队列项/__state 被（1）（2）波及，其队列项被不可逆摘除 → 后续 complete 再也找不到它，
   与“y=440 永不返回 + pid==1 ttwu 不亮”全部探针事实吻合。stock 的 waitqueue 版（waiter 自摘、失败不摘）无此放大。

### 6.3 对卡点的最可能解释

complete()（y=450）确实执行，但 swake_up_locked 未对 pid==1 调 try_to_wake_up（pid==1 ttwu 探针不亮）
⟹ **SM#2 complete 时，kernel_init 的 swait 队列项不在队列中，或 wake_up_state() 因 __state 异常返回 0**。
两者都指向 **kernel_init 的 task/栈侧状态在 8 核 patch 风暴中被破坏**，最可能经由：
- CPU0 在 SM#2 收尾后（8 核 ic ialluis 边界）执行了半新半旧指令（尤其 `__schedule`/`resched_curr`/中断返回路径上的
  LSE NOP-落入站点），破坏调度器数据结构或上下文；
- 或某条 alt 记录 orig 越界导致 `lm_alias(origptr)` 写坏随机物理内存（表内 828 条“字段异常”记录值得逐一核验，见 6.4-4）。

### 6.4 可操作验证（按性价比）

1. **打印 SM#2 complete() 时刻的 swait 队列状态**（单次刷机）：
   在 `swake_up_locked` 入口 dump `q->task_list` 首项 task 的 pid/__state；若队列空 → 入队失败/项被摘；
   若非空但 `wake_up_state` 返回 0 → 打印 kernel_init->__state（应为 TASK_UNINTERRUPTIBLE=2）。
   这直接区分“没入队”与“状态坏”两条分支。
2. **单变量：把 6.12 LSE 改回经典替换**（临时把 `arch/arm64/include/asm/atomic*.h` 的
   `alternative_has_cap_likely(...) ? lse : llsc` 三元改回 5.4 式编译期 `ARM64_LSE_ATOMIC_INSN`，或把
   `alternative_has_cap_likely` 的 `ALTERNATIVE_CB("b %l[l_no]", …, alt_cb_patch_nops)` 改回
   `ALTERNATIVE("b %l[l_no]", "nop", …)` 经典形式）重建对照。若恢复 → 坐实 NOP-落入式 LSE 几何。
3. **读 CTR_EL0.DIC 画格 + 强制验证 ialluis 广播**：确认 DIC=0；在 `icache_inval_all_pou()` 处加
   per-cpu isb 兜底（第二级核返回前已有 isb，重点补 CPU0 侧返回路径）作对照。
4. **核验 alt 表 828 条异常记录**：逐一检查 orig 地址是否落在合法 text/trampoline 区间；
   对 orig 指向数据段的记录，确认 `lm_alias(origptr)` 不会写坏 task_struct/栈（这是“写坏 waiter 状态”的直接物证路径）。
5. 若 2 无效，再回到唤醒链报告 §6.4 的 do_idle/schedule_idle 断点探针方案（确认 CPU0 是否在 IPI 后进入调度）。

---

## 附：关键符号速查

| 符号 | 原厂 5.4 | mainline 6.12 探针构建 |
|---|---|---|
| `wait_for_completion` | `ffffffc0116a0d30` | `ffffffc0810737d4` |
| `wait_for_common` | `ffffffc0116a0610` | —（内联） |
| `complete` | `ffffffc010227a70` | `ffffffc0800e3a88` |
| `complete_all` | `ffffffc010227898` | `ffffffc0800e3b14` |
| `swake_up_locked` | `ffffffc010226d90` | `ffffffc0800e40ac` |
| `try_to_wake_up` | `ffffffc0101f6084` | `ffffffc0800c54cc` |
| `resched_curr` | `ffffffc0101f3b30` | `ffffffc0800c0c70` |
| `scheduler_ipi` | `ffffffc0101fc658` | inline |
| `__schedule` | `ffffffc01169e8e0` | `ffffffc081072008` |
| `do_idle` | `ffffffc01020493c` | `ffffffc0800d9444` |
| `cpu_stopper_thread` | `ffffffc01030adc8` | `ffffffc08016cfd8` |
| `multi_cpu_stop` | `ffffffc01030a420` | `ffffffc08016d6e8` |
| `cpu_stop_signal_done` | —（内联） | `ffffffc08016cf4c` |
| `stop_machine` | `ffffffc01030a2f0` | `ffffffc08016dc80` |
| `__alt_instructions` | `ffffffc0121cd988` | `ffffffc081ce7514` |
| `__alt_instructions_end` | `ffffffc0122b11f0` | `ffffffc081d409b0` |
| `__apply_alternatives` | `ffffffc010114cd8` | `ffffffc080020830` |
| `__apply_alternatives_multi_stop` | `ffffffc010114c0c` | `ffffffc081c55458` |
| `apply_alternatives_all` | `ffffffc01214e354` | `ffffffc081c555b8` |
| `alt_cb_patch_nops` | — | `ffffffc08106edac` |
| KCFI 桩表（wait_queue_func_t） | `ffffffc0116ce9a0`（+0xc = default_wake_function） | — |
