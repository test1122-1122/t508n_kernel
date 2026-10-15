# 跨项目资料

本目录保存**同时涉及 T508N 与 V50/F50** 的文档。设备专属内容请放各项目的 `docs/`。

| 文档 | 说明 |
|---|---|
| [REPORT-mu300-linux-对T508N价值评估.md](REPORT-mu300-linux-对T508N价值评估.md) | 评估 `mu300-linux`（现 `v50/src/mu300-linux`）对 T508N 的价值。**结论：有显著帮助，是最强的同 SoC 参照**——给出 3 个可立即验证的具体缺口 + mainline 移植件；**但不能照搬**（硬件/分区/内存映射/断电根因/内核版本均不同，§7）。**§10（2026-09-29 补）**新增启动链可借鉴清单：boot 镜像 4 条硬约束、`misc`/BCAB 槽位语义、无 UART 日志组合、DTS `no-map` 对照、stock 5.4 双树对比法、模块顺序单文件化 |

## 背景：两个项目为什么在一个工作区

T508N 与 V50/F50 同属 **UNISOC UMS9620 / QogirN6Pro** 平台，历史上：

* T508N 的 vendor 内核树曾借用 ZTE F50/UMS9620 的社区内核（`t508/src/vendor-kernel-ums9620`
  git remote 为 `enceka/android_kernel_zte_ums9620_mifi_f50`）。
* V50 侧 `v50/src/kernel-ums9620-tuned` 是 ZTE U30 Air / F50 的社区内核，被 T508N 报告引用作对照。
* 因此少量资料天然跨项目，单独归档在此，避免污染任一项目的独立性。

**注意**：归属判定以“文档主题”为准；设备专属的内核树即使被另一项目引用，也归其所属设备
（例：`kernel-ums9620-tuned` 归 `v50/src/`）。
