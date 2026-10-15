# 文档总索引

> 工作区全部文档的地图。各项目的完整清单在其 `docs/INDEX.md`。
> 本页只列**跨项目**内容与**总入口**。

## 0. 从哪开始

| 文档 | 用途 |
|---|---|
| [../README.md](../README.md) | 工作区总览：两个项目、目录结构、快速导航 |
| [WORKSPACE.md](WORKSPACE.md) | 目录规范、分类原则、环境与复现、维护约定 |
| [PATH-MAPPING.md](PATH-MAPPING.md) | 2026-09-29 重组：旧→新路径逐项对照、兼容符号链接、已修复脚本 |
| ★ [../t508/docs/HANDOFF.md](../t508/docs/HANDOFF.md) | **T508N 交接入口**（新会话从这开始）：30 秒摘要、红线、状态、目录地图、命令、下一步、给新会话的第一条消息 |
| ★ [../v50/docs/HANDOFF.md](../v50/docs/HANDOFF.md) | **V50 交接入口**（同上结构） |

## 1. 项目文档

| 项目 | 总览 | 资料索引 | 源码清单 | 固件清单 |
|---|---|---|---|---|
| **T508N**（TCL OT5 / UMS9620 / T770） | [t508/README.md](../t508/README.md) | [t508/docs/INDEX.md](../t508/docs/INDEX.md) | [t508/src/README.md](../t508/src/README.md) | [t508/firmware/README.md](../t508/firmware/README.md) |
| **V50 / F50 / MU300**（ZTE / UMS9620） | [v50/README.md](../v50/README.md) | [v50/docs/INDEX.md](../v50/docs/INDEX.md) | [v50/src/README.md](../v50/src/README.md) | [v50/firmware/README.md](../v50/firmware/README.md) |

补充：T508 构建产物见 `t508/build/README.md`，分析工作区见 `t508/work/README.md`；
V50 分析工作区见 `v50/work/README.md`；公共资源见 [shared/README.md](../shared/README.md)。

## 2. 跨项目文档（`docs/cross/`）

| 文档 | 内容 |
|---|---|
| [cross/REPORT-mu300-linux-对T508N价值评估.md](cross/REPORT-mu300-linux-对T508N价值评估.md) | 评估 V50 的 `mu300-linux` 项目/内核对 T508N mainline 工作的可复用价值；**§10（2026-09-29 补）**是启动链借鉴清单：boot 镜像 4 条硬约束、`misc`/BCAB 槽位语义、无 UART 日志组合、DTS `no-map` 对照、stock 5.4 双树对比、模块顺序单文件化；两份 HANDOFF 都指向它 |

## 3. T508N 文档速览（详见 [t508/docs/INDEX.md](../t508/docs/INDEX.md)）

* **总索引 / 交接**：`FINAL-BRINGUP-INDEX.md`、`AGENTS-GUIDE.md`、`reports/HANDOFF-3-SESSIONS.md`
* **启动链逆向**：`reports/REPORT-T508N-启动流程分析.md`、`reports/REPORT-T508N-stock-analysis.md`
* **mainline 适配**：`reports/REPORT-T508N-mainline-bringup.md`、`reports/REPORT-UMS9620-mainline适配.md`、`reports/REPORT-UNISOC-MODS-ON-612.md`、`reports/REPORT-UNISOC-ONLY-CONFIGS.md`
* **同步/竞态深挖**：`reports/REPORT-stock-*.md`、`reports/REPORT-CURRENT-RACE-ANALYSIS.md`、`reports/REPORT-LSE-atomic-completion-analysis.md`
* **USB 专项**：`reports/REPORT-USB-BOOT-FLOW.md`、`reports/REPORT-USB-CONSOLIDATED.md`、`reports/REPORT-STOCK-USB-STACK-MECHANISM.md`、`reports/REPORT-SELF-BUILT-USB-CHAIN.md`
* **vddusb33 调压器**：`reports/TASK-vddusb33-regulator-fix.md`、`reports/GUIDE-vddusb33-stock-design.md`、`reports/REPORT-vddusb33-variant-matrix-offline.md`
* **任务书 prompt**：`prompts/`（33 篇）
* **原厂内核逐子系统反汇编**：`t508/work/disasm/DISASM-A..N*.md` + dump

## 4. V50 文档速览（详见 [v50/docs/INDEX.md](../v50/docs/INDEX.md)）

* **仓库内文档（权威、持续维护）**：`v50/src/mu300-linux/docs/INDEX.md`
  —— `V50.md`、`V50-HARDWARE.md`、`V50-NFC.md`、`CELLULAR.md`、`LOCK-BANDS-CELLS.md`、
  `FINDINGS.md`、`BUILD.md`、`BUILD-MU3351.md`、`FROM-SOURCE-TO-OPENWRT.md`、`DISTROS.md`、
  `ONECLICK-PACKAGE.md`、`OPENWRT-PACKAGES.md`、`V50-LED-OpenWrt.md`
* **逆向分析（仓库外）**：`v50/work/rev/`（INDEX 见该目录）
  —— 官方源码包清单、编译实录、点灯逻辑、lights HAL、MBB HTTP 重定向、社区跑马灯插件、
  OpenWrt 适配、ZTE 开源门户检索、SMC/USB 可达性
* **专题报告**：`v50/docs/reports/`（F50 B09/B16 固件分析、V50 能否用 mu300-linux、
  mu300-linux 项目理解、GitHub 拉取诊断、UFI-TOOLS 与许可插件、V50 一键 Root 包机制）
* **指南**：`v50/docs/GUIDE-AT-4G5G-联网.md`（AT 命令 / 4G-5G 硬件 / 联网在 Linux 上如何生效）
* **会话归档**：`v50/docs/ARCHIVE-20260929-V50会话总结.md`（本项目全部会话的逐次摘要）

## 5. 会话归档

| 文档 | 说明 |
|---|---|
| [t508/docs/session/session.jsonl](../t508/docs/session/session.jsonl) | T508N 原始中断会话日志（约 20,340 行 / 9.7 MB）。**一般不要重读**，优先用 `FINAL-BRINGUP-INDEX.md` 与 `SUMMARY-ALL-ANALYSIS.md` |
| [v50/docs/ARCHIVE-20260929-V50会话总结.md](../v50/docs/ARCHIVE-20260929-V50会话总结.md) | **V50 项目全部会话（2026-08-24 ~ 09-29）的归档**：逐次目标/结论/产物/落点/遗留 + 文档覆盖矩阵。接手 V50 先读它 |
| [v50/docs/reports/](../v50/docs/reports/) | V50 专题报告（含 UFI-TOOLS 与许可插件、一键 Root 包机制两份新增） |

## 6. 技能（skills）

`.dsh/skills/` 保存本工作区可加载的技能说明，其中与设备直接相关的是：

| 技能 | 用途 |
|---|---|
| `t508n-flashing` | TCL T508N 内核/initramfs/vendor_boot/uboot 完整刷写流程与关键坑 |
