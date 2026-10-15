# T508N 任务 A（最终版）：原厂 5.4 vs mainline 6.12 的 completion 唤醒 + 系统级 alternatives 收尾 + smp_cpus_done→do_basic_setup 窗口对比

> 范围：只读反汇编 + 源码对比。符号表一律 `/home/test/t508n/work/stock_kallsyms.map`（**不用 work/System.map**）。
> 反汇编：`aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000 work/stock_Image_full.bin`
> （`file_off = vaddr - 0xffffffc010080000`）。
> 依据：`REPORT-AGENTS-GUIDE-analysis.md`（H1/H2/H3/H4 归因 + 两处更新）、`REPORT-stock-sync-mechanism.md`。

---

## 0. 结论摘要（TL;DR）

1. **completion 原语不是差异点（H3 排除，直接引用已坐实结论）**：原厂 5.4 `completion`
   = 传统 waitqueue，`complete()` 走 `__wake_up_common`（`blr entry->func`），
   mainline 6.12 = swait（`swake_up_locked`→`try_to_wake_up`）。两者都收敛到
   `try_to_wake_up`，单 waiter（CPU0 kernel_init）场景行为等价。§1/§2/§6 只作引用。

2. **`ic ialluis` 已弱化（引用 GUIDE 更新）**：早期 LSE patch 的 `__apply_alternatives`
   收尾已执行过一次 `ic ialluis` 且 boot 顺利到 smp_cpus_done，故它不是可靠 hang。
   §4 仅保留「原厂 patch 收尾序列 + DIC 触发条件」作为「原厂同样路径为什么 tick 不断」的
   事实基础。

3. **smp_cpus_done→do_basic_setup 窗口：原厂与 mainline 结构同源，无 tick/irq/sched 特殊保护**
   （§5）。原厂没有「系统级 patch 后恢复中断/TLB」的特殊动作——`local_irq_enable()` 在
   `start_kernel`、tick 在 `start_kernel`（`time_init`/`tick_init`）就已就位并贯穿
   `smp_cpus_done`。

4. **最关键的、可验证的差异 = errata workaround 覆盖范围（§7）**：
   - 原厂 5.4 只对 **Cortex-A76 r0p0-r3p0** 做 repeat-TLBI（1286807）、
     只对 **A76 r0p0-r2p0** 做 speculative-AT（1165522）。
   - mainline 6.12 额外开启 **4118414**（repeat-TLBI 覆盖 **全部 A76 及 A77/A78/X1..X4/N1..V3**）
     和 **1530923**（speculative-AT 覆盖 **A55 r0p0-r2p0**）。
   - 即：若 T770 的 A76 是 r4p0+ / A55 落在 r0p0-r2p0，则 **mainline 会在中断/上下文切换/TLB
     高频路径上首次打上原厂从未打过的 repeat-TLBI/speculative-AT patch**。这是「原厂 tick
     不断、mainline tick 停摆」最可能的上游差异，建议优先单变量验证（§8）。

---

## 1. 原厂 5.4 completion = 传统 waitqueue（引用结论，不重复反汇编）

已坐实（GUIDE 更新 1）：stock `complete`（`ffffffc010227a70`）：
`casa` 抢 `wait.lock` → `done++` → 遍历 wait_queue 链表 → `blr entry->func`
（`autoremove_wake_function`，带 CFI 类型校验符号
`__typeid__ZTSFiP16wait_queue_entryjiPvE_global_addr`）。
mainline 6.12 = swait。**waitqueue→swait 迁移不是根因。**

关键符号（速查）：

| 符号 | 地址 |
|---|---:|
| `complete` / `complete_all` | `ffffffc010227a70` / `227898` |
| `__wake_up_common` | `ffffffc010226000` |
| `default_wake_function` | `ffffffc0101fa220` |
| `autoremove_wake_function` | `ffffffc010225640` |
| `try_to_wake_up` / `wake_up_process` | `ffffffc0101f6084` / `1f6060` |
| `smp_send_reschedule` | `ffffffc010117470` |
| `wait_for_common` / `wait_for_completion` | `ffffffc0116a0610` / `6a0d30` |

---

## 2. `complete()` 唤醒语义对比（简表）

| 维度 | 原厂 5.4（waitqueue） | mainline 6.12（swait） |
|---|---|---|
| 唤醒数 | `__wake_up_common(wq, TASK_NORMAL, 1, 0, NULL)` → 1 个 exclusive | `swake_up_locked(q, 0)` → `try_to_wake_up(队头)` → 1 个 |
| 路径 | `complete → __wake_up_common → default/autoremove_wake_function → try_to_wake_up` | `complete → complete_with_flags → swake_up_locked → try_to_wake_up` |
| 单 waiter | 等价 | 等价 |

**结论：都汇入 `try_to_wake_up`，唤醒个数差异在 stop_machine（单 waiter）场景不致命。**

---

## 3. 原厂 icache 失效实现：只用 `ic ivau`（重聚焦任务 1）

`__flush_icache_range`（`ffffffc010086e40`，与 `__flush_cache_user_range` 同址）：

```asm
ffffffc010086e94: dc   cvau, x4        ; 数据清到 PoU（按 VA，line=4<<DminLine）
...
ffffffc010086ecc: ic   ivau, x3        ; 指令缓存按 VA 失效（line=4<<IminLine）
ffffffc010086edc: dsb  ish
ffffffc010086ee0: isb
```

`invalidate_icache_range`（`ffffffc010086f20`）：

```asm
ffffffc010086f78: ic   ivau, x3        ; 纯 ic ivau 循环
ffffffc010086f88: dsb  ish
ffffffc010086f8c: isb
```

**结论：原厂 range 级 icache 失效只用 `ic ivau`（PoU）+ `dsb ish` + `isb`，无 `ic ialluis`。
全内核唯一 `ic ialluis` 在 `__apply_alternatives` 尾部（§4），条件执行。**

---

## 4. 原厂 `__apply_alternatives` 尾部条件 `ic ialluis`（重聚焦任务 1/4）

`__apply_alternatives` @ `ffffffc010114cd8`，patch 循环结束后：

```asm
ffffffc010114e94: tbnz w22, #0, 0x...4ed4   ; w22 = is_module：模块 → 直接返回，跳过 icache 维护
ffffffc010114e98: dsb  ish
ffffffc010114e9c: b    0x...4f00

ffffffc010114f00: adrp x8, 0xffffffc01250c000
ffffffc010114f04: ldr  x8, [x8, #1536]       ; cpu_hwcaps[0]（cpu_hwcaps @ 0xffffffc01250c600）
ffffffc010114f08: tbnz w8, #28, 0x...4eac    ; bit28 = ARM64_HAS_CACHE_DIC(28) 置位 → 跳过
ffffffc010114f0c: b    0x...4ea4

ffffffc010114ea4: ic   ialluis               ; ← 唯一 ic ialluis（广播，仅 !DIC 时执行）
ffffffc010114ea8: dsb  ish
ffffffc010114eac: isb
; 之后 0x...4eb0~0x...4ed0 更新 applied_alternatives 位图，返回
```

**触发条件 = `!is_module && !ARM64_HAS_CACHE_DIC`**（5.4.210 cpucaps.h：`ARM64_HAS_CACHE_IDC=27`、
`ARM64_HAS_CACHE_DIC=28`；DIC 由 `CTR_EL0.DIC`(bit29) 决定）。

**与 mainline 6.12 对比**：`icache_inval_all_pou()`（`cacheflush.h:133`）语义相同
（`alternative_has_cap_unlikely(DIC)`）。**原厂也有条件 `ic ialluis`，不是 mainline 独有回归。**

> ⚠️ 结合 GUIDE 更新 2：早期 LSE patch 的 `__apply_alternatives` 已执行过一次 `ic ialluis`
> 且顺利到 smp_cpus_done，说明 `ic ialluis` 并非可靠 hang。因此本条件只用于回答
> 「原厂同样路径为什么 tick 不断」——答案核心在 §7 的 workaround 覆盖差异，而不是 `ic ialluis`。

---

## 5. 原厂 `smp_cpus_done → do_basic_setup` 窗口（重聚焦任务 4，重点）

### 5.1 `local_irq_enable` / tick 的时机（原厂 == mainline）

`start_kernel`（stock @ `ffffffc012148ab8`）的标准顺序（与 mainline 6.12 `init/main.c` 一致）：

```
early_irq_init()   (stock ffffffc01215a940)
init_IRQ()         (stock ffffffc01214c810)
tick_init()        (stock ffffffc01215def8)
time_init()        (stock ffffffc01214d5a0)   # arch timer 在此注册/启动
...
local_irq_enable()                             # 在 rest_init 之前
rest_init() → kernel_init → kernel_init_freeable()
```

**即：进入 `kernel_init_freeable → smp_init → smp_cpus_done` 时，中断已开、arch timer tick
已在跑。原厂与 mainline 完全一致，没有「tick 晚启动」的差异。**

### 5.2 原厂 `smp_cpus_done` 反汇编（`ffffffc01214e4b8`）

```asm
ffffffc01214e4d4: bl printk               ; "SMP: Total of %d processors activated"
ffffffc01214e4d8: bl 0x...14e198          ; setup_cpu_features
ffffffc01214e4dc: bl 0x...14e50c          ; hyp_mode_check
ffffffc01214e4f8: bl 0x...30a2f0          ; stop_machine(fn=__apply_alternatives_multi_stop, NULL, cpu_online_mask)  ← apply_alternatives_all 内联
ffffffc01214e4fc: bl 0x...150948          ; mark_linear_text_alias_ro
```

原厂 `setup_cpu_features`（`ffffffc01214e198`）内部调用序列：

```asm
ffffffc01214e1a8: bl update_cpu_capabilities   (ffffffc0101129f4)
ffffffc01214e1b0: bl enable_cpu_capabilities   (ffffffc01214e0a0)   # 含 KPTI cpu_enable 回调
ffffffc01214e1b4: bl cpus_read_lock
ffffffc01214e1c0: bl static_key_enable_cpuslocked
ffffffc01214e1c4: bl cpus_read_unlock
ffffffc01214e1d0: bl setup_elf_hwcaps
...  printk ×2、setup_elf_hwcaps、cap_set_elf_hwcap ...
```

**注意：原厂 `setup_cpu_features` 内没有 stop_machine**（KPTI 的 `kpti_install_ng_mappings`
在 5.4 是 `enable_cpu_capabilities` 的 cpu_enable 回调，走
`cpu_install_idmap()/cpu_uninstall_idmap()` 直接 remap，**不用 stop_machine**）。

### 5.3 mainline 6.12 `smp_cpus_done`（`arch/arm64/kernel/smp.c:447`）

```c
void __init smp_cpus_done(unsigned int max_cpus)
{
	pr_info("SMP: Total of %d processors activated.\n", num_online_cpus());
	hyp_mode_check();
	setup_system_features();   // → setup_system_capabilities(): update+enable caps + apply_alternatives_all()
	                           //   → kpti_install_ng_mappings(): stop_machine(__kpti_install_ng_mappings)
	                           //   → sve_setup/sme_setup
	setup_user_features();
	mark_linear_text_alias_ro();
}
```

其中 6.12 的 `kpti_install_ng_mappings`（`cpufeature.c:1953`）**用 `stop_machine(...)`**。

### 5.4 窗口内差异表

| 项 | 原厂 5.4 | mainline 6.12 |
|---|---|---|
| `local_irq_enable` 时机 | `start_kernel`（rest_init 前） | 同 |
| tick 启动时机 | `start_kernel`（time_init/tick_init） | 同 |
| 系统级 alternatives | `smp_cpus_done` 内 `apply_alternatives_all`（stop_machine） | 同（在 `setup_system_capabilities` 内） |
| KPTI NG mapping | `enable_cpu_capabilities` 的 **cpu_enable 回调，不用 stop_machine** | `setup_system_features` 里 **显式 stop_machine** |
| KPTI vs alternatives 顺序 | KPTI **先**、alternatives **后** | alternatives **先**、KPTI **后** |
| stop_machine 次数（该窗口） | **1 次**（仅 alternatives） | **2 次**（alternatives + KPTI） |
| 系统级 patch 后的「恢复中断/TLB」动作 | **无**（multi_cpu_stop 的 `local_irq_restore` 恢复 IRQ；KPTI 已在前完成） | **无专门恢复**；但 KPTI 收尾带 `local_flush_tlb_all()`（在 alternatives 之后） |

### 5.5 回答「原厂同样路径为什么 tick 不断 / 有没有恢复动作」

- 原厂**没有**在系统级 patch 后做任何「重新 local_irq_enable / 重启 tick / 刷 TLB」的特殊动作；
  tick/中断在 `start_kernel` 就绪后一路贯穿 `smp_cpus_done`，唯一的中断开关是
  `multi_cpu_stop` 的 per-CPU `local_irq_disable/hard_irq_disable → local_irq_restore`。
- 原厂 tick 不断，**不是**因为它在 `smp_cpus_done` 后多做了什么，而是因为
  **它打进去的 alternatives 内容（§7）不包含 mainline 新增的、作用于 A76 r4p0+/A55 的
  repeat-TLBI/speculative-AT patch**。原厂在 T770 上跑的 TLB/中断路径是「未打这些补丁」的
  裸路径，天然不会被这些 patch 破坏。

---

## 6. 原厂 complete/waitqueue vs mainline swait：唤醒后重新调度层（重聚焦任务 3）

| 层 | 原厂 5.4 | mainline 6.12 |
|---|---|---|
| complete 侧 | `__wake_up_common → default/autoremove_wake_function → try_to_wake_up` | `swake_up_locked → try_to_wake_up` |
| waiter 侧 | `prepare_to_wait（wait_queue_entry 入队）→ set_current_state → schedule` | `__prepare_to_swait（swait_queue 入 task_list）→ set_current_state → schedule` |
| 跨核唤醒 | `try_to_wake_up → ttwu_queue → smp_send_reschedule(cpu)`（**SGI**） | 完全相同 |
| reschedule IPI | `smp_send_reschedule`（`ffffffc010117470`）：`(cpu&63)+1` 算 SGI 目标 → GIC SGI | 标准 arm64 同源 |

**结论：唤醒后重新调度层无差异，都汇入 `try_to_wake_up` + SGI。completion 原语不是丢唤醒点
（H3 排除）。若 waiter 不醒，问题在 `try_to_wake_up` 之后的调度器/IPI 是否推进（H1）。**

---

## 7. 最可疑差异：errata workaround 覆盖范围（重聚焦任务 4 的核心结论）

### 7.1 配置对比（`work/kernel.config` vs `linux-6.12.103/.config`）

| workaround | 原厂 5.4 config | mainline 6.12 config | 作用 |
|---|---|---|---|
| repeat-TLBI 1286807 | **y** | y | A76 **r0p0-r3p0** |
| repeat-TLBI 4118414 | **无** | **y** | **全部 A76** + A77/A78/X1..X4/N1..V3 |
| speculative-AT 1165522 | **y** | y | A76 **r0p0-r2p0** |
| speculative-AT 1530923 | **无** | **y** | **A55 r0p0-r2p0** + Kryo4xx Silver |

### 7.2 MIDR 覆盖范围（mainline 6.12 `arch/arm64/kernel/cpu_errata.c`）

- **1286807**（`cpu_errata.c:208`）：`ERRATA_MIDR_RANGE(MIDR_CORTEX_A76, 0,0, 3,0)` —— 仅 A76 r0p0-r3p0。
- **4118414**（`cpu_errata.c:228`）：`MIDR_ALL_VERSIONS(MIDR_CORTEX_A76)` 及 A76AE/A77/A78/X1..X4/N1/N2/V1..V3 等 —— **覆盖全部 A76 修订**。
- **1165522**（`cpu_errata.c:375`）：`MIDR_RANGE(MIDR_CORTEX_A76, 0,0, 2,0)` —— 仅 A76 r0p0-r2p0。
- **1530923**（`cpu_errata.c:383`）：`MIDR_RANGE(MIDR_CORTEX_A55, 0,0, 2,0)` —— **A55 r0p0-r2p0**。

### 7.3 结论

- T770 = A76（大核）+ A55（小核）。若其 A76 修订 **r4p0+**、A55 修订 **r0p0-r2p0**，
  则：
  - 原厂 5.4：1286807（≤r3p0）、1165522（≤r2p0）**都不命中** → **不打** repeat-TLBI /
    speculative-AT → 中断/上下文切换/TLB 路径是**未 patch 的裸路径**。
  - mainline 6.12：4118414（全部 A76）、1530923（A55 r0-r2）**命中** → **首次**在
    中断/TLB 高频路径打上 repeat-TLBI（99 条）+ speculative-AT（24 条）。
- 这直接解释了「8 核 signal_done 但 waiter 不醒 + 无心跳」：**apply_alternatives_all 首次把
  这些 workaround patch 进中断/调度路径**，若 patch 的指令序列/TLB 时序与 T770 的 DSU/一致性
  互联或 arch timer 交互有问题，最典型表现就是「stop_machine 之后 tick/调度停摆」。

> 注：这是「workaround 内容」差异，不是「apply_alternatives 机制」差异——两者的机制同源
> （stop_machine + multi_cpu_stop + 同一 LSE 原子 + 同一 completion 收敛点），已在
> `REPORT-stock-sync-mechanism.md` / `REPORT-stock-vs-mainline-locks.md` 坐实。

---

## 8. 建议（单变量验证，按性价比）

1. **首选**：临时关掉 `CONFIG_ARM64_ERRATUM_4118414`（及 `1530923`）重建对照刷机。
   - 若恢复 → 根因 = mainline 对 A76 r4p0+/A55 首次启用的 repeat-TLBI/speculative-AT。
   - 这是「原厂没有、mainline 有」的最干净单变量。
2. **次选**：读 `CTR_EL0.DIC`（bit29）画一格，彻底关闭 `ic ialluis` 的残余嫌疑
   （GUIDE 已弱化，但零成本确认 DIC=1 可排除 §4 的最后一层）。
3. **不要**回退 completion 的 swait（§1/§6 已证不是差异点）；**不要**再把
   `icache_inval_all_pou()` 改 `dsb(ish)` 当「对齐原厂」——那是偏离（§4）。
4. 若 Flash 2 已证明「`swake_up_locked` 队列非空且 `try_to_wake_up` 已调、waiter 仍不醒」，
   则问题 100% 在调度器/IPI 推进，与 §7 的 workaround patch 作用路径一致，进一步坐实 §7。

---

## 附录：关键地址速查

| 符号 | 地址 |
|---|---:|
| `start_kernel` / `rest_init` | `ffffffc012148ab8` / `ffffffc011698490` |
| `kernel_init_freeable` / `do_basic_setup` | `ffffffc012148288` / `148598` |
| `smp_cpus_done` / `setup_cpu_features` / `hyp_mode_check` | `ffffffc01214e4b8` / `14e198` / `14e50c` |
| `update_cpu_capabilities` / `enable_cpu_capabilities` | `ffffffc0101129f4` / `ffffffc01214e0a0` |
| `apply_alternatives_all` / `apply_boot_alternatives` | `ffffffc01214e354` / `14e388` |
| `mark_linear_text_alias_ro` | `ffffffc012150948` |
| `kpti_install_ng_mappings` | `ffffffc0101133fc` |
| `early_irq_init` / `init_IRQ` / `time_init` / `tick_init` | `ffffffc01215a940` / `14c810` / `14d5a0` / `15def8` |
| `smp_init` / `sched_init_smp` / `smp_prepare_cpus` | `ffffffc01215e7d8` / `1577fc` / `14e9bc` |
| `__apply_alternatives_multi_stop` / `__apply_alternatives` | `ffffffc010114c0c` / `114cd8` |
| `__flush_icache_range` / `invalidate_icache_range` | `ffffffc010086e40` / `086f20` |
| `cpu_hwcaps` | `ffffffc01250c600`（bit28 = ARM64_HAS_CACHE_DIC） |
| `complete` / `__wake_up_common` / `try_to_wake_up` / `smp_send_reschedule` | `ffffffc010227a70` / `226000` / `1f6084` / `117470` |
