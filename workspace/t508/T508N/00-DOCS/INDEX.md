# T508N 资料索引

> `t508/docs/` 下全部资料的逐份说明。**继续工作请先看 [FINAL-BRINGUP-INDEX.md](FINAL-BRINGUP-INDEX.md)。**
>
> 说明：本目录的历史报告保留**原文路径**（多为重组前的 `/home/test/t508n/work/...`），
> 换算见 [../../docs/PATH-MAPPING.md](../../docs/PATH-MAPPING.md)。

## 1. 入口与总纲

| 文档 | 说明 |
|---|---|
| [AGENTS-GUIDE.md](AGENTS-GUIDE.md) | T508N 工作区指引（给其他 agent 的入口文档） |
| [HANDOFF.md](HANDOFF.md) | ★ **交接入口**：新会话从这里开始（状态/红线/命令/下一步/第一条消息模板） |
| [FINAL-BRINGUP-INDEX.md](FINAL-BRINGUP-INDEX.md) | T508N（Unisoc T770 / UMS9620）mainline 6.12.103 bring-up 最终综合索引 |
| [KERNEL-适配方案.md](KERNEL-适配方案.md) | T508N (T770/UMS9620) Linux 内核适配方案 |
| [SKILL-T508N-framebuffer-debug.md](SKILL-T508N-framebuffer-debug.md) | Skill：T508N 无串口 framebuffer 调试法 |
| [WORK-适配材料清单.md](WORK-适配材料清单.md) | T508N mainline 适配：材料清单与反编译获取任务 |
| [无串口验证指南.md](无串口验证指南.md) | T508N 无串口验证指南 |

## 2. 分析 / 实验报告（`reports/`，53 篇）

按主题归类：

* **启动链与 stock 分析**：`REPORT-T508N-启动流程分析.md`、`REPORT-T508N-stock-analysis.md`、`REPORT-UMS9620-mainline适配.md`
* **mainline 适配与紫光魔改**：`REPORT-T508N-mainline-bringup.md`、`REPORT-UNISOC-MODS-ON-612.md`、`REPORT-UNISOC-ONLY-CONFIGS.md`、`UNISOC-DETAILED.md`、`UNISOC-MODS-AND-CONFIG.md`
* **同步 / 竞态 / LSE 深挖**：`REPORT-stock-sync-mechanism.md`、`REPORT-stock-vs-mainline-locks.md`、`REPORT-stock-vs-mainline-completion.md`、`REPORT-mainline-completion-wakeup-analysis.md`、`REPORT-stock-wakeup-chain-analysis.md`、`REPORT-stock-wakeup-nohz.md`、`REPORT-stock-alternatives-waiter-analysis.md`、`REPORT-stock-crosscluster-visibility.md`、`REPORT-CURRENT-RACE-ANALYSIS.md`、`REPORT-LSE-atomic-completion-analysis.md`、`REPORT-alt-table-integrity.md`
* **driver_init / 内存初始化卡点**：`REPORT-stock-driver-init.md`、`REPORT-stock-memory-dev-init.md`、`REPORT-stock-of-core-init.md`、`REPORT-stock-page-alloc-init.md`、`REPORT-stock-zone-memory.md`
* **USB 专项**：`REPORT-USB-BOOT-FLOW.md`、`REPORT-USB-CONSOLIDATED.md`、`REPORT-STOCK-USB-STACK-MECHANISM.md`、`REPORT-SELF-BUILT-USB-CHAIN.md`、`TASK-A-B-USB-DTS-C-离线成果.md`、`TASK-A-deepdive-SSPHY-clock.md`
* **vddusb33 调压器**：`TASK-vddusb33-regulator-fix.md`、`GUIDE-vddusb33-stock-design.md`、`REPORT-vddusb33-variant-matrix-offline.md`
* **构建与刷机**：`T508N-VENDOR-BUILD-RECIPE.md`、`REPORT-54-custom-build.md`、`REPORT-54-probe-port.md`、`FLASH-20260821-P1.md`
* **U-Boot / 解锁 / 槽位 / 救砖**：`REPORT-T508N-UBOOT-magic64.md`、`REPORT-T508N-BOOT-SLOT-与救砖.md`
* **交接与总结**：`HANDOFF-3-SESSIONS.md`、`FINAL-WRAPUP-20260821.md`、`ARCHIVE-20260823.md`、`SUMMARY-ALL-ANALYSIS.md`、`SESSION-CONTEXT-COMPACT.md`、`SESSION-SPLIT-AND-WORKFLOW.md`、`REPORT-ROOTFS-COMPARE.md`、`REPORT-A76-requirements.md`、`REPORT-SML-psci-cpu-on.md`、`REPORT-AGENTS-GUIDE-analysis.md`、`WORK-适配材料清单.md`（在上一级）

完整清单：

| 文档 | 说明 |
|---|---|
| [ARCHIVE-20260823.md](reports/ARCHIVE-20260823.md) | T508N (TCL TAB, UMS9620/Qogirn6Pro) — 项目归档 |
| [FINAL-WRAPUP-20260821.md](reports/FINAL-WRAPUP-20260821.md) | T508N（TCL OT5 / Unisoc T770 / UMS9620）mainline 6.12.103 bring-up 最终总结 |
| [FLASH-20260821-P1.md](reports/FLASH-20260821-P1.md) | 刷机记录：2026-08-21 P1 单变量实测（boot_a） |
| [GUIDE-vddusb33-stock-design.md](reports/GUIDE-vddusb33-stock-design.md) | GUIDE：按原厂设计补 vddusb33 regmap 环（MFD cell 路径） |
| [HANDOFF-3-SESSIONS.md](reports/HANDOFF-3-SESSIONS.md) | T508N 内核跑通专项交接文档（范围：忽略 USB 及一切外设） |
| [PATCH-diag-sgi-kfence.md](reports/PATCH-diag-sgi-kfence.md) | PATCH-diag-sgi-kfence — T508N 诊断方案：①Unisoc 死循环门移植（SGI 丢失诊断）②KFENCE 内存损坏诊断 |
| [PATCH-mem-sysfs-order.md](reports/PATCH-mem-sysfs-order.md) | PATCH：恢复 mem_sysfs_mutex + memory_dev_init 挪回 cpu_dev_init 之后（T508N 未验证项 2+3） |
| [PATCH-nohz-kick.md](reports/PATCH-nohz-kick.md) | PATCH-nohz-kick：恢复 NOHZ + kick_ilb 改 smp_send_reschedule（IPI_RESCHEDULE） |
| [REPORT-54-custom-build.md](reports/REPORT-54-custom-build.md) | REPORT-54-custom-build：复刻原厂 5.4 内核构建报告（5.4.210 + 紫光魔改 + 原厂 config） |
| [REPORT-54-probe-port.md](reports/REPORT-54-probe-port.md) | REPORT: 5.4 复刻内核 framebuffer 探针移植（清屏 + 启动全程进度条 + 阶段标记） |
| [REPORT-A76-requirements.md](reports/REPORT-A76-requirements.md) | T508N A76 核（CPU4-7）特殊需求分析 —— stock DTS vs mainline DTS 全量对比 |
| [REPORT-AGENTS-GUIDE-analysis.md](reports/REPORT-AGENTS-GUIDE-analysis.md) | T508N 卡点归因分析报告（基于工作区文档 + 纯源码分析） |
| [REPORT-CURRENT-RACE-ANALYSIS.md](reports/REPORT-CURRENT-RACE-ANALYSIS.md) | T508N 当前竞态（A: devtmpfs setup_done 卡 / B: memory_dev_init device_register 卡）根源分析 |
| [REPORT-LSE-atomic-completion-analysis.md](reports/REPORT-LSE-atomic-completion-analysis.md) | T508N：LSE atomic dec_and_test 语义 + thread_ack vs nr_todo + 系统级 alternatives 副作用分析 |
| [REPORT-ROOTFS-COMPARE.md](reports/REPORT-ROOTFS-COMPARE.md) | T508N(UMS9620) 根文件系统 / initramfs / init 投递机制对比报告 |
| [REPORT-SELF-BUILT-USB-CHAIN.md](reports/REPORT-SELF-BUILT-USB-CHAIN.md) | T508N 自制 USB 链规格（v21 收官构型） |
| [REPORT-SML-psci-cpu-on.md](reports/REPORT-SML-psci-cpu-on.md) | REPORT-SML-psci-cpu-on.md — SML EL3 固件 PSCI CPU_ON 路径分析 |
| [REPORT-STOCK-USB-STACK-MECHANISM.md](reports/REPORT-STOCK-USB-STACK-MECHANISM.md) | T508N 原厂栈机制全分析 —— 为什么原厂 USB 能用 |
| [REPORT-T508N-mainline-bringup.md](reports/REPORT-T508N-mainline-bringup.md) | T508N mainline Linux bring-up 汇总（截至 smp_init 之后） |
| [REPORT-T508N-stock-analysis.md](reports/REPORT-T508N-stock-analysis.md) | T508N (UMS9620/T770) stock 内核跨 cluster 同步问题分析报告 |
| [REPORT-T508N-启动流程分析.md](reports/REPORT-T508N-启动流程分析.md) | T508N（紫光展锐 T770 / UMS9620）启动流程逆向分析报告 |
| [REPORT-UMS9620-mainline适配.md](reports/REPORT-UMS9620-mainline适配.md) | UMS9620 mainline 内核适配信息整理 |
| [REPORT-UNISOC-MODS-ON-612.md](reports/REPORT-UNISOC-MODS-ON-612.md) | REPORT-UNISOC-MODS-ON-612：紫光魔改全量移植到干净 6.12.103 树（编译验证 + scheduler_ipi 安全性分析） |
| [REPORT-UNISOC-ONLY-CONFIGS.md](reports/REPORT-UNISOC-ONLY-CONFIGS.md) | REPORT：紫光（Unisoc/SPRD）特有 config 与标准内核的差集分析 |
| [REPORT-USB-BOOT-FLOW.md](reports/REPORT-USB-BOOT-FLOW.md) | T508N（UMS9620 / T770）USB 启动流程分析 |
| [REPORT-USB-CONSOLIDATED.md](reports/REPORT-USB-CONSOLIDATED.md) | T508N USB 成果与 USB 启动流程总览（2026-08-25） |
| [REPORT-alt-table-integrity.md](reports/REPORT-alt-table-integrity.md) | T508N：mainline 6.12 alt 表完整性静态核查（"828 条字段异常记录" 真伪判定） |
| [REPORT-mainline-completion-wakeup-analysis.md](reports/REPORT-mainline-completion-wakeup-analysis.md) | T508N mainline 6.12 completion 唤醒链路分析（stop_machine 卡点 · 第 2 版） |
| [REPORT-stock-alternatives-waiter-analysis.md](reports/REPORT-stock-alternatives-waiter-analysis.md) | T508N：原厂 5.4 alternatives 与 waiter 唤醒交互深度分析（stock vs mainline 6.12 探针构建） |
| [REPORT-stock-crosscluster-visibility.md](reports/REPORT-stock-crosscluster-visibility.md) | T508N：原厂跨 cluster 可见性操作模式分析（普通 store vs 原子/dsb/锁） |
| [REPORT-stock-driver-init.md](reports/REPORT-stock-driver-init.md) | driver_init 链后续卡点分析（devices_init 及之后，对比原厂 5.4） |
| [REPORT-stock-memory-dev-init.md](reports/REPORT-stock-memory-dev-init.md) | REPORT：原厂 5.4 vs mainline 6.12 — memory_dev_init / device_register（device_add）区域对比 |
| [REPORT-stock-of-core-init.md](reports/REPORT-stock-of-core-init.md) | REPORT：原厂 5.4 vs mainline 6.12 — of_core_init / __of_attach_node_sysfs 区域对比 |
| [REPORT-stock-page-alloc-init.md](reports/REPORT-stock-page-alloc-init.md) | 原厂 5.4 的 page_alloc_init_late 区域分析（对比 mainline 6.12 卡点） |
| [REPORT-stock-sync-mechanism.md](reports/REPORT-stock-sync-mechanism.md) | T508N：原厂 stop_machine / alternatives 同步机制分析报告 |
| [REPORT-stock-vs-mainline-completion.md](reports/REPORT-stock-vs-mainline-completion.md) | T508N 任务 A（最终版）：原厂 5.4 vs mainline 6.12 的 completion 唤醒 + 系统级 alternatives 收尾 + smp_cpus_done→do_basic_setup 窗口对比 |
| [REPORT-stock-vs-mainline-locks.md](reports/REPORT-stock-vs-mainline-locks.md) | T508N：原厂 5.4 vs mainline 6.12 并发基础设施对比报告 |
| [REPORT-stock-wakeup-chain-analysis.md](reports/REPORT-stock-wakeup-chain-analysis.md) | T508N：原厂 5.4 完整唤醒链反汇编 vs mainline 6.12（为什么原厂 SM#2 后 CPU0 能调度 waiter） |
| [REPORT-stock-wakeup-nohz.md](reports/REPORT-stock-wakeup-nohz.md) | T508N：异簇唤醒/wakelist/NOHZ idle 唤醒 —— 原厂 5.4 vs mainline 6.12 全链对比与竞态根源 |
| [REPORT-stock-zone-memory.md](reports/REPORT-stock-zone-memory.md) | 原厂 5.4 的 set_zone_contiguous / 内存布局分析（对比 mainline 6.12 卡点） |
| [REPORT-vddusb33-variant-matrix-offline.md](reports/REPORT-vddusb33-variant-matrix-offline.md) | 报告：vddusb33 变体矩阵假说 —— 离线核验（T1 前置结论） |
| [SESSION-CONTEXT-COMPACT.md](reports/SESSION-CONTEXT-COMPACT.md) | T508N 会话压缩上下文（2026-08-16 22:35，turn 14 继续点） |
| [SESSION-SPLIT-AND-WORKFLOW.md](reports/SESSION-SPLIT-AND-WORKFLOW.md) | T508N 工作区 Session 拆分扫描 + 工作流规划 |
| [SUMMARY-ALL-ANALYSIS.md](reports/SUMMARY-ALL-ANALYSIS.md) | T508N bring-up 全部反编译/分析成果汇总（供后续"当前问题分析"子 agent 使用） |
| [T508N-VENDOR-BUILD-RECIPE.md](reports/T508N-VENDOR-BUILD-RECIPE.md) | T508N 真厂内核构建配方（可复现文档） |
| [TASK-A-B-USB-DTS-C-离线成果.md](reports/TASK-A-B-USB-DTS-C-离线成果.md) | T508N 离线任务 A/B/C 成果（2025-08-16） |
| [TASK-A-deepdive-SSPHY-clock.md](reports/TASK-A-deepdive-SSPHY-clock.md) | T508N USB 深挖补遗：SS PHY 完整 init 与 UMS9620 时钟表 |
| [TASK-vddusb33-regulator-fix.md](reports/TASK-vddusb33-regulator-fix.md) | TASK（§23 续）：vddusb33 上电 —— hsphy regmap 链修复（② MFD cell of_node 单点） |
| [UNISOC-DETAILED.md](reports/UNISOC-DETAILED.md) | T508N 紫光魔改 / 内核设置 / 编译环境 详细记录 |
| [UNISOC-MODS-AND-CONFIG.md](reports/UNISOC-MODS-AND-CONFIG.md) | T508N 紫光（Unisoc/SPRD）魔改部位与内核设置汇总 |
| [unisoc_ums9620.md](reports/unisoc_ums9620.md) | — |

### U-Boot / 解锁 / 槽位 / 救砖

| 文档 | 说明 |
|---|---|
| [REPORT-T508N-UBOOT-magic64.md](reports/REPORT-T508N-UBOOT-magic64.md) | T508N U-Boot magic64 解锁：校验链、load base、patch 点、构建验证、实机结果、Windows 一键包 |
| [REPORT-T508N-BOOT-SLOT-与救砖.md](reports/REPORT-T508N-BOOT-SLOT-与救砖.md) | T508N A/B 槽位选择、fastboot 回退原因、misc bootloader_control、槽位镜像重打包与救砖流程 |
| [REPORT-T508N-FOLDER-ORGANIZATION.md](reports/REPORT-T508N-FOLDER-ORGANIZATION.md) | 文件夹整理总报告：盘点范围、关键清单、重复/冲突、校验结果、风险提示 |
| [REPORT-T508N-ARTIFACT-MANIFEST.md](reports/REPORT-T508N-ARTIFACT-MANIFEST.md) | 关键构件清单：每个 img/bin/zip/bat 的 SHA256、权威副本、出处与恢复最短路径 |

## 3. 任务书 prompt（`prompts/`，36 篇）

这些是派发给子会话的任务书，保留以便追溯每份报告的输入与验收标准。

| 文档 | 说明 |
|---|---|
| [prompt-54-probe-port.md](prompts/prompt-54-probe-port.md) | 任务：为复刻 5.4 内核移植 framebuffer 探针（清屏 + 启动全程进度） |
| [prompt-a76-requirements.md](prompts/prompt-a76-requirements.md) | 任务：A76 核（CPU4-7）在 8 核配置下的特殊需求分析（对比原厂 stock DTS） |
| [prompt-alt-table-integrity.md](prompts/prompt-alt-table-integrity.md) | 任务：静态核查 mainline alt 表 828 条异常记录（orig 越界 → lm_alias 写坏内存？） |
| [prompt-build-54-custom.md](prompts/prompt-build-54-custom.md) | 任务：干净环境编译复刻原厂 5.4 内核（5.4.210 + 紫光魔改 + 原厂 config） |
| [prompt-completion-wakeup-compare.md](prompts/prompt-completion-wakeup-compare.md) | 任务 A：原厂 vs mainline 的 completion 唤醒机制对比 |
| [prompt-current-race-analysis.md](prompts/prompt-current-race-analysis.md) | 任务：T508N 当前竞态根源分析（基于全部已有成果） |
| [prompt-disasm-B-sched.md](prompts/prompt-disasm-B-sched.md) | 任务：原厂内核全面反编译 — 子系统 B-sched：调度器 |
| [prompt-disasm-C-irq.md](prompts/prompt-disasm-C-irq.md) | 任务：原厂内核全面反编译 — 子系统 C-irq：中断GIC/Timer |
| [prompt-disasm-D-mm.md](prompts/prompt-disasm-D-mm.md) | 任务：原厂内核全面反编译 — 子系统 D-mm：内存管理 |
| [prompt-disasm-E-devm.md](prompts/prompt-disasm-E-devm.md) | 任务：原厂内核全面反编译 — 子系统 E-devm：驱动模型sysfs |
| [prompt-disasm-F-fs.md](prompts/prompt-disasm-F-fs.md) | 任务：原厂内核全面反编译 — 子系统 F-fs：文件系统VFS |
| [prompt-disasm-a-cpu.md](prompts/prompt-disasm-a-cpu.md) | 任务：原厂内核全面反编译 — 子系统 A：启动/CPU 管理 |
| [prompt-final-index.md](prompts/prompt-final-index.md) | 任务：整理 T508N bring-up 全部成果与文档（最终综合索引） |
| [prompt-lse-atomic-completion.md](prompts/prompt-lse-atomic-completion.md) | 任务 C：LSE atomic 的 dec_and_test 语义 + stop_machine 双原子（thread_ack vs nr_todo） |
| [prompt-mainline-completion-wakeup.md](prompts/prompt-mainline-completion-wakeup.md) | 任务 B：mainline 6.12 completion 唤醒链路在 stop_machine 上下文的分析 |
| [prompt-stock-alternatives-waiter.md](prompts/prompt-stock-alternatives-waiter.md) | 任务：原厂镜像深度分析——alternatives patch 与 waiter 唤醒的交互（针对当前卡点） |
| [prompt-stock-crosscluster-visibility.md](prompts/prompt-stock-crosscluster-visibility.md) | 任务：原厂跨 cluster 可见性操作模式分析（普通 store vs 原子/dsb/锁） |
| [prompt-stock-driver-init.md](prompts/prompt-stock-driver-init.md) | 任务：driver_init 链后续卡点分析（devices_init 及之后，对比原厂 5.4） |
| [prompt-stock-lock-analysis.md](prompts/prompt-stock-lock-analysis.md) | 任务：原厂 vs mainline 并发基础设施对比（锁/原子/多核同步） |
| [prompt-stock-memory-dev-init.md](prompts/prompt-stock-memory-dev-init.md) | 任务：原厂 5.4 的 memory_dev_init / device_register 区域分析（对比 mainline 6.12 间歇卡点） |
| [prompt-stock-of-core-init.md](prompts/prompt-stock-of-core-init.md) | 任务：原厂 5.4 的 of_core_init / __of_attach_node_sysfs 区域分析（对比 mainline 6.12 卡点） |
| [prompt-stock-page-alloc-init.md](prompts/prompt-stock-page-alloc-init.md) | 任务：原厂 5.4 的 page_alloc_init_late 区域分析（对比 mainline 6.12 卡点） |
| [prompt-stock-sync-analysis.md](prompts/prompt-stock-sync-analysis.md) | 任务：原厂 stop_machine/alternatives 同步机制分析 + 跨 cluster cache 一致性方案 |
| [prompt-stock-wakeup-chain.md](prompts/prompt-stock-wakeup-chain.md) | 任务：原厂 5.4 唤醒链反汇编对比（为什么原厂 SM#2 后 CPU0 能正常调度 waiter） |
| [prompt-stock-wakeup-nohz.md](prompts/prompt-stock-wakeup-nohz.md) | 任务：原厂 5.4 的多核唤醒/调度完整处理方式（对比 mainline 6.12 竞态根源） |
| [prompt-stock-zone-memory.md](prompts/prompt-stock-zone-memory.md) | 任务：原厂 5.4 的 set_zone_contiguous / 内存布局分析（对比 mainline 6.12 卡点） |
| [prompt-summary-all.md](prompts/prompt-summary-all.md) | 任务：T508N bring-up 全部反编译/分析进展汇总（供后续问题分析使用） |
| [prompt-unisoc-mods-on-612.md](prompts/prompt-unisoc-mods-on-612.md) | 任务：干净 6.12 树上移植全部紫光魔改并编译验证 |
| [prompt-unisoc-only-configs.md](prompts/prompt-unisoc-only-configs.md) | 任务：找出紫光（Unisoc/SPRD）特有的、标准内核无法复刻的 config |
| [prompt-vddusb33-fastboot-debug.md](prompts/prompt-vddusb33-fastboot-debug.md) | 提示：vddusb33 变体矩阵 —— fastboot 实机调试（T1 → F1/F3 → V1-V3） |
| [prompt-vddusb33-runtime-evidence.md](prompts/prompt-vddusb33-runtime-evidence.md) | 任务：vddusb33 regmap 环 —— 运行时证据定位与修复（H1/H2/H3） |
| [prompt-vddusb33-variant-matrix.md](prompts/prompt-vddusb33-variant-matrix.md) | 提示：vddusb33 regmap 环 —— 变体矩阵假说验证与修复（含测试/验证方法） |
| [prompt-verify-kick-clean.md](prompts/prompt-verify-kick-clean.md) | 任务：干净 6.12 树上验证 kick_ilb 补丁（隔离魔改干扰） |
| [prompt-verify-mem-order.md](prompts/prompt-verify-mem-order.md) | 任务：验证未验证项 2+3 — mem_sysfs_mutex 恢复 + memory_dev_init 顺序对齐 |
| [prompt-verify-nohz-kick.md](prompts/prompt-verify-nohz-kick.md) | 任务：验证未验证项 1 — NOHZ 恢复 + kick_ilb 改 IPI_RESCHEDULE |
| [prompt-verify-sgi-kfence.md](prompts/prompt-verify-sgi-kfence.md) | 任务：验证未验证项 4+5 — Unisoc 死循环门移植 + KFENCE 诊断 |

## 4. 原厂内核逐子系统反汇编（`../work/disasm/`）

| 文档 | 说明 |
|---|---|
| [DISASM-A-cpu-boot.md](../work/disasm/DISASM-A-cpu-boot.md) | DISASM-A：原厂内核 启动/CPU 管理 子系统全面反编译（T508N / UMS9620） |
| [DISASM-B-sched-调度器.md](../work/disasm/DISASM-B-sched-调度器.md) | 原厂内核全面反编译 — 子系统 B-sched：调度器 |
| [DISASM-D-mm-内存管理.md](../work/disasm/DISASM-D-mm-内存管理.md) | DISASM-D-mm：原厂内核内存管理子系统全面反汇编报告 |
| [DISASM-E-devm-驱动模型sysfs.md](../work/disasm/DISASM-E-devm-驱动模型sysfs.md) | DISASM-E-devm：原厂驱动模型/sysfs 子系统全面反汇编（device_add/kobject/kernfs/sysfs/bus/uevent/klist） |
| [DISASM-F-fs-文件系统VFS.md](../work/disasm/DISASM-F-fs-文件系统VFS.md) | DISASM-F：原厂内核反汇编 — 子系统 F-fs（文件系统/VFS） |
| [DISASM-G-drv-clk-时钟.md](../work/disasm/DISASM-G-drv-clk-时钟.md) | DISASM-G-drv-clk：原厂内核 时钟子系统（SPRD_UMS9620_CLK / T770 主时钟）全面反汇编 |
| [DISASM-H-drv-pmic-电源.md](../work/disasm/DISASM-H-drv-pmic-电源.md) | DISASM-H-drv-pmic：原厂 PMIC/电源子系统全面反汇编（MFD_UMP96XX_PMIC + REGULATOR_UMP9620 + ADI 访问桥 + 电源框架核对） |
| [DISASM-I-drv-ufs.md](../work/disasm/DISASM-I-drv-ufs.md) | DISASM-I-drv-ufs：原厂 UFS 驱动（SCSI_UFS_SPRD_QOGIRN6PRO）反汇编报告 |
| [DISASM-J-drv-timer-wdt.md](../work/disasm/DISASM-J-drv-timer-wdt.md) | DISASM-J：系统定时器（SPRD_SYSTIMER）+ 看门狗（SPRD_WATCHDOG_*）反汇编报告 |
| [DISASM-K-mm-slub.md](../work/disasm/DISASM-K-mm-slub.md) | DISASM-K-mm-slub：原厂内核 SLUB 分配器全面反汇编报告 |
| [DISASM-L-rcu.md](../work/disasm/DISASM-L-rcu.md) | DISASM-L：原厂内核 RCU 子系统全面反编译（T508N / UMS9620） |
| [DISASM-M-workqueue.md](../work/disasm/DISASM-M-workqueue.md) | DISASM-M：原厂内核 workqueue（cmwq）子系统反汇编报告 |
| [DISASM-N-cpuidle-cpufreq.md](../work/disasm/DISASM-N-cpuidle-cpufreq.md) | DISASM-N：原厂内核 cpuidle / cpufreq 子系统（含 sprd cpufreq-v2 驱动）反汇编（T508N / UMS9620） |

同目录还有：`DISASM-C-irq-中断GIC/`、`disasm_g/` ~ `disasm_n/`、`E-devm-*.dump`、
`vmlinux_sections.txt`、`vmlinux_segments.txt`、`alt_table_records.csv` 等原始产物。

## 5. 会话归档

| 文件 | 说明 |
|---|---|
| [session/session.jsonl](session/session.jsonl) | 原始中断会话日志（20,340 行 / 9.7 MB）。**一般不要重读**，用 `FINAL-BRINGUP-INDEX.md` 与 `reports/SUMMARY-ALL-ANALYSIS.md` 代替 |

## 6. 相关目录

* 源码：[../src/README.md](../src/README.md)
* 固件：[../firmware/README.md](../firmware/README.md)
* 构建产物：[../build/README.md](../build/README.md)
* 分析工作区：[../work/README.md](../work/README.md)

## 7. 文件夹整理产物（2026-10-09，只读盘点）

> 对 Windows（E:\rootphone\t508n、E:\Download T508N 相关包、C:\platform-tools）与 WSL
> （t508、docs、work、mainline-ref/t508n-mainline、build）做了一次完整盘点、分类、去重、校验与索引。
> 登记 8061 个文件 / 126.3 GiB；SHA256 **8029 个成功、0 失败**；未删除、未移动、未覆盖任何文件。
> 复制/归档阶段见 ORGANIZE-PLAN，**待用户确认后执行**。过程产物在 `../work/folder-audit/`。

| 文档 | 说明 |
|---|---|
| [INVENTORY-T508N.md](INVENTORY-T508N.md) | 盘点总表：路径存在性、分类统计、关键构件清单、大文件/敏感文件登记 |
| [INVENTORY-T508N.csv](INVENTORY-T508N.csv) | 机器可读逐条清单（8767 行：路径/类型/大小/日期/SHA256/用途/是否关键/重复组） |
| [TREE-T508N.txt](TREE-T508N.txt) | 目录树与条目清单（含折叠目录摘要） |
| [CHECKSUMS-T508N.sha256](CHECKSUMS-T508N.sha256) | 8029 条 SHA256（`sha256sum -c` 可校验） |
| [DUPLICATES-T508N.md](DUPLICATES-T508N.md) | 内容完全相同组 1695 个、冗余副本 2995 个、理论可回收 58.02 GiB |
| [CONFLICTS-T508N.md](CONFLICTS-T508N.md) | 同名不同内容 284 组（boot/uboot/misc/spd_dump 谱系冲突与命名错误） |
| [ORGANIZE-PLAN-T508N.md](ORGANIZE-PLAN-T508N.md) | 建议目录结构、权威副本矩阵、复制批次与回滚方案（先计划后执行） |
| [reports/REPORT-T508N-FOLDER-ORGANIZATION.md](reports/REPORT-T508N-FOLDER-ORGANIZATION.md) | 整理总报告 |
| [reports/REPORT-T508N-ARTIFACT-MANIFEST.md](reports/REPORT-T508N-ARTIFACT-MANIFEST.md) | 关键构件清单报告 |

## 8. 归档（B2-精简归档，2026-10-09 执行）

> 归档根：`/home/test/t508n/t508/T508N/`　**253 个关键文件 / 1.48 GiB**，逐文件 SHA256 校验（253/253 OK）。
> 大文件（>256 MiB）、OpenWrt 历史变体、重复副本**只登记不复制**；原始文件零改动。

| 条目 | 说明 |
|---|---|
| [T508N/README.md](../../T508N/README.md) | 归档总说明 + 从零恢复 `no valid os found` 最短路径 |
| `T508N/ORIGIN-MAP.csv` | 253 条「归档路径 → 原始路径 + SHA256」映射 |
| `T508N/MANIFEST.sha256` | 归档自身校验和（261 条，`sha256sum -c` 通过） |
| `T508N/00-DOCS/` | 入口文档与全部历史报告副本 |
| `T508N/01-TOOLS/` | unlock-v2（对比样本）/ unlock-v3（整套）/ magic64（整套）/ spd-dump-versions（5 版） |
| `T508N/02-FIRMWARE/` | stock-critical / device-backup / magic64 / misc-slots / boot-mainline / boot-openwrt |
| `T508N/03-ROOTFS/` | p75 rootfs（112 MiB / 256 MiB） |
| `T508N/05-MANIFESTS/` | INVENTORY / CHECKSUMS / DUPLICATES / CONFLICTS / BIGFILES / OPENWRT-VARIANTS |
| `T508N/06-ARCHIVE/` | 精选 OpenWrt 已知良好版本 + 原始 vendor 备份 |
