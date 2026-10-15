# T508N 工作区 Session 拆分扫描 + 工作流规划

> 生成：2026-08-21（新会话，恢复扫描）。依据：`session.jsonl`、主目录 + `work/` 全部
> `prompt-*.md` / `REPORT-*` / `DISASM-*` / `PATCH-*`、`linux-6.12.103` git 实况（已核实）。
> 本文件**只扫描 + 规划**，不新增分析、未改任何内核树。

---

## 0. TL;DR

- **拆分形态**：整个 bring-up 由 **1 个主会话**（`session.jsonl`，14 turns，turn 12–14
  因上下文超限失败）+ **约 45 个子会话/任务书** 构成——33 份落盘 `prompt-*.md`（一任务一子会话
  一产出）+ 8 份 DISASM G–N 子会话（prompt 未落盘）+ 汇总/索引/验证 4 类。总产出：
  **14 路 DISASM、约 20 份 REPORT、3 份 PATCH 文档、2 份汇总索引**。
- **当前树状态**（`linux-6.12.103`，HEAD `cb8e725f3` + **13 文件未提交**，+151/−51）：
  含 **P1**（`ttwu_queue_cond` 恒 false）、**WDT pet 修正**（DISASM-J §3.3 寄存器布局）、
  **每核 bringup 探针 y=175/180/185**；`boot_linux.img` / `work/mainline_Image` 为
  **2026-08-18 23:41 构建，包含全部改动**（源码最后修改 23:41:05 < 构建 23:41:19）。
- **上次活动与当前断点**：2026-08-18 23:41 构建后未再活动（间隔约 3 天）。用户上次决定
  "暂停修改"；索引生成后 P1 被重新应用（DISASM-M/L 新证据支撑）并重建，**尚未实测**。
- **下一步唯一动作**：把 23:41 镜像刷进 a 槽读屏——这是 P1 + WDT 修正 + 新探针的
  **第一次单变量实测**；按 §4 决策树判定，禁止在读数前改任何代码。

---

## 1. Session 拆分全景（扫描结果）

### 1.1 会话代际

| 代 | 会话/载体 | 时间 | 内容 | 状态 |
|---|---|---|---|---|
| S0 | **主会话** `session.jsonl`（20,340 行） | 08-15~16 | stock 分析 A/B/C/D、kallsyms 恢复、USB 分析（后归档）、lock/sync 报告；turn 12–14 上下文溢出 | 已结束（溢出） |
| S1 | "3 副 session 分工"（HANDOFF 前身） | 08-17 | 已被 HANDOFF-3-SESSIONS 重写替代 | 已废弃 |
| S2 | **任务书时代**：33 份 `prompt-*.md` | 08-16~18 | 一任务一子会话一产出（见 §1.2 映射表） | 全部完成 |
| S3 | DISASM G–N ×8 子会话（prompt 未落盘） | 08-18 23:00–23:14 | clk/PMIC/UFS/timer-wdt/slub/RCU/workqueue/cpuidle-cpufreq 反汇编 | 全部完成 |
| S4 | 汇总/索引子会话 | 08-18 | `prompt-summary-all`→`SUMMARY-ALL-ANALYSIS.md`；`prompt-final-index`→`FINAL-BRINGUP-INDEX.md` | 完成 |
| S5 | 验证子会话 ×4 | 08-18 20:53–21:57 | `prompt-verify-{nohz-kick,mem-order,sgi-kfence,kick-clean}` → 更新 3 份 PATCH 文档 + `REPORT-UNISOC-MODS-ON-612.md` | 完成 |
| S6 | **索引后收尾线**（23:27–23:41） | 08-18 晚 | P1 重应用、WDT pet 修正、每核探针 y=175/180/185、重建镜像 | 完成，**未实测** |
| S7 | 本会话 | 08-21 | 扫描 + 工作流规划 | 进行中 |

### 1.2 任务书 → 产出映射（S2/S3/S4/S5）

**反汇编（DISASM，A–N 共 14 路）**

| 任务书 | 产出 | 关键结论（一句话） |
|---|---|---|
| prompt-disasm-a-cpu | `DISASM-A-cpu-boot.md` | KPTI 回调不用 stop_machine 且在 alternatives 前；PAuth 113 处；buffer_init pre-SMP |
| prompt-disasm-B-sched | `DISASM-B-sched-调度器.md` | scheduler_ipi 是真实函数（fold+wake_list+nohz kick+ksoftirqd）；LSE 经典替换 |
| prompt-disasm-C-irq | `DISASM-C-irq-中断GIC/` | **Unisoc 死循环门**（暗示 SGI 投递间歇问题）；IPI_CPU_STOP 极重；irq_exit 定制 |
| prompt-disasm-D-mm | `DISASM-D-mm-内存管理.md` | pfn_valid NOMAP 拒绝；memmap_init 逐页跳 no-map；KFENCE 回移开启 |
| prompt-disasm-E-devm | `DISASM-E-devm-驱动模型sysfs.md` | mem_sysfs_mutex 全程持有；kernfs 全局 mutex；driver_init 8 成员链 |
| prompt-disasm-F-fs | `DISASM-F-fs-文件系统VFS.md` | shmem mode=0777；fs 主体与上游逐行一致 |
| （G，未落盘） | `DISASM-G-drv-clk-时钟.md` | 原厂时钟驱动是 `.ko` 外置（clk-sprd + ums9620-clk），镜像内无 |
| （H，未落盘） | `DISASM-H-drv-pmic-电源.md` | PMIC 驱动同样外置 `.ko`；ADI 访问桥/regmap-irqchip 已拆 |
| （I，未落盘） | `DISASM-I-drv-ufs.md` | SCSI_UFS_SPRD_QOGIRN6PRO 为独立模块，镜像内无 vendor host 驱动 |
| （J，未落盘） | `DISASM-J-drv-timer-wdt.md` | timer-sprd 与上游一致；**WDT 寄存器布局：LOCK@0x20 键 0xe551、LOAD 17bit HI@0x4/LO@0x0** |
| （K，未落盘） | `DISASM-K-mm-slub.md` | **★修正：原厂 SLUB 全链路用 LSE CASP（9 站点）**——"casp 首大规模使用"不成立 |
| （L，未落盘） | `DISASM-L-rcu.md` | 77 个 RCU 函数；RCU_NOCB/FAST_NO_HZ 等原厂配置已列 |
| （M，未落盘） | `DISASM-M-workqueue.md` | cmwq 与上游一致；**跨簇唤醒通道差异 100% 在调度器**（5.4 wake_list+IPI_RESCHEDULE vs 6.12 CSD+IPI_CALL_FUNC）——P1 依据 |
| （N，未落盘） | `DISASM-N-cpuidle-cpufreq.md` | sprd cpufreq-v2 外置模块；DTS 缺 cpufreq 域/idle-states 挂接 |

**深度分析（REPORT，S2 主产出）**

| 任务书 | 产出 | 关键结论 |
|---|---|---|
| prompt-stock-lock-analysis | `REPORT-stock-vs-mainline-locks.md` | 锁/原子同源；唯一现实差异 = LSE patch 是否生效 |
| prompt-stock-sync-analysis | `REPORT-stock-sync-mechanism.md` | 二级核 `ldr+yield` 纯轮询；PoC 一致性可用 |
| prompt-lse-atomic-completion | `REPORT-LSE-atomic-completion-analysis.md` | nr_todo 必归零 ⇒ complete 必被调（H4 排除） |
| prompt-mainline-completion-wakeup | `REPORT-mainline-completion-wakeup-analysis.md` | arm64 无 TIF_POLLING_NRFLAG ⇒ 跨核唤醒 100% 依赖 SGI |
| prompt-completion-wakeup-compare | `REPORT-stock-vs-mainline-completion.md` | completion 非差异点；swait 无条件摘链是放大因素 |
| prompt-stock-alternatives-waiter | `REPORT-stock-alternatives-waiter-analysis.md` | NOP-落入式对"半新半旧"敏感（已 static_key 弃用） |
| prompt-alt-table-integrity | `REPORT-alt-table-integrity.md` | "828 条异常"是分析误差；30,477 条全合法 |
| prompt-stock-crosscluster-visibility | `REPORT-stock-crosscluster-visibility.md` | 无"裸 store 被单次读"；t508n_sm2_done 是唯一裸 store 跨核点 |
| prompt-stock-wakeup-chain | `REPORT-stock-wakeup-chain-analysis.md` | 唤醒链逐环节等价；卡点压缩到"收到 IPI 之后" |
| prompt-stock-wakeup-nohz | `REPORT-stock-wakeup-nohz.md` | 竞态 A 唯一失败点 = kthread_run 二次跨簇唤醒；修复 P1–P5 |
| prompt-stock-page-alloc-init | `REPORT-stock-page-alloc-init.md` | 原厂 page_alloc_init_late 几乎无事可做 |
| prompt-stock-zone-memory | `REPORT-stock-zone-memory.md` | **原厂 pfn_valid 拒绝 NOMAP → 过洞卡点（已修 §3#3）** |
| prompt-stock-driver-init | `REPORT-stock-driver-init.md` | 原厂 driver_init 8 成员；DEVTMPFS 链后嫌疑高 |
| prompt-stock-of-core-init | `REPORT-stock-of-core-init.md` | phandle_cache 差异（5.4 循环外预填充） |
| prompt-stock-memory-dev-init | `REPORT-stock-memory-dev-init.md` | 6.12 删 mem_sysfs_mutex；device_add 内无 completion ⇒ 卡死 = 锁/分配 |
| prompt-current-race-analysis | `REPORT-CURRENT-RACE-ANALYSIS.md` | 最可疑 = SLUB/内存损坏 / A76 被楔住 / GIC-SGI 丢失 |
| prompt-a76-requirements | `REPORT-A76-requirements.md` | stock 三簇 4+3+1 + capacity/DPC/cpufreq 域（已补三簇+capacity/DPC） |
| prompt-unisoc-only-configs | `REPORT-UNISOC-ONLY-CONFIGS.md` | C 层 312 项紫光独有；T770 世代驱动全部未上游 |
| prompt-unisoc-mods-on-612 | `REPORT-UNISOC-MODS-ON-612.md` | 魔改全量移植干净树编译通过；scheduler_ipi 在 stop_machine 下无死锁 |
| prompt-build-54-custom | `REPORT-54-custom-build.md` | **5.4 复刻 0 error**；镜像 31.6MB |
| prompt-54-probe-port | `REPORT-54-probe-port.md` | 5.4 探针移植；正常满格路径 = stage 0-9 + y=400 + y=460 |
| prompt-verify-nohz-kick | `PATCH-nohz-kick.md`（更新） | kick_ilb→smp_send_reschedule 完整设计 |
| prompt-verify-mem-order | `PATCH-mem-sysfs-order.md`（更新） | mem_sysfs_mutex 恢复 + memory_dev_init 挪位 |
| prompt-verify-sgi-kfence | `PATCH-diag-sgi-kfence.md`（更新） | SGI 计数诊断层 + KFENCE 诊断方案 |
| prompt-verify-kick-clean | 并入 `REPORT-UNISOC-MODS-ON-612.md` | 干净树验证（build-612-clean.log） |
| prompt-summary-all | `SUMMARY-ALL-ANALYSIS.md` | 全部分析汇总（16 项假设排除） |
| prompt-final-index | `FINAL-BRINGUP-INDEX.md` | 最终综合索引（唯一入口） |

### 1.3 当前树实况（已核实，与索引 §5.1 的差异 = S6 收尾线）

git 状态：HEAD `cb8e725f3` + **13 文件未提交**（+151/−51）。索引说 11 文件，差 3 处：

| 文件 | 索引时 | 现在（新增/变化） |
|---|---|---|
| `kernel/sched/core.c` | 无修改（P1 已回退） | **P1 重新应用**：`ttwu_queue_cond()` 恒 false（注释引 DISASM-M/L） |
| `arch/arm64/kernel/smp.c` | 无探针 | **每核探针 y=175/180/185**（x=8+(cpu-1)*28，cpu=1..8） |
| `arch/arm64/kernel/t508n_dbg.c` | 有 y=140 心跳 + 新镜像标识 | **删 y=140 心跳与标识块；WDT pet 按 DISASM-J 修正**（LOCK@0x20 0xe551、LOAD HI@0x4=15/LO@0x0=0，30s） |

config 快照（`.config` 关键项）：`HZ_PERIODIC=y`（NO_HZ_IDLE=n）、`NR_CPUS=32`、`PREEMPT=y`、
`JUMP_LABEL=y`、`ARM64_LSE_ATOMICS=y`、errata 1286807/1165522=y（4118414/1530923/2441007 均 n）、
`MEMORY_HOTPLUG=y`、`BLK_DEV_INITRD=y`、`SLUB_DEBUG=y`；**KFENCE=n、SOFTLOCKUP_DETECTOR=n、DEVTMPFS=n**。

### 1.4 当前探针布局（boot_linux.img 23:41 构建 = 当前树）

| y | 含义 | 坐标 |
|---|---|---|
| 20 | 进度条 stage 0..9 | x=8+n*30（stage 9 = smp_init 完成/二级核上线） |
| 60 | tick 状态 | 长条=正常、中条=无 tick |
| 100 | 异常级别 EL1/EL2/EL3 | x=8/38/68 |
| 130 | **8 核 bringup 完成**（`smp_init` 内 `bringup_nonboot_cpus` 返回后） | x=8 宽 24（**上次观测：不亮 = 卡点**） |
| 175 | **CPU0 发起 boot cpuN**（`__cpu_up` 内） | 每核 x=8+(cpu-1)*28 宽 24 |
| 180 | **核到达 `secondary_start_kernel`** | 每核同上 |
| 185 | **核完成上线通知**（`set_cpu_online` 后、`complete(&cpu_running)` 前） | 每核同上 |
| 210 | of_core_init | x=276 进入；x=8 kset 后；x=44+n*10 节点计数；x=296 循环完成 |
| 220 | do_basic_setup | x=8 cpuset_init_smp；x=44 driver_init；x=80 init_irq_proc；x=116 do_ctors |
| 400 | do_initcalls 完成 | 与 stop_machine 每核 fn 返回格重叠（x=8+(cpu%8)*24） |
| 420 | wait_for_initramfs 后 | 与每核 signal_done 格重叠 |
| 440 | console_on_rootfs 后 | x=8 |
| 460 | exec init 前 | x=8，行号 9 |

---

## 2. 工作流规划

### 2.0 原则（沿用手写纪律）

1. **一次一个变量**：每次刷机只含一项行为改动（探针/WDT 修正属可观测性/正确性，不算变量，
   但解读读数时必须先扣除其影响）。
2. 不碰 USB/eMMC/pinctrl/PMIC/DRM 等外设；不加 CCI/CMN/DSU；不恢复无条件 `ic ialluis`。
3. 运行时实验**串行**（需要同一块屏）；分析/文档整理**可并行**（子会话互不写树）。
4. 每次实测后回填 `REPORT-stock-sync-mechanism.md` / 更新索引，保持唯一入口文档新鲜。

### 2.1 目标与验收（不变）

8 核上线 → alternatives 完整 → driver_init 不卡 → 其余 initcall 走完 → `exec /init`（y=460 亮）。

### 2.2 立即工作流（W1：P1 单变量实测，串行）

```
[用户] fastboot flash boot_a boot_linux.img && fastboot reboot
[用户] 读屏拍照 → [本会话] 按 §2.3 决策树判定
```

**重要前置说明（写在任务书里）**：23:41 镜像相对上次实测（22:34）含 3 项改动——
① **P1（行为变量）**：所有唤醒直接入队 + IPI_RESCHEDULE，弃 CSD+IPI_CALL_FUNC；
② **WDT pet 修正（正确性）**：旧代码 `0xe551→CTRL@0x8`、LOAD 直写 0xffffffff 属误喂/超喂，
   若此前"稳定 9 格"实为 **WDT 复位循环**，本次画面会不同（可能直接通过或卡点变化）；
③ **每核探针 175/180/185（纯观测）**。判定时先看 175/180/185 分清"哪一核卡"，再归因变量。

### 2.3 读数决策树（W1 判定）

| 读数 | 判定 | 下一步 |
|---|---|---|
| 185 不全亮（某核缺） | 该核卡在 `secondary_start_kernel` 内（idmap 卸载→notify_cpu_starting→set_cpu_online 之间） | 在该核路径加逐点探针（一次一点），查 idmap/通知/psci 返回 |
| 180 不全亮 | 该核未到达 secondary（boot_secondary/PSCI 失败或取指问题） | 查 `__cpu_up` 返回、`cpu_boot_status`、idmap 覆盖 |
| 175 不全亮 | CPU0 未发起该核（maxcpus/topology/`__cpu_up` 前） | 查 `cpu_possible/online` 掩码、`setup_max_cpus` |
| 175/180/185 全亮、130 不亮 | **CPU0 卡在 `bringup_nonboot_cpus` 等 `complete(&cpu_running)` 未醒 = 跨核唤醒丢失（P1 目标场景）** | P1 生效则消失；仍现 → W2 诊断轨道（先 SGI 计数层） |
| 130 亮、stage 9 不亮 | bringup 后、stage 9 前卡（num_nodes/num_cpus/smp_cpus_done/SM#2） | 查 smp_init 后续行 + SM#2 决策（alternative.c t508n_sm2_done） |
| stage 9 亮 | 进入 do_basic_setup 阶段 | 读 y=210/220 逐段；卡哪格定位哪个 initcall |
| 全通到 y=400/420/440/460 | **验收达成** | 回填报告；剩余项转 §2.5 收尾 |

辅助观察：y=60（tick 是否推进，区分"慢"与"死"）、y=100（EL 级别）、画面是否整体闪回
（WDT 复位迹象，新 pet 修正后应消失）。

### 2.4 诊断轨道（W2，仅当 W1 后仍卡或需根因时启用；每次一步）

1. **SGI 计数层（PATCH-diag-sgi-kfence P1–P3，纯观测，先于一切改动）**：发送侧
   `smp_cross_call` vs 接收侧 `do_handle_IPI` 按核计数对照——"发了没到"直接证据；每核心跳
   区分"核死 vs 收不到 IPI"。
2. **KFENCE（K1–K3）**：`CONFIG_KFENCE=y`（sample=100）+ pstore console + fault/report 画屏挂钩；
   `KFENCE_STATIC_KEYS` 必须 n。诊断内存被踩。
3. **诊断盲区消除**：`CONFIG_SOFTLOCKUP_DETECTOR=y`（stock 开、mainline 关）；DEBUG_LIST/
   DEBUG_OBJECTS/DEBUG_MUTEXES 一轮。
4. **二分隔离**：`maxcpus=4`（纯 A55）/ `maxcpus=1` / `nohz=off` / `cpuidle.off=1`——确认
   "含 A76 才现、纯 A55 消失"的边界（一次一项）。
5. **SLUB/LSE A/B**：`slub_debug=U`（或 FZPU）一轮；`t508n_apply_lse_early` 不 enable
   static_key（强制全 LL/SC）一轮——DISASM-K 已修正"casp 首用"假设，此项降级为排障手段。
6. **A/B 死循环门（层 2）**：原厂门等价移植，可控弄死疑似核复现竞态（与计数层联合）。
7. **调度侧修复（确诊 CSD 失效后）**：PATCH-nohz-kick（kick_ilb→SGI#0，先 3 文件 + NO_HZ_IDLE=n
   回归 0，再 NO_HZ_IDLE=y）；P2（5.4 式 `rq->wake_list`+IPI_RESCHEDULE，改动大，最后选项）。

### 2.5 并行收尾轨道（W3，可立即派发子会话；不写树，只写文档/补丁文件）

| 轨道 | 内容 | 前置 |
|---|---|---|
| T1 索引同步 | 把 DISASM G–N 结论并入 `FINAL-BRINGUP-INDEX.md`（§6.4 从 6 路扩到 14 路；§7.2 修正"casp 首用"假设——DISASM-K；§5.1 更新为 13 文件；§9.2 镜像时间 23:41；§10 候选顺序微调） | 无（读多写少，互斥于运行实验） |
| T2 红线对齐 | `icache_inval_all_pou()` 双保险（DIC=0 时执行 `ic ialluis`）vs "不用无条件 ic ialluis"红线——代码/文档二选一，需决策 | 用户决策 |
| T3 补丁复核 | 3 份 PATCH 在 `linux-6.12-clean` 的 apply --check 复核（部分已做：build-612-clean.log + REPORT-UNISOC-MODS-ON-612） | 无 |
| T4 遗留项 | `t508n_sm2_done` 改 release/acquire（crosscluster 建议）；A76 revision MIDR 读屏方案；DTS cpufreq 域（低优先，无 sprd cpufreq 驱动） | 无 |

### 2.6 当前明确不做的

- 不改 6.12 树直到 W1 读数完成（变量控制）。
- 不重跑已完成的大反汇编/符号恢复。
- 不重复验证已排除的 16 项假设（见 `SUMMARY-ALL-ANALYSIS.md` §4）。
- 不在一次刷机里同时改多个 config。

---

## 3. 立即下一步

1. （本会话已完成）扫描 + 本规划文件。
2. （用户）刷 23:41 镜像：`fastboot flash boot_a boot_linux.img && fastboot reboot`，拍屏。
3. （本会话）按 §2.3 决策树判定 → 分流 W1 后续 / W2 诊断 / 验收。
4. （可选，随时）并行派发 T1/T3 文档轨道（子会话），不阻塞刷机。
