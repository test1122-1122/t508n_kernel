# T508N 卡点归因分析报告（基于工作区文档 + 纯源码分析）

> 生成：2026-08-17。范围：只读分析，不执行刷机/重建/改代码。
> 输入：2026-08-16 晚间实测读屏结果 + 工作区全部活动文档 + mainline 6.12.103 当前 git 状态。

---

## 1. 文档成果汇总表（来源 → 关键结论 → 是否已确认事实）

| 来源文件 | 关键结论 | 是否已确认 |
|---|---|---|
| `AGENTS-GUIDE.md` | 当前唯一目标=内核跑通到 `/init`；HEAD=`cb8e725f3`；stop_machine.c 于 09:04 改、boot_linux.img 于 08:57 构建（**不含最新 stop_machine 探针**）；红线：不用 `ic ialluis`、不碰外设、不怀疑 PoC cache | 已确认事实 |
| `HANDOFF-3-SESSIONS.md` | 探针坐标表：y=120(x=8 vdso后/x=80 CPU0 __apply_alternatives 返回/x=44 stop_machine 返回)；y=370 进 fn、y=390 fn 完成、y=410 fn 返回、y=430 signal_done 后、y=450 最后 complete、y=440 wait_for_completion 前/后 | 已确认事实（坐标） |
| `SESSION-CONTEXT-COMPACT.md` | stock 二级核等待=`ldr+yield` 纯轮询无 dcache/dsb/wfe/sev；stock 无 CCI/CMN/DSU 初始化；mainline 曾整体跳过 `apply_alternatives_all()`（12,074 LSE + 99 repeat-TLBI + 24 speculative-AT 未 patch）是最可疑根因 | 已确认事实 |
| `REPORT-stock-sync-mechanism.md` | stock `__apply_alternatives_multi_stop` 二级核纯 `yield` 轮询普通 `ldr`；CPU0 普通 `str all_alternatives_applied=1`，唯一屏障来自 `__apply_alternatives` 尾部 `dsb ish + 条件 ic ialluis + dsb ish + isb`；stock stop_machine 完成计数用 LSE `ldaddal`/LL/SC fallback，与 mainline 同源 | 已确认事实 |
| `REPORT-stock-vs-mainline-locks.md` | 锁/mutex/rwsem/atomic 与原厂同源，唯一差异=LSE patch 是否生效；原厂 `smp_cpus_done` 走 stop_machine 完整 alternatives，mainline 曾跳过 | 已确认事实 |
| `SKILL-T508N-framebuffer-debug.md` | 探针 API；**坑 #4：`ic ialluis`（广播）在 T770 上 hang**，`ic iallu` 清自身 I-cache 取指异常，原厂只用 `ic ivau`；坑 #5：stop_machine 跨 cluster 竞态（加探针能过、不加卡）；心跳=y=140（WDT 定时器每 2s） | 已确认事实 |
| `session.jsonl`（抽查） | **关键差异点**：turn 12 曾把 `icache_inval_all_pou()` 改成 `dsb(ish)`（去掉 `ic ialluis`）；该修改在**当前工作树已丢失** | 已确认事实（见 §2 一致性核对） |
| 三个 `prompt-*.md`（任务 A/B/C） | 已把卡点收敛为「completion 不放行」；并列出三条假设（nr_todo 未归零 / complete 唤醒失败 / waiter 不在队列） | 任务书，非结论 |

---

## 2. 当前代码/探针状态（与文档一致性核对）

`git status --short`：仅 `arch/arm64/kernel/alternative.c`、`kernel/stop_machine.c` 未提交（20 insertions / 8 deletions），与文档一致。

**探针坐标核对（与文档一致 ✓）**

| 坐标 | 位置 | 状态 |
|---|---|---|
| y=120 x=8 | `alternative.c:270` vdso 后 | 一致 |
| y=120 x=80 | `alternative.c:253` CPU0 `__apply_alternatives` 返回 | 一致 |
| y=120 x=44 | `alternative.c:279` stop_machine 后 | 一致 |
| y=370 / y=390 | `alternative.c:236 / 259` 每核进 fn / fn 完成 | 一致 |
| y=410 / y=430 | `stop_machine.c:521 / 525` fn 返回 / signal_done 后 | 一致 |
| y=450 | `stop_machine.c:528` 最后一个 complete() | 一致（**新增，未刷机**） |
| y=440 x=8 / x=44 | `stop_machine.c:435 / 437` wait_for_completion 前/后 | 一致（**新增，未刷机**） |

**⚠️ 发现一处文档与代码不一致（重要）**

- 文档红线（`HANDOFF` §5、`SKILL` 坑#4）与 `session.jsonl` turn 12 都明确：**`icache_inval_all_pou()` 应改为 `dsb(ish)`（去掉 `ic ialluis`）**。
- 但当前 `arch/arm64/include/asm/cacheflush.h:133-140` **仍是未修改的 mainline 版本**：
  ```c
  static __always_inline void icache_inval_all_pou(void)
  {
      if (alternative_has_cap_unlikely(ARM64_HAS_CACHE_DIC))
          return;
      asm("ic	ialluis");   // ← 仍在，未按计划改成 dsb(ish)
      dsb(ish);
  }
  ```
- 而当前已恢复的 `apply_alternatives_all()` → `__apply_alternatives`（`alternative.c:189`）**会调用 `icache_inval_all_pou()`**。即：当前代码在系统级 patch 收尾时会执行 `ic ialluis`（当 `ARM64_HAS_CACHE_DIC` 未置位时），这正是 SKILL 坑#4 说的 T770 hang 指令。

**构建状态核对**：`work/mainline_Image`、`boot_linux.img` = 08:57；`stop_machine.c` = 09:04。即当前 boot 镜像不含 y=440/450 探针，与文档一致，必须先重建才可刷。

---

## 3. 卡点归因分析（假设排序 + 论证）

### 3.0 已证实的硬事实链（论证基础）

1. **atomic 本身可用**：`multi_cpu_stop` 状态机经历了 `PREPARE→DISABLE_IRQ→RUN→EXIT` 共 3 次状态迁移，每次都需要 `atomic_dec_and_test(&thread_ack)` 把 8 减到 0。8 核都走到 y=410（fn 返回）说明 `thread_ack` 至少归零 3 次 → **LSE `ldaddal` 跨 8 核正确**。
2. **`nr_todo` 必然归零**：`cpu_stop_init_done(&done, cpumask_weight(cpu_online_mask))`，8 核在线 → `nr_todo=8`；8 核都过 y=430（signal_done）→ `atomic_dec_and_test(&nr_todo)` 被调 8 次 → 必有一次返回 true → **`complete()` 必然被调用**。
3. **`nr_todo` 与 `thread_ack` 用同一原语**：`arch/arm64/include/asm/atomic_lse.h` 中 `atomic_dec_and_test` = `__lse_atomic_fetch_sub` = `ldaddal`（acq+rel 全序）。两者语义逐字相同。既然 `thread_ack` 能归零，`nr_todo` 就没有理由不能归零。

### 3.1 假设排序（最终）

| 排名 | 假设 | 判定 |
|---|---|---|
| **H1** | **complete() 已调用，但唤醒"不生效"**——waiter（CPU0 kernel_init）被 `try_to_wake_up` 标成 runnable 后，调度器没有真正把它重新调度回 CPU0；佐证是「无心跳」= tick 停摆 = 调度器无法推进 | **最可能** |
| **H2** | **complete() 已调用，但 stop_machine 后 tick/IPI 整体停摆**（H1 的上游根因），最可疑诱因是系统级 alternatives patch 收尾的 `ic ialluis`（当前代码未按计划改成 `dsb(ish)`）或刚恢复的 errata workaround 对中断/TLB 路径的副作用 | **H1 的根因候选** |
| **H3** | **complete() 调用但 `swake_up` 空操作 / waiter 不在队列（丢唤醒）** | 基本排除，留低概率 |
| **H4** | **`nr_todo` 未归零（atomic 坏 / 初值>8）** | 排除 |

### 3.2 逐条论证

#### H4：`nr_todo` 未归零 —— 排除
- `thread_ack` 归零 3 次证明 `atomic_dec_and_test`（LSE `ldaddal`）正确，`nr_todo` 用同一原语、同一次数（8）递减，必归零。
- 唯一能让 `nr_todo` 不归零的情形是初值 > 8，即 `cpumask_weight(cpu_online_mask) > 8`，但 T770 是 8 核、探针恰好 8 块，不成立。
- **结论：`complete()` 必然被调用，排除 H4。**

#### H3：waiter 不在队列 / `swake_up` 空操作 —— 基本排除
- mainline 6.12 completion 用 **swait**（`include/linux/completion.h` → `struct swait_queue_head wait`），`wait_for_completion` → `do_wait_for_common`（`kernel/sched/completion.c:81`）在**持锁**下先查 `x->done`，再 `__prepare_to_swait` 入队、`set_current_state`、解锁 `schedule`。
- `complete()` → `complete_with_flags`（`completion.c:16`）在**同一把 `raw_spinlock`** 下 `x->done++` 再 `swake_up_locked`。锁保证「查 done / 入队」与「done++ / 唤醒」不可交错，故不存在经典丢唤醒竞态：
  - waiter 先锁：入队→睡→被唤醒；
  - complete 先锁：`done++`→队列空→空唤醒→waiter 后锁看到 `done!=0` 直接返回不睡。
- stop_machine 场景 waiter 只有 1 个（CPU0 kernel_init），swait 的「只唤醒队头一个」语义与 waitqueue 的「遍历唤醒」差异**不致命**。
- 差异点（stock 5.4 waitqueue vs mainline 6.12 swait）最终都收敛到 `try_to_wake_up`；swait 自 v5.8 起在 arm64 大规模验证，不是新代码。
- **结论：纯软件层面丢唤醒被排除；H3 只在「`try_to_wake_up` 实际未把任务真正投入运行」这一层仍可能与 H1 合并成立。**

#### 3.2.1 补充坐实：stock 5.4 `complete()` 确为 waitqueue（本次反汇编验证）

对 stock `complete`（`0xffffffc010227a70`）反汇编（`work/stock_Image_full.bin`，`--adjust-vma=0xffffffc010080000`）：

```asm
ffffffc010227a9c: add x19, x0, #0x8        ; x19 = &completion.wait.lock (offset 8)
ffffffc010227ad4: casa w8, w2, [x19]       ; LSE casa 抢 wait.lock (spin_lock)
ffffffc010227ae4: ldr w8, [x20]            ; x20=&completion -> done
ffffffc010227af0: add w8, w8, #1 ; str w8,[x20]  ; done++
ffffffc010227af8: ldr x8, [x20, #16]!      ; &completion.wait.head (list_head)
ffffffc010227afc: cmp x20, x8 ; b.eq ...   ; 队列空判断
; 遍历 wait_queue 链表，逐项检查 entry->func 是否 == autoremove_wake_function
ffffffc010227b2c: ldur x8, [x8, #-8]       ; entry->func 指针
ffffffc010227b4c: blr x8                    ; 调 func = autoremove_wake_function
ffffffc010227b94: stlrb w8, [x19]           ; release 存回 = spin_unlock
```

结论（坐实任务 A 第 1 问）：**stock 5.4 的 completion = 传统 waitqueue**（`wait_queue_head_t` 双向链表 + `autoremove_wake_function`），而非 swait；其唤醒路径 `autoremove_wake_function → default_wake_function → try_to_wake_up`。
mainline 6.12 用 swait（`swake_up_locked → try_to_wake_up`）。

**差异评估**：结构差异（双向链表+autoremove vs 单链表+直接 ttwu、一次唤醒一个 vs 遍历）在 stop_machine 场景（waiter 仅 1 个）下**行为等价**；锁类型（spinlock vs raw_spinlock）在非 PREEMPT_RT 下等价；两者内存序都由 completion 内部锁保证。**因此 waitqueue→swait 迁移不构成根因**，但它是两者最明显的实现差异，值得作为背景记录。

#### H1：唤醒不生效（waiter 被标 runnable 但不再被调度）—— 最可能
- 事实支撑：8 核全过 y=430 → `complete()` 被调用；x=44 不亮 → waiter 未返回；**「无心跳」→ `t508n_wdt_pet`（y=140，每 2s 定时器）不再触发 → 定时器 tick 停摆**。
- 机制推演：
  1. 最后一个 `signal_done` 的核（大概率是 A76 集群某核，与 CPU0 跨 cluster）调用 `complete()` → `try_to_wake_up(kernel_init)`。
  2. `kernel_init` 是 CPU0 上的任务。若 waker 不是 CPU0，`try_to_wake_up` 走 `ttwu_queue` → `smp_send_reschedule(CPU0)`（reschedule IPI）；若 waker 是 CPU0 的 `migration/0`，则靠 `TIF_NEED_RESCHED` + 之后 `schedule()`。
  3. 无论哪条路，**让 `kernel_init` 真正重新跑都需要 CPU0 的调度器推进**——这需要 tick（周期调度）或 IPI（抢占调度）正常。
  4. 而「无心跳」直接证明 tick 没有推进。因此即便 `kernel_init` 已被 `complete()` 置为 runnable，也永远不被调度回来 → `wait_for_completion` 永不返回 → x=44 不亮。
- **「无心跳」是本归因的关键判别证据**：如果只是 completion 丢唤醒而系统调度正常，心跳应继续闪烁。心跳死了，说明问题在「调度/中断推进」，不在 completion 原语本身。

#### H2：stop_machine 后 tick/IPI 停摆的诱因 —— H1 的上游，候选根因
按可疑度：

1. **`ic ialluis` 广播 I-cache 失效的 T770 副作用（代码/文档不一致）—— 但已弱化**
   - 当前代码在系统级 patch 收尾执行 `ic ialluis`（`cacheflush.h:138`），而 SKILL 坑#4 说「`ic ialluis` 在 T770 上 hang」。
   - **关键新证据（第二次深挖）**：`alternative_has_cap_unlikely(ARM64_HAS_CACHE_DIC)`（`alternative-macros.h:249`）**本身就是一个 alternative 站点**——它被编译成 `ALTERNATIVE("nop","b l_yes",DIC)`。即 DIC 检查只有在「DIC cap 被 patch 进这段代码」后才生效。
   - 推论：`t508n_apply_lse_early()`（`alternative.c:320`）只 patch LSE，**DIC 站点此时仍是 `nop` → `icache_inval_all_pou()` 会执行 `ic ialluis`**。而 boot 已顺利通过早期 LSE patch（`t508n_pass(140)` 之后到达 `smp_cpus_done`）→ **`ic ialluis` 至少在单核早期语境下没有 hang**。
   - 再加上实测 x=80 亮（CPU0 也过了系统级 patch 的 `ic ialluis`），说明 **`ic ialluis` 并非可靠 hang，SKILL 坑#4 是语境相关/过时描述**。因此「`ic ialluis` 直接挂住」作为当前卡点的解释**显著弱化**。
   - 残余可能（未排除）：系统级 `ic ialluis` 是**广播**操作，在 8 核同时关中断 spin 的 stop_machine 语境下，可能对其它 7 核或 GIC/互联留下副作用 → 事后 tick/IPI 停摆。这需要 Flash 3a 单变量验证。
   - 无论如何，当前代码与「文档计划=dsb(ish)」的不一致仍应纠正（一致性/红线要求），只是它不再是头号嫌疑。

2. **恢复的 errata workaround 改变 TLB/中断时序**
   - 现在 `apply_alternatives_all()` 恢复后，`ARM64_WORKAROUND_REPEAT_TLBI`（99 条，来自 A76 1286807/4118414，均已 `=y`）与 `ARM64_WORKAROUND_SPECULATIVE_AT`（24 条，来自 1165522/1530923）首次真正 patch 进内核。
   - 这些 workaround 直接改写 `tlbi` 序列（重复 TLBI）和 AT 后插 ISB/SB，作用于 `__flush_tlb_*`、context-switch、`switch_mm` 等**中断/调度高频路径**。
   - 它们是标准 mainline A76/A55 workaround，通常正确；但「首次启用 + T770 无串口」场景下，若与某处有交互问题，最可能的表现就是 tick/调度路径异常。**作为第二候选，需单变量验证。**

3. **跨核唤醒 IPI 在 stop_machine 后不送达**
   - `try_to_wake_up` 跨 cluster 唤醒依赖 `smp_send_reschedule`（SGI）。若 stop_machine 的 `hard_irq_disable()`/`local_irq_restore()` 或刚 patch 的 GIC 代码使 SGI 无法投递，则唤醒不生效。可并入 H1 的机制，与「无心跳」同源（都是中断不投递）。

---

## 4. 验证方案（探针级，一次刷机最多验证一个变量）

> 目标：把「complete 是否被调用 / waiter 是否入队 / tick 是否活着」三个变量逐个钉死。探针只加在 `kernel/sched/completion.c`、`kernel/stop_machine.c`、`kernel/sched/core.c`（非 idmap，无 `.idmap.text` 超限风险）。

### Flash 1：先刷当前已改好的代码（含 y=440/450，唯一新增变量 = 完成路径探针）
重建 → 刷机 → 读：
| 判据 | 结论 |
|---|---|
| y=440 **x=8** 亮（wait 前） | waiter 确实进入 `wait_for_completion`（排除「没走到 wait」） |
| y=450 亮 | `complete()` **被调用** → 直接坐实 H4 排除、坐实「complete 已调」 |
| y=440 **x=44** 亮 | waiter 返回 → 通过 |
| y=450 亮 且 y=440 x=44 不亮 | **确诊 H1：complete 调了但 waiter 没醒** |
| y=450 不亮 | 推翻 §3.0 推演，需回头查 `nr_todo` 初值/队列（H4 复活） |
| y=140 心跳仍不闪 | 坐实「tick 死」= H2 方向 |

### Flash 2：若确诊「complete 调了但 waiter 没醒」（单变量 = 唤醒链路）
在 `complete_with_flags()` 里、`swake_up_locked()` 调用**之后**加一格探针；并在 `swake_up_locked()`（`kernel/sched/swait.c`）的 `list_empty(&q->task_list)` 分支加另一格探针：
- 若「队列空」格亮 → waiter 不在队列 → H3 复活（查 `__prepare_to_swait` 时序）。
- 若「队列非空、try_to_wake_up 已调」格亮但仍不醒 → 唤醒链路正常，问题 100% 在「调度器/IPI 不推进」→ 锁定 H1/H2，进入 Flash 3。

### Flash 3：定位「tick/IPI 停摆」的诱因（单变量 = 二选一）
- **方案 3a（首选，验证 `ic ialluis` 假设）**：把 `icache_inval_all_pou()` 按文档计划改回 `dsb(ish)`（去掉 `ic ialluis`），其余不动，重刷。若 stop_machine 返回 + 心跳恢复 → **根因= `ic ialluis`**，坐实 §2 的代码/文档不一致。
- **方案 3b（验证 errata 假设）**：临时关掉 `CONFIG_ARM64_ERRATUM_4118414/1286807`（repeat-TLBI）重刷对照；若恢复则根因在 workaround 交互。
- 另加：早期读 `CTR_EL0` 的 DIC bit 画到屏幕（1 格=DIC=1），一次性判定 `ic ialluis` 到底有没有被执行（成本极低，可并入 3a 同一次刷机，不算第二个变量，因为只是观测不改变行为）。

---

## 5. 下一步建议（仅建议，不执行）

1. **立即先做 Flash 1**（代码已就绪，只差重建+刷机），用 y=440/450 把「complete 是否被调用、waiter 是否醒」钉死——这是当前性价比最高的一步，能把假设空间砍半。
2. **优先修正 §2 发现的代码/文档不一致**：`icache_inval_all_pou()` 当前仍是 `ic ialluis`，与「不用无条件 `ic ialluis`」红线及 turn 12 已做过的 `dsb(ish)` 修改相悖。这是最有据可查的 H2 诱因，建议作为 Flash 3 的首选单变量。
3. **不要**继续在 cache 可见性（PoC/dcache）上做功——文档已多次证明原厂裸协议可行、PoC 一致性可用；也不要碰 CCI/CMN/USB 等外设。
4. 确认 DIC 值（观测项，可零成本并入任意一次刷机），以排除/坐实 `ic ialluis` 是否真的在系统级 patch 路径执行。

---

## 结论摘要（供父会话）

- **卡点已收敛到 completion 唤醒"不生效"，而非计数/原子问题**：8 核 signal_done 意味着 `atomic_dec_and_test(nr_todo)` 必然归零、`complete()` 必然被调用（thread_ack 归零 3 次已证明 LSE 原子正确）；x=44 不亮 + **「无心跳」**共同指向「complete 后 waiter 被标 runnable，但 tick/IPI 停摆使调度器无法把它重新调度回 CPU0」。
- **已坐实**：stock 5.4 `complete()`=waitqueue、mainline 6.12=swait（本次反汇编），但两者都收敛到 `try_to_wake_up`、单 waiter 场景行为等价 → **waitqueue→swait 迁移不是根因**。
- **已弱化**：`ic ialluis` 的「直接 hang」假设——`alternative_has_cap_unlikely(DIC)` 本身是 alternative 站点，早期 LSE patch 在 DIC 未应用时已执行过 `ic ialluis` 且 boot 通过，说明它并非可靠 hang（SKILL 坑#4 语境相关/过时）。代码与文档的不一致仍应纠正，但不再是头号嫌疑。
- **最可能根因（H1/H2）**：stop_machine 后 tick/IPI 停摆（「无心跳」）使调度器无法推进；深层诱因待定位，候选=系统级 patch 收尾的 `ic ialluis` 广播副作用 / 刚恢复的 repeat-TLBI+speculative-AT workaround 首次作用于中断/TLB 路径 / 跨核唤醒 IPI 不送达。
- **下一步**：先重建刷 Flash 1（y=440/450 已就绪）钉死「complete 是否被调用 + waiter 是否醒 + 心跳是否活着」三个变量；据此再单变量定位 tick 停摆诱因。
