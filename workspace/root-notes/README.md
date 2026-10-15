# T508N / V50 工作区总览

> 本工作区根目录名 `t508n` 是历史遗留；它实际同时归档**两个 UNISOC UMS9620 平台设备**的 Linux 移植项目：
> **TCL T508N** 与 **ZTE V50 / F50 / MU300**。
>
> 2026-09-29 完成物理重组：按 **项目 → 资料 / 源码 / 固件 / 构建产物 / 工具 / 分析工作区** 分类，
> 两个项目彻底分离，公共资源抽到 `shared/`。旧的顶层入口路径由兼容符号链接保留，
> 完整旧→新对照见 [docs/PATH-MAPPING.md](docs/PATH-MAPPING.md)。

---

## 1. 两个项目一览

| | **T508N** | **V50 / F50 / MU300** |
|---|---|---|
| 设备 | TCL T508N（板级代号 OT5，FIH 代工） | ZTE V50 / MU3351；ZTE F50 / MU300 |
| SoC | Unisoc **T770 / UMS9620**（QogirN6Pro） | Unisoc **UMS9620**（T760/T770 系列） |
| 形态 | 带 320×480 LCD 的终端（无 UART/USB 调试口） | 口袋 5G 路由器（有 adb/USB，root 后刷机） |
| CPU 拓扑 | 4×A55 + 3×A76 + 1×A76（CPU7 独立簇） | 同族 UMS9620 |
| 原厂内核 | 5.4.210（Unisoc fork，clang12/LTO/KCFI） | 5.4.210（F50）/ 5.4.254（MU3351、U30 Air） |
| 目标 | **mainline 6.12.103 跑到 initramfs `/init`**（外设不在范围） | 在 **vendor 5.4** 上跑 **Ubuntu 24.04 / OpenWrt**，保留 Android，可一键回退 |
| 调试手段 | LCD framebuffer 探针（320×480 BGRA @0xb0000000）+ fastboot + pstore | SSH / USB 串口 / adb / SP 下载模式 |
| 主要阶段 | 2026-08-15 ~ 2026-08-28 | 2026-09-18 ~ 2026-09-29 |
| 入口文档 | [t508/README.md](t508/README.md) | [v50/README.md](v50/README.md) |
| 资料索引 | [t508/docs/INDEX.md](t508/docs/INDEX.md) | [v50/docs/INDEX.md](v50/docs/INDEX.md) |
| **交接文档** | **[t508/docs/HANDOFF.md](t508/docs/HANDOFF.md)** | **[v50/docs/HANDOFF.md](v50/docs/HANDOFF.md)** |

两个项目没有共享源码（内核树各自独立），但**使用同一套交叉工具链与下载缓存**，见 [shared/README.md](shared/README.md)。

---

## 2. 顶层目录结构

```
/home/test/t508n/
├── README.md                  # 本文件：工作区总览
├── docs/                      # 跨项目文档
│   ├── INDEX.md               #   文档总索引
│   ├── WORKSPACE.md           #   目录规范、分类原则、环境与复现说明
│   ├── PATH-MAPPING.md        #   旧→新路径对照 + 兼容符号链接
│   └── cross/                 #   跨项目资料（如 mu300-linux 对 T508N 的评估）
│
├── t508/                      # ===== TCL T508N（OT5 / UMS9620 / T770）=====
│   ├── README.md              #   项目总览
│   ├── docs/                  #   资料：报告、prompt、会话归档
│   │   ├── INDEX.md           #     资料总索引（逐份说明）
│   │   ├── reports/           #     分析/实验报告（50 篇）
│   │   ├── prompts/           #     任务书 prompt（33 篇）
│   │   └── session/           #     原始会话日志（session.jsonl，一般勿重读）
│   ├── src/                   #   源码：内核树、设备树、归档仓库
│   │   └── README.md          #     源码清单与 git 状态
│   ├── firmware/              #   原始固件/镜像/证据材料（约 21 GB）
│   │   └── README.md
│   ├── build/                 #   构建产物：boot 镜像、initramfs、安装包
│   │   └── README.md
│   ├── tools/                 #   构建/刷写/打包工具
│   └── work/                  #   分析工作区（反汇编、日志、截图、证据）
│       └── README.md
│
├── v50/                       # ===== ZTE V50 / F50 / MU300（UMS9620）=====
│   ├── README.md              #   项目总览
│   ├── docs/                  #   资料：逆向分析报告（来自原 work/rev）
│   │   ├── INDEX.md
│   │   └── reports/           #     V50/F50 专题分析
│   ├── src/                   #   源码
│   │   ├── README.md
│   │   ├── mu300-linux/       #     主仓库（原 v50-linux，git，9 GB）
│   │   ├── zte-oss/           #     ZTE 官方 GPL 源码包（5.9 GB）
│   │   ├── kernel-ums9620-tuned/  # U30 Air 社区内核树
│   │   └── mu300-linux-snapshots/ # 历史快照与补丁
│   ├── firmware/              #   固件备份与提取文件（约 4.9 GB）
│   │   └── README.md
│   ├── build/                 #   预编译产物（prebuilt/）
│   ├── tools/                 #   ufitools（APK/手册逆向）
│   └── work/                  #   分析工作区（rev/ 逆向报告、整机测试镜像）
│       └── README.md
│
└── shared/                    # ===== 公共资源 =====
    ├── README.md
    ├── toolchains/            #   aarch64-linux-musl、arm-gnu-toolchain-13.2
    ├── dl/                    #   源码/工具链下载缓存
    ├── host/                  #   宿主机构建辅助（openssl-local、host-libs 等）
    ├── packages/              #   宿主机构建依赖（.deb）
    └── tools/                 #   pylib、tools7z、pipcache
```

各目录的详细内容、文件清单与用途见对应 `README.md` / `INDEX.md`。

---

## 3. 快速导航

**我是新手，先看什么？**

1. 本文件（总览） → 2. 选一个项目 → 3. **该项目的 `HANDOFF.md`（交接入口）** → 4. 该项目的 `docs/INDEX.md`。

**开两个会话分别研究（推荐入口）**

| 会话 | 第一条该读的文档 | 项目总览 | 资料索引 |
|---|---|---|---|
| **T508N**（mainline 6.12 bring-up） | ★ [t508/docs/HANDOFF.md](t508/docs/HANDOFF.md) → [FINAL-BRINGUP-INDEX.md](t508/docs/FINAL-BRINGUP-INDEX.md) | [t508/README.md](t508/README.md) | [t508/docs/INDEX.md](t508/docs/INDEX.md) |
| **V50 / F50 / MU300**（跑 Linux，保留安卓） | ★ [v50/docs/HANDOFF.md](v50/docs/HANDOFF.md) → [会话归档](v50/docs/ARCHIVE-20260929-V50会话总结.md) | [v50/README.md](v50/README.md) | [v50/docs/INDEX.md](v50/docs/INDEX.md) |

两份 `HANDOFF.md` 都是**自包含**的：30 秒摘要、红线、当前状态、目录地图、关键事实、常用命令、
下一步候选、文档地图，以及一节"给新会话的第一条消息（可直接粘贴）"。开新对话时把那一节复制过去即可。

**继续 T508N 的 mainline bring-up：**
* 交接入口：[t508/docs/HANDOFF.md](t508/docs/HANDOFF.md)
* 自包含主索引：[t508/docs/FINAL-BRINGUP-INDEX.md](t508/docs/FINAL-BRINGUP-INDEX.md)
* 源码树：[t508/src/linux-6.12.103/](t508/src/linux-6.12.103/)（HEAD `cb8e725f3` + 13 个未提交文件）
* 刷机流程：加载 `t508n-flashing` skill（`.dsh/skills/t508n-flashing.md`）

**在 V50/F50 上装 Linux：**
* 交接入口：[v50/docs/HANDOFF.md](v50/docs/HANDOFF.md)
* 主仓库：[v50/src/mu300-linux/README.md](v50/src/mu300-linux/README.md)
* 项目索引：[v50/src/mu300-linux/docs/INDEX.md](v50/src/mu300-linux/docs/INDEX.md)
* V50 特有差异：[v50/src/mu300-linux/docs/V50.md](v50/src/mu300-linux/docs/V50.md)

**我要找某份旧报告：** 查 [docs/PATH-MAPPING.md](docs/PATH-MAPPING.md) 或直接用文件名搜 `docs/`。

---

## 4. 分类原则（本次整理采用）

| 类别 | 判定 | 典型位置 |
|---|---|---|
| **资料**（docs/materials） | 报告、分析、prompt、会话归档、逆向结论 | `<project>/docs/` |
| **源码**（src） | 可编译的树、设备树、代码仓库、补丁 | `<project>/src/` |
| **固件**（firmware） | 原厂镜像、分区 dump、payload、提取出的文件系统、证据 | `<project>/firmware/` |
| **构建产物**（build） | 编译输出的 Image/boot.img/initramfs/模块 | `<project>/build/` |
| **工具**（tools） | 打包/刷写/逆向脚本与二进制工具 | `<project>/tools/`（项目专用）、`shared/tools/`（通用） |
| **分析工作区**（work） | 反汇编输出、日志、截图、临时证据、中间产物 | `<project>/work/` |
| **公共资源**（shared） | 交叉工具链、下载缓存、宿主机依赖 | `shared/` |

归属判定以**设备**为准（不是“哪个月在用”）。例：`kernel-ums9620-tuned` 是 ZTE U30 Air/F50 内核，
归 `v50/src/`，尽管 T508N 的报告曾引用它作为参考。

---

## 5. 环境与复现

* **交叉工具链**：`shared/toolchains/arm-gnu-toolchain-13.2/`（aarch64-none-linux-gnu-）、
  `shared/toolchains/aarch64-linux-musl-cross/`（musl 静态）。
  旧路径 `/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu` 仍有兼容软链。
* **下载缓存**：`shared/dl/`。
* **宿主机构建依赖**：`shared/packages/*.deb`、`shared/host/`、`shared/tools/tools7z/`。
* 各项目的具体构建命令见其 `README.md` 与 `docs/`。

---

## 6. 本次重组的可追溯性

* 迁移前基线（文件数 945,698 / 目录 66,522）与迁移后校验记录见 [docs/reorg/](docs/reorg/)。
* 逐项旧→新映射：288 条，见 [docs/PATH-MAPPING.md](docs/PATH-MAPPING.md)。
* 受影响的 14 个脚本已修复为绝对新路径；历史文档保留原文并给映射表。
* 迁移后**普通文件数** = 迁移前 945,697 + 新增文档 19 = 945,716，**未丢文件**。

---

## 7. 免责声明

两个项目均涉及对自有设备的固件修改与刷机，存在变砖风险。原厂固件、厂商驱动、签名密钥等
版权归各自所有者；本工作区仅作个人研究归档，不分发专有二进制到公共仓库。
`t508/tools/ot5_boot_key.pem` 等私钥文件**不应外传**。
