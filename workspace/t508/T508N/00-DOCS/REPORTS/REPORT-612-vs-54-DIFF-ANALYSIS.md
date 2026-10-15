# REPORT-612-vs-54-DIFF-ANALYSIS — 6.12 与 5.4 逐项差异分析（CPU1 PSCI CPU_ON 卡点）

> 生成：2026-10-05。只做源码/反汇编/配置静态对照，不刷机、不读 session.jsonl。
> 已知卡点（2026-08-21 实测）：
> `__cpu_up(1)` → `boot_secondary(1)` → PSCI CPU_ON SMC 不返回；
> CPU1 从未到达 `secondary_start_kernel`（y=180/185 不亮），CPU0 停在 y=175。

---

## 0. 结论先行

5.4 复刻与 mainline 6.12 跑在同一台设备、同一 SML/ATF、同一 vendor_boot DTB 上：

```text
5.4：8 核上线 + /init 用户态 ✅
6.12：CPU1 CPU_ON SMC 不返回 ❌
```

因此卡点基本可锁定在 **6.12 内核侧的二级核 entry/idmap/SMP 路径**，而不是 SML/ATF/DTB/硬件。

当前证据下，最大嫌疑按优先级：

| 优先级 | 差异点 | 证据 | 为什么可疑 |
|---|---|---|---|
| **P0** | 6.12 `create_idmap()` 的二级核 idmap 自定义改动 | `mmu.c` diff：`idmap_ptes` 扩大、fb 映射复用最后一页、验证代码 | **只影响 secondary CPU**；CPU1 用的就是 `idmap_pg_dir`；改动晚于 8 核成功点 |
| **P1** | 6.12 `secondary_entry` 自定义 `dcache_inval_poc(idmap_pg_dir)` | `head.S` diff：clean 6.12 无此段 | 只在 CPU1 入口执行；放在 `init_kernel_el` 前，假设 MMU/cache 状态 |
| P2 | 6.12 `__cpu_setup` 新代码（TCR2/PIRE、cpacr=0、AMU 等） | `proc.S` clean vs 5.4 | CPU0 也走同一段，但 CPU1 缓存/页表上下文不同，可能放大 |
| P3 | 6.12 `init_kernel_el` 重构 | clean 6.12 与 5.4 `el2_setup` | CPU0 同样路径，理论上应一致 |
| P4 | 早期 LSE static_key / alternatives | `cpufeature.c`、`alternative.c` | 历史 D2 实验已强制 LL/SC，仍卡 → 大体排除 |
| P5 | config/DTS/拓扑差异 | `.config`、DTSI | 同一 vendor_boot DTB 下 5.4 能跑；P1 轮也排除过 DTB |

---

## 1. 相同点（先排除掉的）

| 项 | 5.4 | 6.12 | 判定 |
|---|---|---|---|
| PSCI CPU_ON 调用约定 | `0xc4000003`（SMC64） | `0xc4000003`（SMC64） | 相同 |
| `psci` DTS 节点 | `arm,psci-0.2` + `method="smc"` | 相同 | 相同 |
| CPU enable-method | `psci` | `psci` | 相同 |
| `cpu_psci_cpu_boot` 参数 | `cpu_logical_map(cpu), __pa_symbol(secondary_entry)` | 相同 | 相同 |
| 实际刷入的 `vendor_boot` DTB | 旧 vendor DTB | 旧 vendor DTB（本轮未刷 vendor_boot） | 5.4 也能跑，排除 DTB |
| SML/ATF | 同一固件 | 同一固件 | 排除“SML 不认 SMC64”的旧假设 |

6.12 `psci_0_2_cpu_on` 反汇编确认：

```text
mov x0, #3
movk x0, #0xc400, lsl #16    // 0xc4000003 SMC64 CPU_ON
blr invoke_psci_fn
```

5.4 `psci_cpu_on` 运行时同样会经 `psci_0_2_set_functions()` 写入
`PSCI_FN_NATIVE(0_2, CPU_ON)`，64 位下即 `0xc4000003`。

---

## 2. P0：6.12 二级核 idmap 自定义改动

### 2.1 clean 6.12 的 `secondary_entry`

```asm
SYM_FUNC_START(secondary_entry)
    mov x0, xzr
    bl  init_kernel_el
    b   secondary_startup
SYM_FUNC_END(secondary_entry)
```

### 2.2 当前 6.12 树改成了

```asm
SYM_FUNC_START(secondary_entry)
    // T508N: invalidate idmap 页表 cache, 防止 SML 残留 stale 数据
    adrp x0, idmap_pg_dir
    add  x1, x0, #PAGE_SIZE
    adr_l x2, dcache_inval_poc
    blr  x2
    mov x0, xzr
    bl   init_kernel_el
    b    secondary_startup
SYM_FUNC_END(secondary_entry)
```

差异点：

- 这段是 **secondary-only**，CPU0 primary entry 不走；
- 它位于 `init_kernel_el` 之前，OS 假定此时 CPU1 处于“可做 cache 维护、MMU/cache 状态已知”的上下文；
- 如果 CPU1 进入时的 cache/MMU 状态与假设不符，可能在第一条 `dcache_inval_poc` 就异常，
  而 SML 可能在等待 CPU1 启动握手 → SMC 不返回。

### 2.3 `create_idmap()` 的自定义改动（更可疑）

当前 6.12 相对 clean 6.12：

1. `idmap_ptes` 从 `[IDMAP_LEVELS - 1][PAGE_SIZE]` 扩大为 `[IDMAP_LEVELS][PAGE_SIZE]`；
2. 额外给 **secondary idmap** 加了 `0xb0000000..0xb0100000` 的 framebuffer 映射；
3. 额外对 `idmap_pg_dir` / `idmap_ptes` 做 `dcache_clean_poc`；
4. 增加大量 `t508n_idmap_*` 验证代码，直接读 `idmap_ptes[2]` 并扫描。

关键风险：

- `create_idmap()` 生成的 `idmap_pg_dir` 是 **CPU1 开启 MMU 时直接用**的页表；
- CPU0 primary boot 用的是另一个 `init_idmap_pg_dir`，所以这类 bug **不会让 CPU0 挂**；
- 自定义 fb 映射把 `idmap_ptes` 的最后一页同时用作新映射的页表页，
  同时 `map_range()` 内部还会继续 `*pte += PAGE_SIZE`；
- 这有“页表页复用/越界分配”与“PMD 表共享”的风险，可能让 `.idmap.text` 或
  `secondary_entry` 的映射在 CPU1 侧变成错误条目；
- 一旦 CPU1 开 MMU 后取指/数据异常，SML 的 CPU_ON 握手可能等不到，表现为 CPU0 SMC 不返回。

时间线旁证：

```text
2026-08-16 00:11  6.12 曾实现 8 核上线（见 FINAL-BRINGUP-INDEX §2）
2026-08-16 14:30  mmu.c 自定义 idmap/fb 映射时间戳
2026-08-16 16:10  head.S secondary_entry 自定义 invalidate 时间戳
2026-08-16 22:53  HEAD 提交（baseline，含这些改动）
```

即：**8 核成功发生在这组 secondary idmap 改动之前**。这是目前最强的回归相关性证据。

---

## 3. P2：`__cpu_setup` 6.12 vs 5.4

6.12 clean `__cpu_setup` 新代码：

- 先 `msr cpacr_el1, xzr`（5.4 写 `0x300000`）；
- 读取 `ID_AA64PFR0_EL1`，有 AMU 则清 `amuserenr_el0`；
- MAIR/TCR 默认值按新架构组合；
- 读 `SYS_ID_AA64MMFR3_EL1.S1PIE`，非零才写 `PIRE0_EL1/PIR_EL1/TCR2_EL1`；
- 最终返回 `INIT_SCTLR_EL1_MMU_ON`。

评估：

- CPU0 primary path 也调用 `__cpu_setup`，若这些指令在 T770 上非法，CPU0 也应挂；
- 但 CPU1 是 **MMU off 冷启动 + secondary entry**，若 `__cpu_setup` 后的 SCTLR/TCR 与
  CPU0 不完全同上下文，仍可能出问题；
- 这条优先级低于 P0/P1，但应作为后续单变量实验之一。
- 还要确认 `SYS_ID_AA64MMFR3_EL1` 在 T770 上是否可读；
  如果不可读，clean 6.12 在 CPU0 就会挂，但当前 CPU0 能跑，说明 CPU0 侧没问题；
  CPU1 与 CPU0 走同一函数，理论上相同。

---

## 4. P3：`init_kernel_el` 重构

6.12 用 `init_kernel_el` 取代 5.4 的：

```asm
bl el2_setup
bl set_cpu_boot_mode_flag
```

`init_kernel_el` 内部包含 EL2/EL1 状态处理、HCR/CPTR/CNTHCTL 等配置。
CPU0 primary path 也走它，因此“指令本身导致 CPU1 挂”的可能性较低；
但 secondary entry 调用时 `x0=0`（MMU off）与 CPU0 的启动状态不同，
若 SML 释放 CPU1 时的 EL 与 6.12 假设不一致，仍可能在 `init_kernel_el` 内异常。
优先级低于 P0/P1。

---

## 5. P4：LSE / alternatives

历史 D2 实验：

```text
强制 system_uses_lse_atomics() == false
→ 仍然 9 格 + y=175 CPU1
```

因此当前卡点**不是 LSE 指令混用直接导致**。
但注意：D2 只排除了 LSE 语义分支，不等于排除了：

- `t508n_apply_lse_early()` 中 `static_branch_enable()` 的 IPI/kick_all_cpus_sync 时序；
- alternatives multi_stop 对 `idmap_pg_dir` 或 early text 的影响。

因此仍建议在 P0/P1 实验后再做：

```text
不调用 t508n_apply_lse_early()
```

单变量测试。

---

## 6. P5：config / DTS / 其他自定义

| 差异 | 说明 | 判定 |
|---|---|---|
| `CONFIG_RANDOMIZE_BASE` | 5.4=y，6.12=n | 6.12 更简单，不应是卡点 |
| AMU/MTE/errata | 6.12 多几项 | CPU0 同路径；先降级处理 |
| DTS 三簇 vs 旧 DTB | 本轮 6.12 实际仍用旧 vendor DTB | 已排除 |
| `ttwu_queue_cond` P1 | 曾测试无效 | 排除 |
| stop_machine/alternatives 探针 | 只是显示/计数 | 不是卡点 |
| NOMAP `pfn_valid` | mm 修复 | 不影响 CPU1 entry |

---

## 7. 建议的单变量实验顺序

### 实验 A（最高优先）：移除 secondary idmap 自定义 fb 映射

目标：只动 `create_idmap()` 中新增的 fb 映射块，保留 `dcache_clean_poc` 和 CPU1 invalidate。
观察：

```text
y=130 是否亮
y=175/180/185 是否变化
```

若 CPU1 上线 → 基本锁定是该 fb 映射/页表页复用问题。

### 实验 B：移除 `secondary_entry` 的 `dcache_inval_poc`

目标：恢复 clean 6.12：

```asm
SYM_FUNC_START(secondary_entry)
    mov x0, xzr
    bl  init_kernel_el
    b   secondary_startup
```

保留 `create_idmap()` 的 CPU0 `dcache_clean_poc`。
若 CPU1 上线 → 锁定 CPU1 入口 cache 操作。

### 实验 C：加入 CPU1 早期物理地址探针

利用树中已有 `t508n_sec_paint_xy` 宏，在：

```text
secondary_entry 第 1 条指令前
custom dcache_inval_poc 之后
init_kernel_el 之后
__cpu_setup 之后 / __enable_mmu 之后
```

各画不同 x 位置小块。判读：

```text
全不亮        -> CPU1 根本没执行到 entry（SML/CPU_ON 侧）
只亮第 1 个   -> 死在 custom dcache_inval
亮到第 2 个   -> 死在 init_kernel_el
亮到第 3 个   -> 死在 __cpu_setup / MMU enable
全亮但 y180 不亮 -> 死在 secondary_start_kernel 之前
```

这是最直接定位“CPU1 死在哪一条”的办法。

### 实验 D：`t508n_apply_lse_early()` 不调用

若 A/B/C 均未定位，再做。

---

## 8. 未验证/保留

- 当前分析为静态对照，未在真机验证；所有“嫌疑”必须通过 A/B/C/D 单变量实验确认；
- SML/ATF 侧未重新验证；5.4 同一 SML 能启 CPU1，因此先假设内核侧；
- 完整 SML 256KB 镜像与 CPU_ON handler 仍未定位，作为最终兜底；
- 本文未读取 `session.jsonl`。

---

## 9. 实验 C 真机实施记录（2026-10-05 21:07）

### 9.1 已插入的 CPU1 early probes

`arch/arm64/kernel/head.S`：

```text
secondary_entry:
    bti c
    t508n_sec_paint_xy 8, 160      // CPU1 进入 entry
    ... custom dcache_inval_poc(idmap_pg_dir) ...
    t508n_sec_paint_xy 32, 160     // 通过 idmap cache invalidate
    ... bl init_kernel_el ...
    t508n_sec_paint_xy 56, 160     // 通过 EL2/EL1 初始化

secondary_startup:
    ... bl __cpu_setup ...
    t508n_sec_paint_xy 80, 160     // 通过 __cpu_setup
    ... bl __enable_mmu ...
    t508n_sec_paint_va_xy 104, 160 // 通过 __enable_mmu
```

所有探针画在 y=160，x=8/32/56/80/104，不改变功能逻辑。

### 9.2 镜像

```text
build/boot-images/boot_612_expC.img
md5    = e2ae55830a72657e2773e9900547cb68
kernel = f4950a1ddefbcdbe0d1521c185c347f9
```

### 9.3 刷入

```text
[21:07:04] fastboot device: T6PA03CJ3EH02ZW fastboot
[21:07:04] current-slot: 'a'
[21:07:04] flashing boot_a (6.12 Experiment C)
Sending 'boot_a' (65536 KB)   OKAY [  1.778s]
Writing 'boot_a'              OKAY [  0.418s]
Finished. Total time: 2.206s
[21:07:07] rebooting
```

### 9.4 判读表

| y=160 亮块 | 结论 |
|---|---|
| x=8 不亮 | CPU1 未执行到 entry（SML/CPU_ON/入口地址侧） |
| 只亮 x=8 | 死在 custom dcache_inval_poc |
| 亮 x=8,32 | 死在 init_kernel_el |
| 亮 x=8,32,56 | 死在 __cpu_setup |
| 亮 x=8,32,56,80 | 死在 __enable_mmu / secondary idmap |
| 5 个全亮但 y180 不亮 | 死在 secondary_start_kernel 之前 |

当前状态：**已刷入并 reboot；等待 y=160 探针读屏结果。**

---

## 10. 实验 C 真机读屏结果（2026-10-05 照片分析）

照片：`Image "1791206131214..jpg"`，sha256:83a7a097...；1080x2387。
用 ffmpeg raw 取像素 + scipy 连通域 + 透视反算 FB 坐标，读出的关键状态：

| 行/位置 | 结果 | 判读 |
|---|---|---|
| y=20 stage bar | stage0–8 亮，stage9 灭 | `smp_init` 未完成 |
| y=60 tick | 未绘制（代码里无调用点，不是卡点） | — |
| y=160 ExpC x=8/32/56/80 | 亮（occ 0.67–0.74） | CPU1 已过 entry / idmap inval / init_kernel_el / __cpu_setup |
| y=160 ExpC x=104 | 灭（occ 0.09） | **探针自身问题**：`t508n_sec_paint_va_xy` 硬编码 `0xffffff80b0000000`，而 T508n DRAM 从 0x80000000 起，正确线性别名应为 `0xffffff8030000000`；不是 CPU1 未过 MMU |
| CPU1 y=175 | 亮 | CPU0 已发起 boot_secondary(CPU1) |
| CPU1 y=180 | 亮 | CPU1 已到 `secondary_start_kernel` |
| CPU1 y=185 | 亮 | CPU1 已 `set_cpu_online` |
| CPU1 y=190 | 照片反算块高约 21–23 FB px，倾向亮 | **CPU0 很可能已从 SMC 返回**（待新实验确认） |
| CPU1 y=195 | 灭 | CPU0 很可能卡在 `wait_for_completion_timeout` |
| y=130 | 灭 | `smp_init` 未返回 |
| y=400/460 | 灭 | 未到 do_initcalls/exec init |

**相对上次结论的推进：CPU1 本体已经成功启动并进入
`secondary_start_kernel` / `set_cpu_online`；剩余卡点集中在 CPU0 侧
`boot_secondary` 返回后的 completion 等待/唤醒（或 SMC 仍未真正返回）。**

因此下一步实验目标：用**不重叠的独立 x 坐标**把
`CPU0-SMC返回` / `CPU0-wait返回` / `CPU1-complete返回` 三个点彻底分开。

---

## 11. 实验 C stage2：分离探针（2026-10-06 07:26 已刷入）

### 11.1 设计

为区分“SMC 未返回 / completion 唤醒丢失 / complete 内部卡住”，把原本在
y=175/180/185/190/195 重叠的标记改到不同 x：

| y | x | 事件 | 来源 |
|---|---|---|---|
| 175 | 8 + (cpu-1)*28 | CPU0 发起 boot_secondary | `__cpu_up` |
| 180 | 40 + (cpu-1)*28 | CPU1 到 secondary_start_kernel | `secondary_start_kernel` |
| 185 | 72 + (cpu-1)*28 | CPU1 set_cpu_online | `secondary_start_kernel` |
| 190 | 240 + (cpu-1)*4 | CPU0 boot_secondary 返回 | `__cpu_up` |
| 195 | 268 + (cpu-1)*3 | CPU0 wait_for_completion_timeout 返回 | `__cpu_up` |
| 201 | 8 | CPU1 complete() 已返回 | `secondary_start_kernel` |
| 160 | 8/32/56/80/104 | ExpC 五级进度 | `head.S` |

同时修正 `secondary_startup` 中 x104 标记：从错误线性 VA
`t508n_sec_paint_va_xy` 改为走 secondary idmap 恒等映射的
`t508n_sec_paint_xy`（`create_idmap()` 已把 0xb0000000 映射为 NC）。

### 11.2 镜像与刷入

```text
build/boot-images/boot_612_stage2.img
md5 = d0c72ac1603e9aba3d80e823922ce938

[07:26:53] current-slot: 'a'
Sending 'boot_a' (65536 KB)   OKAY [  1.764s]
Writing 'boot_a'              OKAY [  0.418s]
[07:26:55] rebooting
```

### 11.3 判读矩阵

| y180 x40 | y185 x72 | y190 x244 | y195 x268 | y201 x8 | 结论 |
|---|---|---|---|---|---|
| 亮 | 亮 | 灭 | 灭 | 亮 | CPU1 已上线，但 CPU0 SMC 仍不返回 → SML CPU_ON 未归还 |
| 亮 | 亮 | 亮 | 灭 | 亮 | CPU0 SMC 已返回，卡在 completion 唤醒/超时 → IPI/wakeup 丢失 |
| 亮 | 亮 | 亮 | 灭 | 灭 | CPU0 SMC 已返回，CPU1 卡在 complete() 内部 |
| 亮 | 亮 | 亮 | 亮 | 亮 | 已过该卡点，下一处才是 `smp_init` 返回前 |
| 灭 | — | — | — | — | CPU1 未到 secondary_start_kernel |

---

## 12. 实验 C stage2 真机读屏结果（2026-10-06 07:27 照片）

照片：`IMG_20261006_072726..jpg`，sha256:bae7099c...。
按透视反算（stage bar 定 y=20，Experiment C 行定 y=160，row label 列定 x=120）：

| 位置 | 结果 | 含义 |
|---|---|---|
| y=20 stage0–8 | 亮 | `smp_init` 未完成 |
| y=20 stage9 | 灭 | 同上 |
| y=160 x=8/32/56/80/104 | 长白条覆盖 x≈8–128 | 五级 ExpC 全过（x104 修正后可见） |
| y=175 x=8 | 亮 | CPU0 发起 boot CPU1 |
| y=180 x=40 | 亮 | CPU1 到 `secondary_start_kernel` |
| y=185 x=72 | 亮 | CPU1 `set_cpu_online` |
| y=190 x=244 | 亮 | **CPU0 已从 `boot_secondary`（PSCI CPU_ON SMC）返回** |
| y=195 x=268 | 灭 | **CPU0 未从 `wait_for_completion_timeout` 返回/超时** |
| y=201 x=8 | 亮 | **CPU1 `complete()` 已返回** |

### 结论

卡点已从“SMC 不返回 / CPU1 不启动”推进为：

```text
CPU1 已上线并完成 complete()
CPU0 已从 SMC 返回
CPU0 卡在 wait_for_completion_timeout：
  既没有 completion 唤醒，也没有 5s timeout 唤醒
```

这是典型的 **completion 唤醒/超时路径丢失**，而非 SML/ATF CPU_ON 不归还。
可能方向：
- IPI_RESCHEDULE 唤醒丢失（6.12 自定义 `ttwu_queue_cond` / IPI 路径）；
- CPU0 进入 sleep 后本地 timer/PPI 未送达或 timer wakeup 丢失；
- HZ/tick/softirq 自定义改动。

---

## 13. 实验 C stage3：忙等定位 + 计时器路径探针（2026-10-06 07:30）

在 `__cpu_up` 中加入纯观测忙等，绕过 scheduler completion wait：

| y | x | 含义 |
|---|---|---|
| 202 | 8 | `boot_secondary` 返回错误 |
| 202 | 40 | `boot_secondary` 返回 0 |
| 203 | 8/36/64/92/120 | 忙等循环每秒进度（最多 5 个） |
| 204 | 8 | 忙等期间看到 `cpu_online` |
| 204 | 40 | 忙等 5s 超时 |
| 195 | 268 | 随后仍调用原 `wait_for_completion_timeout` 并返回 |
| 160 | 8/32/56/80/104 | ExpC 五级 |
| 175/180/185/190/201 | 8/40/72/244/8 | stage2 各阶段 |

判读：

| y202 x40 | y203 进度 | y204 x8 | y195 x268 | 结论 |
|---|---|---|---|---|
| 亮 | 亮/增长 | 亮 | — | CPU0 可运行，CPU1 online 可见；scheduler wait 唤醒丢失 |
| 亮 | 亮/增长 | 灭 | — | 忙等 5s 内 CPU0 看不到 CPU1 online（可见性/内存屏障问题） |
| 亮 | 灭 | 灭 | — | CPU0 进入忙等后立即卡死 |
| 灭 | — | — | — | `boot_secondary` 未返回/返回错误 |

镜像：

```text
build/boot-images/boot_612_stage3.img
md5 = e5f8477fe079ac4f188674513d28d3ef
```

当前状态：**stage3 已构建；下一步刷入 boot_a 读屏。**

---

## 14. Stage3 真机结果（2026-10-06 07:32 照片）

| 标记 | 结果 | 含义 |
|---|---|---|
| stage0–8 / stage9 | 亮 / 灭 | `smp_init` 未完成 |
| y160 ExpC 五格 | 全亮 | CPU1 五级全过 |
| y175/180/185/190 | 亮 | SMC 已返回、CPU1 已到 online |
| y202 x40 | 亮 | `boot_secondary` 返回 0 |
| y203 x8、x36 | 亮；x64/92/120 灭 | 忙等循环至少跑了约 1–2s 才退出，说明看到 `cpu_online` |
| y204 x8 亮 / x40 弱 | 倾向“忙等看到 online” | 非 5s 超时 |
| y201 x8 | 亮 | CPU1 `complete()` 已返回 |
| **y195 x268** | **灭** | 随后原 `wait_for_completion_timeout` 仍不返回 |

### 关键结论

1. CPU0 可继续执行、能读 `cpu_online(1)`，也能 `udelay`，所以不是 CPU0 整体卡死。
2. stage2/stage3 两次都确认：
   - `boot_secondary` 已返回；
   - CPU1 已 online 且 `complete()` 已返回；
   - **CPU0 一旦进入 `wait_for_completion_timeout`，既没有 completion 唤醒，也没有 5s 超时唤醒。**
3. 现象等价于：**CPU0 进入 schedule sleep 后收不到任何唤醒事件（IPI/timer）**。
   不是 completion 计数丢失，而是唤醒投递/本地 timer 事件路径有问题。

---

## 15. Stage4：忙等替代 completion wait（workaround + tick 探针）

Stage4 修改 `__cpu_up`：

- 保留 `boot_secondary` 与 y190/y202 标记；
- **删除 `wait_for_completion_timeout`，改为 `udelay(1000)` 忙等最多 5s**，轮询 `cpu_online(cpu)`；
- 在忙等第 1 秒检查 `jiffies` 是否相对 `j0` 推进：
  - y=205 x=8：tick 活着；
  - y=205 x=40：tick 死了；
- y=203 每秒进度：x=8/36/64/92/120；
- y=204 x=8 / x=40：忙等看到 online / 超时；
- y=195 x=268：忙等结束（替代 wait 返回）。

判读：

| y205 | y204 | y130/stage9 | 结论 |
|---|---|---|---|
| x=8 | x=8 | 亮/亮 | tick 正常、只是 completion 唤醒丢失；忙等 workaround 可继续启动 |
| x=40 | x=8 | 灭 | tick 没推进 → arch timer/PPI/wakeup 路径坏，后续必须修 timer 事件 |
| x=40 | x=40 | 灭 | CPU1 online 不可见 + tick 死 |
| x=8 | x=40 | 灭 | tick 活但 CPU1 online 不可见/CPU1 未完成 online |

镜像：

```text
build/boot-images/boot_612_stage4.img
md5 = c3569aaf2cf579416bf00dac3b6a793e
```

当前状态：**stage4 已构建，准备刷入并读屏。**

---

## 16. Stage4 真机结果（2026-10-06 07:37 照片）

对比 stage3/stage4 照片与像素反算：

| 标记 | 结果 | 判读 |
|---|---|---|
| stage0–8 / stage9 | 亮 / 灭 | `smp_init` 仍未完成 |
| y160 ExpC 五格 | 全亮 | — |
| y175/180/185/190 | 亮 | SMC/CPU1 路径保持 |
| y201 x8 | 亮 | CPU1 `complete()` 返回 |
| y202 x40 | 亮 | `boot_secondary` 返回 0 |
| y203 x8（忙等第 0 块） | 疑似亮 | 进了 busy loop 第一拍 |
| y203 x36/x64/x92/x120 | 未确认/偏暗 | 未走完更多拍 |
| y204 x8/x40 | 不明确 | — |
| **y205 x8/x40（tick 探针）** | **未见** | 未到 i=1000 的 tick 检查点 |
| **y195 x268（替代 wait 返回）** | **灭** | CPU0 没走完忙等 |

与 stage3 共同结论：

```text
CPU0 最多走到“刚进入 busy loop / 忙着色几拍”，
随后无法继续向前推进；
不是 wait_for_completion_timeout 单独的计数问题，
而是 CPU0 一旦要等（udelay / scheduler sleep）就停住。
```

Stage4 的忙等使用 `udelay(1000)`，其底层是 arch counter；
如果 arch timer counter 读取异常，`udelay` 本身也可能永久自旋。
因此 next step：**纯 cpu_relax 自旋 + 隔离测试 udelay(1)**。

---

## 17. Stage5：纯自旋 + udelay 隔离（2026-10-06 07:39）

`__cpu_up`：

| y | x | 含义 |
|---|---|---|
| 202 | 8 / 40 | boot_secondary 错误 / 成功 |
| **206** | **8** | **执行 `udelay(1)` 之前** |
| **206** | **40** | **执行 `udelay(1)` 之后（若不亮=udelay 卡死）** |
| 203 | 8 | 纯 cpu_relax 自旋开始 |
| 204 | 8 / 40 | 自旋看到 online / 自旋上限耗尽 |
| 195 | 268 | 自旋结束（替代 completion wait） |
| 201 | 8 | CPU1 complete() 返回 |

镜像：

```text
build/boot-images/boot_612_stage5.img
md5 = e8f8da2c82761469b9b109e7e32f884f
```

判读：

| y206 x8 | y206 x40 | y204 x8 | y195 x268 | 结论 |
|---|---|---|---|---|
| 亮 | 灭 | — | 灭 | `udelay(1)` 在 CPU0 上卡死（arch counter/delay 路径） |
| 亮 | 亮 | 亮 | 亮 | udelay 正常；纯自旋可过 completion 卡点 |
| 亮 | 亮 | 灭 | 亮 | cpu_online 不可见但自旋结束 |
| 亮 | 亮 | 亮 | 灭 | 自旋过 online 后仍卡在后续 paint/section |

---

## 18. Stage5 刷入记录（2026-10-06 07:47）

```text
[07:47:43] fastboot device: T6PA03CJ3EH02ZW
[07:47:43] current-slot: 'a'
Flashing boot_a (6.12 stage5 pure-spin)
Sending 'boot_a' (65536 KB)   OKAY [  1.774s]
Writing 'boot_a'              OKAY [  0.418s]
[07:47:46] rebooting
```

镜像：

```text
build/boot-images/boot_612_stage5.img
md5 = e8f8da2c82761469b9b109e7e32f884f
```

当前状态：**stage5 已刷入，等待 y=206/y=204/y=195 读屏。**

---

## 19. 5.4 与 6.12 的 arch timer / PPI / delay 逻辑对比（2026-10-06）

这一节专门回答：为什么 5.4 能正常等待、6.12 一进入
`wait_for_completion_timeout` / `udelay` 就停。

### 19.1 实测/本地 DTB 事实

| 对象 | timer interrupts | timer-names | 说明 |
|---|---|---|---|
| 实机 vendor_boot DTB（`firmware/stock/vendor_boot.dtb` / `ot5-vendor.dtb`） | `<PPI 13>,<PPI 14>,<PPI 11>,<PPI 10>` | 无 | **4 个 PPI，无 hyp-virt PPI12**；当前设备 vendor_boot 是这一版 |
| 新的 6.12 已知可 8 核 DTB（`vendor_boot_linux.img` 内嵌 160099B） | `<13>,<14>,<11>,<10>,<12>` | sec-phys/phys/virt/hyp-phys/hyp-virt | **5 个 PPI，含 PPI12**；8 核成功时用的就是这版 |

设备当前是 stock 4-PPI DTB；6.12 8 核成功那轮用的是 5-PPI DTB（PPI12）。

### 19.2 5.4 路径

- `src/linux-5.4-custom/.config`：`# CONFIG_ARM64_VHE is not set`
  → `el2_setup` 从 EL2 drop 到 EL1，`is_kernel_in_hyp_mode() == false`。
- `arch_timer_select_ppi()`（5.4，stock 逻辑）：
  - `is_kernel_in_hyp_mode()` false；
  - `is_hyp_mode_available()` true（CPU 从 EL2 启动）→ 跳过 VIRT；
  - 返回 **`ARCH_TIMER_PHYS_NONSECURE_PPI` = PPI14**。
- `arch_counter_register()`：
  - `!is_hyp_mode_available()` false 且 `uses_ppi != VIRT`
  - → 选 **physical counter `arch_counter_get_cntpct`**。
- `arch/arm64/lib/delay.c`（5.4）：
  ```c
  void __delay(unsigned long cycles) {
      cycles_t start = get_cycles();     // <- arch_timer_read_counter()
      ...
  }
  ```
  `get_cycles()` = `arch_timer_read_counter()` = 上面选中的 physical counter。
- 结果：EL1 physical non-secure timer + PPI14 + physical counter，
  定时器、timeout、SGI 唤醒路径都能工作。

### 19.3 6.12 当前路径（stock 4-PPI DTB）

- 6.12 支持 VHE，`finalise_el2` 优先 VHE；T770 A55/A76 上内核运行在 **EL2**
  → `is_kernel_in_hyp_mode() == true`。
- `arch_timer_select_ppi()`（含移植的 03/17 改动）：
  ```c
  if (is_kernel_in_hyp_mode()) {
      if (arch_timer_ppi[ARCH_TIMER_HYP_VIRT_PPI])   // PPI12
          return ARCH_TIMER_HYP_VIRT_PPI;
      return ARCH_TIMER_HYP_PPI;                     // PPI10
  }
  ```
  当前 stock DTB **没有 PPI12**，所以返回 **`ARCH_TIMER_HYP_PPI` = PPI10**。
- `arch_counter_register()`：`uses_ppi == HYP_PPI` → 用 physical counter。
- `arch/arm64/lib/delay.c`（6.12）：
  ```c
  static cycles_t notrace __delay_cycles(void)
  {
      return __arch_counter_get_cntvct_stable();   // 硬编码 CNTVCT
  }
  ```
  **注意：无论 arch_timer 驱动选的是 PPI10/physical counter，delay 都读 CNTVCT。**
- 结果：
  - timer IRQ 走 **HYP_PPI10（EL2 physical timer）**；
  - `udelay` 走 **CNTVCT virtual counter**；
  - 5.4 用的是 EL1 physical non-secure timer/PPI14 + physical counter；
  - 二者 timer 硬件路径不同。

### 19.4 为什么这能解释当前卡点

如果 T770 的 **PPI10（EL2 physical timer）没有正确接入/触发**，
而 6.12 又因为 stock DTB 缺 PPI12 只能回退到 PPI10：

```text
clock event 中断不触发
→ jiffies/timer wheel 不推进
→ wait_for_completion_timeout 的 5s timeout 永远不到期
→ CPU0 在 sleep 中醒不过来完成/等待
```

同时 6.12 `delay.c` 硬编码 CNTVCT，如果该 virtual counter 在当前
VHE/SML 配置下不被允许/不推进，则 `udelay(1)` 也会直接自旋卡死。

这与实测现象一致：

```text
CPU1 已 online 且 complete() 已返回
CPU0 从 SMC 返回
CPU0 既等不到 completion 唤醒，也等不到 timeout
进入 busy loop 后 udelay 也走不动
```

### 19.5 5.4 与 6.12 差异表

| 项 | 5.4 | 6.12 当前（stock DTB） | 6.12 已知可工作（5-PPI DTB） |
|---|---|---|---|
| CPU 运行级别 | EL1（`CONFIG_ARM64_VHE` 未开） | EL2 VHE | EL2 VHE |
| timer PPI | PHYS_NONSECURE=14 | HYP_PPI=10（无 PPI12 回退） | HYP_VIRT=12 |
| clock event | EL1 physical non-secure timer | EL2 physical timer | EL2 virtual timer |
| counter | physical CNTPCT | physical（arch_timer driver）/ CNTVCT（delay.c 硬编码） | virtual CNTVCT |
| delay.c | `get_cycles()` → abstract counter | hardcoded CNTVCT | hardcoded CNTVCT |
| 8 核 bringup | ✅ | ❌ 当前卡点 | ✅（曾成功） |

### 19.6 建议的单变量实验

**实验 1（优先，已知历史成功配置）**：只动 `vendor_boot_a`，刷入
`build/boot-images/vendor_boot_linux.img`（stock vendor_boot + 5-PPI 160099B DTB，
含 PPI12），boot_a 保持当前/任一 6.12 诊断镜像。

- 期望：6.12 VHE 选择 `HYP_VIRT=PPI12`，EL2 virtual timer 工作；
  jiffies/timeout/completion 唤醒恢复。
- 安全：只写 vendor_boot_a，b 槽不动；dtbo_a 保持 noop/stock 不动。

**实验 2（若仍不工作）**：把 6.12 `arch/arm64/lib/delay.c` 改成 5.4 式
`get_cycles()`/`arch_timer_read_counter()`，排除 delay.c 硬编码 CNTVCT 的问题。

**实验 3**：加 DAIF / `timer_handler` 入口 / `do_handle_IPI` 入口探针，
确认 stage5 后 PPI/SGI 是否真的没到。

---

## 20. 已刷入 5-PPI DTB（vendor_boot_a）— 2026-10-06 07:53

按 5.4 vs 6.12 arch timer 对比结论，执行单变量实验：

```text
[07:53:39] current-slot: 'a'
fastboot flash vendor_boot_a C:\platform-tools\vendor_boot_linux.img
Sending 'vendor_boot_a' (102400 KB)   OKAY [  2.660s]
Writing 'vendor_boot_a'               OKAY [  0.650s]
Finished. Total time: 3.321s
[07:53:43] rebooting
```

- `vendor_boot_a` = `build/boot-images/vendor_boot_linux.img`
  md5 `bfdf92a6356edea4ba6f35dc16759dc0`；
- 内嵌 160099B DTB 有 **5 个 timer PPI**，含 **hyp-virt PPI12**；
- `boot_a` 仍是 stage5 镜像（纯自旋 + udelay 隔离）；
- `boot_b`、`dtbo`、`trustos`、`teecfg`、`uboot` 均未动。

预期（若 timer PPI 是根因）：

```text
6.12 VHE 现在选择 HYP_VIRT PPI12
→ clock event / jiffies / timeout 恢复
→ y206 x40（udelay 后）亮
→ y204 x8（自旋看到 online）亮
→ y195 x268（自旋结束）亮
→ stage9 / y130 可能开始推进
```

下一步等待读屏：`y206/x40`、`y204/x8`、`y195`、`stage9`、`y130`。

---

## 21. 两次 DTB 实验：compact 失败与 full-stock+PPI12 重试（2026-10-06 07:54–07:56）

### 21.1 第一次：`vendor_boot_linux.img`（compact 57 节点 DTB）

`vendor_boot_linux.img` 的 DTB 段虽然 entry size 写 160099，但实际 FDT
是 **57 节点 compact OT5 DTB**（369 行 DTS），不是 948 节点 full stock DTB。

当前设备 `dtbo_a` 是原厂 stock overlay；把 compact DTB 和原厂 overlay
一起给 LK 合并，属于“基底被换、overlay 没换”：
- 实测读屏：**未清屏，卡在清屏之前**
- 说明 boot chain 在 kernel `t508n_fbready()/t508n_clear()` 之前就挂了，
  不是内核 timer 代码本身的问题。

**教训：6.12 compact DTB 必须配 `noop_dtbo`；当前 dtbo_a=原厂，不能直接换 compact DTB。**

### 21.2 第二次：full-stock 160099B DTB + PPI12（兼容原厂 dtbo）

为避免 overlay 不兼容，重新做了一个 **full stock DTB + 第五个 timer PPI12**：

```text
源: firmware/stock/ot5-vendor.dtb（full stock 160099B）
改: timer interrupts 增加 <PPI 12>
    （去掉 arm,no-tick-in-suspend 以腾出 64B 空间，仅影响 suspend）
新 DTB: /tmp/full_stock_ppi12_fit.dtb（160076B）
vendor_boot: farmware/stock-images/vendor_boot_a.img 原位替换 FDT
```

新镜像：

```text
/mnt/c/platform-tools/vendor_boot_full_ppi12.img
md5 = 017364afb0975099c6cd18bbeaf4714e
内嵌 DTB md5 = e27eac89a8c990d9ae66967b4ce54ccd
timer = <13>,<14>,<11>,<10>,<12>
```

刷入（只动 vendor_boot_a）：

```text
[07:56:33] current-slot: 'a'
Sending 'vendor_boot_a' (102400 KB)   OKAY [  2.682s]
Writing 'vendor_boot_a'               OKAY [  0.648s]
[07:56:37] rebooting
```

判读目标：

| 现象 | 结论 |
|---|---|
| 能清屏、stage0 出现 | full DTB 与 boot chain 兼容；PPI12 实验有效进入 kernel |
| 仍卡清屏前 | 问题不在 compact DTB/overlay，而在更底层（AVB/DTB 处理或内核 early） |
| 清屏后 y195/y130 变化 | PPI12 timer 生效，timeout/wakeup 恢复 |

当前状态：**full-stock+PPI12 vendor_boot_a 已刷入并 reboot；等待读屏。**
