# 工作区规范与约定

> 适用于 `/home/test/t508n` 整棵目录树。重组日期：2026-09-29。

## 1. 为什么这样分

重组前，两个设备的资料、源码、固件、构建产物混在同一个 `work/` 里（40 GB、212 个条目），
仅靠文件名和时间区分。重组后按 **设备 → 用途** 两级分类：

```
<project>/{docs,src,firmware,build,tools,work}
```

* **设备是第一分类维度**：打开 `t508/` 只看到 T508N 的东西，打开 `v50/` 只看到 V50/F50 的东西。
* **用途是第二分类维度**：资料与源码分离，固件与构建产物分离，可编译的东西不混在报告里。
* **公共资源抽到 `shared/`**：工具链、下载缓存、宿主机依赖，避免两份拷贝。

## 2. 各类目录的边界

| 目录 | 放什么 | 不放什么 |
|---|---|---|
| `docs/` | 报告、分析、prompt、逆向结论、会话归档；**文本资料** | 可编译源码、二进制镜像 |
| `src/` | 内核/设备树/代码仓库、补丁、源码压缩包 | 编译产物、原厂 dump |
| `firmware/` | 原厂镜像、分区 dump、payload、提取出的文件系统、证据 | 自己编译出的 Image |
| `build/` | 自己构建的产物：boot 镜像、initramfs、模块、安装包、构建日志 | 原厂固件 |
| `tools/` | 打包/刷写/逆向脚本、工具二进制、私钥（敏感） | 大批原始材料 |
| `work/` | 反汇编输出、运行时日志、截图、中间证据、一次性脚本 | 已定稿的报告（应移入 `docs/`） |
| `shared/` | 交叉工具链、下载缓存、宿主机依赖 | 任何设备专属内容 |

> 实践中 `work/` 与 `docs/` 的界线是“是否定稿”。分析过程中产物留在 `work/`，
> 结论沉淀成报告后放 `docs/`。

## 3. 命名约定

* 报告：`REPORT-<主题>.md`；修复方案：`PATCH-<主题>.md`；任务书：`prompt-<主题>.md`；
  指南：`GUIDE-<主题>.md`；错误诊断：`DIAG-<主题>.md`。
* 归档：`ARCHIVE-<日期>.md`；阶段总结：`FINAL-WRAPUP-<日期>.md`、`FINAL-BRINGUP-INDEX.md`。
* 日期格式 `YYYYMMDD`。中文文件名保留（大量历史文档已用中文，不强制改名）。

## 4. 环境

### 交叉工具链

| 工具链 | 位置 | 用途 |
|---|---|---|
| `aarch64-none-linux-gnu-` | `shared/toolchains/arm-gnu-toolchain-13.2/` | T508N 各内核树（5.4/6.12）、V50 内核 |
| `aarch64-linux-musl-` | `shared/toolchains/aarch64-linux-musl-cross/` | 静态 busybox / initramfs |

旧路径兼容软链仍在：`/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu`。

### 宿主机构建依赖

* `shared/packages/`：`make`、`libssl-dev` 的 `.deb`。
* `shared/host/openssl-local/`、`shared/host/host-libs/`：宿主可能需要的本地库。
* `shared/tools/tools7z/`：解包工具（`7za`）。
* `shared/tools/pylib/`、`shared/tools/pipcache/`：Python 依赖与缓存。

### 下载缓存

`shared/dl/`：`linux-6.12.103.tar.xz`、`busybox-1.36.1.tar.bz2`、两个工具链压缩包。

### 4.3 本次会话确认的环境限制（2026-09-29）

| 项 | 现象 | 应对 |
|---|---|---|
| **WSL 没有 USB 总线** | `/sys/bus` 下没有 `usb`；`lsusb` 未安装；usbipd-win 即使 bind 也无法把设备挂进这个 WSL 内核 | 设备插 **Windows** 侧：跑一键包 / `install.ps1`；或在 WSL 里包装 `adb.exe`（本地路径要用 `wslpath -w` 转换），或让 Windows adb 以 `-a` 监听 + `ADB_SERVER_SOCKET=<Windows IP>:5037` |
| adb 通路 | `ADB_SERVER_SOCKET=tcp:127.0.0.1:5037` 已设，但从 WSL 连接被拒（非 mirrored 网络）；`/mnt/c/platform-tools/adb.exe` 可直接用 | 无线 ADB 也是一条路（V50 B26 自带），`adb connect <手机IP>:5555` |
| Docker | 引擎（Rancher Desktop 的 WSL 发行版）当时未运行，`docker.sock` 不存在 → `kernel/build-all.sh`、`openwrt/build-rootfs.sh`、`install.sh --build` 都不能跑 | 走**本机 LLVM 路线**编内核（已验证，见 `v50/src/mu300-linux/docs/BUILD-MU3351.md` §3）；或用已暂存的 prebuilt 资产 |
| 网络 | `github.com` 常不可达（TCP 超时），但 `api.github.com`、`codeload.github.com`、`downloads.openwrt.org`、`pypi.org`、`ports.ubuntu.com` 可达 | release 资产用暂存（`work/release/<tag>/SHA256SUMS`），源码用 `tools/stage-*.sh` |
| 7-Zip | WSL 内没有 `7z` | 用 `/mnt/c/Program Files/7-Zip/7z.exe`（传 **Windows** 路径，`wslpath -w`；相对路径也行，但别传 `/mnt/...`） |
| 构建依赖 | 已装 `lz4`（打包 boot 镜像必需）；`poppler-utils` 未装（需要 PDF 转文本时再装）；LLVM 18 全套在 `/usr/lib/llvm-18/bin` | — |
| 主机规格 | 8 核 / 15 GiB / 根分区 793 GB 可用 | 内核全量构建（含 ThinLTO）要几 GB 与数十分钟；建议 `O=` 出树构建并保留 `work/kbuild/` 做增量 |

## 5. 复现各自的构建

| 项目 | 入口 |
|---|---|
| T508N 原厂式 5.4 内核 | `t508/docs/reports/T508N-VENDOR-BUILD-RECIPE.md`、`t508/docs/reports/REPORT-54-custom-build.md` |
| T508N mainline 6.12 | `t508/docs/reports/HANDOFF-3-SESSIONS.md`、`t508/docs/FINAL-BRINGUP-INDEX.md` |
| T508N 打包/刷写工具 | `t508/tools/`（`sprd_mkbootimg.py`、`patch_vendor_boot.py`、`build_*.sh`） |
| T508N 刷写流程 | 技能 `t508n-flashing`（`.dsh/skills/t508n-flashing.md`） |
| V50/F50 全流程 | `v50/src/mu300-linux/docs/BUILD.md`、`FROM-SOURCE-TO-OPENWRT.md` |
| V50/MU3351 官方树 | `v50/src/mu300-linux/docs/BUILD-MU3351.md`；原始包在 `v50/src/zte-oss/` |

## 6. 安全与保密

以下内容**不得**提交到公共仓库或外传：

| 内容 | 位置 | 原因 |
|---|---|---|
| boot 签名私钥 | `t508/tools/ot5_boot_key.pem`、`test_key.pem` | 私钥 |
| V50 整机备份 | `v50/firmware/v50-backup/`、`v50/firmware/v50-full/` | 含 prodnv / IMEI / 校准数据 |
| GitHub PAT | `t508/src/t508n-repo/.git/config` 等 git remote | 令牌泄露 |
| 原厂固件与厂商驱动 | `t508/firmware/`、`v50/src/zte-oss/`、`v50/firmware/` | 版权归厂商 |

> 处理建议：不要把工作区整体推到公共仓库；分享单个项目时用 `.gitignore` 排除上述路径。

## 7. 如何新增材料（保持分类不退化）

1. 先判断**属于哪个设备**；不属于任何设备（纯工具/缓存）才放 `shared/`。
2. 再判断**用途**：文本分析 → `docs/`；可编译 → `src/`；原厂镜像 → `firmware/`；
   自己的编译输出 → `build/`；过程产物 → `work/`。
3. 大文件（>100 MB）尽量留在原目录并写说明，不要拷进 `docs/`。
4. 新增报告时同步更新该项目的 `docs/INDEX.md`。

## 8. 本次重组的记录

| 项目 | 位置 |
|---|---|
| 迁移日志（逐条 MOVE/LINK） | [reorg/move.log](reorg/move.log) |
| 迁移前基线（文件数/大小） | [reorg/baseline.txt](reorg/baseline.txt)、[baseline_top.tsv](reorg/baseline_top.tsv)、[baseline_work.tsv](reorg/baseline_work.tsv) |
| 逐项旧→新映射（288 条） | [PATH-MAPPING.md](PATH-MAPPING.md) |
| 生成脚本 | [reorg/move1.sh](reorg/move1.sh)、[move2.sh](reorg/move2.sh)、[links.sh](reorg/links.sh)、[gen_map.py](reorg/gen_map.py) |

校验结果：迁移前后**普通文件数一致**（945,698 → 945,697，差额是基线文件自身），
无文件丢失。旧顶层入口由兼容软链保留，详见 PATH-MAPPING.md。
