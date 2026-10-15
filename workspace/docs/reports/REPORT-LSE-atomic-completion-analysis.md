# T508N：LSE atomic dec_and_test 语义 + thread_ack vs nr_todo + 系统级 alternatives 副作用分析

> 任务 C 报告（refocused 版本）。
> 范围：纯源码 + 反汇编分析，不刷机、不改代码。
> 符号表：`work/stock_kallsyms.map`；反汇编映射
> `file_off = vaddr - 0xffffffc010080000`，
> `/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000`。

---

## 结论先行（TL;DR）

1. **atomic 层面没有任何未排除的隐患**。`thread_ack` 归零 3 次 + 原厂反汇编逐字节同构，
   严格证明 `atomic_dec_and_test`（LSE `ldaddal` 全序）在 T770 8 核上正确。
   因此 `nr_todo` 必归零、`complete()` 必被调用。H4（atomic 坏/计数丢失）**正式排除**。

2. **`ic ialluis` 进一步排除（含"单核 vs 8 核"语境差异分析，见 §3.5）**：
   - **原厂 5.4 `__apply_alternatives` 尾部就有一条「条件 `ic ialluis`」**，
     判定位是 `cpu_hwcaps` 的 bit28（语义上就是 DIC），与 mainline 的
     `alternative_has_cap_unlikely(ARM64_HAS_CACHE_DIC)` 完全同构。
   - A55/A76 的 `CTR_EL0.DIC=0`（CTR_EL0 = `0x8444c004`/`0x84448004`，bit29=0），
     所以**原厂每次系统级 alternatives 都真实执行了广播 `ic ialluis`，且 8 核在
     stop_machine 关中断 spin 语境下正常运行**。原厂能跑 ⇒ `ic ialluis` 在
     stop_machine 收尾语境下对 T770 无害。
   - 早期 LSE patch（`t508n_apply_lse_early`）也已在 DIC 未 patch 时执行过
     `ic ialluis` 且通过（`t508n_pass(140)`），进一步佐证「`ic ialluis` 直接 hang」
     是语境相关/过时描述。
   - **单核(早期) vs 8核(系统级) 的唯一环境差异是广播域大小与 7 核关中断 spin；
     而这两点恰好被"原厂 8 核同语境同指令跑通"直接证伪**。故 `ic ialluis` 不可能是
     "第 2 次执行才 tick 停摆"的诱因。

3. **REPEAT_TLBI / SPECULATIVE_AT 是本次刷机的最大新变量（任务 4 升级为核心，§5）**：
   - REPEAT_TLBI(99)：patch 目标是所有 `flush_tlb_*` 收尾的 `__tlbi_sync_s1ish()`，
     指令 = 在 `dsb ish` 后**多插 `tlbi vale1is, xzr + dsb ish`**。它作用于
     context-switch/`switch_mm` 的 TLB flush 路径，**不直接碰 tick 中断处理本身**。
   - **关键新发现（A76 revision 依赖差异）**：原厂只有 `1286807`（匹配 A76 r0p0-r3p0）；
     mainline 额外有 `4118414`（`MIDR_ALL_VERSIONS(A76)` ⇒ 匹配**所有** A76 revision）。
     若 T770 的 A76 是 r4p0+，则**原厂不启用 REPEAT_TLBI、mainline 会启用** ——
     这是两内核在 A76 上唯一真实的 workaround 差异，需读 `MIDR_EL1` 确认。
   - SPECULATIVE_AT(24)：patch 目标 = KVM/hyp 的 `at` 后插 `isb`（host 无 `at` 高频点，
     `grep` 证实 host 侧 `_AT` 全是 C 宏 `_AT(type,val)` 而非指令）。bring-up 不跑 guest，
     对 boot tick 无影响。

4. **根因仍指向 H1**：`complete()` 已调但 waiter（CPU0 kernel_init）没被真正调度回来，
   上游是「无心跳 = tick/IPI 停摆」。**不要**再往 atomic 方向查；workaround 是否相关
   需 §5.4 的单变量回退实验定案。

---

## 1. mainline 6.12 `atomic_dec_and_test` 在 LSE 下的实现（原任务 1）

### 1.1 调用链（精确到函数）

```
atomic_dec_and_test(v)                          include/linux/atomic/atomic-instrumented.h:1380
  └ kcsan_mb(); instrument_atomic_read_write();  (KCSAN 关闭时是空)
  └ raw_atomic_dec_and_test(v)                  atomic-arch-fallback.h:2275
      └ 无 arch_atomic_dec_and_test (arm64 未定义) →
        raw_atomic_dec_return(v) == 0
      └ arch_atomic_dec_return(v) == 0
          └ arch_atomic_sub_return(1, v)        arch/arm64/include/asm/atomic.h (全序, 无后缀)
              └ __lse_ll_sc_body(atomic_sub_return, 1, v)   asm/lse.h:19
```

### 1.2 LSE 版本（`arch/arm64/include/asm/atomic_lse.h`）

```c
// ATOMIC_OP_ADD_SUB_RETURN( ) 展开:
__lse_atomic_sub_return(1, v) = __lse_atomic_fetch_sub(1, v) - 1
__lse_atomic_fetch_sub(1, v)  = __lse_atomic_fetch_add(-1, v)
// ATOMIC_FETCH_OP( , al, add, ldadd, "memory") 展开:
asm volatile(".arch_extension lse\n"
             " ldaddal %w[i], %w[old], %[v]"     // ← 关键指令
             : [v] "+Q"(v->counter), [old] "=r"(old)
             : [i] "r"(-1) : "memory");
return old;   // sub_return = old - 1; 判定 (old - 1) == 0 ⇔ old == 1
```

**回答（原任务 1 三问）：**

| 问题 | 答案 |
|---|---|
| 用哪条指令 | **`ldaddal`**（不是 `staddl`、不是 `casa`）。`atomic_dec_and_test` = `ldaddal w(-1), w_old, [v]` + `add w_result, w(-1), w_old` + `cbnz` |
| 内存序 | **full ordering（acquire+release）**。后缀 `al` = Acquire-Release。`_relaxed`=`ldadd`，`_acquire`=`ldadda`，`_release`=`ldaddl`；dec_and_test 走无后缀全序版 |
| 返回值判定 | `add w8, w8(-1), w9(old)` 得到 `old-1`，`cbnz`（==0 则 true）。等价于「旧值==1」 |

### 1.3 LL/SC 版本（`atomic_ll_sc.h` ATOMIC_OP_RETURN(, dmb ish, , l, ...)）

```asm
prfm pstl1strm, [v]
1: ldxr   w0, [v]      ; 注意: 无 acquire 后缀
   sub    w0, w0, #1
   stlxr  w1, w0, [v]  ; release store
   cbnz   w1, 1b
   dmb    ish           ; 全序屏障
```

**等价性**：LL/SC 用 `ldxr(relaxed) + stlxr(release) + dmb ish(full)`，LSE 用单条
`ldaddal`（acq+rel）。两者都是**全序**，语义等价。返回值都是 `old-1`，判定相同。

### 1.4 反汇编实证（stock 5.4 内联实现 = mainline 逐字同构）

`multi_cpu_stop`（`ffffffc01030a420`）内 `thread_ack` 的 dec_and_test：

```asm
; LSE 路径 (alternative patch 后):
ffffffc01030a55c: mov  w8, #0x1
ffffffc01030a560: neg  w8, w8              ; -1
ffffffc01030a564: ldaddal w8, w9, [x24]    ; old = fetch_add(-1)  [x24=&thread_ack]
ffffffc01030a568: add  w8, w8, w9          ; result = -1 + old = old-1
ffffffc01030a56c: cbnz w8, <not_last>      ; !=0 → 继续 spin
ffffffc01030a570: ldr  w8, [x19,#16]       ; = num_threads
ffffffc01030a578: str  w8, [x19,#36]       ; atomic_set(thread_ack, N)   (plain str)
ffffffc01030a57c: dmb  ishst               ; smp_wmb()
ffffffc01030a584: str  w8, [x19,#32]       ; WRITE_ONCE(state, newstate)

; LL/SC 路径:
ffffffc01030a5f0: prfm pstl1strm, [x24]
ffffffc01030a5f4: ldxr w8, [x24]
ffffffc01030a5f8: sub  w8, w8, w10         ; w10=1
ffffffc01030a5fc: stlxr w9, w8, [x24]
ffffffc01030a600: cbnz w9, 0x...5f4
ffffffc01030a604: dmb  ish
```

`cpu_stopper_thread`（`ffffffc01030adc8`）内 `nr_todo` 的 dec_and_test + `complete()`：

```asm
ffffffc01030aedc: blr  x22                 ; fn(arg) 返回
ffffffc01030aee8: str  w0, [x21,#4]        ; if(ret) done->ret = ret
; cpu_stop_signal_done() 内联:
ffffffc01030aef4: mov  w8, #0x1
ffffffc01030aef8: neg  w8, w8
ffffffc01030aefc: ldaddal w8, w9, [x21]    ; [x21=&nr_todo]  — 与 thread_ack 完全相同
ffffffc01030af00: add  w8, w8, w9
ffffffc01030af04: cbnz w8, <skip_complete>
ffffffc01030af08: add  x0, x21, #0x8       ; &done->completion
ffffffc01030af0c: bl   0xffffffc010227a70  ; complete(&completion)
```

**结论**：原厂 5.4 与 mainline 6.12 的 `atomic_dec_and_test` 指令级一致——
LSE 都是 `ldaddal`，LL/SC 都是 `ldxr/stlxr/dmb ish`，返回值 `old-1`，判零 `cbnz`。

---

## 2. §3.0 推演复核：thread_ack 归零 ⇒ nr_todo 必归零 ⇒ complete 必调用（原任务 1 复核）

### 2.1 逐字对比 thread_ack 与 nr_todo（无漏洞）

| 维度 | thread_ack（multi_cpu_stop） | nr_todo（cpu_stop_done） |
|---|---|---|
| 初值设置 | `set_state`: `atomic_set(thread_ack, num_threads)` | `cpu_stop_init_done`: `atomic_set(nr_todo, n)` |
| 初值 | `num_online_cpus()` = 8 | `cpumask_weight(cpu_online_mask)` = 8 |
| 初值时机 | 发布 `state` 前（`atomic_set` → `smp_wmb()` → `WRITE_ONCE(state)`） | 入队 work 前（`atomic_set` → `init_completion` → `queue_stop_cpus_work` 持 `stopper->lock` + `wake_up_process`） |
| 递减原语 | `atomic_dec_and_test(&thread_ack)` | `atomic_dec_and_test(&nr_todo)` |
| 递减次数 | 8 核 × 3 个状态迁移 = 24 次（每次 8→0） | 8 核 × 1 = 8 次（8→0） |
| 并发写 | 无（每个核恰好 1 次/状态） | 无（每个核恰好 1 次） |
| 归零后动作 | `set_state`: `atomic_set + smp_wmb + WRITE_ONCE(state)`（纯写+poll，无锁无唤醒） | `complete()`: `raw_spin_lock + done++ + swake_up_locked`（锁+唤醒） |
| 读者 | 各核 `READ_ONCE(state)` 轮询 | waiter 睡眠在 swait，等 `try_to_wake_up` 唤醒 |

### 2.2 漏洞排查结论

1. **atomic_set 与 atomic_dec 的交互**：两者都是对齐 32-bit `atomic_t`；`atomic_set`=plain
   `str`(WRITE_ONCE)，`dec_and_test`=`ldaddal`(全序)。无撕裂、无部分写。
2. **内存序**：dec 是 `ldaddal`（acq+rel 全序），与 set 之间由全序原子保证「后减看到先 set」。
   thread_ack 额外有 `smp_wmb()`（`dmb ishst`）保障 `atomic_set(N)` 先于 `WRITE_ONCE(state)`；
   nr_todo 的 set 先于 `raw_spin_lock` 保护的入队，锁提供 release/acquire 配对。
3. **初值时机**：两者初值都在任何递减前写入，且 count 都是 8，不可能 >8。
4. **归零判据**：`ldaddal` 返回值 `old`，`old-1==0 ⇔ old==1`，恰好只有第 8 次递减返回 true。

**结论：§3.0 推演成立，无漏洞。** 8 核都过 y=430（signal_done）⇒ `atomic_dec_and_test(nr_todo)`
被调 8 次 ⇒ 必有一次 `old==1` ⇒ 返回 true ⇒ `complete()` 必然被调用。**H4 排除。**
`complete()` 之后卡住的原因只在唤醒/调度路径（H1），不在 atomic。

---

## 3. icache_inval_all_pou() 与 CTR_EL0.DIC 深挖（原任务 2）

### 3.1 判定路径

```c
// arch/arm64/include/asm/cacheflush.h:133
static __always_inline void icache_inval_all_pou(void)
{
    if (alternative_has_cap_unlikely(ARM64_HAS_CACHE_DIC))   // ← 关键
        return;
    asm("ic ialluis");
    dsb(ish);
}
```

```c
// arch/arm64/include/asm/alternative-macros.h:249
alternative_has_cap_unlikely(cpucap):
    ALTERNATIVE("nop", "b l_yes", cpucap)   // 默认 nop→return false; patch 后 b→return true
```

- `ARM64_HAS_CACHE_DIC` 是 `ARM64_CPUCAP_SYSTEM_FEATURE`（`cpufeature.c:2556`），
  matches = `has_cache_dic`（`cpufeature.c:1701`）= `CTR_EL0.bit29`。
- 置位时序：`smp_cpus_done()` → `setup_system_features()` → `setup_system_capabilities()`
  （`cpufeature.c:3553`）里 **`update_cpu_capabilities(SCOPE_SYSTEM)`（行 3560）
  先于 `apply_alternatives_all()`（行 3562）**。所以 DIC cap 是否在 `system_cpucaps`
  中，在系统级 patch 前已定案。
- 关键机制：`alternative_has_cap_unlikely(DIC)` 本身是 alternative 站点。在
  `__apply_alternatives` 内部循环 patch 完所有条目后（含 DIC 条目）才调用
  `icache_inval_all_pou()`（`alternative.c:189`），所以此处 DIC 分支已被正确 patch：
  DIC=1 → `b l_yes` → 提前 return（**不执行 `ic ialluis`**）；
  DIC=0 → 保持 `nop` → 执行 `ic ialluis`。

### 3.2 T770 的 DIC 值 = 0 ⇒ `ic ialluis` 真实执行

Cortex-A55 / A76 的 `CTR_EL0` 复位值：

| core | CTR_EL0 | bit29(DIC) | bit28(IDC) |
|---|---|---|---|
| Cortex-A76 | `0x8444c004` | **0** | **0** |
| Cortex-A55 | `0x84448004` | **0** | **0** |

（A76 `0x8444c004` 为 QEMU/内核公开值，见 [patchew QEMU A76 ctr](https://patchew.org/QEMU/20260129160917.1415092-1-peter.maydell@linaro.org/diff/20200901151823.29785-1-peter.maydell@linaro.org/#46)；`CTR_EL0` 字段定义见 [ARM DDI 0595](https://developer.arm.com/documentation/ddi0595/2020-12/AArch64-Registers/CTR-EL0--Cache-Type-Register?lang=en)）

因此 DIC=0，`icache_inval_all_pou()` **不会**提前 return，`ic ialluis` **真实执行**。

### 3.3 `ic ialluis` 在 8 核 stop_machine 语境下的行为

`ic ialluis` = "Invalidate all instruction cache to PoU, Inner Shareable"，是**广播**
操作：使 Inner Shareable 域内**所有 PE** 的 I-cache 失效。系统级 alternatives 时：
- CPU0 在 `__apply_alternatives` 尾部执行 `ic ialluis`；
- 其余 7 核处于 `multi_cpu_stop` 状态机（`MULTI_STOP_DISABLE_IRQ` 后 `hard_irq_disable`），
  在 `__apply_alternatives_multi_stop` 的 `while (!all_alternatives_applied) cpu_relax()` spin。
- 广播失效所有核 I-cache 后，各核继续取指时重取（已 patch）指令。

**副作用评估**：架构上 `ic ialluis` 在 stop_machine 收尾是标准、且必要的操作（patch 了 text
必须失效 I-cache）。理论上广播对互联/一致性有压力，但见 §4——原厂同语境同指令跑通。

### 3.4 用探针测 DIC 值（一次刷机，纯观测，不改变行为）

最省事：在 `icache_inval_all_pou()` 或 `t508n_apply_lse_early()` 里读 `CTR_EL0` 画屏：

```c
// 任意已在线性映射、MMU 已开的 early C 上下文（如 t508n_apply_lse_early 内）
#include <asm/sysreg.h>
static inline bool t508n_ctr_dic(void)
{
    return read_cpuid_cachetype() & BIT(CTR_EL0_DIC_SHIFT);   // bit 29
}
// 画屏: t508n_paint_bar(8 + (t508n_ctr_dic()?24:0), y_空闲, 24, 8);
```

建议坐标：用某行空闲格（如 y=330 或 y=360），`DIC=1` 亮一格、`DIC=0` 不亮。
另可顺带读 IDC（bit 28）。**这是观测项，不改变行为，可与 Flash 3a（去 `ic ialluis`）
同一次刷机，不算第二个变量。**

### 3.5 单核(早期) vs 8核(系统级) `ic ialluis` 的语境差异（任务 2 refocus）

问题：`ic ialluis` 早期执行过一次没死（`t508n_apply_lse_early` → `__apply_alternatives`
收尾 → DIC 未 patch → 真执行 `ic ialluis`），系统级再执行一次为何"可能" tick 停摆？

两个语境的实质差异只有两点：

| 维度 | 早期单核（t508n_apply_lse_early） | 系统级 8 核（stop_machine 收尾） |
|---|---|---|
| 广播域 PE 数 | **1**（只有 CPU0 online，`ic ialluis` 广播无对端） | **8**（7 个二级核在 spin） |
| 对端 PE 状态 | 无 | 7 核 `hard_irq_disable()` + `while(!flag) cpu_relax()` 紧循环取指 |
| 执行者上下文 | boot CPU，IRQ off，无调度器 | CPU0 在 stop_machine fn 内，其余 7 核关中断 spin |
| 后续动作 | 继续单核 boot | 置 flag → 二级核退出 → ack_state → EXIT → signal_done → complete |

**结论：这 2 点差异都被"原厂同语境"直接证伪** —— 原厂 `__apply_alternatives`
（§4.1）在**完全相同**的 8 核 stop_machine 收尾语境下执行同一条广播 `ic ialluis`，
且 7 个二级核同样是关中断 `yield` spin，并正常跑通 Android。

因此「第 2 次 `ic ialluis` 因广播域变大/对端 spin 而诱发 tick 停摆」**不成立**：
- 若广播对 7 个 spin 核有害，原厂会挂——但它没挂；
- `ic ialluis` 的广播完成是架构保证的（PoU 内所有 PE 的 I-cache 失效），对端 spin
  取指 `yield` 循环会被正常重取，不需要对端做任何事。

**真正的差异不在 `ic ialluis` 本身，而在"第 2 次 `__apply_alternatives` 同时把
REPEAT_TLBI/SPECULATIVE_AT 等 workaround 首次 patch 进内核"** —— 这才是 8 核语境与
早期语境的实质性区别（早期只 patch 了 LSE）。见 §5。

---

## 4. 反汇编原厂 5.4 等价函数：`ic ialluis` 到底用不用（原任务 3，关键证据）

### 4.1 原厂 `__apply_alternatives` 尾部（`ffffffc010114cd8`，含 DIC 条件 `ic ialluis`）

```asm
ffffffc010114e94: tbnz w22, #0, 0x...14ed4   ; is_module → 跳过 cache 维护
ffffffc010114e98: dsb  ish
ffffffc010114e9c: b    0x...14f00            ; → DIC 检查
ffffffc010114ea0: nop                          ; (alternative 占位)
ffffffc010114ea4: ic   ialluis                ; ← 广播失效（DIC=0 时执行）
ffffffc010114ea8: dsb  ish
ffffffc010114eac: isb
ffffffc010114eb0: ... bitmap_or(applied_alternatives) ...
...
ffffffc010114f00: adrp x8, 0xffffffc01250c000
ffffffc010114f04: ldr  x8, [x8, #1536]        ; x8 = cpu_hwcaps  (0xffffffc01250c600)
ffffffc010114f08: tbnz w8, #28, 0x...14eac   ; bit28 置位 → 跳到 isb（跳过 ic ialluis）
ffffffc010114f0c: b    0x...14ea4            ; 否则 → 执行 ic ialluis
```

`0xffffffc01250c600` 在 `stock_kallsyms.map` 中是 **`cpu_hwcaps`**（B 段，64-bit 位图）。
即：**原厂 `__apply_alternatives` 尾部 = `dsb ish; [cpu_hwcaps.bit28==DIC ? skip : ic ialluis]; dsb ish; isb`**。

这**逐字等价**于 mainline 的：
```c
dsb(ish);
icache_inval_all_pou();   // = alternative_has_cap_unlikely(DIC) ? return : (ic ialluis; dsb ish);
isb();
```

> 注：原厂用运行时位图测试（`tbnz cpu_hwcaps,#28`），mainline 用 alternative-patch 分支
> （`ALTERNATIVE("nop","b l_yes",DIC)`）。两者语义相同：**DIC=0 就执行 `ic ialluis`**。

### 4.2 原厂 `__flush_icache_range`（`ffffffc010086e40`）只用 `ic ivau`

```asm
; dc cvau 循环 (clean D-cache by VA):
ffffffc010086e78: mrs  x3, ctr_el0
ffffffc010086e94: dc   cvau, x4            ; ← 按 VA
ffffffc010086ea4: dsb  ish
; ic ivau 循环 (invalidate I-cache by VA):
ffffffc010086eb0: mrs  x3, ctr_el0
ffffffc010086ecc: ic   ivau, x3            ; ← 按 VA（不是 ialluis）
ffffffc010086edc: dsb  ish
ffffffc010086ee0: isb
```

**即**：原厂普通 cache 维护确实只用 `ic ivau`（"只用 ic ivau" 成立）；**但系统级
alternatives 收尾用的是 `ic ialluis`（条件 DIC）**——两者并存，语境不同。

### 4.3 结论：`ic ialluis` 被排除为根因

- 原厂在 **完全相同的 stop_machine 收尾语境**（smp_cpus_done → stop_machine →
  `__apply_alternatives`，7 个二级核关中断 spin）执行**同一条广播 `ic ialluis`**，
  且正常跑通 Android。
- 因此「`ic ialluis` 在 T770 上 hang」是**语境相关或过时**的描述（很可能来自早期
  单核 patch / `ic iallu` 非广播 / MMU 未就绪语境），**不能解释当前卡点**。
- SKILL 坑#4 应更新为：普通路径只用 `ic ivau`；系统级 alternatives 收尾的
  `ic ialluis` 与原厂一致，可保留。代码与文档「改成 dsb(ish)」的不一致是**一致性
  问题**，不是功能根因。

---

## 5. REPEAT_TLBI / SPECULATIVE_AT workaround：patch 目标 + tick 路径影响 + 回退方案（任务 4，核心）

背景：这两个 workaround 是**本次刷机（恢复完整 `apply_alternatives_all()`）才首次真正
patch 进 mainline 内核**的最大新变量。之前跳过时内核一直跑"无 workaround"版本。

### 5.1 配置对比（原厂 vs mainline，含 A76 revision 依赖差异）

| errata | 原厂 5.4 | mainline 6.12 | 匹配范围 | 结论 |
|---|---|---|---|---|
| 1286807 (repeat-TLBI) | `=y` | `=y` | A76 **r0p0–r3p0** | 两边一致 |
| **4118414 (erratum 4193800)** | 无 | `=y` | A76 **ALL_VERSIONS** | **⚠ mainline 对所有 A76 打 REPEAT_TLBI，原厂只对 r0p0–r3p0** |
| 3194386 | 无 | `=y` | (C1/A510 等，非 T770) | 无影响 |
| 2441007 (repeat-TLBI A55) | 无 | **未开** | — | 两边都不对 A55 打 REPEAT_TLBI |
| 1165522 (spec-AT) | `=y` | `=y` | A76 r0p0–r2p0 | 两边一致 |
| 1530923 (spec-AT A55) | 无 | `=y` | A55 r0p0–r2p0 | **mainline 多打 A55**（但全在 KVM，见 5.3） |

**关键点**：`4118414` 的 `match_list` 是 `MIDR_ALL_VERSIONS(MIDR_CORTEX_A76)`
（`cpu_errata.c:228-234`），并 `select ARM64_WORKAROUND_REPEAT_TLBI`。
→ **若 T770 的 A76 revision > r3p0（即 r4p0+），原厂(仅1286807)不打 REPEAT_TLBI、
mainline(4118414)会打** —— 这是两内核在 A76 TLB 路径上唯一真实的差异。
→ **必须读 `MIDR_EL1`（`read_cpuid_id()`）确认 A76 变体/revision**，才能判定这个
差异是否真的生效（见 5.5 探针）。

### 5.2 REPEAT_TLBI：patch 目标函数 + 指令序列

**patch 模板**（`arch/arm64/include/asm/tlbflush.h:176-202`）：
```c
#define __repeat_tlbi_sync(op, arg...) do {                        \
    if (!alternative_has_cap_unlikely(ARM64_WORKAROUND_REPEAT_TLBI)) \
        break;   /* 未 patch: nop → break, 跳过 */                  \
    __tlbi(op, ##arg);  /* = tlbi vale1is, xzr (stage1) / vale2is (hyp) */ \
    dsb(ish);                                                       \
} while (0)

static inline void __tlbi_sync_s1ish(void)
{
    dsb(ish);
    __repeat_tlbi_sync(vale1is, 0);   // ← REPEAT_TLBI alternative 站点
}
```

**patch 后的指令**（以 `__tlbi_sync_s1ish` 为例）：
```asm
dsb  ish                ; 原本
tlbi vale1is, xzr       ; ← 额外重复（patch 后从 "nop" 变 "b" 落入此序列）
dsb  ish                ; ← 额外屏障
```

**patch 目标函数（`__tlbi_sync_s1ish` 的所有调用点，即 99 条 REPEAT_TLBI 的落点）**
（`tlbflush.h` 全部 `static inline`，每处调用点各内联一份 → 各占 1 条 alternative）：

| 函数 | 行号 | 触发场景 |
|---|---|---|
| `flush_tlb_all()` | 285 | 全局 TLB 失效 |
| `flush_tlb_mm()` | 293 | mm 级失效（`switch_mm`/`check_and_switch_context` ASID 回绕、`execve` 等） |
| `flush_tlb_page()` | 324 | 单页失效（`mprotect`/`munmap`） |
| `arch_tlbbatch_flush()` | 364 | TLB batch 收尾（`munmap` 大范围） |
| `flush_tlb_kernel_range()` / `__flush_tlb_kernel_pgtable()` / `__flush_tlb_range()` | 492/522/536 | 内核/范围失效 |
| `arch/arm64/kernel/sys_compat.c:39` | — | 32-bit compat `__flush_tlb` |

### 5.3 SPECULATIVE_AT：patch 目标 + host 侧确认

**patch 模板**：`ALTERNATIVE("nop", "isb", ARM64_WORKAROUND_SPECULATIVE_AT)`
→ patch 后在 `at` 指令后插 `isb`（阻断对损坏翻译的投机访问）。

**消费站点**（`grep` 全部命中）：
- `arch/arm64/kvm/at.c:441`、`arch/arm64/include/asm/kvm_mmu.h:328`
- `arch/arm64/kvm/hyp/vhe/switch.c:208`、`hyp/nvhe/mem_protect.c:316`
- 其余为 runtime `cpus_have_final_cap(...)`（非 alternative，不参与 patch）

**host（非 KVM）侧确认无 `at` 指令**：对 `arch/arm64/`（排除 kvm/hyp）`grep at\ s1e/__at/read_at`
无命中；命中的 `_AT(...)` 全是 `pgtable-hwdef.h` 里的 C 宏 `_AT(type, val)`（cast），
**不是 AT 指令**。⇒ SPECULATIVE_AT 的 alternative 站点 100% 在 KVM/hyp，bring-up 不跑
guest，**对 boot/tick 无影响**。

### 5.4 对 tick 中断路径的影响评估

tick 路径（arm64）：
```
GIC → gic_handle_irq → el1_irq → arch_timer_handler_virt/phys
    → timer_handler → tick_handle_periodic → update_process_times
        ├ scheduler_tick → task_tick_fair → check_preempt_tick → resched_curr
        │     └ set_tsk_need_resched + smp_send_reschedule (SGI IPI)
        └ run_local_timers → (timer wheel 回调)
```
逐环节核对：
1. **tick 中断处理本身不含 TLBI、不含 `at`**：`scheduler_tick`/`update_process_times`
   只做记账 + 可能发 reschedule IPI，**无 TLB 操作、无 `at`**。
2. **REPEAT_TLBI 不落在 tick 处理内**，只落在 TLB flush（`switch_mm` 的 mm 切换 /
   `munmap`/`mprotect`）。且 stop_machine 卡点场景里 waiter=内核线程、stopper=内核线程，
   **共享 `init_mm`/lazy mm，几乎不发生真正的 mm 切换 ⇒ 连 `switch_mm`→`flush_tlb` 都不触发**。
3. **SPECULATIVE_AT 不落在 host 路径**（5.3），tick 更无涉。

**结论**：两个 workaround 都**不直接修改 tick 中断处理代码**；它们与"无心跳 = tick 停摆"
在**时间上重合**（同一时刻被 patch），但**因果链上不直接相关**。不过 REPEAT_TLBI 是唯一
"在 A76 上原厂可能不打、mainline 打"的差异（§5.1 的 4118414），且一旦 A76 revision>r3p0
就真实生效——**必须用单变量回退实验 + MIDR 读值定案，不能纯靠静态分析排除**。

### 5.5 验证 / 回退方案（单变量）

| 步骤 | 操作 | 判据 |
|---|---|---|
| 0 | **读 MIDR_EL1 画屏**（零成本观测，可并入任意一次刷机） | A76 变体/revision 是否 >r3p0（决定 4118414 差异是否真实） |
| 1 | Flash 1（y=440/450）钉死 complete/waiter/心跳 | 见 §6 |
| 2a | **回退 REPEAT_TLBI**：`.config` 临时关 `CONFIG_ARM64_ERRATUM_4118414` **和** `CONFIG_ARM64_ERRATUM_1286807`（+`CONFIG_ARM64_ERRATUM_3194386` 一并关，保持干净），其余不动，重建刷机 | 若 tick 恢复/stop_machine 返回 → REPEAT_TLBI 坐实；否则排除 |
| 2b | **回退 SPECULATIVE_AT**：临时关 `CONFIG_ARM64_ERRATUM_1530923`（+`1165522`） | 预期无变化（KVM-only），可一次性归档 |

> 注意红线「不在一次刷机里同时改多个变量」：2a 与 2b 分开刷。2a 优先级最高，
> 因为它是唯一"原厂 vs mainline 在 A76 上可能有差异"的项。若 2a/2b 都无变化，
> 则 workaround 彻底排除，回到 H1（唤醒/IPI/tick 投递本身）。

### 5.6 小结

| workaround | patch 后指令 | 目标函数 | 是否原厂差异 | 能否解释 tick 停摆 |
|---|---|---|---|---|
| REPEAT_TLBI | 末尾多 `tlbi vale1is/vale2is + dsb ish` | 所有 `flush_tlb_*`/`switch_mm` TLB flush | **是（若 A76>r3p0，4118414 使 mainline 打而原厂不打）** | **不能直接**（不碰 tick 处理），但需回退实验定案 |
| SPECULATIVE_AT | `at` 后插 `isb` | 100% KVM/hyp | 是（多 A55 1530923） | **基本不可能**（guest 未运行） |

---

## 6. 最终结论（原任务 5）

### 6.1 atomic 层面还有未排除的隐患吗？

**没有。** 三条独立证据：
1. `thread_ack` 在 8 核上成功归零 3 次（PREPARE→DISABLE_IRQ→RUN→EXIT），证明
   `ldaddal` 跨 8 核 / 跨 cluster 正确。
2. 原厂 5.4 的 `multi_cpu_stop`、`cpu_stopper_thread` 里 `thread_ack` 与 `nr_todo`
   的 `atomic_dec_and_test` 是**逐字相同的 `ldaddal + add + cbnz`**，与 mainline 同构。
3. §2 漏洞排查：初值都是 8、时序都有屏障保障、无并发写、归零判据唯一。

⇒ `nr_todo` 必归零、`complete()` 必被调用。**H4 正式排除，勿再分析 atomic。**

### 6.2 `ic ialluis` vs errata workaround，哪个更可能是 tick 停摆诱因？

排序（已按本轮 refocus 重排）：

1. **（排除）`ic ialluis`**：原厂同语境(8核 spin)执行同指令跑通 + 早期单核已跑过一次没死
   ⇒ 单核/8核语境差异不构成根因（§3.5）。**降级为「一致性整改项」。**
2. **（需单变量定案，不能纯静态排除）REPEAT_TLBI**：它是唯一"原厂 vs mainline 在 A76 上
   **可能有**差异"的项——若 T770 A76 revision > r3p0，则 mainline 因 `4118414`(ALL_VERSIONS)
   会打 REPEAT_TLBI、原厂(仅 1286807)不打（§5.1）。但它**不落在 tick 处理内**（§5.4），
   因果链上不直接。**优先级最高的回退实验（§5.5 步骤 2a）+ 读 MIDR_EL1 定案。**
3. **（排除）SPECULATIVE_AT**：站点 100% KVM/hyp，guest 未运行，对 boot/tick 无影响。
4. **（仍最可能）H1**：complete 已调、waiter 未醒，上游是「无心跳 = tick/IPI 停摆」。
   若 2a 回退无变化，则锁定 H1——诱因转向**跨核唤醒 IPI（`smp_send_reschedule`/SGI）
   投递**或 stop_machine 后 `local_irq_restore`/GIC 状态，而不是 cache/原子/workaround。

### 6.3 修复建议（按优先级）

1. **读 MIDR_EL1 画屏**（零成本，可并入任意刷机）：确认 A76 revision，判定 §5.1 的
   4118414 差异是否真实生效。
2. **先刷 Flash 1**（重建 + y=440/450）钉死「complete 是否被调 / waiter 是否醒 /
   心跳是否活」三个变量。这是把 H1 从推测变事实的唯一一步。
3. **单变量回退 REPEAT_TLBI（§5.5 步骤 2a）**：临时关 `CONFIG_ARM64_ERRATUM_4118414` +
   `1286807`（+`3194386`）重刷。若 tick 恢复/stop_machine 返回 → REPEAT_TLBI 坐实；
   否则彻底排除 workaround，回 H1。
4. **若确诊 H1**：把排查重心放到 `try_to_wake_up` 跨核路径 → `ttwu_queue` →
   `smp_send_reschedule` → GIC SGI 投递，以及 `multi_cpu_stop` 退出时的
   `local_irq_restore` 是否真正重开了 CPU0 的 IRQ/GIC（可在 `cpu_stopper_thread` 的
   `preempt_count_dec` 后、`goto repeat` 前加探针，再在 `schedule()` 返回处加探针
   确认调度器在推进）。
5. **一致性整改（非根因，但应做）**：`icache_inval_all_pou()` 与文档「dsb(ish)」不一致；
   要么按红线改成 `dsb(ish)`，要么更新红线说明「系统级 alternatives 收尾的 `ic ialluis`
   与原厂一致可保留」。二选一，避免文档与代码长期漂移。

---

## 附录：本报告用到的原厂地址表

| 符号 | 地址 |
|---|---|
| `multi_cpu_stop` | `ffffffc01030a420` |
| `stop_cpus` | `ffffffc01030a60c` |
| `cpu_stopper_thread` | `ffffffc01030adc8` |
| `complete` | `ffffffc010227a70` |
| `__flush_icache_range` | `ffffffc010086e40` |
| `__apply_alternatives_multi_stop` | `ffffffc010114c0c` |
| `__apply_alternatives` | `ffffffc010114cd8` |
| `apply_alternatives_all` | `ffffffc01214e354` |
| `cpu_hwcaps` | `ffffffc01250c600` |
| `try_to_wake_up` | `ffffffc0101f6084` |
| `swake_up_locked` | `ffffffc010226d90` |
| `__wake_up_common` | `ffffffc010226000` |
