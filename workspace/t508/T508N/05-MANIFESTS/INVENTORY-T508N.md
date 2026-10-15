# T508N 项目文件夹盘点总表（INVENTORY）

> 生成时间：2026-10-09 16:23:12 CST
> 设备：TCL T508N / OT5，Unisoc UMS9620（T770），3GB LPDDR4x + eMMC
> 范围：Windows `E:/rootphone/t508n`、`E:/Download` T508N 相关包、`C:/platform-tools`；
>       WSL `/home/test/t508n/t508`、`/home/test/t508n` 顶层入口
> 本表为**只读盘点**产物：未删除、未移动、未覆盖任何原始文件。

## 1. 盘点范围与路径存在性

| 用户指定路径 | WSL 对应 | 状态 | 文件数 | 体积 |
|---|---|---|---:|---:|
| `E:/rootphone/t508n` | `/mnt/e/rootphone/t508n` | 存在 | 791 | 39.11 GiB |
| `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` | `同左` | 存在（v2 工具目录） | — | — |
| `E:/rootphone/t508n/2unlock` | `同左` | 存在（真机原始备份） | — | — |
| `E:/rootphone/t508n/savebrock` | `同左` | 存在（stock dump #1） | — | — |
| `E:/rootphone/t508n/savebrocka` | `同左` | 存在（stock dump #2） | — | — |
| `E:/Download/ums9620_tcl_t508n_v3 (1)` | `同左` | 存在（v3 工具） | 23 | 143.40 MiB |
| `E:/Download/ums9620_tcl_t508n_magic64` | `同左` | 存在（magic64 一键包） | 41 | 29.43 MiB |
| `E:/Download/ums9620_tcl_t508n_magic64.zip` | `同左` | 存在 | 10 | 83.34 MiB |
| `E:/Download/ums9620_tcl_t508n_v2 或类似 v2 工具目录` | `E:/Download` | 未找到目录；仅有 v2 的 zip 与 E:/rootphone 下的解压目录 | 10 | 83.34 MiB |
| `E:/Download/ums9620_tcl_t508n_v6(.zip)` | `同左` | 存在（额外发现，未在任务书中） | 10 | 83.34 MiB |
| `C:/platform-tools` | `/mnt/c/platform-tools` | 存在（含 T508N 构建产物与 srag 工具） | 441 | 15.67 GiB |
| `/home/test/t508n/t508` | `同左` | 存在 | 6754 | 71.27 GiB |
| `/home/test/t508n/t508/docs` | `同左` | 存在 | — | — |
| `/home/test/t508n/t508/work` | `同左` | 存在 | — | — |
| `/home/test/t508n/t508/mainline-ref/t508n-mainline` | `同左` | 存在 | — | — |
| `/home/test/t508n/t508/build` | `同左` | 存在 | — | — |

未找到（如实记录，不做猜测）：

- `E:/Download/ums9620_tcl_t508n_v2` 目录不存在。v2 内容以 `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/` 与 `E:/Download/2、解BL锁工具ums9620_tcl_t508n_v2.zip` 形式存在。
- `rootfs-vendor-usb` 未找到同名文件/目录；相关物证为 `boot_54openwrt_vendor_usbserial*.img` 与 `work/build-vendor-usb-modules.log`。
- `E:/Download/ums9620_tcl_t508n_v2` 之外不存在 v4/v5 工具目录。

## 2. 口径与方法

| 项 | 做法 |
|---|---|
| 元数据 | find -printf 一次遍历，记录 路径/类型/大小/mtime/软链目标 |
| 哈希 | SHA256（8 MiB 分块，6 线程），敏感文件跳过 |
| 折叠 | 大体积上游源码树/解包产物折叠为 1 条摘要（见第 5 节） |
| 工具 | 只读：find / sha256 / python3；无 adb / fastboot / spd_dump / dd 写操作 |
| 敏感 | 匹配 .pem/.key/.p12/.pfx/.jks/keystore/*secret*/*credential*/*token*/calinv*/t508n_key 的文件不读取内容、不哈希、不复制 |
| 过程产物 | `/home/test/t508n/t508/work/folder-audit/`（raw TSV、entries.json、hashes.tsv、脚本） |

## 3. 总览

| 指标 | 数值 |
|---|---:|
| 登记条目（含目录/软链/折叠摘要） | 8767 |
| 登记文件 | 8061 |
| 已计算 SHA256 | 8029（92.14 GiB） |
| 未哈希（非敏感） | 0（0 B） |
| 敏感文件（只登记） | 32 |
| 关键构件 | 1336 |
| 内容重复组 | 1695 |
| 折叠目录 | 21（合计 34.09 GiB） |

### 3.1 分类统计

| 用途分类 | 文件数 | 体积 | 其中关键 |
|---|---:|---:|---:|
| 文档 | 1387 | 35.15 MiB | 454 |
| 内核/设备树产物 | 1211 | 256.98 MiB | 0 |
| 源码/补丁 | 856 | 198.77 MiB | 0 |
| 日志/证据 | 705 | 1.37 GiB | 0 |
| 其他 | 683 | 144.15 MiB | 0 |
| 构建中间产物 | 651 | 33.25 MiB | 0 |
| stock 分区镜像 | 507 | 19.67 GiB | 507 |
| 数据清单 | 271 | 5.51 MiB | 0 |
| Shell 脚本/工具 | 217 | 249.4 KiB | 0 |
| 反汇编/证据 | 195 | 22.56 MiB | 0 |
| 分区/文件系统镜像 | 170 | 63.01 GiB | 0 |
| Python 脚本/工具 | 127 | 981.6 KiB | 0 |
| Windows 驱动文件 | 126 | 5.11 MiB | 0 |
| 截图/图片 | 124 | 3.83 MiB | 0 |
| 分区表/配置 | 109 | 1.72 MiB | 0 |
| 共享库/静态库 | 93 | 17.32 MiB | 0 |
| U-Boot 镜像 | 69 | 391.42 MiB | 69 |
| 解锁工具/载荷 | 66 | 33.80 MiB | 66 |
| 校验清单 | 59 | 5.2 KiB | 59 |
| SPD/BROM 工具 | 50 | 4.10 MiB | 23 |
| APK 包 | 45 | 589.34 MiB | 0 |
| fastboot/adb 工具 | 44 | 77.95 MiB | 4 |
| Windows 脚本 | 35 | 52.8 KiB | 35 |
| 解锁/FDL 载荷 | 32 | 27.07 MiB | 32 |
| 敏感文件（只登记不读取） | 32 | 75.06 MiB | 0 |
| 二进制载荷 | 24 | 96.83 MiB | 0 |
| 启动链二进制/载荷 | 19 | 60.19 MiB | 19 |
| 压缩包 | 17 | 537.10 MiB | 0 |
| Windows PowerShell 脚本 | 17 | 14.8 KiB | 0 |
| 安装包 | 15 | 3.90 MiB | 0 |
| 构建描述 | 14 | 10.3 KiB | 0 |
| stock misc 镜像 | 13 | 13.00 MiB | 13 |
| misc 槽位控制 | 11 | 2.01 MiB | 11 |
| 解锁工具压缩包 | 9 | 111.84 MiB | 9 |
| vbmeta 镜像 | 9 | 36.0 KiB | 9 |
| 原始数据/截图 | 8 | 18.55 MiB | 0 |
| initramfs 归档 | 8 | 107.92 MiB | 0 |
| magic64 U-Boot | 7 | 21.00 MiB | 7 |
| Android 平台工具 | 6 | 4.97 MiB | 0 |
| 整机备份压缩包 | 6 | 5.03 GiB | 6 |
| 内核镜像产物 | 6 | 177.23 MiB | 6 |
| stock U-Boot 镜像 | 4 | 12.00 MiB | 4 |
| stock miscdata 镜像 | 1 | 1.00 MiB | 1 |
| magic64 一键包 | 1 | 4.96 MiB | 1 |
| 镜像备份(.bak) | 1 | 105.42 MiB | 1 |
| 会话记录 | 1 | 9.65 MiB | 0 |

## 4. 关键构件清单

（只列 是否关键=是 的项；完整逐条清单见 `INVENTORY-T508N.csv`，路径树见 `TREE-T508N.txt`）

### 4.1 解锁工具/载荷（66）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `~/t508/work/srag/t508n-v3` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `~/t508/work/srag/t508n-v3` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `~/t508/work/srag/t508n-v6` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `~/t508/work/srag/t508n-v6` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `C:/platform-tools/srag-v3` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `C:/platform-tools/srag-v3` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `C:/platform-tools/srag-v6` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `C:/platform-tools/srag-v6` |
| `fdl2-cboot.bin` | 1.81 MiB | 2026-10-06 18:38 | `2b5ed2415d169746` | G48 | `C:/platform-tools/srag` |
| `fdl2-sign.bin` | 1.81 MiB | 2026-10-06 18:38 | `39b1616566ca3a95` | G27 | `C:/platform-tools/srag` |
| `fdl2-cboot.bin` | 1.81 MiB | 2026-10-09 14:38 | `2b5ed2415d169746` | G48 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fdl2-sign.bin` | 1.81 MiB | 2026-10-09 14:38 | `39b1616566ca3a95` | G27 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `E:/rootphone/t508n/2unlock` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `E:/rootphone/t508n/2unlock` |
| `fdl2-cboot.bin` | 1.81 MiB | 2024-03-08 22:51 | `2b5ed2415d169746` | G48 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `fdl2-sign.bin` | 1.81 MiB | 2024-03-08 22:28 | `39b1616566ca3a95` | G27 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2024-03-08 22:54 | `e0846c50271cfa8b` | G71 | `~/t508/work/srag/t508n-v3` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2024-03-08 22:28 | `682e17e3485eb827` | G61 | `~/t508/work/srag/t508n-v3` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2024-03-08 22:54 | `e0846c50271cfa8b` | G71 | `C:/platform-tools/srag-v3` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2024-03-08 22:28 | `682e17e3485eb827` | G61 | `C:/platform-tools/srag-v3` |
| `spl.bin` | 65.5 KiB | 2026-10-09 14:41 | `682e17e3485eb827` | G61 | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2026-10-09 14:38 | `e0846c50271cfa8b` | G71 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2026-10-09 14:38 | `682e17e3485eb827` | G61 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2024-03-08 22:54 | `e0846c50271cfa8b` | G71 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `spl.bin` | 65.5 KiB | 2026-10-09 08:38 | `682e17e3485eb827` | G61 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2024-03-08 22:28 | `682e17e3485eb827` | G61 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2024-03-08 22:54 | `e0846c50271cfa8b` | G71 | `E:/rootphone/t508n/2unlock` |
| `spl.bin` | 65.5 KiB | 2026-10-08 13:57 | `354dadd5f23bcf4d` |  | `E:/rootphone/t508n/2unlock` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2024-03-08 22:28 | `682e17e3485eb827` | G61 | `E:/rootphone/t508n/2unlock` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 2024-03-08 22:54 | `e0846c50271cfa8b` | G71 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | 2024-03-08 22:28 | `682e17e3485eb827` | G61 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `spl_a.bin` | 63.8 KiB | 2026-08-15 16:40 | `021645f832e39a88` |  | `~/t508/firmware/payloads` |
| `pgpt.bin` | 32.0 KiB | 2026-10-06 17:45 | `0fc0fd1a2f1793fd` | G95 | `~/t508/work/srag/t508n-v6` |
| `pgpt.bin` | 32.0 KiB | 2026-10-08 22:05 | `0fc0fd1a2f1793fd` | G95 | `C:/platform-tools/srag-v6` |
| `pgpt.bin` | 32.0 KiB | 2026-10-06 18:39 | `0fc0fd1a2f1793fd` | G95 | `C:/platform-tools/srag` |
| `pgpt.bin` | 32.0 KiB | 2026-10-09 08:47 | `0fc0fd1a2f1793fd` | G95 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `pgpt.bin` | 32.0 KiB | 2026-10-08 22:01 | `0fc0fd1a2f1793fd` | G95 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `gen_spl-unlock.exe` | 10.5 KiB | 2024-07-28 09:30 | `3b8dce4360ee3c72` | G227 | `~/t508/work/srag/t508n-v6` |
| `gen_spl-unlock.exe` | 10.5 KiB | 2024-07-28 09:30 | `3b8dce4360ee3c72` | G227 | `C:/platform-tools/srag-v6` |
| `gen_spl-unlock.exe` | 10.5 KiB | 2026-10-06 18:38 | `3b8dce4360ee3c72` | G227 | `C:/platform-tools/srag` |
| `sprdpart_check.bin` | 3.1 KiB | 2026-10-05 18:50 | `6dcdb75df51181d3` | G2 | `~/t508/work/srag/t508n-v6` |
| `sprdpart_check.bin` | 3.1 KiB | 2026-10-05 18:50 | `6dcdb75df51181d3` | G2 | `C:/platform-tools/srag-v6` |
| `sprdpart_check.bin` | 3.1 KiB | 2026-10-06 18:38 | `6dcdb75df51181d3` | G2 | `C:/platform-tools/srag` |
| `unlock_emmc.bat` | 1.6 KiB | 2024-03-23 17:51 | `41d1f9f905a16b6a` | G224 | `~/t508/work/srag/t508n-v3` |
| `unlock_emmc.bat` | 1.6 KiB | 2024-03-23 17:51 | `41d1f9f905a16b6a` | G224 | `C:/platform-tools/srag-v3` |
| `unlock_emmc.bat` | 1.6 KiB | 2024-03-23 17:51 | `41d1f9f905a16b6a` | G224 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `unlock_emmc.bat` | 1.5 KiB | 2024-03-09 00:17 | `27a631cd2f232b1b` | G401 | `E:/rootphone/t508n/2unlock` |
| `unlock_emmc.bat` | 1.5 KiB | 2024-03-09 00:17 | `27a631cd2f232b1b` | G401 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `unlock_autopatch_9620_autodl.bat` | 1.5 KiB | 2024-10-17 17:52 | `4fa8bd9db6fcb6ca` | G232 | `~/t508/work/srag/t508n-v6` |
| `unlock_autopatch_9620_autodl.bat` | 1.5 KiB | 2024-10-17 17:52 | `4fa8bd9db6fcb6ca` | G232 | `C:/platform-tools/srag-v6` |
| `unlock_autopatch_9620_autodl.bat` | 1.5 KiB | 2026-10-06 18:38 | `4fa8bd9db6fcb6ca` | G232 | `C:/platform-tools/srag` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2026-09-13 05:24 | `3842a80777dcde85` | G15 | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/vendor` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `~/t508/work/srag/t508n-v3` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `~/t508/work/srag/t508n-v6` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `C:/platform-tools/srag-v3` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `C:/platform-tools/srag-v6` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2026-10-06 18:38 | `3842a80777dcde85` | G15 | `C:/platform-tools/srag` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2026-10-09 14:35 | `3842a80777dcde85` | G15 | `C:/platform-tools/t508n-uboot-magic64` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2026-10-09 14:38 | `3842a80777dcde85` | G15 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `E:/rootphone/t508n/2unlock` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `custom_exec_no_verify_65012f48.bin` | 96 B | 2024-01-25 11:31 | `3842a80777dcde85` | G15 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |

### 4.2 解锁/FDL 载荷（32）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `~/t508/work/srag/t508n-v3` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `~/t508/work/srag/t508n-v6` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `C:/platform-tools/srag-v3` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `C:/platform-tools/srag-v6` |
| `fdl2-dl.bin` | 1.81 MiB | 2026-10-06 18:38 | `bbd5e1d7ecdd057a` | G16 | `C:/platform-tools/srag` |
| `fdl2-dl.bin` | 1.81 MiB | 2026-10-09 14:35 | `bbd5e1d7ecdd057a` | G16 | `C:/platform-tools/t508n-uboot-magic64` |
| `fdl2-dl.bin` | 1.81 MiB | 2026-10-09 14:38 | `bbd5e1d7ecdd057a` | G16 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/rootphone/t508n/2unlock` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `fdl2-dl.bin` | 1.81 MiB | 2024-03-08 22:49 | `bbd5e1d7ecdd057a` | G16 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `fdl2-dl.bin` | 1011.2 KiB | 2026-09-13 05:24 | `95fd4ecaeaf0d7d7` |  | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/vendor` |
| `fdl2-dl.bin` | 874.4 KiB | 2026-08-01 06:31 | `8ee9dde679aeb0e6` | G864 | `~/t508/work/srag-refs/UnisocBypass/fdl` |
| `fdl2-dl.bin` | 874.4 KiB | 2026-08-01 06:31 | `8ee9dde679aeb0e6` | G864 | `~/t508/work/srag/UnisocBypass-main/fdl` |
| `fdl1-dl.bin` | 60.6 KiB | 2026-09-13 05:24 | `4bebf3d2f1961002` | G402 | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/vendor` |
| `fdl1-dl.bin` | 60.6 KiB | 2024-03-06 01:08 | `4bebf3d2f1961002` | G402 | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300` |
| `fdl1-dl.bin` | 58.7 KiB | 2026-08-01 06:31 | `367f85f177e17b55` | G863 | `~/t508/work/srag-refs/UnisocBypass/fdl` |
| `fdl1-dl.bin` | 58.7 KiB | 2026-08-01 06:31 | `367f85f177e17b55` | G863 | `~/t508/work/srag/UnisocBypass-main/fdl` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `~/t508/work/srag/t508n-v3` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `~/t508/work/srag/t508n-v6` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `C:/platform-tools/srag-v3` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `C:/platform-tools/srag-v6` |
| `fdl1-dl.bin` | 52.9 KiB | 2026-10-06 18:38 | `352a013e9a010a39` | G17 | `C:/platform-tools/srag` |
| `fdl1-dl.bin` | 52.9 KiB | 2026-10-09 14:35 | `352a013e9a010a39` | G17 | `C:/platform-tools/t508n-uboot-magic64` |
| `fdl1-dl.bin` | 52.9 KiB | 2026-10-09 14:38 | `352a013e9a010a39` | G17 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `E:/rootphone/t508n/2unlock` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `fdl1-dl.bin` | 52.9 KiB | 2024-03-09 14:50 | `352a013e9a010a39` | G17 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |

### 4.3 SPD/BROM 工具（23）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `spd_dump.exe` | 79.0 KiB | 2025-04-25 10:39 | `2e4c117eb22ff800` | G113 | `~/t508/work/srag/t508n-v6` |
| `spd_dump.exe` | 79.0 KiB | 2025-04-25 10:39 | `2e4c117eb22ff800` | G113 | `C:/platform-tools/srag-v6` |
| `spd_dump.exe` | 79.0 KiB | 2026-10-06 18:38 | `2e4c117eb22ff800` | G113 | `C:/platform-tools/srag` |
| `spd_dump.exe` | 79.0 KiB | 2026-10-09 14:35 | `2e4c117eb22ff800` | G113 | `C:/platform-tools/t508n-uboot-magic64` |
| `spd_dump.exe` | 69.5 KiB | 2025-02-21 18:03 | `d0ab6cb48e418ca7` | G410 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `spd_dump.exe` | 69.5 KiB | 2025-02-21 18:03 | `d0ab6cb48e418ca7` | G410 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `spd_dump.exe` | 39.5 KiB | 2024-03-16 21:35 | `5c75c4ea64d61226` | G109 | `~/t508/work/srag/t508n-v3` |
| `spd_dump.exe` | 39.5 KiB | 2024-03-16 21:35 | `5c75c4ea64d61226` | G109 | `C:/platform-tools/srag-v3` |
| `spd_dump.exe` | 39.5 KiB | 2026-10-09 14:38 | `5c75c4ea64d61226` | G109 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `spd_dump.exe` | 39.5 KiB | 2024-03-16 21:35 | `5c75c4ea64d61226` | G109 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `spd_dump_interactive.exe` | 39.5 KiB | 2024-02-27 17:56 | `e9a0c94ff9d6daac` |  | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300` |
| `spd_dump.exe` | 39.0 KiB | 2024-02-27 17:56 | `366b0c05beee1bf2` |  | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300` |
| `spd_dump.exe` | 35.5 KiB | 2024-02-27 17:55 | `2d2864b2a557c74a` | G400 | `E:/rootphone/t508n/2unlock` |
| `spd_dump.exe` | 35.5 KiB | 2024-02-27 17:55 | `2d2864b2a557c74a` | G400 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `chsize.exe` | 10.0 KiB | 2024-02-04 18:55 | `71dd62f4b94bab01` | G69 | `~/t508/work/srag/t508n-v3` |
| `chsize.exe` | 10.0 KiB | 2024-07-28 09:31 | `fa32406ab5b472d5` | G226 | `~/t508/work/srag/t508n-v6` |
| `chsize.exe` | 10.0 KiB | 2024-02-04 18:55 | `71dd62f4b94bab01` | G69 | `C:/platform-tools/srag-v3` |
| `chsize.exe` | 10.0 KiB | 2024-07-28 09:31 | `fa32406ab5b472d5` | G226 | `C:/platform-tools/srag-v6` |
| `chsize.exe` | 10.0 KiB | 2026-10-06 18:38 | `fa32406ab5b472d5` | G226 | `C:/platform-tools/srag` |
| `chsize.exe` | 10.0 KiB | 2026-10-09 14:38 | `71dd62f4b94bab01` | G69 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `chsize.exe` | 10.0 KiB | 2024-02-04 18:55 | `71dd62f4b94bab01` | G69 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `chsize.exe` | 10.0 KiB | 2024-02-04 18:55 | `71dd62f4b94bab01` | G69 | `E:/rootphone/t508n/2unlock` |
| `chsize.exe` | 10.0 KiB | 2024-02-04 18:55 | `71dd62f4b94bab01` | G69 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |

### 4.4 fastboot/adb 工具（4）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `fastboot.exe` | 2.10 MiB | 2026-04-15 18:36 | `dd55fef77ab2753b` |  | `/mnt/c/platform-tools` |
| `fastboot.exe` | 1.55 MiB | 2026-08-16 07:51 | `89cd293569093a16` | G403 | `~/t508/tools/winfastboot` |
| `fastboot.exe` | 1.55 MiB | 2022-11-04 14:21 | `89cd293569093a16` | G403 | `E:/rootphone/t508n/4、切换分区FastbootEnhance` |
| `FastbootEnhance.exe` | 137.0 KiB | 2022-11-04 23:03 | `e59b465758416ca9` |  | `E:/rootphone/t508n/4、切换分区FastbootEnhance` |

### 4.5 U-Boot 镜像（69）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `uboot_log.img` | 16.14 MiB | 2026-08-22 22:32 | `e49cde73a72680d3` |  | `~/t508/work/uboot/images` |
| `uboot_base.img` | 16.00 MiB | 2026-08-15 19:48 | `2c85d8eecd131c4e` | G119 | `~/t508/firmware/device-dumps` |
| `uboot_now.img` | 16.00 MiB | 2026-08-15 19:53 | `2c85d8eecd131c4e` | G119 | `~/t508/firmware/device-dumps` |
| `uboot_log.img` | 16.00 MiB | 2024-03-09 12:16 | `c2b558ca76b251eb` | G104 | `~/t508/firmware/stock-images` |
| `uboot_ev.img` | 16.00 MiB | 2026-08-15 20:37 | `230c51ce7e4c3674` |  | `~/t508/work/evidence/evidence_203711` |
| `uboot_log_2.img` | 16.00 MiB | 2026-08-15 19:21 | `f889ecb36af5c2dc` | G404 | `~/t508/work/uboot/images` |
| `uboot_log_3.img` | 16.00 MiB | 2026-08-15 19:45 | `2c85d8eecd131c4e` | G119 | `~/t508/work/uboot/images` |
| `uboot_log_new.img` | 16.00 MiB | 2026-08-15 18:54 | `2c2541173510dac4` | G422 | `~/t508/work/uboot/images` |
| `uboot_log2.img` | 16.00 MiB | 2024-01-16 18:19 | `f889ecb36af5c2dc` | G404 | `E:/rootphone/t508n/linux_support` |
| `uboot_log3.img` | 16.00 MiB | 2024-01-16 18:20 | `2c85d8eecd131c4e` | G119 | `E:/rootphone/t508n/linux_support` |
| `uboot_log.img` | 16.00 MiB | 2024-03-09 12:16 | `c2b558ca76b251eb` | G104 | `E:/rootphone/t508n/savebrock/Image` |
| `uboot_log.img` | 16.00 MiB | 2024-03-09 12:16 | `c2b558ca76b251eb` | G104 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_log.img` | 16.00 MiB | 2024-03-09 12:16 | `c2b558ca76b251eb` | G104 | `E:/rootphone/t508n/savebrocka/Image` |
| `uboot_log.img` | 16.00 MiB | 2024-03-09 12:16 | `c2b558ca76b251eb` | G104 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `uboot_log.img` | 16.00 MiB | 2024-01-16 18:23 | `2c2541173510dac4` | G422 | `/mnt/e/rootphone/t508n` |
| `uboot_a.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `~/t508/firmware/stock-images` |
| `uboot_b.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `~/t508/firmware/stock-images` |
| `uboot_t508n_lk_orig.bin` | 3.00 MiB | 2026-08-24 08:02 | `8b5a6131946b7e91` | G8 | `~/t508/firmware/stock` |
| `uboot_patched_v5.img` | 3.00 MiB | 2026-08-24 22:42 | `dcaca8d2ed9405b7` | G96 | `~/t508/src/t508n-repo/images` |
| `uboot_modified.img` | 3.00 MiB | 2026-08-01 06:31 | `068ab9e6eae2a698` | G877 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_stock.img` | 3.00 MiB | 2026-08-01 06:31 | `cd6c48aa59948f94` | G881 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_unlocked.img` | 3.00 MiB | 2026-08-01 06:31 | `7c2da3da88f684cd` | G880 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_modified.img` | 3.00 MiB | 2026-08-01 06:31 | `068ab9e6eae2a698` | G877 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `uboot_stock.img` | 3.00 MiB | 2026-08-01 06:31 | `cd6c48aa59948f94` | G881 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `uboot_unlocked.img` | 3.00 MiB | 2026-08-01 06:31 | `7c2da3da88f684cd` | G880 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `uboot.bin` | 3.00 MiB | 2026-08-24 08:01 | `8b5a6131946b7e91` | G8 | `~/t508/work/srag/t508n-v6` |
| `uboot_patched_usbon.img` | 3.00 MiB | 2026-08-25 13:03 | `7917ea86ace761f3` | G114 | `~/t508/work/srag/t508n-v6` |
| `uboot_patched_usbon_v2.img` | 3.00 MiB | 2026-08-24 08:49 | `6934b05548751873` | G115 | `~/t508/work/srag/t508n-v6` |
| `uboot_patched_usbon_v3w2.img` | 3.00 MiB | 2026-08-24 08:41 | `b8735dcaf79cec24` | G116 | `~/t508/work/srag/t508n-v6` |
| `uboot_patched_v4.img` | 3.00 MiB | 2026-08-24 08:46 | `ef28912bda6677d2` | G117 | `~/t508/work/srag/t508n-v6` |
| `uboot_patched_v5.img` | 3.00 MiB | 2026-08-24 22:34 | `dcaca8d2ed9405b7` | G96 | `~/t508/work/srag/t508n-v6` |
| `uboot_t508n_lk_orig.bin` | 3.00 MiB | 2026-08-25 17:14 | `8b5a6131946b7e91` | G8 | `~/t508/work/srag/t508n-v6` |
| `uboot.bin` | 3.00 MiB | 2026-08-24 08:02 | `8b5a6131946b7e91` | G8 | `~/t508/work/uboot/analysis` |
| `uboot_patched_usbon.img` | 3.00 MiB | 2026-08-24 08:09 | `7917ea86ace761f3` | G114 | `~/t508/work/uboot/images` |
| `uboot_patched_usbon_v2.img` | 3.00 MiB | 2026-08-24 08:22 | `6934b05548751873` | G115 | `~/t508/work/uboot/images` |
| `uboot_patched_usbon_v3w2.img` | 3.00 MiB | 2026-08-24 08:36 | `b8735dcaf79cec24` | G116 | `~/t508/work/uboot/images` |
| `uboot_patched_v4.img` | 3.00 MiB | 2026-08-24 08:44 | `ef28912bda6677d2` | G117 | `~/t508/work/uboot/images` |
| `uboot_patched_v5.img` | 3.00 MiB | 2026-08-24 22:33 | `dcaca8d2ed9405b7` | G96 | `~/t508/work/uboot/images` |
| `uboot.bin` | 3.00 MiB | 2026-08-24 08:01 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/srag-v6` |
| `uboot_patched_usbon.img` | 3.00 MiB | 2026-08-25 13:03 | `7917ea86ace761f3` | G114 | `C:/platform-tools/srag-v6` |
| `uboot_patched_usbon_v2.img` | 3.00 MiB | 2026-08-24 08:49 | `6934b05548751873` | G115 | `C:/platform-tools/srag-v6` |
| `uboot_patched_usbon_v3w2.img` | 3.00 MiB | 2026-08-24 08:41 | `b8735dcaf79cec24` | G116 | `C:/platform-tools/srag-v6` |
| `uboot_patched_v4.img` | 3.00 MiB | 2026-08-24 08:46 | `ef28912bda6677d2` | G117 | `C:/platform-tools/srag-v6` |
| `uboot_patched_v5.img` | 3.00 MiB | 2026-08-24 22:34 | `dcaca8d2ed9405b7` | G96 | `C:/platform-tools/srag-v6` |
| `uboot_t508n_lk_orig.bin` | 3.00 MiB | 2026-08-25 17:14 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/srag-v6` |
| `uboot.bin` | 3.00 MiB | 2026-10-06 18:38 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/srag` |
| `uboot_patched_usbon.img` | 3.00 MiB | 2026-10-06 18:38 | `7917ea86ace761f3` | G114 | `C:/platform-tools/srag` |
| `uboot_patched_usbon_v2.img` | 3.00 MiB | 2026-10-06 18:38 | `6934b05548751873` | G115 | `C:/platform-tools/srag` |
| `uboot_patched_usbon_v3w2.img` | 3.00 MiB | 2026-10-06 18:38 | `b8735dcaf79cec24` | G116 | `C:/platform-tools/srag` |
| `uboot_patched_v4.img` | 3.00 MiB | 2026-10-06 18:38 | `ef28912bda6677d2` | G117 | `C:/platform-tools/srag` |
| `uboot_patched_v5.img` | 3.00 MiB | 2026-10-06 18:38 | `dcaca8d2ed9405b7` | G96 | `C:/platform-tools/srag` |
| `uboot_t508n_lk_orig.bin` | 3.00 MiB | 2026-10-06 18:38 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/srag` |
| `uboot_a.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrock/Image` |
| `uboot_b.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrock/Image` |
| `uboot_a.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_b.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_a.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrocka/Image` |
| `uboot_b.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrocka/Image` |
| `uboot_a.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `uboot_b.img` | 3.00 MiB | 2024-03-09 12:16 | `8b5a6131946b7e91` | G8 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `uboota.bin` | 1.81 MiB | 2026-10-09 08:38 | `39b1616566ca3a95` | G27 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `ubootb.bin` | 1.81 MiB | 2026-10-09 08:38 | `39b1616566ca3a95` | G27 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `uboota.bin` | 1.81 MiB | 2026-10-08 13:57 | `403a9a89cf418674` |  | `E:/rootphone/t508n/2unlock` |
| `ubootb.bin` | 1.81 MiB | 2026-10-08 13:57 | `f7fe95924f49c7e1` |  | `E:/rootphone/t508n/2unlock` |
| `uboota.bin` | 1.81 MiB | 2026-10-09 14:41 | `a2641056c560545f` | G286 | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `ubootb.bin` | 1.81 MiB | 2026-10-09 14:41 | `a2641056c560545f` | G286 | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `uboot_a.bin` | 1.81 MiB | 2026-08-15 16:40 | `095f7fe9c04d6662` | G265 | `~/t508/firmware/payloads` |
| `uboot_a.bin` | 1.81 MiB | 2026-08-28 18:05 | `095f7fe9c04d6662` | G265 | `~/t508/work/vddusb33/decomp-handoff-full/firmware` |
| `uboot_a.bin` | 1.81 MiB | 2026-08-28 18:05 | `095f7fe9c04d6662` | G265 | `~/t508/work/vddusb33/decomp-handoff/firmware` |

### 4.6 stock U-Boot 镜像（4）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `uboot_a_stock.img` | 3.00 MiB | 2026-10-09 14:35 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/t508n-uboot-magic64` |
| `uboot_b_stock.img` | 3.00 MiB | 2026-10-09 14:35 | `8b5a6131946b7e91` | G8 | `C:/platform-tools/t508n-uboot-magic64` |
| `uboot_a_stock.img` | 3.00 MiB | 2026-10-09 14:38 | `8b5a6131946b7e91` | G8 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboot_b_stock.img` | 3.00 MiB | 2026-10-09 14:38 | `8b5a6131946b7e91` | G8 | `E:/Download/ums9620_tcl_t508n_magic64` |

### 4.7 magic64 U-Boot（7）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | 2026-10-09 14:30 | `d4961f76747369a2` | G110 | `~/t508/work/uboot/images` |
| `uboot_t508n_magic64_probe.img` | 3.00 MiB | 2026-10-09 14:35 | `fde6b307b86cc946` | G223 | `C:/platform-tools/t508n-uboot-magic64` |
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | 2026-10-09 14:35 | `d4961f76747369a2` | G110 | `C:/platform-tools/t508n-uboot-magic64` |
| `uboot_t508n_magic64_probe.img` | 3.00 MiB | 2026-10-09 14:31 | `fde6b307b86cc946` | G223 | `/mnt/c/platform-tools` |
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | 2026-10-09 14:31 | `d4961f76747369a2` | G110 | `/mnt/c/platform-tools` |
| `uboot_t508n_magic64_probe.img` | 3.00 MiB | 2026-10-09 14:38 | `fde6b307b86cc946` | G223 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | 2026-10-09 14:38 | `d4961f76747369a2` | G110 | `E:/Download/ums9620_tcl_t508n_magic64` |

### 4.8 misc 槽位控制（11）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `misc_slot_a.img` | 1.00 MiB | 2026-10-09 14:55 | `82bc63d7f0bb9907` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `misc_slot_b.img` | 1.00 MiB | 2026-10-09 14:55 | `4948bee107cb8b3e` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `misc-wipe.bin` | 2.0 KiB | 2024-05-16 14:57 | `bd6b67e852d6072e` | G231 | `~/t508/work/srag/t508n-v6` |
| `misc-wipe.bin` | 2.0 KiB | 2024-05-16 14:57 | `bd6b67e852d6072e` | G231 | `C:/platform-tools/srag-v6` |
| `misc-wipe.bin` | 2.0 KiB | 2026-10-06 18:38 | `bd6b67e852d6072e` | G231 | `C:/platform-tools/srag` |
| `misc-fastbootd.bin` | 84 B | 2024-03-06 01:50 | `0a23c55ca21259d9` | G70 | `~/t508/work/srag/t508n-v3` |
| `misc-fastbootd.bin` | 84 B | 2024-03-06 01:50 | `0a23c55ca21259d9` | G70 | `C:/platform-tools/srag-v3` |
| `misc-fastbootd.bin` | 84 B | 2026-10-09 14:38 | `0a23c55ca21259d9` | G70 | `E:/Download/ums9620_tcl_t508n_magic64` |
| `misc-fastbootd.bin` | 84 B | 2024-03-06 01:50 | `0a23c55ca21259d9` | G70 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `misc-fastbootd.bin` | 84 B | 2024-03-06 01:50 | `0a23c55ca21259d9` | G70 | `E:/rootphone/t508n/2unlock` |
| `misc-fastbootd.bin` | 84 B | 2024-03-06 01:50 | `0a23c55ca21259d9` | G70 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |

### 4.9 stock misc 镜像（13）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `misc.img` | 1.00 MiB | 2024-03-09 12:14 | `7db67f82f318dc58` | G64 | `~/t508/firmware/stock-images` |
| `miscdata.img` | 1.00 MiB | 2024-03-09 12:14 | `288619bd309dab10` | G65 | `~/t508/firmware/stock-images` |
| `misc.img` | 1.00 MiB | 2026-10-08 13:32 | `7db67f82f318dc58` | G64 | `/mnt/c/platform-tools` |
| `misc.img` | 1.00 MiB | 2026-10-08 20:48 | `7db67f82f318dc58` | G64 | `C:/platform-tools/stock` |
| `miscdata.img` | 1.00 MiB | 2026-10-08 20:48 | `288619bd309dab10` | G65 | `C:/platform-tools/stock` |
| `misc.img` | 1.00 MiB | 2024-03-09 12:14 | `7db67f82f318dc58` | G64 | `E:/rootphone/t508n/savebrock/Image` |
| `miscdata.img` | 1.00 MiB | 2024-03-09 12:14 | `288619bd309dab10` | G65 | `E:/rootphone/t508n/savebrock/Image` |
| `misc.img` | 1.00 MiB | 2024-03-09 12:14 | `7db67f82f318dc58` | G64 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `miscdata.img` | 1.00 MiB | 2024-03-09 12:14 | `288619bd309dab10` | G65 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `misc.img` | 1.00 MiB | 2024-03-09 12:14 | `7db67f82f318dc58` | G64 | `E:/rootphone/t508n/savebrocka/Image` |
| `miscdata.img` | 1.00 MiB | 2024-03-09 12:14 | `288619bd309dab10` | G65 | `E:/rootphone/t508n/savebrocka/Image` |
| `misc.img` | 1.00 MiB | 2024-03-09 12:14 | `7db67f82f318dc58` | G64 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `miscdata.img` | 1.00 MiB | 2024-03-09 12:14 | `288619bd309dab10` | G65 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |

### 4.10 stock miscdata 镜像（1）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `stock-miscdata.img` | 1.00 MiB | 2026-10-09 08:58 | `288619bd309dab10` | G65 | `C:/platform-tools/stock` |

### 4.11 stock 分区镜像（507）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `blackbox.img` | 500.00 MiB | 2024-03-09 12:14 | `19f6ff0da7fcec67` | G121 | `E:/rootphone/t508n/savebrock/Image` |
| `blackbox.img` | 500.00 MiB | 2024-03-09 12:14 | `19f6ff0da7fcec67` | G121 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `blackbox.img` | 500.00 MiB | 2024-03-09 12:14 | `19f6ff0da7fcec67` | G121 | `E:/rootphone/t508n/savebrocka/Image` |
| `blackbox.img` | 500.00 MiB | 2024-03-09 12:14 | `19f6ff0da7fcec67` | G121 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `boot_54vendor.img` | 105.42 MiB | 2026-08-28 21:13 | `397aa485abe5e709` |  | `~/t508/build/boot-images` |
| `vendor_boot_a_usb2.img` | 100.00 MiB | 2026-08-23 12:38 | `42435e808b5ad4e0` |  | `~/t508/build/boot-artifacts` |
| `vendor_boot_a_v3.img` | 100.00 MiB | 2026-08-23 20:35 | `c9152fabe622393e` |  | `~/t508/build/boot-artifacts` |
| `vendor_boot_linux_new.img` | 100.00 MiB | 2026-08-21 20:20 | `7c8f112ceaa45826` | G118 | `~/t508/build/boot-artifacts` |
| `vendor_boot_linux.img` | 100.00 MiB | 2026-10-05 18:39 | `7c8f112ceaa45826` | G118 | `~/t508/build/boot-images/t508n-linux-boot-latest` |
| `vendor_boot_linux.img` | 100.00 MiB | 2026-10-05 18:39 | `7c8f112ceaa45826` | G118 | `~/t508/build/boot-images` |
| `vendor_boot_a.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `~/t508/firmware/stock-images` |
| `vendor_boot_b.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `~/t508/firmware/stock-images` |
| `vendor_boot_a_stock.img` | 100.00 MiB | 2026-10-06 12:59 | `1c9563b23bad3a67` | G28 | `/mnt/c/platform-tools` |
| `vendor_boot_full_ppi12.img` | 100.00 MiB | 2026-10-06 07:56 | `35ef1ca645a2c8b5` |  | `/mnt/c/platform-tools` |
| `vendor_boot_linux.img` | 100.00 MiB | 2026-10-06 07:53 | `7c8f112ceaa45826` | G118 | `/mnt/c/platform-tools` |
| `vendor_boot_linux.img` | 100.00 MiB | 2026-08-15 19:01 | `5872eec55ef74c08` |  | `E:/rootphone/t508n/linux_support` |
| `vendor_boot_a.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrock/Image` |
| `vendor_boot_b.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrock/Image` |
| `vendor_boot_a.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `vendor_boot_b.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `vendor_boot_a.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrocka/Image` |
| `vendor_boot_b.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrocka/Image` |
| `vendor_boot_a.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `vendor_boot_b.img` | 100.00 MiB | 2024-03-09 12:16 | `1c9563b23bad3a67` | G28 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `boot_54custom.img` | 64.00 MiB | 2026-08-18 21:24 | `37bced71ac619476` |  | `~/t508/build/boot-images` |
| `boot_54custom_builtin.img` | 64.00 MiB | 2026-08-18 21:43 | `5b0fb1ded99ad7d8` |  | `~/t508/build/boot-images` |
| `boot_54custom_cinit_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:07 | `300f7623781fafc2` | G294 | `~/t508/build/boot-images` |
| `boot_54custom_clean_debug_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:04 | `8b2db38f702e3f0f` | G295 | `~/t508/build/boot-images` |
| `boot_54custom_clean_debug_builtin_v50align.img` | 64.00 MiB | 2026-10-05 19:48 | `81e36d4845a994a7` | G296 | `~/t508/build/boot-images` |
| `boot_54custom_clean_debug_mknod_v50align.img` | 64.00 MiB | 2026-10-05 20:01 | `fb58dceab31929d9` | G297 | `~/t508/build/boot-images` |
| `boot_54custom_clean_debug_v50align.img` | 64.00 MiB | 2026-10-05 19:38 | `afca39e9c202b41b` | G298 | `~/t508/build/boot-images` |
| `boot_54custom_minshell_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:32 | `7a74345f508c72ec` | G299 | `~/t508/build/boot-images` |
| `boot_54custom_probe.img` | 64.00 MiB | 2026-08-18 21:41 | `610971b79d4b9931` | G300 | `~/t508/build/boot-images` |
| `boot_54custom_probe_v50align.img` | 64.00 MiB | 2026-10-05 19:12 | `de2ab49bc7107013` | G301 | `~/t508/build/boot-images` |
| `boot_54custom_v50init_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:29 | `a9710e18f84232d3` | G302 | `~/t508/build/boot-images` |
| `boot_54openwrt_bindpersist.img` | 64.00 MiB | 2026-10-06 19:54 | `41b14633110a4e8f` | G303 | `~/t508/build/boot-images` |
| `boot_54openwrt_bindpersist2.img` | 64.00 MiB | 2026-10-06 19:56 | `b95b0dc476ba7e77` | G304 | `~/t508/build/boot-images` |
| `boot_54openwrt_diag1.img` | 64.00 MiB | 2026-10-06 17:34 | `d7d37ede55b07087` | G305 | `~/t508/build/boot-images` |
| `boot_54openwrt_diag2.img` | 64.00 MiB | 2026-10-06 17:48 | `c58f36a19501b11f` | G306 | `~/t508/build/boot-images` |
| `boot_54openwrt_dwc3_gadget.img` | 64.00 MiB | 2026-10-06 17:44 | `77bbc6ad65a3077e` | G307 | `~/t508/build/boot-images` |
| `boot_54openwrt_emmc_ncm.img` | 64.00 MiB | 2026-10-06 20:14 | `bbd43fcd9e9aa47e` | G308 | `~/t508/build/boot-images` |
| `boot_54openwrt_emmc_persistent.img` | 64.00 MiB | 2026-10-06 19:40 | `2e65e5238eb5d85c` | G309 | `~/t508/build/boot-images` |
| `boot_54openwrt_emmc_persistent2.img` | 64.00 MiB | 2026-10-06 19:43 | `a1ac131414e88250` | G310 | `~/t508/build/boot-images` |
| `boot_54openwrt_emmc_persistent3.img` | 64.00 MiB | 2026-10-06 19:51 | `5b436216609772ae` | G311 | `~/t508/build/boot-images` |
| `boot_54openwrt_emmc_rndis2.img` | 64.00 MiB | 2026-10-06 20:10 | `3cc19abd7c068906` | G312 | `~/t508/build/boot-images` |
| `boot_54openwrt_final7_autoreboot.img` | 64.00 MiB | 2026-10-06 19:46 | `7ac44856d0e62a64` | G313 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_auto.img` | 64.00 MiB | 2026-10-06 20:43 | `34a2dbb9490c7361` | G314 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_dbglog.img` | 64.00 MiB | 2026-10-06 22:32 | `c3e6a8515c4b9a89` | G315 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_iptables.img` | 64.00 MiB | 2026-10-06 23:57 | `9e7e495487f2fec7` | G316 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_iptables_rndis.img` | 64.00 MiB | 2026-10-07 00:06 | `2c4a61e5bd4a003a` | G317 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_iptables_rndis2.img` | 64.00 MiB | 2026-10-07 00:14 | `80b90ebd8a23fcf5` | G318 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto.img` | 64.00 MiB | 2026-10-07 00:36 | `60d2593ef95fa432` | G319 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto10_luci_keypad.img` | 64.00 MiB | 2026-10-07 16:58 | `eed3829371220520` | G320 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto11_gnss.img` | 64.00 MiB | 2026-10-07 17:11 | `d7263db24b62cad4` | G321 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto12_gnss2.img` | 64.00 MiB | 2026-10-07 18:10 | `f2a1564159c7405f` | G322 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto13_gnss_nmea.img` | 64.00 MiB | 2026-10-07 21:01 | `39ae9a703df2eca9` | G323 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto14_gps_luci.img` | 64.00 MiB | 2026-10-07 21:22 | `089271e3c43ca777` | G324 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto15_hotspot_net.img` | 64.00 MiB | 2026-10-08 08:53 | `73a61f5cc410dc20` | G325 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto2.img` | 64.00 MiB | 2026-10-07 07:24 | `921714234a8dee7d` | G326 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto3_pmicwdtfix.img` | 64.00 MiB | 2026-10-07 07:34 | `aaffa0be7e551977` | G327 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto4_pmicwdtfix2.img` | 64.00 MiB | 2026-10-07 07:37 | `b0acb4ec643dbf1f` | G328 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto5_keypad.img` | 64.00 MiB | 2026-10-07 15:55 | `f08fffe401b8e8af` | G111 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto6_keyd.img` | 64.00 MiB | 2026-10-07 16:37 | `f08fffe401b8e8af` | G111 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto7_keyd.img` | 64.00 MiB | 2026-10-07 16:39 | `76b9b21bd90c6d45` | G329 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto8_ui.img` | 64.00 MiB | 2026-10-07 16:51 | `ea2bcdb9ac9d353e` | G330 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_luci_rndis_auto9_uname.img` | 64.00 MiB | 2026-10-07 16:55 | `2a2fa8ac222aad67` | G331 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data.img` | 64.00 MiB | 2026-10-06 22:56 | `045ce5f29b2b1b77` | G332 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data2.img` | 64.00 MiB | 2026-10-06 23:02 | `b7e7f8143b946075` | G333 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data3_pmicwdt.img` | 64.00 MiB | 2026-10-06 23:13 | `85674a5f414132a5` | G334 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data4_pmicwdt2.img` | 64.00 MiB | 2026-10-06 23:16 | `596c79d55ffa3bd2` | G335 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data5_pmicwdt3.img` | 64.00 MiB | 2026-10-06 23:18 | `47d78c0a61f439b1` | G336 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_mu300data6_wdf.img` | 64.00 MiB | 2026-10-06 23:26 | `d3425c2bff14efee` | G337 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_nft_luci.img` | 64.00 MiB | 2026-10-06 23:43 | `d77210ada0a61774` | G338 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_nft_only.img` | 64.00 MiB | 2026-10-06 23:48 | `9f2ea15f8dacab05` | G339 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_propshim.img` | 64.00 MiB | 2026-10-06 21:12 | `39841209ab50c6d3` | G340 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_propshim_dbg.img` | 64.00 MiB | 2026-10-06 21:15 | `ec31361c22442576` | G341 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_propshim_fix.img` | 64.00 MiB | 2026-10-06 21:17 | `c4b4bc22ecfffacb` | G342 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_test.img` | 64.00 MiB | 2026-10-06 20:38 | `b23738198e9c1efa` | G343 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_trustyfix.img` | 64.00 MiB | 2026-10-06 21:20 | `5dc53a06d97b9596` | G344 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_trustyfix2.img` | 64.00 MiB | 2026-10-06 21:28 | `35454b5e25344940` | G345 | `~/t508/build/boot-images` |
| `boot_54openwrt_hotspot_trustyfix3.img` | 64.00 MiB | 2026-10-06 21:41 | `5f84525c911cea28` | G346 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag.img` | 64.00 MiB | 2026-10-06 18:00 | `9221b3c0d8c2f3ba` | G347 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag2.img` | 64.00 MiB | 2026-10-06 18:41 | `e592ca8b26108160` | G348 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag3.img` | 64.00 MiB | 2026-10-06 18:52 | `64ef728b4ec1030f` | G349 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag4.img` | 64.00 MiB | 2026-10-06 18:55 | `c9aa948bd6f36bea` | G350 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag5.img` | 64.00 MiB | 2026-10-06 18:57 | `7bb5d0b65bb2a367` | G351 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag6.img` | 64.00 MiB | 2026-10-06 18:59 | `1aaef101df84520d` | G352 | `~/t508/build/boot-images` |
| `boot_54openwrt_minidiag7.img` | 64.00 MiB | 2026-10-06 19:03 | `494182da2cc609ef` | G353 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_diag.img` | 64.00 MiB | 2026-10-06 19:08 | `415a91aea90fc28e` | G354 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_diag2.img` | 64.00 MiB | 2026-10-06 19:10 | `c72d979d8ad35cc2` | G355 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final.img` | 64.00 MiB | 2026-10-06 19:05 | `b86aecbb55204187` | G356 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final2.img` | 64.00 MiB | 2026-10-06 19:14 | `8b112b534c7646c2` | G357 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final3.img` | 64.00 MiB | 2026-10-06 19:17 | `84aab6a0f44bf73f` | G358 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final4.img` | 64.00 MiB | 2026-10-06 19:22 | `007319853cc67469` | G359 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final5.img` | 64.00 MiB | 2026-10-06 19:26 | `6dee88d60a4e59c8` | G360 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final6.img` | 64.00 MiB | 2026-10-06 19:29 | `922aa4be54d5bb23` | G361 | `~/t508/build/boot-images` |
| `boot_54openwrt_musb_final7_emmc.img` | 64.00 MiB | 2026-10-06 19:33 | `71388ae08d2af16f` | G362 | `~/t508/build/boot-images` |
| `boot_54openwrt_stable_emmc.img` | 64.00 MiB | 2026-10-06 20:00 | `b13ca5c4e0bb186e` | G363 | `~/t508/build/boot-images` |
| `boot_54openwrt_stable_emmc_rndis.img` | 64.00 MiB | 2026-10-06 20:02 | `bd92e8620d29eaab` | G364 | `~/t508/build/boot-images` |
| `boot_54openwrt_stockandroid2_usb.img` | 64.00 MiB | 2026-10-06 17:26 | `a085d9d13570816e` | G365 | `~/t508/build/boot-images` |
| `boot_54openwrt_stockandroid_usb.img` | 64.00 MiB | 2026-10-06 17:15 | `7346cd99d9c3a5ad` | G366 | `~/t508/build/boot-images` |
| `boot_54openwrt_t508.img` | 64.00 MiB | 2026-10-06 12:59 | `638831faeb8859c2` | G367 | `~/t508/build/boot-images` |
| `boot_54openwrt_t508_debug1.img` | 64.00 MiB | 2026-10-06 13:17 | `8af251afaf3f22f3` | G368 | `~/t508/build/boot-images` |
| `boot_54openwrt_t508_debug2.img` | 64.00 MiB | 2026-10-06 13:21 | `519137efa3762dd2` | G369 | `~/t508/build/boot-images` |
| `boot_54openwrt_t508_debug3.img` | 64.00 MiB | 2026-10-06 13:29 | `2ce5653cdd53de4e` | G370 | `~/t508/build/boot-images` |
| `boot_54openwrt_t508_debug4_usbserial.img` | 64.00 MiB | 2026-10-06 15:21 | `d27d4a590a7cfa0a` | G371 | `~/t508/build/boot-images` |
| `boot_54openwrt_usbchain_max.img` | 64.00 MiB | 2026-10-06 16:03 | `77c7ac5125667091` | G372 | `~/t508/build/boot-images` |
| `boot_54openwrt_vendor_usbserial.img` | 64.00 MiB | 2026-10-06 15:30 | `996d696fd20f5470` | G373 | `~/t508/build/boot-images` |
| `boot_54openwrt_vendor_usbserial2.img` | 64.00 MiB | 2026-10-06 15:46 | `fd13433b4bd9941e` | G374 | `~/t508/build/boot-images` |
| `boot_54openwrt_vendor_usbserial3.img` | 64.00 MiB | 2026-10-06 15:56 | `9f040ba4c7c36d8b` | G375 | `~/t508/build/boot-images` |
| `boot_54openwrt_wlan_test.img` | 64.00 MiB | 2026-10-06 20:29 | `064a3a8199aaf0b6` | G376 | `~/t508/build/boot-images` |
| `boot_54openwrt_wlan_test2.img` | 64.00 MiB | 2026-10-06 20:34 | `ca46ed100461ffff` | G377 | `~/t508/build/boot-images` |
| `boot_54openwrt_wlan_test3.img` | 64.00 MiB | 2026-10-06 20:36 | `8c9a8a5b0d5c8af4` | G378 | `~/t508/build/boot-images` |
| `boot_612_expC.img` | 64.00 MiB | 2026-10-05 21:05 | `054c476eef1a3ba7` | G379 | `~/t508/build/boot-images` |
| `boot_612_expC_rowlabels.img` | 64.00 MiB | 2026-10-05 21:13 | `6de60e6c9f1d9432` | G380 | `~/t508/build/boot-images` |
| `boot_612_stage2.img` | 64.00 MiB | 2026-10-06 07:26 | `7845acaa9df7e7b7` | G381 | `~/t508/build/boot-images` |
| `boot_612_stage3.img` | 64.00 MiB | 2026-10-06 07:31 | `c46ad03025155028` | G382 | `~/t508/build/boot-images` |
| `boot_612_stage4.img` | 64.00 MiB | 2026-10-06 07:36 | `0b3fc2df560804d9` | G383 | `~/t508/build/boot-images` |
| `boot_612_stage5.img` | 64.00 MiB | 2026-10-06 07:41 | `1826168d9aa3b831` | G384 | `~/t508/build/boot-images` |
| `boot_612_stage6a_delay618.img` | 64.00 MiB | 2026-10-06 08:02 | `cec9755dae64eeeb` | G385 | `~/t508/build/boot-images` |
| `boot_linux.img` | 64.00 MiB | 2026-10-05 18:56 | `56fa6c271346a13f` | G225 | `~/t508/build/boot-images` |
| `boot_linux_sgi.img` | 64.00 MiB | 2026-10-05 18:56 | `56fa6c271346a13f` | G225 | `~/t508/build/boot-images` |
| `boot_linux.img` | 64.00 MiB | 2026-10-05 18:39 | `825b3084f795de09` | G782 | `~/t508/build/boot-images/t508n-linux-boot-latest` |
| `boot_a.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `~/t508/firmware/stock-images` |
| `boot_b.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `~/t508/firmware/stock-images` |
| `cache.img` | 64.00 MiB | 2024-03-09 12:14 | `494344b0d7737407` | G97 | `~/t508/firmware/stock-images` |
| `metadata.img` | 64.00 MiB | 2024-03-09 12:14 | `b2a64c4e721ddee8` | G74 | `~/t508/firmware/stock-images` |
| `boot_54custom_cinit_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:07 | `300f7623781fafc2` | G294 | `/mnt/c/platform-tools` |
| `boot_54custom_clean_debug_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:04 | `8b2db38f702e3f0f` | G295 | `/mnt/c/platform-tools` |
| `boot_54custom_clean_debug_builtin_v50align.img` | 64.00 MiB | 2026-10-05 19:48 | `81e36d4845a994a7` | G296 | `/mnt/c/platform-tools` |
| `boot_54custom_clean_debug_mknod_v50align.img` | 64.00 MiB | 2026-10-05 20:01 | `fb58dceab31929d9` | G297 | `/mnt/c/platform-tools` |
| `boot_54custom_clean_debug_v50align.img` | 64.00 MiB | 2026-10-05 19:39 | `afca39e9c202b41b` | G298 | `/mnt/c/platform-tools` |
| `boot_54custom_minshell_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:32 | `7a74345f508c72ec` | G299 | `/mnt/c/platform-tools` |
| `boot_54custom_probe.img` | 64.00 MiB | 2026-10-05 19:07 | `610971b79d4b9931` | G300 | `/mnt/c/platform-tools` |
| `boot_54custom_probe_v50align.img` | 64.00 MiB | 2026-10-05 19:12 | `de2ab49bc7107013` | G301 | `/mnt/c/platform-tools` |
| `boot_54custom_v50init_bigbar_v50align.img` | 64.00 MiB | 2026-10-05 20:29 | `a9710e18f84232d3` | G302 | `/mnt/c/platform-tools` |
| `boot_54openwrt_bindpersist.img` | 64.00 MiB | 2026-10-06 19:54 | `41b14633110a4e8f` | G303 | `/mnt/c/platform-tools` |
| `boot_54openwrt_bindpersist2.img` | 64.00 MiB | 2026-10-06 19:56 | `b95b0dc476ba7e77` | G304 | `/mnt/c/platform-tools` |
| `boot_54openwrt_diag1.img` | 64.00 MiB | 2026-10-06 17:34 | `d7d37ede55b07087` | G305 | `/mnt/c/platform-tools` |
| `boot_54openwrt_diag2.img` | 64.00 MiB | 2026-10-06 17:48 | `c58f36a19501b11f` | G306 | `/mnt/c/platform-tools` |
| `boot_54openwrt_dwc3_gadget.img` | 64.00 MiB | 2026-10-06 17:44 | `77bbc6ad65a3077e` | G307 | `/mnt/c/platform-tools` |
| `boot_54openwrt_emmc_ncm.img` | 64.00 MiB | 2026-10-06 20:14 | `bbd43fcd9e9aa47e` | G308 | `/mnt/c/platform-tools` |
| `boot_54openwrt_emmc_persistent.img` | 64.00 MiB | 2026-10-06 19:40 | `2e65e5238eb5d85c` | G309 | `/mnt/c/platform-tools` |
| `boot_54openwrt_emmc_persistent2.img` | 64.00 MiB | 2026-10-06 19:43 | `a1ac131414e88250` | G310 | `/mnt/c/platform-tools` |
| `boot_54openwrt_emmc_persistent3.img` | 64.00 MiB | 2026-10-06 19:51 | `5b436216609772ae` | G311 | `/mnt/c/platform-tools` |
| `boot_54openwrt_emmc_rndis2.img` | 64.00 MiB | 2026-10-06 20:10 | `3cc19abd7c068906` | G312 | `/mnt/c/platform-tools` |
| `boot_54openwrt_final7_autoreboot.img` | 64.00 MiB | 2026-10-06 19:46 | `7ac44856d0e62a64` | G313 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_auto.img` | 64.00 MiB | 2026-10-06 20:43 | `34a2dbb9490c7361` | G314 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_dbglog.img` | 64.00 MiB | 2026-10-06 22:32 | `c3e6a8515c4b9a89` | G315 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_iptables.img` | 64.00 MiB | 2026-10-06 23:57 | `9e7e495487f2fec7` | G316 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_iptables_rndis.img` | 64.00 MiB | 2026-10-07 00:06 | `2c4a61e5bd4a003a` | G317 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_iptables_rndis2.img` | 64.00 MiB | 2026-10-07 00:14 | `80b90ebd8a23fcf5` | G318 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto.img` | 64.00 MiB | 2026-10-07 00:36 | `60d2593ef95fa432` | G319 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto10_luci_keypad.img` | 64.00 MiB | 2026-10-07 16:58 | `eed3829371220520` | G320 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto11_gnss.img` | 64.00 MiB | 2026-10-07 17:11 | `d7263db24b62cad4` | G321 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto12_gnss2.img` | 64.00 MiB | 2026-10-07 18:10 | `f2a1564159c7405f` | G322 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto13_gnss_nmea.img` | 64.00 MiB | 2026-10-07 21:01 | `39ae9a703df2eca9` | G323 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto14_gps_luci.img` | 64.00 MiB | 2026-10-07 21:22 | `089271e3c43ca777` | G324 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto15_hotspot_net.img` | 64.00 MiB | 2026-10-08 08:53 | `73a61f5cc410dc20` | G325 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto2.img` | 64.00 MiB | 2026-10-07 07:24 | `921714234a8dee7d` | G326 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto3_pmicwdtfix.img` | 64.00 MiB | 2026-10-07 07:34 | `aaffa0be7e551977` | G327 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto4_pmicwdtfix2.img` | 64.00 MiB | 2026-10-07 07:37 | `b0acb4ec643dbf1f` | G328 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto5_keypad.img` | 64.00 MiB | 2026-10-07 15:55 | `f08fffe401b8e8af` | G111 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto6_keyd.img` | 64.00 MiB | 2026-10-07 16:37 | `f08fffe401b8e8af` | G111 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto7_keyd.img` | 64.00 MiB | 2026-10-07 16:39 | `76b9b21bd90c6d45` | G329 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto8_ui.img` | 64.00 MiB | 2026-10-07 16:51 | `ea2bcdb9ac9d353e` | G330 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_luci_rndis_auto9_uname.img` | 64.00 MiB | 2026-10-07 16:55 | `2a2fa8ac222aad67` | G331 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data.img` | 64.00 MiB | 2026-10-06 22:56 | `045ce5f29b2b1b77` | G332 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data2.img` | 64.00 MiB | 2026-10-06 23:02 | `b7e7f8143b946075` | G333 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data3_pmicwdt.img` | 64.00 MiB | 2026-10-06 23:13 | `85674a5f414132a5` | G334 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data4_pmicwdt2.img` | 64.00 MiB | 2026-10-06 23:16 | `596c79d55ffa3bd2` | G335 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data5_pmicwdt3.img` | 64.00 MiB | 2026-10-06 23:18 | `47d78c0a61f439b1` | G336 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_mu300data6_wdf.img` | 64.00 MiB | 2026-10-06 23:26 | `d3425c2bff14efee` | G337 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_nft_luci.img` | 64.00 MiB | 2026-10-06 23:43 | `d77210ada0a61774` | G338 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_nft_only.img` | 64.00 MiB | 2026-10-06 23:48 | `9f2ea15f8dacab05` | G339 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_propshim.img` | 64.00 MiB | 2026-10-06 21:12 | `39841209ab50c6d3` | G340 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_propshim_dbg.img` | 64.00 MiB | 2026-10-06 21:15 | `ec31361c22442576` | G341 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_propshim_fix.img` | 64.00 MiB | 2026-10-06 21:17 | `c4b4bc22ecfffacb` | G342 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_test.img` | 64.00 MiB | 2026-10-06 20:38 | `b23738198e9c1efa` | G343 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_trustyfix.img` | 64.00 MiB | 2026-10-06 21:20 | `5dc53a06d97b9596` | G344 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_trustyfix2.img` | 64.00 MiB | 2026-10-06 21:28 | `35454b5e25344940` | G345 | `/mnt/c/platform-tools` |
| `boot_54openwrt_hotspot_trustyfix3.img` | 64.00 MiB | 2026-10-06 21:41 | `5f84525c911cea28` | G346 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag.img` | 64.00 MiB | 2026-10-06 18:00 | `9221b3c0d8c2f3ba` | G347 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag2.img` | 64.00 MiB | 2026-10-06 18:41 | `e592ca8b26108160` | G348 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag3.img` | 64.00 MiB | 2026-10-06 18:52 | `64ef728b4ec1030f` | G349 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag4.img` | 64.00 MiB | 2026-10-06 18:55 | `c9aa948bd6f36bea` | G350 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag5.img` | 64.00 MiB | 2026-10-06 18:57 | `7bb5d0b65bb2a367` | G351 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag6.img` | 64.00 MiB | 2026-10-06 18:59 | `1aaef101df84520d` | G352 | `/mnt/c/platform-tools` |
| `boot_54openwrt_minidiag7.img` | 64.00 MiB | 2026-10-06 19:03 | `494182da2cc609ef` | G353 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_diag.img` | 64.00 MiB | 2026-10-06 19:08 | `415a91aea90fc28e` | G354 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_diag2.img` | 64.00 MiB | 2026-10-06 19:10 | `c72d979d8ad35cc2` | G355 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final.img` | 64.00 MiB | 2026-10-06 19:05 | `b86aecbb55204187` | G356 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final2.img` | 64.00 MiB | 2026-10-06 19:14 | `8b112b534c7646c2` | G357 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final3.img` | 64.00 MiB | 2026-10-06 19:17 | `84aab6a0f44bf73f` | G358 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final4.img` | 64.00 MiB | 2026-10-06 19:22 | `007319853cc67469` | G359 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final5.img` | 64.00 MiB | 2026-10-06 19:26 | `6dee88d60a4e59c8` | G360 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final6.img` | 64.00 MiB | 2026-10-06 19:29 | `922aa4be54d5bb23` | G361 | `/mnt/c/platform-tools` |
| `boot_54openwrt_musb_final7_emmc.img` | 64.00 MiB | 2026-10-06 19:33 | `71388ae08d2af16f` | G362 | `/mnt/c/platform-tools` |
| `boot_54openwrt_stable_emmc.img` | 64.00 MiB | 2026-10-06 20:00 | `b13ca5c4e0bb186e` | G363 | `/mnt/c/platform-tools` |
| `boot_54openwrt_stable_emmc_rndis.img` | 64.00 MiB | 2026-10-06 20:02 | `bd92e8620d29eaab` | G364 | `/mnt/c/platform-tools` |
| `boot_54openwrt_stockandroid2_usb.img` | 64.00 MiB | 2026-10-06 17:26 | `a085d9d13570816e` | G365 | `/mnt/c/platform-tools` |
| `boot_54openwrt_stockandroid_usb.img` | 64.00 MiB | 2026-10-06 17:15 | `7346cd99d9c3a5ad` | G366 | `/mnt/c/platform-tools` |
| `boot_54openwrt_t508.img` | 64.00 MiB | 2026-10-06 12:59 | `638831faeb8859c2` | G367 | `/mnt/c/platform-tools` |
| `boot_54openwrt_t508_debug1.img` | 64.00 MiB | 2026-10-06 13:17 | `8af251afaf3f22f3` | G368 | `/mnt/c/platform-tools` |
| `boot_54openwrt_t508_debug2.img` | 64.00 MiB | 2026-10-06 13:21 | `519137efa3762dd2` | G369 | `/mnt/c/platform-tools` |
| `boot_54openwrt_t508_debug3.img` | 64.00 MiB | 2026-10-06 13:29 | `2ce5653cdd53de4e` | G370 | `/mnt/c/platform-tools` |
| `boot_54openwrt_t508_debug4_usbserial.img` | 64.00 MiB | 2026-10-06 15:21 | `d27d4a590a7cfa0a` | G371 | `/mnt/c/platform-tools` |
| `boot_54openwrt_usbchain_max.img` | 64.00 MiB | 2026-10-06 16:03 | `77c7ac5125667091` | G372 | `/mnt/c/platform-tools` |
| `boot_54openwrt_vendor_usbserial.img` | 64.00 MiB | 2026-10-06 15:30 | `996d696fd20f5470` | G373 | `/mnt/c/platform-tools` |
| `boot_54openwrt_vendor_usbserial2.img` | 64.00 MiB | 2026-10-06 15:46 | `fd13433b4bd9941e` | G374 | `/mnt/c/platform-tools` |
| `boot_54openwrt_vendor_usbserial3.img` | 64.00 MiB | 2026-10-06 15:56 | `9f040ba4c7c36d8b` | G375 | `/mnt/c/platform-tools` |
| `boot_54openwrt_wlan_test.img` | 64.00 MiB | 2026-10-06 20:32 | `064a3a8199aaf0b6` | G376 | `/mnt/c/platform-tools` |
| `boot_54openwrt_wlan_test2.img` | 64.00 MiB | 2026-10-06 20:34 | `ca46ed100461ffff` | G377 | `/mnt/c/platform-tools` |
| `boot_54openwrt_wlan_test3.img` | 64.00 MiB | 2026-10-06 20:36 | `8c9a8a5b0d5c8af4` | G378 | `/mnt/c/platform-tools` |
| `boot_612_expC.img` | 64.00 MiB | 2026-10-05 21:05 | `054c476eef1a3ba7` | G379 | `/mnt/c/platform-tools` |
| `boot_612_expC_rowlabels.img` | 64.00 MiB | 2026-10-05 21:13 | `6de60e6c9f1d9432` | G380 | `/mnt/c/platform-tools` |
| `boot_612_stage2.img` | 64.00 MiB | 2026-10-06 07:26 | `7845acaa9df7e7b7` | G381 | `/mnt/c/platform-tools` |
| `boot_612_stage3.img` | 64.00 MiB | 2026-10-06 07:31 | `c46ad03025155028` | G382 | `/mnt/c/platform-tools` |
| `boot_612_stage4.img` | 64.00 MiB | 2026-10-06 07:36 | `0b3fc2df560804d9` | G383 | `/mnt/c/platform-tools` |
| `boot_612_stage5.img` | 64.00 MiB | 2026-10-06 07:41 | `1826168d9aa3b831` | G384 | `/mnt/c/platform-tools` |
| `boot_612_stage6a_delay618.img` | 64.00 MiB | 2026-10-06 08:02 | `cec9755dae64eeeb` | G385 | `/mnt/c/platform-tools` |
| `boot_a_stock_android54.img` | 64.00 MiB | 2026-10-05 19:07 | `f252428edfe0c512` | G9 | `/mnt/c/platform-tools` |
| `boot_linux_t508n_latest.img` | 64.00 MiB | 2026-10-05 18:57 | `56fa6c271346a13f` | G225 | `/mnt/c/platform-tools` |
| `boota.img` | 64.00 MiB | 2026-10-08 21:25 | `f252428edfe0c512` | G9 | `/mnt/c/platform-tools` |
| `bootb.img` | 64.00 MiB | 2026-10-08 21:25 | `f252428edfe0c512` | G9 | `/mnt/c/platform-tools` |
| `boot_a.img` | 64.00 MiB | 2026-10-08 20:48 | `f252428edfe0c512` | G9 | `C:/platform-tools/stock` |
| `boot_b.img` | 64.00 MiB | 2026-10-08 20:48 | `f252428edfe0c512` | G9 | `C:/platform-tools/stock` |
| `metadata.img` | 64.00 MiB | 2026-10-08 20:48 | `b2a64c4e721ddee8` | G74 | `C:/platform-tools/stock` |
| `boota.img` | 64.00 MiB | 2026-10-09 08:39 | `f252428edfe0c512` | G9 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `bootb.img` | 64.00 MiB | 2026-10-09 08:39 | `f252428edfe0c512` | G9 | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `boota.img` | 64.00 MiB | 2026-01-20 22:53 | `5901a6e5e54248d2` |  | `E:/rootphone/t508n/2unlock` |
| `bootb.img` | 64.00 MiB | 2026-01-20 22:53 | `75d2347389ff2d82` |  | `E:/rootphone/t508n/2unlock` |
| `boota.img` | 64.00 MiB | 2026-10-08 20:39 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `bootb.img` | 64.00 MiB | 2026-10-08 20:39 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `boot_linux.img` | 64.00 MiB | 2026-08-15 19:01 | `054cba9f790cfb44` |  | `E:/rootphone/t508n/linux_support` |
| `boot_a.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrock/Image` |
| `boot_b.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrock/Image` |
| `cache.img` | 64.00 MiB | 2024-03-09 12:14 | `494344b0d7737407` | G97 | `E:/rootphone/t508n/savebrock/Image` |
| `metadata.img` | 64.00 MiB | 2024-03-09 12:14 | `b2a64c4e721ddee8` | G74 | `E:/rootphone/t508n/savebrock/Image` |
| `boot_a.img` | 64.00 MiB | 2026-01-21 15:30 | `0af61bf449eef5fb` | G120 | `E:/rootphone/t508n/savebrock` |
| `boot_b.img` | 64.00 MiB | 2026-01-21 15:26 | `0af61bf449eef5fb` | G120 | `E:/rootphone/t508n/savebrock` |
| `boot_a.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `boot_b.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `cache.img` | 64.00 MiB | 2024-03-09 12:14 | `494344b0d7737407` | G97 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `metadata.img` | 64.00 MiB | 2024-03-09 12:14 | `b2a64c4e721ddee8` | G74 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `boot_a.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrocka/Image` |
| `boot_b.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrocka/Image` |
| `cache.img` | 64.00 MiB | 2024-03-09 12:14 | `494344b0d7737407` | G97 | `E:/rootphone/t508n/savebrocka/Image` |
| `metadata.img` | 64.00 MiB | 2024-03-09 12:14 | `b2a64c4e721ddee8` | G74 | `E:/rootphone/t508n/savebrocka/Image` |
| `boot_a.img` | 64.00 MiB | 2026-01-21 15:30 | `0af61bf449eef5fb` | G120 | `E:/rootphone/t508n/savebrocka` |
| `boot_b.img` | 64.00 MiB | 2026-01-21 15:26 | `0af61bf449eef5fb` | G120 | `E:/rootphone/t508n/savebrocka` |
| `boot_a.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `boot_b.img` | 64.00 MiB | 2024-03-09 12:14 | `f252428edfe0c512` | G9 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `cache.img` | 64.00 MiB | 2024-03-09 12:14 | `494344b0d7737407` | G97 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `metadata.img` | 64.00 MiB | 2024-03-09 12:14 | `b2a64c4e721ddee8` | G74 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `isedata.img` | 34.00 MiB | 2024-03-09 12:14 | `dd34b32ba0522e06` | G100 | `~/t508/firmware/stock-images` |
| `isedata.img` | 34.00 MiB | 2024-03-09 12:14 | `dd34b32ba0522e06` | G100 | `E:/rootphone/t508n/savebrock/Image` |
| `isedata.img` | 34.00 MiB | 2024-03-09 12:14 | `dd34b32ba0522e06` | G100 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `isedata.img` | 34.00 MiB | 2024-03-09 12:14 | `dd34b32ba0522e06` | G100 | `E:/rootphone/t508n/savebrocka/Image` |
| `isedata.img` | 34.00 MiB | 2024-03-09 12:14 | `dd34b32ba0522e06` | G100 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `ch_sys_a.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `~/t508/firmware/stock-images` |
| `ch_sys_b.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `~/t508/firmware/stock-images` |
| `common_rs2_a.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `~/t508/firmware/stock-images` |
| `common_rs2_b.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `~/t508/firmware/stock-images` |
| `ch_sys_a.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrock/Image` |
| `ch_sys_b.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrock/Image` |
| `common_rs2_a.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrock/Image` |
| `common_rs2_b.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrock/Image` |
| `ch_sys_a.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `ch_sys_b.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `common_rs2_a.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `common_rs2_b.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `ch_sys_a.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrocka/Image` |
| `ch_sys_b.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrocka/Image` |
| `common_rs2_a.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrocka/Image` |
| `common_rs2_b.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrocka/Image` |
| `ch_sys_a.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `ch_sys_b.img` | 16.00 MiB | 2024-03-09 12:14 | `b760e5b39023309c` | G30 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `common_rs2_a.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `common_rs2_b.img` | 16.00 MiB | 2024-03-09 12:14 | `080acf35a507ac98` | G31 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `hypervsior_a.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `~/t508/firmware/stock-images` |
| `hypervsior_b.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `~/t508/firmware/stock-images` |
| `hypervsior_a.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrock/Image` |
| `hypervsior_b.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrock/Image` |
| `hypervsior_a.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `hypervsior_b.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `hypervsior_a.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrocka/Image` |
| `hypervsior_b.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrocka/Image` |
| `hypervsior_a.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `hypervsior_b.img` | 10.00 MiB | 2024-03-09 12:14 | `e5b844cc57f57094` | G33 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `dtbo.img` | 8.00 MiB | 2026-08-15 16:54 | `841f17485837c28b` | G14 | `~/t508/build/boot-artifacts` |
| `common_rs1_a.img` | 8.00 MiB | 2024-03-09 12:14 | `4d3561c8bb581083` | G98 | `~/t508/firmware/stock-images` |
| `common_rs1_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `~/t508/firmware/stock-images` |
| `dtb_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `~/t508/firmware/stock-images` |
| `dtb_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `~/t508/firmware/stock-images` |
| `dtbo_a.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `~/t508/firmware/stock-images` |
| `dtbo_b.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `~/t508/firmware/stock-images` |
| `fbootlogo.img` | 8.00 MiB | 2024-03-09 12:14 | `2f5a2a4781781a95` | G32 | `~/t508/firmware/stock-images` |
| `init_boot_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `~/t508/firmware/stock-images` |
| `init_boot_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `~/t508/firmware/stock-images` |
| `dtbo_a_stock.img` | 8.00 MiB | 2026-10-06 15:52 | `841f17485837c28b` | G14 | `/mnt/c/platform-tools` |
| `dtb_a.img` | 8.00 MiB | 2026-10-08 20:48 | `2daeb1f36095b44b` | G4 | `C:/platform-tools/stock` |
| `dtb_b.img` | 8.00 MiB | 2026-10-08 20:48 | `2daeb1f36095b44b` | G4 | `C:/platform-tools/stock` |
| `dtbo_a.img` | 8.00 MiB | 2026-10-08 20:48 | `841f17485837c28b` | G14 | `C:/platform-tools/stock` |
| `dtbo_b.img` | 8.00 MiB | 2026-10-08 20:48 | `841f17485837c28b` | G14 | `C:/platform-tools/stock` |
| `init_boot_a.img` | 8.00 MiB | 2026-10-08 20:48 | `2daeb1f36095b44b` | G4 | `C:/platform-tools/stock` |
| `init_boot_b.img` | 8.00 MiB | 2026-10-08 20:48 | `2daeb1f36095b44b` | G4 | `C:/platform-tools/stock` |
| `common_rs1_a.img` | 8.00 MiB | 2024-03-09 12:14 | `4d3561c8bb581083` | G98 | `E:/rootphone/t508n/savebrock/Image` |
| `common_rs1_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/Image` |
| `dtb_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/Image` |
| `dtb_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/Image` |
| `dtbo_a.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrock/Image` |
| `dtbo_b.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrock/Image` |
| `fbootlogo.img` | 8.00 MiB | 2024-03-09 12:14 | `2f5a2a4781781a95` | G32 | `E:/rootphone/t508n/savebrock/Image` |
| `init_boot_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/Image` |
| `init_boot_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/Image` |
| `common_rs1_a.img` | 8.00 MiB | 2024-03-09 12:14 | `4d3561c8bb581083` | G98 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `common_rs1_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `dtb_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `dtb_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `dtbo_a.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `dtbo_b.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `fbootlogo.img` | 8.00 MiB | 2024-03-09 12:14 | `2f5a2a4781781a95` | G32 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `init_boot_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `init_boot_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `common_rs1_a.img` | 8.00 MiB | 2024-03-09 12:14 | `4d3561c8bb581083` | G98 | `E:/rootphone/t508n/savebrocka/Image` |
| `common_rs1_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/Image` |
| `dtb_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/Image` |
| `dtb_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/Image` |
| `dtbo_a.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrocka/Image` |
| `dtbo_b.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrocka/Image` |
| `fbootlogo.img` | 8.00 MiB | 2024-03-09 12:14 | `2f5a2a4781781a95` | G32 | `E:/rootphone/t508n/savebrocka/Image` |
| `init_boot_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/Image` |
| `init_boot_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/Image` |
| `common_rs1_a.img` | 8.00 MiB | 2024-03-09 12:14 | `4d3561c8bb581083` | G98 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `common_rs1_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `dtb_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `dtb_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `dtbo_a.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `dtbo_b.img` | 8.00 MiB | 2024-03-09 12:14 | `841f17485837c28b` | G14 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `fbootlogo.img` | 8.00 MiB | 2024-03-09 12:14 | `2f5a2a4781781a95` | G32 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `init_boot_a.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `init_boot_b.img` | 8.00 MiB | 2024-03-09 12:14 | `2daeb1f36095b44b` | G4 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `l_agdsp_a.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `~/t508/firmware/stock-images` |
| `l_agdsp_b.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `~/t508/firmware/stock-images` |
| `trustos_a.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `~/t508/firmware/stock-images` |
| `trustos_b.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `~/t508/firmware/stock-images` |
| `l_agdsp_a.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrock/Image` |
| `l_agdsp_b.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrock/Image` |
| `trustos_a.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrock/Image` |
| `trustos_b.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrock/Image` |
| `l_agdsp_a.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `l_agdsp_b.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `trustos_a.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `trustos_b.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `l_agdsp_a.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrocka/Image` |
| `l_agdsp_b.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrocka/Image` |
| `trustos_a.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrocka/Image` |
| `trustos_b.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrocka/Image` |
| `l_agdsp_a.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `l_agdsp_b.img` | 6.00 MiB | 2024-03-09 12:14 | `8aac4db1f78b2f63` | G35 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `trustos_a.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `trustos_b.img` | 6.00 MiB | 2024-03-09 12:16 | `3cece7ea3aebff5d` | G45 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `ise_a.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `~/t508/firmware/stock-images` |
| `ise_b.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `~/t508/firmware/stock-images` |
| `spl_a.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `~/t508/firmware/stock-images` |
| `spl_b.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `~/t508/firmware/stock-images` |
| `spl_patched.img` | 4.00 MiB | 2026-08-01 06:31 | `105691c982d9a862` | G878 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `spl_stock.img` | 4.00 MiB | 2026-08-01 06:31 | `0e50cb3b9b83884a` | G198 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `spl_patched.img` | 4.00 MiB | 2026-08-01 06:31 | `105691c982d9a862` | G878 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `spl_stock.img` | 4.00 MiB | 2026-08-01 06:31 | `0e50cb3b9b83884a` | G198 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `ise_a.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrock/Image` |
| `ise_b.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrock/Image` |
| `spl_a.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrock/Image` |
| `spl_b.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrock/Image` |
| `ise_a.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `ise_b.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `spl_a.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `spl_b.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `ise_a.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrocka/Image` |
| `ise_b.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrocka/Image` |
| `spl_a.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrocka/Image` |
| `spl_b.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrocka/Image` |
| `ise_a.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `ise_b.img` | 4.00 MiB | 2024-03-09 12:14 | `bb9f8df61474d25e` | G34 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `spl_a.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `spl_b.img` | 4.00 MiB | 2024-03-09 12:15 | `54a9ed5bcd46b832` | G11 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `pm_sys_a_stock.img` | 1.00 MiB | 2026-10-06 22:23 | `78ba4df7048efe2d` | G73 | `~/t508/work/android_root_info` |
| `pm_sys_a_stock_exact.img` | 1.00 MiB | 2026-10-06 22:23 | `78ba4df7048efe2d` | G73 | `~/t508/work/android_root_info` |
| `pm_sys_b_stock.img` | 1.00 MiB | 2026-10-06 22:23 | `78ba4df7048efe2d` | G73 | `~/t508/work/android_root_info` |
| `pm_sys_b_stock_exact.img` | 1.00 MiB | 2026-10-06 22:23 | `78ba4df7048efe2d` | G73 | `~/t508/work/android_root_info` |
| `pm_sys_a_stock.img` | 1.00 MiB | 2026-10-06 22:24 | `78ba4df7048efe2d` | G73 | `/mnt/c/platform-tools` |
| `pm_sys_b_stock.img` | 1.00 MiB | 2026-10-06 22:24 | `78ba4df7048efe2d` | G73 | `/mnt/c/platform-tools` |
| `boot_cur.img` | 1.00 MiB | 2026-08-15 19:48 | `8528d2f8409befb4` | G670 | `~/t508/firmware/device-dumps` |
| `dtbo_cur.img` | 1.00 MiB | 2026-08-15 19:48 | `ed6b7c4636140132` |  | `~/t508/firmware/device-dumps` |
| `avbmeta_rs_a.img` | 1.00 MiB | 2024-03-09 12:13 | `30e14955ebf13522` | G10 | `~/t508/firmware/stock-images` |
| `avbmeta_rs_b.img` | 1.00 MiB | 2024-03-09 12:13 | `30e14955ebf13522` | G10 | `~/t508/firmware/stock-images` |
| `ftmdata1.img` | 1.00 MiB | 2024-03-09 12:14 | `30e14955ebf13522` | G10 | `~/t508/firmware/stock-images` |
| `ftmdata2.img` | 1.00 MiB | 2024-03-09 12:14 | `30e14955ebf13522` | G10 | `~/t508/firmware/stock-images` |
| `pm_sys_a.img` | 1.00 MiB | 2024-03-09 12:15 | `7fced4ab43bb1af0` | G42 | `~/t508/firmware/stock-images` |

（该类共 507 项，其余见 CSV）

### 4.12 Windows 脚本（35）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `backup_all_partitions.bat` | 8.4 KiB | 2026-10-01 23:37 | `3525fbd4532cfadc` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/tools` |
| `01-一键解锁并写magic64.bat` | 3.0 KiB | 2026-10-09 14:38 | `7d0d551aa4a7546e` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fastboot.bat` | 3.0 KiB | 2024-03-09 12:13 | `fc8554038c0de0d1` | G99 | `~/t508/firmware/stock-images` |
| `fastboot.bat` | 3.0 KiB | 2024-03-09 12:13 | `fc8554038c0de0d1` | G99 | `E:/rootphone/t508n/savebrock/Image` |
| `fastboot.bat` | 3.0 KiB | 2026-07-12 19:12 | `fc8554038c0de0d1` | G99 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `fastboot.bat` | 3.0 KiB | 2024-03-09 12:13 | `fc8554038c0de0d1` | G99 | `E:/rootphone/t508n/savebrocka/Image` |
| `fastboot.bat` | 3.0 KiB | 2026-07-12 19:12 | `fc8554038c0de0d1` | G99 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `run_flash_stock.bat` | 2.7 KiB | 2026-10-06 21:51 | `20e3c089299bd22d` |  | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `flash_all bat版.bat` | 2.6 KiB | 2026-07-12 19:12 | `7c8994eb4d5daffe` | G407 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `flash_all bat版.bat` | 2.6 KiB | 2026-07-12 19:12 | `7c8994eb4d5daffe` | G407 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump` |
| `flash_all bat版.bat` | 2.6 KiB | 2026-08-18 20:56 | `8504184530e829bf` | G412 | `E:/rootphone/t508n/savebrock/Image` |
| `flash_all bat版.bat` | 2.6 KiB | 2026-08-18 20:56 | `8504184530e829bf` | G412 | `E:/rootphone/t508n/savebrocka/Image` |
| `flash-critical-android-fastboot.cmd` | 1.5 KiB | 2026-10-08 20:48 | `894cbe8ccffce80c` |  | `/mnt/c/platform-tools` |
| `flash-stock-android-from-working-spd.cmd` | 899 B | 2026-10-08 20:37 | `d4f4e0c50219c4d0` |  | `/mnt/c/platform-tools` |
| `flash-uboot-magic64.bat` | 735 B | 2026-10-09 14:35 | `6fc385b6e48191e7` |  | `C:/platform-tools/t508n-uboot-magic64` |
| `flash-stock-android.cmd` | 724 B | 2026-10-08 14:26 | `b68ce39f749abbd4` |  | `C:/platform-tools/srag-v3` |
| `flash-stock-android.cmd` | 724 B | 2026-10-08 14:26 | `841ddb403be298d8` |  | `C:/platform-tools/srag-v6` |
| `02-仅写magic64-uboot.bat` | 676 B | 2026-10-09 14:38 | `b1c28321a9ae2c58` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `03-探针验证.bat` | 671 B | 2026-10-09 14:38 | `b3111bb09aef3683` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `restore_stock.bat` | 630 B | 2026-08-15 18:41 | `701892b511387530` |  | `~/t508/tools` |
| `flash-uboot-probe.bat` | 626 B | 2026-10-09 14:35 | `46ba64e5a66ccb95` |  | `C:/platform-tools/t508n-uboot-magic64` |
| `04-备份uboot.bat` | 618 B | 2026-10-09 14:38 | `bf042b0b038872d6` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `restore-stock-uboot.bat` | 615 B | 2026-10-09 14:35 | `8b8c792d3b2f2797` |  | `C:/platform-tools/t508n-uboot-magic64` |
| `05-恢复原厂uboot.bat` | 605 B | 2026-10-09 14:38 | `f3f59e51dd763c32` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `fastboot-flash-uboot.bat` | 564 B | 2026-10-09 14:35 | `36f1224f7e3040c9` |  | `C:/platform-tools/t508n-uboot-magic64` |
| `06-切换到B槽-mainline.bat` | 556 B | 2026-10-09 14:55 | `a1364a0637036a8d` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `07-切换到A槽-openwrt.bat` | 554 B | 2026-10-09 14:55 | `2108ca4abe15123e` |  | `E:/Download/ums9620_tcl_t508n_magic64` |
| `flash-dual-openwrt-mainline.cmd` | 519 B | 2026-10-09 08:58 | `b835dd10aed2bb28` |  | `/mnt/c/platform-tools` |
| `read-uboot-log.bat` | 477 B | 2026-10-09 14:35 | `322f1a30c3d17a0c` |  | `C:/platform-tools/t508n-uboot-magic64` |
| `restore_bootab.bat` | 421 B | 2026-10-08 21:06 | `9a3187dfb492c2c7` |  | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2` |
| `restore_stock.bat` | 272 B | 2026-08-15 19:41 | `3c2a5bedc1da2fd8` | G413 | `E:/rootphone/t508n/savebrock/Image` |
| `restore_stock.bat` | 272 B | 2026-08-15 19:41 | `3c2a5bedc1da2fd8` | G413 | `E:/rootphone/t508n/savebrocka/Image` |
| `刷入uboot，7G解bl脚本（需要解bl后执行）.bat` | 198 B | 2026-01-21 15:45 | `491e2c4de1fe54a2` |  | `/mnt/e/rootphone/t508n` |
| `点我开始root.bat` | 180 B | 2024-03-09 11:43 | `fee004f10b84695d` |  | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300` |
| `recover-a.cmd` | 152 B | 2026-10-08 13:32 | `c4b0196545bcb11c` |  | `/mnt/c/platform-tools` |

### 4.13 内核镜像产物（6）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `vendor54_Image` | 61.27 MiB | 2026-08-28 21:12 | `aa7f160ea346b9cd` |  | `~/t508/build/images` |
| `mainline_Image` | 42.69 MiB | 2026-10-05 18:56 | `b53306c7a2df61d3` |  | `~/t508/build/images` |
| `stock54-custom_Image` | 30.84 MiB | 2026-08-18 21:43 | `3035654d11d7d7e1` |  | `~/t508/build/images` |
| `Image.lk` | 18.39 MiB | 2026-10-08 12:46 | `99805a744cfe20a8` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `Image` | 16.89 MiB | 2026-10-08 12:46 | `5451a40043937f50` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `System.map` | 7.13 MiB | 2026-08-15 23:14 | `bfc69d6da2b6428d` |  | `~/t508/firmware/stock` |

### 4.14 文档（454）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `REPORT-UNISOC-ONLY-CONFIGS.md` | 75.3 KiB | 2026-08-18 21:27 | `a8c8659d17310418` | G478 | `~/t508/docs/reports` |
| `REPORT-UNISOC-ONLY-CONFIGS.md` | 75.3 KiB | 2026-08-23 23:42 | `a8c8659d17310418` | G478 | `~/t508/src/t508n-repo/docs` |
| `SUMMARY-ALL-ANALYSIS.md` | 65.1 KiB | 2026-08-18 07:48 | `17927056a7d349f6` | G487 | `~/t508/docs/reports` |
| `SUMMARY-ALL-ANALYSIS.md` | 65.1 KiB | 2026-08-23 23:42 | `17927056a7d349f6` | G487 | `~/t508/src/t508n-repo/docs` |
| `FINAL-BRINGUP-INDEX.md` | 42.0 KiB | 2026-10-09 15:14 | `cdb5e8d5f61e1356` |  | `~/t508/docs` |
| `FINAL-BRINGUP-INDEX.md` | 41.4 KiB | 2026-08-23 23:42 | `0c6de3e3b2848be2` |  | `~/t508/src/t508n-repo/docs` |
| `DISASM-A-cpu-boot.md` | 39.3 KiB | 2026-08-23 23:42 | `27d4172c010b8fa5` | G485 | `~/t508/src/t508n-repo/docs` |
| `DISASM-A-cpu-boot.md` | 39.3 KiB | 2026-08-18 20:35 | `27d4172c010b8fa5` | G485 | `~/t508/work/disasm` |
| `DISASM-L-rcu.md` | 33.9 KiB | 2026-08-23 23:42 | `c2b18ff2b0c3f87e` | G507 | `~/t508/src/t508n-repo/docs` |
| `DISASM-L-rcu.md` | 33.9 KiB | 2026-08-18 23:00 | `c2b18ff2b0c3f87e` | G507 | `~/t508/work/disasm` |
| `ARCHIVE-20260823.md` | 33.8 KiB | 2026-08-25 20:44 | `bea75eb6ea0837e9` | G122 | `~/t508/docs/reports` |
| `ARCHIVE-20260823.md` | 33.8 KiB | 2026-08-25 20:44 | `bea75eb6ea0837e9` | G122 | `~/t508/src/t508n-repo/docs` |
| `ARCHIVE-20260823.md` | 33.8 KiB | 2026-08-28 18:05 | `bea75eb6ea0837e9` | G122 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `ARCHIVE-20260823.md` | 33.8 KiB | 2026-08-28 18:05 | `bea75eb6ea0837e9` | G122 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `DISASM-F-fs-文件系统VFS.md` | 33.0 KiB | 2026-08-23 23:42 | `bdd93ef2e965dc45` | G471 | `~/t508/src/t508n-repo/docs` |
| `DISASM-F-fs-文件系统VFS.md` | 33.0 KiB | 2026-08-18 20:34 | `bdd93ef2e965dc45` | G471 | `~/t508/work/disasm` |
| `逆向分析报告.md` | 32.2 KiB | 2026-10-01 23:37 | `f841e1780edee9a7` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `REPORT-CURRENT-RACE-ANALYSIS.md` | 30.9 KiB | 2026-08-18 08:03 | `523c4816b1400b10` | G520 | `~/t508/docs/reports` |
| `REPORT-CURRENT-RACE-ANALYSIS.md` | 30.9 KiB | 2026-08-23 23:42 | `523c4816b1400b10` | G520 | `~/t508/src/t508n-repo/docs` |
| `Timer.md` | 30.1 KiB | 2026-08-18 20:28 | `5a4b493d0913bb2e` |  | `~/t508/work/disasm/DISASM-C-irq-中断GIC` |
| `REPORT-stock-driver-init.md` | 30.1 KiB | 2026-08-17 22:38 | `2315ad9820daf3c5` | G523 | `~/t508/docs/reports` |
| `REPORT-stock-driver-init.md` | 30.1 KiB | 2026-08-23 23:42 | `2315ad9820daf3c5` | G523 | `~/t508/src/t508n-repo/docs` |
| `DISASM-H-drv-pmic-电源.md` | 29.8 KiB | 2026-08-23 23:42 | `f1bd55e43de4acaf` | G468 | `~/t508/src/t508n-repo/docs` |
| `DISASM-H-drv-pmic-电源.md` | 29.8 KiB | 2026-08-18 23:01 | `f1bd55e43de4acaf` | G468 | `~/t508/work/disasm` |
| `DISASM-E-devm-驱动模型sysfs.md` | 29.8 KiB | 2026-08-23 23:42 | `814ee1f3446b86a4` | G514 | `~/t508/src/t508n-repo/docs` |
| `DISASM-E-devm-驱动模型sysfs.md` | 29.8 KiB | 2026-08-18 20:28 | `814ee1f3446b86a4` | G514 | `~/t508/work/disasm` |
| `DISASM-D-mm-内存管理.md` | 29.8 KiB | 2026-08-23 23:42 | `195bf14c88ae9e98` | G518 | `~/t508/src/t508n-repo/docs` |
| `DISASM-D-mm-内存管理.md` | 29.8 KiB | 2026-08-18 20:35 | `195bf14c88ae9e98` | G518 | `~/t508/work/disasm` |
| `REPORT-612-vs-54-DIFF-ANALYSIS.md` | 29.7 KiB | 2026-10-06 07:56 | `3d697dde618979d1` |  | `~/t508/docs/reports` |
| `DISASM-G-drv-clk-时钟.md` | 29.1 KiB | 2026-08-23 23:42 | `feb98b031366d011` | G489 | `~/t508/src/t508n-repo/docs` |
| `DISASM-G-drv-clk-时钟.md` | 29.1 KiB | 2026-08-18 23:09 | `feb98b031366d011` | G489 | `~/t508/work/disasm` |
| `DISASM-K-mm-slub.md` | 28.8 KiB | 2026-08-23 23:42 | `5c98b675a76acfa1` | G498 | `~/t508/src/t508n-repo/docs` |
| `DISASM-K-mm-slub.md` | 28.8 KiB | 2026-08-18 23:04 | `5c98b675a76acfa1` | G498 | `~/t508/work/disasm` |
| `REPORT-stock-wakeup-nohz.md` | 28.7 KiB | 2026-08-17 23:23 | `f3db1323c75715d5` | G482 | `~/t508/docs/reports` |
| `REPORT-stock-wakeup-nohz.md` | 28.7 KiB | 2026-08-23 23:42 | `f3db1323c75715d5` | G482 | `~/t508/src/t508n-repo/docs` |
| `REPORT-LSE-atomic-completion-analysis.md` | 28.4 KiB | 2026-08-17 12:28 | `4784287155859820` | G460 | `~/t508/docs/reports` |
| `REPORT-LSE-atomic-completion-analysis.md` | 28.4 KiB | 2026-08-23 23:42 | `4784287155859820` | G460 | `~/t508/src/t508n-repo/docs` |
| `DISASM-M-workqueue.md` | 28.4 KiB | 2026-08-23 23:42 | `ca723ddae85c4bf0` | G492 | `~/t508/src/t508n-repo/docs` |
| `DISASM-M-workqueue.md` | 28.4 KiB | 2026-08-18 23:05 | `ca723ddae85c4bf0` | G492 | `~/t508/work/disasm` |
| `REPORT-stock-memory-dev-init.md` | 28.3 KiB | 2026-08-18 19:15 | `cd9d6dd07ffec8a2` | G534 | `~/t508/docs/reports` |
| `REPORT-stock-memory-dev-init.md` | 28.3 KiB | 2026-08-23 23:42 | `cd9d6dd07ffec8a2` | G534 | `~/t508/src/t508n-repo/docs` |
| `ARCHIVE-20260823.md` | 27.5 KiB | 2026-08-25 19:18 | `247b19063eb6feb2` |  | `~/t508/work/work-bak-local` |
| `DISASM-B-sched-调度器.md` | 27.3 KiB | 2026-08-23 23:42 | `f3a2c94c02c95ca0` | G510 | `~/t508/src/t508n-repo/docs` |
| `DISASM-B-sched-调度器.md` | 27.3 KiB | 2026-08-18 20:21 | `f3a2c94c02c95ca0` | G510 | `~/t508/work/disasm` |
| `PATCH-diag-sgi-kfence.md` | 27.2 KiB | 2026-08-18 21:00 | `8526d898299d8b1d` | G532 | `~/t508/docs/reports` |
| `PATCH-diag-sgi-kfence.md` | 27.2 KiB | 2026-08-23 23:42 | `8526d898299d8b1d` | G532 | `~/t508/src/t508n-repo/docs` |
| `REPORT-stock-alternatives-waiter-analysis.md` | 26.8 KiB | 2026-08-17 13:58 | `072c4dfd789f7b06` | G481 | `~/t508/docs/reports` |
| `REPORT-stock-alternatives-waiter-analysis.md` | 26.8 KiB | 2026-08-23 23:42 | `072c4dfd789f7b06` | G481 | `~/t508/src/t508n-repo/docs` |
| `DISASM-I-drv-ufs.md` | 25.7 KiB | 2026-08-23 23:42 | `d28f856c06a94f03` | G527 | `~/t508/src/t508n-repo/docs` |
| `DISASM-I-drv-ufs.md` | 25.7 KiB | 2026-08-18 23:14 | `d28f856c06a94f03` | G527 | `~/t508/work/disasm` |
| `DISASM-N-cpuidle-cpufreq.md` | 25.3 KiB | 2026-08-23 23:42 | `44175b5a13b0f52d` | G496 | `~/t508/src/t508n-repo/docs` |
| `DISASM-N-cpuidle-cpufreq.md` | 25.3 KiB | 2026-08-18 23:07 | `44175b5a13b0f52d` | G496 | `~/t508/work/disasm` |
| `PATCH-nohz-kick.md` | 25.0 KiB | 2026-08-18 21:01 | `420edbb8f06b25ff` | G506 | `~/t508/docs/reports` |
| `PATCH-nohz-kick.md` | 25.0 KiB | 2026-08-23 23:42 | `420edbb8f06b25ff` | G506 | `~/t508/src/t508n-repo/docs` |
| `REPORT-UNISOC-MODS-ON-612.md` | 24.9 KiB | 2026-08-18 22:12 | `e54a6f0b2200bf45` | G515 | `~/t508/docs/reports` |
| `REPORT-UNISOC-MODS-ON-612.md` | 24.9 KiB | 2026-08-23 23:42 | `e54a6f0b2200bf45` | G515 | `~/t508/src/t508n-repo/docs` |
| `FINAL-WRAPUP-20260821.md` | 24.9 KiB | 2026-08-22 16:06 | `ce9f0a23b4b59819` | G454 | `~/t508/docs/reports` |
| `FINAL-WRAPUP-20260821.md` | 24.9 KiB | 2026-08-23 23:42 | `ce9f0a23b4b59819` | G454 | `~/t508/src/t508n-repo/docs` |
| `REPORT-ROOTFS-COMPARE.md` | 23.8 KiB | 2026-08-22 22:13 | `618fd85e916a0406` | G476 | `~/t508/docs/reports` |
| `REPORT-ROOTFS-COMPARE.md` | 23.8 KiB | 2026-08-23 23:42 | `618fd85e916a0406` | G476 | `~/t508/src/t508n-repo/docs` |
| `REPORT-USB-BOOT-FLOW.md` | 23.3 KiB | 2026-08-25 15:21 | `5766d45d69d818cb` |  | `~/t508/docs/reports` |
| `REPORT-STOCK-USB-STACK-MECHANISM.md` | 23.1 KiB | 2026-08-25 15:21 | `54cbbb1e425f7006` |  | `~/t508/docs/reports` |
| `DISASM-J-drv-timer-wdt.md` | 23.1 KiB | 2026-08-23 23:42 | `262262208282c2da` | G477 | `~/t508/src/t508n-repo/docs` |
| `DISASM-J-drv-timer-wdt.md` | 23.1 KiB | 2026-08-18 23:01 | `262262208282c2da` | G477 | `~/t508/work/disasm` |
| `REPORT-stock-crosscluster-visibility.md` | 22.9 KiB | 2026-08-17 20:12 | `c9d85c070787df3a` | G521 | `~/t508/docs/reports` |
| `REPORT-stock-crosscluster-visibility.md` | 22.9 KiB | 2026-08-23 23:42 | `c9d85c070787df3a` | G521 | `~/t508/src/t508n-repo/docs` |
| `REPORT-stock-wakeup-chain-analysis.md` | 22.7 KiB | 2026-08-17 13:16 | `4e88401c2cdf8f77` | G524 | `~/t508/docs/reports` |
| `REPORT-stock-wakeup-chain-analysis.md` | 22.7 KiB | 2026-08-23 23:42 | `4e88401c2cdf8f77` | G524 | `~/t508/src/t508n-repo/docs` |
| `PATCH-mem-sysfs-order.md` | 22.1 KiB | 2026-08-18 21:03 | `bb4d30dcd8438bfc` | G529 | `~/t508/docs/reports` |
| `PATCH-mem-sysfs-order.md` | 22.1 KiB | 2026-08-23 23:42 | `bb4d30dcd8438bfc` | G529 | `~/t508/src/t508n-repo/docs` |
| `REPORT-A76-requirements.md` | 20.4 KiB | 2026-08-18 08:58 | `d81ef01f19443f8c` | G504 | `~/t508/docs/reports` |
| `REPORT-A76-requirements.md` | 20.4 KiB | 2026-08-23 23:42 | `d81ef01f19443f8c` | G504 | `~/t508/src/t508n-repo/docs` |
| `INDEX.md` | 20.2 KiB | 2026-10-09 15:14 | `542977da9de99aa7` |  | `~/t508/docs` |
| `REPORT-stock-zone-memory.md` | 19.9 KiB | 2026-08-17 22:09 | `1045789f2abc41c1` | G473 | `~/t508/docs/reports` |
| `REPORT-stock-zone-memory.md` | 19.9 KiB | 2026-08-23 23:42 | `1045789f2abc41c1` | G473 | `~/t508/src/t508n-repo/docs` |
| `HANDOFF.md` | 19.8 KiB | 2026-10-09 15:14 | `2e3c9295713d88f5` |  | `~/t508/docs` |
| `REPORT-USB-CONSOLIDATED.md` | 19.0 KiB | 2026-08-28 20:26 | `cc097d61650cb62a` |  | `~/t508/docs/reports` |
| `REPORT-T508N-启动流程分析.md` | 18.9 KiB | 2026-08-15 17:13 | `a547a5a931e50a0e` | G499 | `~/t508/docs/reports` |
| `REPORT-T508N-启动流程分析.md` | 18.9 KiB | 2026-08-23 23:42 | `a547a5a931e50a0e` | G499 | `~/t508/src/t508n-repo/docs` |
| `magic64-生成与验证报告.md` | 18.9 KiB | 2026-10-01 23:37 | `d5b9a6a9fd8d4645` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `REPORT-stock-of-core-init.md` | 18.9 KiB | 2026-08-18 18:48 | `ad56f8bc7e047c52` | G512 | `~/t508/docs/reports` |
| `REPORT-stock-of-core-init.md` | 18.9 KiB | 2026-08-23 23:42 | `ad56f8bc7e047c52` | G512 | `~/t508/src/t508n-repo/docs` |
| `REPORT-AGENTS-GUIDE-analysis.md` | 18.2 KiB | 2026-08-17 12:15 | `37d7415781b4af52` | G467 | `~/t508/docs/reports` |
| `REPORT-AGENTS-GUIDE-analysis.md` | 18.2 KiB | 2026-08-23 23:42 | `37d7415781b4af52` | G467 | `~/t508/src/t508n-repo/docs` |
| `FLASH-20260821-P1.md` | 16.9 KiB | 2026-08-23 11:38 | `acf20e0bc618ac78` | G536 | `~/t508/docs/reports` |
| `FLASH-20260821-P1.md` | 16.9 KiB | 2026-08-23 23:42 | `acf20e0bc618ac78` | G536 | `~/t508/src/t508n-repo/docs` |
| `SESSION-SPLIT-AND-WORKFLOW.md` | 16.3 KiB | 2026-08-21 20:06 | `2d1679763226218d` | G530 | `~/t508/docs/reports` |
| `SESSION-SPLIT-AND-WORKFLOW.md` | 16.3 KiB | 2026-08-23 23:42 | `2d1679763226218d` | G530 | `~/t508/src/t508n-repo/docs` |
| `REPORT-mainline-completion-wakeup-analysis.md` | 16.1 KiB | 2026-08-17 12:30 | `ed6790ee5ddba78a` | G480 | `~/t508/docs/reports` |
| `REPORT-mainline-completion-wakeup-analysis.md` | 16.1 KiB | 2026-08-23 23:42 | `ed6790ee5ddba78a` | G480 | `~/t508/src/t508n-repo/docs` |
| `REPORT-stock-vs-mainline-completion.md` | 15.7 KiB | 2026-08-17 12:29 | `9db95b829c0c477b` | G464 | `~/t508/docs/reports` |
| `REPORT-stock-vs-mainline-completion.md` | 15.7 KiB | 2026-08-23 23:42 | `9db95b829c0c477b` | G464 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 15.3 KiB | 2026-09-13 05:24 | `73d5674d3fd598a8` |  | `~/t508/work/srag-refs/zte-f50-toolkit` |
| `TASK-A-B-USB-DTS-C-离线成果.md` | 15.1 KiB | 2026-08-16 21:40 | `924bf348361ca011` | G519 | `~/t508/docs/reports` |
| `TASK-A-B-USB-DTS-C-离线成果.md` | 15.1 KiB | 2026-08-23 23:42 | `924bf348361ca011` | G519 | `~/t508/src/t508n-repo/docs` |
| `REPORT-USB-CONSOLIDATED.md` | 14.6 KiB | 2026-08-28 18:05 | `b1f61798328639cb` | G1695 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `REPORT-USB-CONSOLIDATED.md` | 14.6 KiB | 2026-08-28 18:05 | `b1f61798328639cb` | G1695 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `README.md` | 14.3 KiB | 2026-10-01 23:37 | `1434cbafe5848736` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root` |
| `DISASSEMBLY.md` | 14.2 KiB | 2026-08-01 06:31 | `69eb4643aa1fad54` | G855 | `~/t508/work/srag-refs/UnisocBypass/analysis/bootrom` |
| `DISASSEMBLY.md` | 14.2 KiB | 2026-08-01 06:31 | `69eb4643aa1fad54` | G855 | `~/t508/work/srag/UnisocBypass-main/analysis/bootrom` |
| `REPORT-54-CLEAN-CHECKPOINTS-DEVMEM.md` | 14.0 KiB | 2026-10-05 20:34 | `6b16a792ea027d24` |  | `~/t508/docs/reports` |
| `REPORT-stock-page-alloc-init.md` | 13.8 KiB | 2026-08-17 21:41 | `e1eb1e9de4ab51ca` | G451 | `~/t508/docs/reports` |
| `REPORT-stock-page-alloc-init.md` | 13.8 KiB | 2026-08-23 23:42 | `e1eb1e9de4ab51ca` | G451 | `~/t508/src/t508n-repo/docs` |
| `REPORT.md` | 13.8 KiB | 2026-08-24 08:09 | `7f54e294616155c4` |  | `~/t508/work/uboot/analysis` |
| `REPORT-alt-table-integrity.md` | 13.6 KiB | 2026-08-17 19:47 | `34102afaac587ef3` | G474 | `~/t508/docs/reports` |
| `REPORT-alt-table-integrity.md` | 13.6 KiB | 2026-08-23 23:42 | `34102afaac587ef3` | G474 | `~/t508/src/t508n-repo/docs` |
| `COMPARISON.md` | 13.4 KiB | 2026-08-01 06:31 | `f7e2224dd861b18d` | G910 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test` |
| `COMPARISON.md` | 13.4 KiB | 2026-08-01 06:31 | `f7e2224dd861b18d` | G910 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test` |
| `REPORT-T508N-stock-analysis.md` | 12.9 KiB | 2026-08-16 20:55 | `ecbea6dc7800fc25` | G458 | `~/t508/docs/reports` |
| `REPORT-T508N-stock-analysis.md` | 12.9 KiB | 2026-08-23 23:42 | `ecbea6dc7800fc25` | G458 | `~/t508/src/t508n-repo/docs` |
| `2026-08-29.md` | 12.4 KiB | 2026-08-29 21:47 | `b36ebc85f86441f0` |  | `E:/rootphone/t508n/savebrocka/.workbuddy/memory` |
| `TEE-Firewall-Register-Release.md` | 12.0 KiB | 2026-08-28 20:22 | `010c0f983b35755d` |  | `~/t508/work/decomp-analysis` |
| `SMC-DISPATCH-USB-REPORT.md` | 12.0 KiB | 2026-08-24 23:57 | `a22e1edfa8ee6384` |  | `~/t508/src/t508n-repo/docs` |
| `方法论-unisoc-uboot-magic64-offline.md` | 11.4 KiB | 2026-10-01 23:37 | `09bf516217c64205` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `UNISOC-MODS-AND-CONFIG.md` | 11.3 KiB | 2026-08-18 20:51 | `d84b47379973da29` | G495 | `~/t508/docs/reports` |
| `UNISOC-MODS-AND-CONFIG.md` | 11.3 KiB | 2026-08-23 23:42 | `d84b47379973da29` | G495 | `~/t508/src/t508n-repo/docs` |
| `SLOT_B_CHAINLOAD.md` | 11.2 KiB | 2026-08-01 06:31 | `9e81577d5f432995` | G1683 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `SLOT_B_CHAINLOAD.md` | 11.2 KiB | 2026-08-01 06:31 | `9e81577d5f432995` | G1683 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `UNISOC-DETAILED.md` | 11.1 KiB | 2026-08-18 21:10 | `f997470a1000d5c9` | G508 | `~/t508/docs/reports` |
| `UNISOC-DETAILED.md` | 11.1 KiB | 2026-08-23 23:42 | `f997470a1000d5c9` | G508 | `~/t508/src/t508n-repo/docs` |
| `REPORT-SML-psci-cpu-on.md` | 10.9 KiB | 2026-08-21 21:20 | `8b959b50c701d002` | G453 | `~/t508/docs/reports` |
| `REPORT-SML-psci-cpu-on.md` | 10.9 KiB | 2026-08-23 23:42 | `8b959b50c701d002` | G453 | `~/t508/src/t508n-repo/docs` |
| `REPORT-vddusb33-variant-matrix-offline.md` | 10.8 KiB | 2026-08-25 21:58 | `82f283721595088b` |  | `~/t508/docs/reports` |
| `REPORT-SGI-IPI-COUNTS-20261005.md` | 10.8 KiB | 2026-10-05 18:39 | `76676f0d86633995` |  | `~/t508/docs/reports` |
| `prompt-vddusb33-fastboot-debug.md` | 10.8 KiB | 2026-08-25 21:58 | `4877e566fd3ca752` |  | `~/t508/docs/prompts` |
| `REPORT-stock-sync-mechanism.md` | 10.5 KiB | 2026-08-16 22:32 | `c6b1444b27f1c87d` | G486 | `~/t508/docs/reports` |
| `REPORT-stock-sync-mechanism.md` | 10.5 KiB | 2026-08-23 23:42 | `c6b1444b27f1c87d` | G486 | `~/t508/src/t508n-repo/docs` |
| `REPORT-stock-vs-mainline-locks.md` | 10.5 KiB | 2026-08-16 21:59 | `babdcdc2b869d1d6` | G446 | `~/t508/docs/reports` |
| `REPORT-stock-vs-mainline-locks.md` | 10.5 KiB | 2026-08-23 23:42 | `babdcdc2b869d1d6` | G446 | `~/t508/src/t508n-repo/docs` |
| `MAGIC64_EXPLAINED.md` | 10.4 KiB | 2026-08-01 06:31 | `eff65d915f993424` | G1682 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `MAGIC64_EXPLAINED.md` | 10.4 KiB | 2026-08-01 06:31 | `eff65d915f993424` | G1682 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `REPORT-54-custom-build.md` | 10.4 KiB | 2026-08-18 21:24 | `8cbbc0fd832d984f` | G483 | `~/t508/docs/reports` |
| `REPORT-54-custom-build.md` | 10.4 KiB | 2026-08-23 23:42 | `8cbbc0fd832d984f` | G483 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 10.2 KiB | 2026-08-01 06:31 | `cac406aa0fb2ff5e` | G837 | `~/t508/work/srag-refs/UnisocBypass` |
| `README.md` | 10.2 KiB | 2026-08-01 06:31 | `cac406aa0fb2ff5e` | G837 | `~/t508/work/srag/UnisocBypass-main` |
| `triage.md` | 9.7 KiB | 2026-08-28 19:25 | `3e842db948a2127f` |  | `~/t508/work/decomp-analysis` |
| `README.md` | 9.6 KiB | 2026-08-01 06:31 | `f22aaebbc585b1bd` | G890 | `~/t508/work/srag-refs/UnisocBypass/oc` |
| `README.md` | 9.6 KiB | 2026-08-01 06:31 | `f22aaebbc585b1bd` | G890 | `~/t508/work/srag/UnisocBypass-main/oc` |
| `OC_FEASIBILITY.md` | 9.6 KiB | 2026-08-01 06:31 | `ccd40631076ac163` | G901 | `~/t508/work/srag-refs/UnisocBypass/oc` |
| `OC_FEASIBILITY.md` | 9.6 KiB | 2026-08-01 06:31 | `ccd40631076ac163` | G901 | `~/t508/work/srag/UnisocBypass-main/oc` |
| `README.md` | 9.5 KiB | 2026-09-29 19:30 | `56dd64a7673f5ad8` |  | `/home/test/t508n` |
| `FLASH-20261005-SGI-IPI-BOOT-A.md` | 9.5 KiB | 2026-10-05 19:05 | `6256c50bf159cf2b` |  | `~/t508/docs/reports` |
| `UBOOT-LK-ANALYSIS.md` | 9.5 KiB | 2026-08-24 22:11 | `cfae74425c3e20e6` |  | `~/t508/src/t508n-repo/docs` |
| `CHANGELOG.md` | 9.5 KiB | 2026-08-01 06:31 | `28f9d0e5a52e7d8d` | G924 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test` |
| `CHANGELOG.md` | 9.5 KiB | 2026-08-01 06:31 | `28f9d0e5a52e7d8d` | G924 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test` |
| `USB-REMAINING-ANALYSIS.md` | 9.4 KiB | 2026-08-28 20:36 | `1acd073e46905875` |  | `~/t508/work/decomp-analysis` |
| `UBOOT-TEE-USB-LOAD-POWER.md` | 9.1 KiB | 2026-08-24 22:19 | `84f2610a4fd40117` |  | `~/t508/src/t508n-repo/docs` |
| `UMS9620_PORT.md` | 9.0 KiB | 2026-08-01 06:31 | `ce00633f3a92a09a` | G1676 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `UMS9620_PORT.md` | 9.0 KiB | 2026-08-01 06:31 | `ce00633f3a92a09a` | G1676 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `widevine_crash_analysis.md` | 8.9 KiB | 2026-08-01 06:31 | `c4e390b4e48d9d93` | G1674 | `~/t508/work/srag-refs/UnisocBypass/oc` |
| `widevine_crash_analysis.md` | 8.9 KiB | 2026-08-01 06:31 | `c4e390b4e48d9d93` | G1674 | `~/t508/work/srag/UnisocBypass-main/oc` |
| `REPORT-V50ALIGN-BOOT-20261005.md` | 8.9 KiB | 2026-10-05 19:33 | `851f2c0944f8c32c` |  | `~/t508/docs/reports` |
| `README.md` | 8.6 KiB | 2026-08-01 06:31 | `c1fdef46607b38e4` | G886 | `~/t508/work/srag-refs/UnisocBypass/oc/voltage-ceiling-hunt` |
| `README.md` | 8.6 KiB | 2026-08-01 06:31 | `c1fdef46607b38e4` | G886 | `~/t508/work/srag/UnisocBypass-main/oc/voltage-ceiling-hunt` |
| `HANDOFF-3-SESSIONS.md` | 8.4 KiB | 2026-08-17 09:19 | `39b3d25ee0c112b2` | G444 | `~/t508/docs/reports` |
| `HANDOFF-3-SESSIONS.md` | 8.4 KiB | 2026-08-23 23:42 | `39b3d25ee0c112b2` | G444 | `~/t508/src/t508n-repo/docs` |
| `TASK-vddusb33-regulator-fix.md` | 8.0 KiB | 2026-08-25 20:13 | `f14d2f907272a355` | G285 | `~/t508/docs/reports` |
| `TASK-vddusb33-regulator-fix.md` | 8.0 KiB | 2026-08-28 18:05 | `f14d2f907272a355` | G285 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `TASK-vddusb33-regulator-fix.md` | 8.0 KiB | 2026-08-28 18:05 | `f14d2f907272a355` | G285 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `rank-a.md` | 7.8 KiB | 2026-08-28 19:25 | `7755c754595bfd75` |  | `~/t508/work/decomp-analysis` |
| `AVB_RESIGN.md` | 7.8 KiB | 2026-08-01 06:31 | `2cb5c816ed3a1b9b` | G1681 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `AVB_RESIGN.md` | 7.8 KiB | 2026-08-01 06:31 | `2cb5c816ed3a1b9b` | G1681 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `ANALYSIS.md` | 7.7 KiB | 2026-08-01 06:31 | `f490d979d8ac45e4` | G1678 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `ANALYSIS.md` | 7.7 KiB | 2026-08-01 06:31 | `f490d979d8ac45e4` | G1678 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `REPORT-54-probe-port.md` | 7.6 KiB | 2026-08-18 21:35 | `84c82f78bc1c1bd3` | G490 | `~/t508/docs/reports` |
| `REPORT-54-probe-port.md` | 7.6 KiB | 2026-08-23 23:42 | `84c82f78bc1c1bd3` | G490 | `~/t508/src/t508n-repo/docs` |
| `无串口验证指南.md` | 7.2 KiB | 2026-08-15 19:23 | `1bb008b4f39cd046` | G531 | `~/t508/docs` |
| `无串口验证指南.md` | 7.2 KiB | 2026-08-23 23:42 | `1bb008b4f39cd046` | G531 | `~/t508/src/t508n-repo/docs` |
| `TEE-Trust-Analysis.md` | 7.2 KiB | 2026-08-28 20:22 | `945018d952478557` |  | `~/t508/work/decomp-analysis` |
| `SESSION-CONTEXT-COMPACT.md` | 7.0 KiB | 2026-08-17 09:19 | `2ccfc4c4c85180ee` | G472 | `~/t508/docs/reports` |
| `SESSION-CONTEXT-COMPACT.md` | 7.0 KiB | 2026-08-23 23:42 | `2ccfc4c4c85180ee` | G472 | `~/t508/src/t508n-repo/docs` |
| `WORK-适配材料清单.md` | 6.8 KiB | 2026-08-16 21:05 | `84219805994c204a` | G500 | `~/t508/docs` |
| `WORK-适配材料清单.md` | 6.8 KiB | 2026-08-23 23:42 | `84219805994c204a` | G500 | `~/t508/src/t508n-repo/docs` |
| `GUIDE.md` | 6.7 KiB | 2026-08-28 20:36 | `6424503b211fb26f` |  | `~/t508/work/decomp-analysis` |
| `README.md` | 6.7 KiB | 2026-09-29 19:30 | `1ce0a7efe1ee2b2f` |  | `~/t508` |
| `FOUND_IT.md` | 6.5 KiB | 2026-08-01 06:31 | `e117b5129ba0e3e6` | G884 | `~/t508/work/srag-refs/UnisocBypass/oc/voltage-ceiling-hunt` |
| `FOUND_IT.md` | 6.5 KiB | 2026-08-01 06:31 | `e117b5129ba0e3e6` | G884 | `~/t508/work/srag/UnisocBypass-main/oc/voltage-ceiling-hunt` |
| `操作步骤.md` | 6.5 KiB | 2026-10-01 23:37 | `67d02a68c1772531` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `REPORT-UMS9620-mainline适配.md` | 6.3 KiB | 2026-08-16 12:02 | `ee7e727beef460de` | G470 | `~/t508/docs/reports` |
| `TASK-A-deepdive-SSPHY-clock.md` | 6.3 KiB | 2026-08-16 21:39 | `07be82201792eada` | G455 | `~/t508/docs/reports` |
| `REPORT-UMS9620-mainline适配.md` | 6.3 KiB | 2026-08-23 23:42 | `ee7e727beef460de` | G470 | `~/t508/src/t508n-repo/docs` |
| `TASK-A-deepdive-SSPHY-clock.md` | 6.3 KiB | 2026-08-23 23:42 | `07be82201792eada` | G455 | `~/t508/src/t508n-repo/docs` |
| `prompt-vddusb33-runtime-evidence.md` | 6.2 KiB | 2026-08-26 14:37 | `2789eb6a8118f3fc` | G284 | `~/t508/docs/prompts` |
| `prompt-vddusb33-runtime-evidence.md` | 6.2 KiB | 2026-08-28 18:05 | `2789eb6a8118f3fc` | G284 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `prompt-vddusb33-runtime-evidence.md` | 6.2 KiB | 2026-08-28 18:05 | `2789eb6a8118f3fc` | G284 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `USB-IMAGE-SIDE-REPLICA-CHECKLIST.md` | 6.2 KiB | 2026-08-24 22:54 | `cfde221a433864b4` |  | `~/t508/src/t508n-repo/docs` |
| `REPORT-SELF-BUILT-USB-CHAIN.md` | 6.1 KiB | 2026-08-25 17:31 | `4dfd324aed7e8101` | G221 | `~/t508/docs/reports` |
| `REPORT-SELF-BUILT-USB-CHAIN.md` | 6.1 KiB | 2026-08-28 18:05 | `4dfd324aed7e8101` | G221 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `REPORT-SELF-BUILT-USB-CHAIN.md` | 6.1 KiB | 2026-08-28 18:05 | `4dfd324aed7e8101` | G221 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `REPORT-SELF-BUILT-USB-CHAIN.md` | 6.1 KiB | 2026-08-25 19:18 | `4dfd324aed7e8101` | G221 | `~/t508/work/work-bak-local` |
| `unisoc_ums9620.md` | 6.1 KiB | 2026-08-24 07:55 | `e000678adef501f1` |  | `/mnt/e/Download` |
| `KERNEL-TEE-CALL-ROUTINES.md` | 6.1 KiB | 2026-08-24 22:31 | `96a27bcb232f6552` |  | `~/t508/src/t508n-repo/docs` |
| `MODULE_LOADING_PATH.md` | 6.1 KiB | 2026-08-01 06:31 | `42318ddd96bcbb47` | G883 | `~/t508/work/srag-refs/UnisocBypass/oc/voltage-ceiling-hunt` |
| `MODULE_LOADING_PATH.md` | 6.1 KiB | 2026-08-01 06:31 | `42318ddd96bcbb47` | G883 | `~/t508/work/srag/UnisocBypass-main/oc/voltage-ceiling-hunt` |
| `prompt-stock-alternatives-waiter.md` | 6.0 KiB | 2026-08-17 13:29 | `369ec66e12db8a7f` | G461 | `~/t508/docs/prompts` |
| `prompt-stock-alternatives-waiter.md` | 6.0 KiB | 2026-08-23 23:42 | `369ec66e12db8a7f` | G461 | `~/t508/src/t508n-repo/docs` |
| `UBOOT_UNLOCK.md` | 5.9 KiB | 2026-08-01 06:31 | `381aa983af3e1971` | G1684 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `UBOOT_UNLOCK.md` | 5.9 KiB | 2026-08-01 06:31 | `381aa983af3e1971` | G1684 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `README.md` | 5.9 KiB | 2026-09-13 05:24 | `ebb42a3f88989b7c` |  | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/w_spd_dump` |
| `KERNEL-适配方案.md` | 5.9 KiB | 2026-08-15 18:17 | `ef23efb17121f9f0` | G488 | `~/t508/docs` |
| `KERNEL-适配方案.md` | 5.9 KiB | 2026-08-23 23:42 | `ef23efb17121f9f0` | G488 | `~/t508/src/t508n-repo/docs` |
| `AGENTS-GUIDE.md` | 5.8 KiB | 2026-08-17 09:21 | `a4a44b2379da2a30` | G449 | `~/t508/docs` |
| `AGENTS-GUIDE.md` | 5.8 KiB | 2026-08-23 23:42 | `a4a44b2379da2a30` | G449 | `~/t508/src/t508n-repo/docs` |
| `USB-IPA-POWER-ROOT-CAUSE.md` | 5.8 KiB | 2026-08-24 22:35 | `4259df9218a40ddf` |  | `~/t508/src/t508n-repo/docs` |
| `README_zh.md` | 5.8 KiB | 2026-09-13 05:24 | `48cfe5003060b806` |  | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/w_spd_dump` |
| `prompt-vddusb33-variant-matrix.md` | 5.8 KiB | 2026-08-25 21:44 | `1a96cb0b7567cd4e` | G282 | `~/t508/docs/prompts` |
| `prompt-vddusb33-variant-matrix.md` | 5.8 KiB | 2026-08-28 18:05 | `1a96cb0b7567cd4e` | G282 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `prompt-vddusb33-variant-matrix.md` | 5.8 KiB | 2026-08-28 18:05 | `1a96cb0b7567cd4e` | G282 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `REPORT-54-OPENWRT-T508N.md` | 5.7 KiB | 2026-10-06 15:18 | `5014bebaff2c70b7` |  | `~/t508/docs/reports` |
| `GUIDE-vddusb33-stock-design.md` | 5.6 KiB | 2026-08-25 20:18 | `289090b81f51234b` | G283 | `~/t508/docs/reports` |
| `GUIDE-vddusb33-stock-design.md` | 5.6 KiB | 2026-08-28 18:05 | `289090b81f51234b` | G283 | `~/t508/work/vddusb33/decomp-handoff-full/docs` |
| `GUIDE-vddusb33-stock-design.md` | 5.6 KiB | 2026-08-28 18:05 | `289090b81f51234b` | G283 | `~/t508/work/vddusb33/decomp-handoff/docs` |
| `FACTORY-USB-ENABLE-PATH.md` | 5.6 KiB | 2026-08-24 22:38 | `a3d863e4bbeb50e2` |  | `~/t508/src/t508n-repo/docs` |
| `USB-ENABLE-IN-BOOT-GUIDE.md` | 5.4 KiB | 2026-08-24 22:28 | `c9f6f096aa9b2b9d` |  | `~/t508/src/t508n-repo/docs` |
| `REPORT.md` | 5.4 KiB | 2026-08-01 06:31 | `41e2e08ee548a0e2` | G974 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/000-stock-baseline` |
| `项目完成总结.md` | 5.4 KiB | 2026-10-01 23:37 | `e485c00be41d7c7a` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `REPORT.md` | 5.4 KiB | 2026-08-01 06:31 | `41e2e08ee548a0e2` | G974 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/000-stock-baseline` |
| `REPORT-T508N-BOOT-SLOT-与救砖.md` | 5.3 KiB | 2026-10-09 15:13 | `86a6e4834d2a9d71` |  | `~/t508/docs/reports` |
| `刷写范围与风险.md` | 5.3 KiB | 2026-10-01 23:37 | `6a556e6afbb71ca4` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/docs` |
| `README.md` | 5.3 KiB | 2026-08-01 06:31 | `45ff55178aa6c9eb` | G845 | `~/t508/work/srag-refs/UnisocBypass/keys` |
| `README.md` | 5.3 KiB | 2026-08-01 06:31 | `45ff55178aa6c9eb` | G845 | `~/t508/work/srag/UnisocBypass-main/keys` |
| `REPORT-T508N-mainline-bringup.md` | 5.2 KiB | 2026-08-16 19:32 | `7f4c4d0f3d0c8d9b` | G450 | `~/t508/docs/reports` |
| `REPORT-T508N-mainline-bringup.md` | 5.2 KiB | 2026-08-23 23:42 | `7f4c4d0f3d0c8d9b` | G450 | `~/t508/src/t508n-repo/docs` |
| `T508N-VENDOR-BUILD-RECIPE.md` | 5.2 KiB | 2026-08-23 00:13 | `b2ce32ad2ea8e6c7` | G463 | `~/t508/docs/reports` |
| `T508N-VENDOR-BUILD-RECIPE.md` | 5.2 KiB | 2026-08-23 23:42 | `b2ce32ad2ea8e6c7` | G463 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-wakeup-chain.md` | 5.1 KiB | 2026-08-17 13:01 | `fa3a17ed77cfa7a1` | G466 | `~/t508/docs/prompts` |
| `prompt-stock-wakeup-chain.md` | 5.1 KiB | 2026-08-23 23:42 | `fa3a17ed77cfa7a1` | G466 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-wakeup-nohz.md` | 5.0 KiB | 2026-08-17 23:05 | `b08a23595f5fa38c` | G445 | `~/t508/docs/prompts` |
| `prompt-stock-wakeup-nohz.md` | 5.0 KiB | 2026-08-23 23:42 | `b08a23595f5fa38c` | G445 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-sync-analysis.md` | 4.9 KiB | 2026-08-16 22:28 | `db52ca513fc4b0a7` | G509 | `~/t508/docs/prompts` |
| `prompt-stock-sync-analysis.md` | 4.9 KiB | 2026-08-23 23:42 | `db52ca513fc4b0a7` | G509 | `~/t508/src/t508n-repo/docs` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `fcf556dbbd628d85` | G1368 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/019-uv37.5-oc2100-ov1075` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `fcf556dbbd628d85` | G1368 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/019-uv37.5-oc2100-ov1075` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `00571f1c4e7d13b4` | G1116 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/029-gpu1000-userspace-ov850` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `00571f1c4e7d13b4` | G1116 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/029-gpu1000-userspace-ov850` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `1c174ebee78493b6` | G1086 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/008-cpu-uv37.5-gpu-uv25` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `1c174ebee78493b6` | G1086 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/008-cpu-uv37.5-gpu-uv25` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `7c86cda087fd42c4` | G1611 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `7c86cda087fd42c4` | G1611 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `35245025213a12bf` | G1336 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `35245025213a12bf` | G1336 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `e2c9ca13a429f374` | G1448 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/009-cpu-uv50-gpu-uv37.5` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `e2c9ca13a429f374` | G1448 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/009-cpu-uv50-gpu-uv37.5` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `130e4246389c30eb` | G1182 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/026-cpu2150-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `130e4246389c30eb` | G1182 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/026-cpu2150-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `2b05f7e452d3349f` | G1055 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `2b05f7e452d3349f` | G1055 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `1a7f7a201782b5a6` | G932 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/028-gpu950-userspace-ov850` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `1a7f7a201782b5a6` | G932 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/028-gpu950-userspace-ov850` |
| `prompt-stock-crosscluster-visibility.md` | 4.8 KiB | 2026-08-17 20:00 | `614de73ef9fcd6e1` | G513 | `~/t508/docs/prompts` |
| `prompt-stock-crosscluster-visibility.md` | 4.8 KiB | 2026-08-23 23:42 | `614de73ef9fcd6e1` | G513 | `~/t508/src/t508n-repo/docs` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `9d087485a24febe6` | G1150 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `9d087485a24febe6` | G1150 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `REPORT-diff.md` | 4.8 KiB | 2026-08-28 19:25 | `15549e8aff570369` |  | `~/t508/work/decomp-analysis` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `ec34aff6aaef5f39` | G1214 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/027-cpu2200-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `ec34aff6aaef5f39` | G1214 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/027-cpu2200-uv37.5-gpu900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `3e7e892b03ed52ae` | G1305 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/012-cpu-uv87.5` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `3e7e892b03ed52ae` | G1305 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/012-cpu-uv87.5` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `68da2691e2b1b888` | G1580 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/020-gpu-oc900` |
| `REPORT.md` | 4.8 KiB | 2026-08-01 06:31 | `68da2691e2b1b888` | G1580 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/020-gpu-oc900` |
| `README.md` | 4.8 KiB | 2026-10-01 23:37 | `13e07cb607a3d402` |  | `~/t508/work/srag-refs/VN300E-UMS9620-root/build` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `2f842c6b677cfe46` | G1026 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/013-cpu-uv100` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `2f842c6b677cfe46` | G1026 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/013-cpu-uv100` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `9ea8c162c239853f` | G993 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/011-cpu-uv75` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `9ea8c162c239853f` | G993 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/011-cpu-uv75` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `042cf5294bbbee92` | G1401 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/010-cpu-uv62.5` |
| `REPORT.md` | 4.7 KiB | 2026-08-01 06:31 | `042cf5294bbbee92` | G1401 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/010-cpu-uv62.5` |
| `无串口验证指南.md` | 4.6 KiB | 2026-08-15 18:24 | `ebe6e09b2049c4c1` |  | `E:/rootphone/t508n/linux_support` |
| `prompt-current-race-analysis.md` | 4.6 KiB | 2026-08-18 07:49 | `0b1db9885582dbd1` | G457 | `~/t508/docs/prompts` |
| `prompt-current-race-analysis.md` | 4.6 KiB | 2026-08-23 23:42 | `0b1db9885582dbd1` | G457 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-zone-memory.md` | 4.5 KiB | 2026-08-17 21:48 | `59a40c13d4bc782c` | G525 | `~/t508/docs/prompts` |
| `prompt-stock-zone-memory.md` | 4.5 KiB | 2026-08-23 23:42 | `59a40c13d4bc782c` | G525 | `~/t508/src/t508n-repo/docs` |
| `prompt-completion-wakeup-compare.md` | 4.5 KiB | 2026-08-17 09:10 | `e269d1e9b8afff4a` | G517 | `~/t508/docs/prompts` |
| `prompt-completion-wakeup-compare.md` | 4.5 KiB | 2026-08-23 23:42 | `e269d1e9b8afff4a` | G517 | `~/t508/src/t508n-repo/docs` |
| `prompt-mainline-completion-wakeup.md` | 4.4 KiB | 2026-08-17 09:11 | `d8ae2545a375cbfc` | G502 | `~/t508/docs/prompts` |
| `prompt-mainline-completion-wakeup.md` | 4.4 KiB | 2026-08-23 23:42 | `d8ae2545a375cbfc` | G502 | `~/t508/src/t508n-repo/docs` |
| `REPORT-T508N-UBOOT-magic64.md` | 4.4 KiB | 2026-10-09 15:13 | `593b2fa39baeb515` |  | `~/t508/docs/reports` |
| `README.md` | 4.4 KiB | 2026-09-29 19:18 | `394dd50a966d2414` |  | `~/t508/tools` |
| `T508N-PORTING-STATUS.md` | 4.4 KiB | 2026-07-11 10:49 | `456f33ce6be15545` |  | `/mnt/e/rootphone/t508n` |
| `prompt-stock-driver-init.md` | 4.4 KiB | 2026-08-17 22:19 | `377f6a96abaf3496` | G494 | `~/t508/docs/prompts` |
| `prompt-stock-driver-init.md` | 4.4 KiB | 2026-08-23 23:42 | `377f6a96abaf3496` | G494 | `~/t508/src/t508n-repo/docs` |
| `UBOOT-TEE-CALL-ROUTINES.md` | 4.3 KiB | 2026-08-24 22:24 | `0bd691fc9fbcb42f` |  | `~/t508/src/t508n-repo/docs` |
| `prompt-unisoc-mods-on-612.md` | 4.3 KiB | 2026-08-18 21:58 | `6936b836c03bc33d` | G459 | `~/t508/docs/prompts` |
| `prompt-unisoc-mods-on-612.md` | 4.3 KiB | 2026-08-23 23:42 | `6936b836c03bc33d` | G459 | `~/t508/src/t508n-repo/docs` |
| `DELTA.md` | 4.3 KiB | 2026-10-08 11:12 | `40daec79a03f50f1` |  | `~/t508/mainline-ref/t508n-mainline` |
| `USB-TEE-PERMISSION-QUERY.md` | 4.1 KiB | 2026-08-24 22:42 | `701e30c85fde8593` |  | `~/t508/src/t508n-repo/docs` |
| `prompt-lse-atomic-completion.md` | 4.1 KiB | 2026-08-17 09:11 | `4cec861e34ad83c7` | G465 | `~/t508/docs/prompts` |
| `prompt-lse-atomic-completion.md` | 4.1 KiB | 2026-08-23 23:42 | `4cec861e34ad83c7` | G465 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-memory-dev-init.md` | 4.1 KiB | 2026-08-18 19:03 | `ba57d2bb4a16c4f3` | G484 | `~/t508/docs/prompts` |
| `prompt-stock-memory-dev-init.md` | 4.1 KiB | 2026-08-23 23:42 | `ba57d2bb4a16c4f3` | G484 | `~/t508/src/t508n-repo/docs` |
| `prompt-build-54-custom.md` | 3.9 KiB | 2026-08-18 21:14 | `e7780a39e0ece123` | G533 | `~/t508/docs/prompts` |
| `prompt-build-54-custom.md` | 3.9 KiB | 2026-08-23 23:42 | `e7780a39e0ece123` | G533 | `~/t508/src/t508n-repo/docs` |
| `RECOVERY.md` | 3.8 KiB | 2026-10-09 15:13 | `62bef07fef279636` |  | `E:/Download/ums9620_tcl_t508n_magic64/docs` |
| `WORKFLOW.md` | 3.8 KiB | 2026-08-01 06:31 | `2a51685c1e91890a` | G1679 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `WORKFLOW.md` | 3.8 KiB | 2026-08-01 06:31 | `2a51685c1e91890a` | G1679 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `prompt-a76-requirements.md` | 3.8 KiB | 2026-08-18 08:39 | `ccaffd2dd29bfce6` | G516 | `~/t508/docs/prompts` |
| `prompt-a76-requirements.md` | 3.8 KiB | 2026-08-23 23:42 | `ccaffd2dd29bfce6` | G516 | `~/t508/src/t508n-repo/docs` |
| `prompt-stock-lock-analysis.md` | 3.7 KiB | 2026-08-16 21:52 | `9b0d04e764696cfd` | G452 | `~/t508/docs/prompts` |
| `prompt-stock-lock-analysis.md` | 3.7 KiB | 2026-08-23 23:42 | `9b0d04e764696cfd` | G452 | `~/t508/src/t508n-repo/docs` |
| `UBOOT-MAGIC64.md` | 3.7 KiB | 2026-10-09 15:13 | `a17651d2db12a42b` |  | `E:/Download/ums9620_tcl_t508n_magic64/docs` |
| `RECOVERY.md` | 3.7 KiB | 2026-10-09 15:12 | `ba7d4f42411a3f03` |  | `~/t508/mainline-ref/t508n-mainline` |
| `prompt-54-probe-port.md` | 3.7 KiB | 2026-08-18 21:31 | `4593d8e6f948695a` | G456 | `~/t508/docs/prompts` |
| `prompt-54-probe-port.md` | 3.7 KiB | 2026-08-23 23:42 | `4593d8e6f948695a` | G456 | `~/t508/src/t508n-repo/docs` |
| `SKILL-T508N-framebuffer-debug.md` | 3.7 KiB | 2026-08-16 19:32 | `bfaa88133c596a8e` | G493 | `~/t508/docs` |
| `SKILL-T508N-framebuffer-debug.md` | 3.7 KiB | 2026-08-23 23:42 | `bfaa88133c596a8e` | G493 | `~/t508/src/t508n-repo/docs` |
| `CHAIN_OF_TRUST.md` | 3.7 KiB | 2026-08-01 06:31 | `79a2d1e453fcfaed` | G1680 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `CHAIN_OF_TRUST.md` | 3.7 KiB | 2026-08-01 06:31 | `79a2d1e453fcfaed` | G1680 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `SIMGHDR_FORMAT.md` | 3.6 KiB | 2026-08-01 06:31 | `dfcd0167de0444ea` | G1677 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `SIMGHDR_FORMAT.md` | 3.6 KiB | 2026-08-01 06:31 | `dfcd0167de0444ea` | G1677 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `prompt-stock-page-alloc-init.md` | 3.6 KiB | 2026-08-17 21:15 | `7bc9515c5c1b630b` | G503 | `~/t508/docs/prompts` |
| `prompt-stock-page-alloc-init.md` | 3.6 KiB | 2026-08-23 23:42 | `7bc9515c5c1b630b` | G503 | `~/t508/src/t508n-repo/docs` |
| `UBOOT-MAGIC64.md` | 3.6 KiB | 2026-10-09 15:12 | `8b31125d52e876f3` |  | `~/t508/mainline-ref/t508n-mainline` |
| `REPORT-USB-A-MUSB-ROUTE.md` | 3.6 KiB | 2026-10-06 17:43 | `b70a6a60b56a9ce3` |  | `~/t508/docs/reports` |
| `prompt-stock-of-core-init.md` | 3.6 KiB | 2026-08-18 14:08 | `adc44111fff964d9` | G448 | `~/t508/docs/prompts` |
| `prompt-stock-of-core-init.md` | 3.6 KiB | 2026-08-23 23:42 | `adc44111fff964d9` | G448 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 3.5 KiB | 2026-08-23 23:47 | `642474fcaf2054e7` |  | `~/t508/src/t508n-repo` |
| `README.md` | 3.5 KiB | 2026-08-28 18:05 | `beb32c9256d791c3` | G1694 | `~/t508/work/vddusb33/decomp-handoff-full` |
| `README.md` | 3.5 KiB | 2026-08-28 18:05 | `beb32c9256d791c3` | G1694 | `~/t508/work/vddusb33/decomp-handoff` |
| `prompt-verify-kick-clean.md` | 3.4 KiB | 2026-08-18 21:57 | `cbe1e8ec7fe6616e` | G475 | `~/t508/docs/prompts` |
| `prompt-verify-kick-clean.md` | 3.4 KiB | 2026-08-23 23:42 | `cbe1e8ec7fe6616e` | G475 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 3.3 KiB | 2026-09-29 19:17 | `eb7f3a6b2006a7c2` |  | `~/t508/src` |
| `prompt-alt-table-integrity.md` | 3.3 KiB | 2026-08-17 13:59 | `383b2b65c0556c73` | G447 | `~/t508/docs/prompts` |
| `prompt-alt-table-integrity.md` | 3.3 KiB | 2026-08-23 23:42 | `383b2b65c0556c73` | G447 | `~/t508/src/t508n-repo/docs` |
| `prompt-final-index.md` | 3.3 KiB | 2026-08-18 22:35 | `04a5be00ac740f64` | G497 | `~/t508/docs/prompts` |
| `prompt-final-index.md` | 3.3 KiB | 2026-08-23 23:42 | `04a5be00ac740f64` | G497 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 3.2 KiB | 2026-09-29 19:17 | `05b1b8b8a880a469` |  | `~/t508/firmware` |
| `README.md` | 3.2 KiB | 2026-08-23 23:42 | `9cb0ca4428cd8414` |  | `~/t508/src/t508n-repo/tools` |
| `README.md` | 3.1 KiB | 2026-10-09 15:14 | `c9a08c7cfffa634e` |  | `~/t508/work` |
| `REPORT.md` | 3.1 KiB | 2026-08-01 06:31 | `67e8d4feb24a93fc` | G1253 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/004-gpu-uv37.5-cpu-uv25` |
| `REPORT.md` | 3.1 KiB | 2026-08-01 06:31 | `67e8d4feb24a93fc` | G1253 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/004-gpu-uv37.5-cpu-uv25` |
| `SLOT-CONTROL.md` | 3.1 KiB | 2026-10-09 15:13 | `f16a90c0af402db0` |  | `E:/Download/ums9620_tcl_t508n_magic64/docs` |
| `prompt-summary-all.md` | 3.1 KiB | 2026-08-18 07:43 | `3ff36edec956684e` | G501 | `~/t508/docs/prompts` |
| `prompt-summary-all.md` | 3.1 KiB | 2026-08-23 23:42 | `3ff36edec956684e` | G501 | `~/t508/src/t508n-repo/docs` |
| `REPORT.md` | 3.0 KiB | 2026-08-01 06:31 | `ae02b00e74af7453` | G1498 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/014-repro-cpu-uv37.5` |
| `REPORT.md` | 3.0 KiB | 2026-08-01 06:31 | `ae02b00e74af7453` | G1498 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/014-repro-cpu-uv37.5` |
| `SLOT-CONTROL.md` | 3.0 KiB | 2026-10-09 15:12 | `882229dbfa587fb8` |  | `~/t508/mainline-ref/t508n-mainline` |
| `README.md` | 3.0 KiB | 2026-08-01 06:31 | `9ec11d63c930d3e8` | G912 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test` |
| `README.md` | 3.0 KiB | 2026-08-01 06:31 | `9ec11d63c930d3e8` | G912 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test` |
| `REPORT.md` | 2.9 KiB | 2026-08-01 06:31 | `17665ea5cf5b7f05` | G1285 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/007-stock-both` |
| `REPORT.md` | 2.9 KiB | 2026-08-01 06:31 | `17665ea5cf5b7f05` | G1285 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/007-stock-both` |
| `BOOT-CHAIN.md` | 2.9 KiB | 2026-10-09 15:13 | `c24bc47324052471` |  | `E:/Download/ums9620_tcl_t508n_magic64/docs` |
| `README.md` | 2.9 KiB | 2026-10-09 15:12 | `cb678a1e3255b3da` |  | `~/t508/mainline-ref/t508n-mainline` |
| `key_moduli.md` | 2.9 KiB | 2026-08-01 06:31 | `682e3c259175ee4e` | G861 | `~/t508/work/srag-refs/UnisocBypass/analysis` |
| `key_moduli.md` | 2.9 KiB | 2026-08-01 06:31 | `682e3c259175ee4e` | G861 | `~/t508/work/srag/UnisocBypass-main/analysis` |
| `REPORT-USB-MUSB-FINAL-SUCCESS.md` | 2.8 KiB | 2026-10-06 19:58 | `64c2110cacb90033` |  | `~/t508/docs/reports` |
| `BOOT-CHAIN.md` | 2.8 KiB | 2026-10-09 15:12 | `393a123f8db44d4f` |  | `~/t508/mainline-ref/t508n-mainline` |
| `README.md` | 2.7 KiB | 2026-08-01 06:31 | `8e0aaf35079b967d` | G1151 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `README.md` | 2.7 KiB | 2026-08-01 06:31 | `8e0aaf35079b967d` | G1151 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `DHTB_FORMAT.md` | 2.6 KiB | 2026-08-01 06:31 | `737aa5e641e1a411` | G1675 | `~/t508/work/srag-refs/UnisocBypass/docs` |
| `DHTB_FORMAT.md` | 2.6 KiB | 2026-08-01 06:31 | `737aa5e641e1a411` | G1675 | `~/t508/work/srag/UnisocBypass-main/docs` |
| `SOURCES.md` | 2.6 KiB | 2026-10-09 15:12 | `59a976a522455566` |  | `~/t508/mainline-ref/t508n-mainline` |
| `REPORT.md` | 2.6 KiB | 2026-08-01 06:31 | `a285ce0302385a20` | G1427 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/005-stock-cpu-only` |
| `REPORT.md` | 2.6 KiB | 2026-08-01 06:31 | `a285ce0302385a20` | G1427 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/005-stock-cpu-only` |
| `REPORT.md` | 2.6 KiB | 2026-08-01 06:31 | `43362ac79839e903` | G1246 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/006-stock-gpu-only` |
| `REPORT.md` | 2.6 KiB | 2026-08-01 06:31 | `43362ac79839e903` | G1246 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/006-stock-gpu-only` |
| `prompt-unisoc-only-configs.md` | 2.5 KiB | 2026-08-18 21:17 | `394724a6fe9cc519` | G535 | `~/t508/docs/prompts` |
| `prompt-unisoc-only-configs.md` | 2.5 KiB | 2026-08-23 23:42 | `394724a6fe9cc519` | G535 | `~/t508/src/t508n-repo/docs` |
| `FLASH-20261005-B54-PROBE-BOOT-A.md` | 2.4 KiB | 2026-10-05 19:08 | `77a3624db36df3f0` |  | `~/t508/docs/reports` |
| `prompt-disasm-a-cpu.md` | 2.3 KiB | 2026-08-18 20:09 | `c56a6d6146787893` | G526 | `~/t508/docs/prompts` |
| `prompt-disasm-a-cpu.md` | 2.3 KiB | 2026-08-23 23:42 | `c56a6d6146787893` | G526 | `~/t508/src/t508n-repo/docs` |
| `prompt-disasm-B-sched.md` | 2.3 KiB | 2026-08-18 20:09 | `9221f8baa156d15b` | G469 | `~/t508/docs/prompts` |
| `prompt-disasm-B-sched.md` | 2.3 KiB | 2026-08-23 23:42 | `9221f8baa156d15b` | G469 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 2.3 KiB | 2026-10-08 11:29 | `99ec91ed56254fb3` |  | `~/t508/mainline-ref/t508n-mainline/config` |
| `prompt-disasm-E-devm.md` | 2.3 KiB | 2026-08-18 20:09 | `b99dee861ec623fe` | G505 | `~/t508/docs/prompts` |
| `prompt-disasm-E-devm.md` | 2.3 KiB | 2026-08-23 23:42 | `b99dee861ec623fe` | G505 | `~/t508/src/t508n-repo/docs` |
| `prompt-disasm-C-irq.md` | 2.3 KiB | 2026-08-18 20:09 | `42f5db57a048d2e4` | G528 | `~/t508/docs/prompts` |
| `prompt-disasm-C-irq.md` | 2.3 KiB | 2026-08-23 23:42 | `42f5db57a048d2e4` | G528 | `~/t508/src/t508n-repo/docs` |
| `prompt-disasm-F-fs.md` | 2.3 KiB | 2026-08-18 20:09 | `91fc56e02101576b` | G491 | `~/t508/docs/prompts` |
| `prompt-disasm-F-fs.md` | 2.3 KiB | 2026-08-23 23:42 | `91fc56e02101576b` | G491 | `~/t508/src/t508n-repo/docs` |
| `BUILD-REPORT.md` | 2.3 KiB | 2026-10-08 11:34 | `e7b8607ce6ee8374` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `prompt-disasm-D-mm.md` | 2.3 KiB | 2026-08-18 20:09 | `eda4684d7d7a688b` | G522 | `~/t508/docs/prompts` |
| `prompt-disasm-D-mm.md` | 2.3 KiB | 2026-08-23 23:42 | `eda4684d7d7a688b` | G522 | `~/t508/src/t508n-repo/docs` |
| `UBOOT-MAGIC64-REPORT.md` | 2.2 KiB | 2026-10-09 15:13 | `42caa094f56faf18` |  | `E:/Download/ums9620_tcl_t508n_magic64/docs` |
| `NEXT.md` | 2.2 KiB | 2026-10-09 15:12 | `f5400dce038a2329` |  | `~/t508/mainline-ref/t508n-mainline` |
| `UBOOT-MAGIC64-REPORT.md` | 2.2 KiB | 2026-10-09 15:13 | `30cdaa4da1298662` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `prompt-verify-nohz-kick.md` | 2.1 KiB | 2026-08-18 20:53 | `684ef1b6ac45b273` | G511 | `~/t508/docs/prompts` |
| `prompt-verify-nohz-kick.md` | 2.1 KiB | 2026-08-23 23:42 | `684ef1b6ac45b273` | G511 | `~/t508/src/t508n-repo/docs` |
| `prompt-verify-sgi-kfence.md` | 2.1 KiB | 2026-08-18 20:53 | `79ed47be6e8a9e10` | G462 | `~/t508/docs/prompts` |
| `prompt-verify-sgi-kfence.md` | 2.1 KiB | 2026-08-23 23:42 | `79ed47be6e8a9e10` | G462 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 2.1 KiB | 2026-09-29 19:17 | `9258fd37169194a6` |  | `~/t508/build` |
| `prompt-verify-mem-order.md` | 2.0 KiB | 2026-08-18 20:53 | `29d5e51cee091373` | G479 | `~/t508/docs/prompts` |
| `prompt-verify-mem-order.md` | 2.0 KiB | 2026-08-23 23:42 | `29d5e51cee091373` | G479 | `~/t508/src/t508n-repo/docs` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `4586cc1e20d3c8b5` | G856 | `~/t508/work/srag-refs/UnisocBypass/analysis/bootrom` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `4586cc1e20d3c8b5` | G856 | `~/t508/work/srag/UnisocBypass-main/analysis/bootrom` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `b4bd96ad4906e42a` | G1337 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `b4bd96ad4906e42a` | G1337 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `a8c28346b6c84b39` | G862 | `~/t508/work/srag-refs/UnisocBypass/fdl` |
| `README.md` | 1.9 KiB | 2026-08-01 06:31 | `a8c28346b6c84b39` | G862 | `~/t508/work/srag/UnisocBypass-main/fdl` |
| `MAINLINE-BOOT-ONESHOT.md` | 1.9 KiB | 2026-10-08 11:43 | `b6ee0ba7a219cd03` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `README.md` | 1.7 KiB | 2026-08-01 06:31 | `601858066519feb7` | G1572 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/022-gpu-oc900-ov1000` |
| `README.md` | 1.7 KiB | 2026-08-01 06:31 | `601858066519feb7` | G1572 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/022-gpu-oc900-ov1000` |
| `README.md` | 1.7 KiB | 2026-08-01 06:31 | `5c25f8ca18a3f971` | G1270 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/002-gpu-uv-all-25mV` |
| `README.md` | 1.7 KiB | 2026-08-01 06:31 | `5c25f8ca18a3f971` | G1270 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/002-gpu-uv-all-25mV` |
| `DRIVER-MAP.md` | 1.5 KiB | 2026-10-08 11:12 | `0d61266ad0978a37` |  | `~/t508/mainline-ref/t508n-mainline` |
| `MAINLINE-BOOT-RESULT.md` | 1.5 KiB | 2026-10-08 12:22 | `2bd507dffc62ba02` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `README.md` | 1.4 KiB | 2026-08-01 06:31 | `b1a32e832badaa42` | G1612 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `README.md` | 1.4 KiB | 2026-08-01 06:31 | `b1a32e832badaa42` | G1612 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `README.md` | 1.3 KiB | 2026-08-01 06:31 | `6048060675f1c3a8` | G1056 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `README.md` | 1.3 KiB | 2026-08-01 06:31 | `6048060675f1c3a8` | G1056 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `README.md` | 1.3 KiB | 2026-08-01 06:31 | `b3bec27e368bd3f7` | G960 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/001-gpu-uv-top-12.5mV` |

（该类共 454 项，其余见 CSV）

### 4.15 校验清单（59）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `SHA256SUMS` | 401 B | 2026-10-08 11:34 | `651bd479851e136b` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `SHA256SUMS` | 249 B | 2026-10-05 18:39 | `6f56e7319114e6b9` |  | `~/t508/build/boot-images/t508n-linux-boot-latest` |
| `mainline-boot.sha256` | 121 B | 2026-10-08 11:43 | `9bad6d9f8ff582e7` |  | `~/t508/mainline-ref/t508n-mainline/out` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5014246c2d662456` | G216 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/002-gpu-uv-all-25mV` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5014246c2d662456` | G216 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/003-gpu-uv-all-25mV-gpu-only-load` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `ffad1881a167e91b` | G1252 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/004-gpu-uv37.5-cpu-uv25` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/005-stock-cpu-only` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/006-stock-gpu-only` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/007-stock-both` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `f199b1eff8936db1` | G1443 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/009-cpu-uv50-gpu-uv37.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `56ec77d59f9d70c2` | G1396 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/010-cpu-uv62.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `be1476ea3943a9be` | G988 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/011-cpu-uv75` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `bca9cd9b2d094135` | G1300 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/012-cpu-uv87.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `fb140d0385385437` | G1021 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/013-cpu-uv100` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `3a6065a3714c361b` | G1497 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/014-repro-cpu-uv37.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `61cc358b58738cf7` | G1440 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/015-uv37.5-trip115` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `e80ff9c5c1c73e33` | G1479 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/016-uv37.5-trip115-cpu2100` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `f905f0d5ff34cffb` | G1243 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/017-uv37.5-trip109-stock-freq` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `e9028a9dbd2b3a9a` | G1018 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/018-uv37.5-stock-trips-freq2002` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `25366b359f07c18d` | G1363 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/019-uv37.5-oc2100-ov1075` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `7b8ed7a5a9dbfb0a` | G1574 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/020-gpu-oc900` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5014246c2d662456` | G216 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/002-gpu-uv-all-25mV` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5014246c2d662456` | G216 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/003-gpu-uv-all-25mV-gpu-only-load` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `ffad1881a167e91b` | G1252 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/004-gpu-uv37.5-cpu-uv25` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/005-stock-cpu-only` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/006-stock-gpu-only` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `5f0ddc3a7198ad8c` | G92 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/007-stock-both` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `f199b1eff8936db1` | G1443 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/009-cpu-uv50-gpu-uv37.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `56ec77d59f9d70c2` | G1396 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/010-cpu-uv62.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `be1476ea3943a9be` | G988 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/011-cpu-uv75` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `bca9cd9b2d094135` | G1300 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/012-cpu-uv87.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `fb140d0385385437` | G1021 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/013-cpu-uv100` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `3a6065a3714c361b` | G1497 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/014-repro-cpu-uv37.5` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `61cc358b58738cf7` | G1440 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/015-uv37.5-trip115` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `e80ff9c5c1c73e33` | G1479 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/016-uv37.5-trip115-cpu2100` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `f905f0d5ff34cffb` | G1243 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/017-uv37.5-trip109-stock-freq` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `e9028a9dbd2b3a9a` | G1018 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/018-uv37.5-stock-trips-freq2002` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `25366b359f07c18d` | G1363 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/019-uv37.5-oc2100-ov1075` |
| `vendor_boot.img.signed.sha256` | 89 B | 2026-08-01 06:31 | `7b8ed7a5a9dbfb0a` | G1574 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/020-gpu-oc900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `284ed240514c32c8` | G1081 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/008-cpu-uv37.5-gpu-uv25` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `ac61d657d78b89a9` | G1145 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `75cadf20c3f34642` | G1571 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/022-gpu-oc900-ov1000` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `2c279f3517b67488` | G1331 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `09c6f6b8cc005fde` | G1050 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `5b8c7cebadd7bf17` | G1606 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `87495a8bc05a8723` | G1177 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/026-cpu2150-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `42ea6d6102c9ecd1` | G1209 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/027-cpu2200-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `6adf59814ea6e79c` | G926 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/028-gpu950-userspace-ov850` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `07f08d7af4a3dab3` | G1110 | `~/t508/work/srag-refs/UnisocBypass/oc/oc_test/experiments/029-gpu1000-userspace-ov850` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `284ed240514c32c8` | G1081 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/008-cpu-uv37.5-gpu-uv25` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `ac61d657d78b89a9` | G1145 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/021-gpu-oc900-ov850` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `75cadf20c3f34642` | G1571 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/022-gpu-oc900-ov1000` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `2c279f3517b67488` | G1331 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/023-gpu-oc900-mali-supply` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `09c6f6b8cc005fde` | G1050 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/024-gpu-oc900-mali-only` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `5b8c7cebadd7bf17` | G1606 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/025-cpu2100-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `87495a8bc05a8723` | G1177 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/026-cpu2150-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `42ea6d6102c9ecd1` | G1209 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/027-cpu2200-uv37.5-gpu900` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `6adf59814ea6e79c` | G926 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/028-gpu950-userspace-ov850` |
| `vendor_boot.img.signed.sha256` | 65 B | 2026-08-01 06:31 | `07f08d7af4a3dab3` | G1110 | `~/t508/work/srag/UnisocBypass-main/oc/oc_test/experiments/029-gpu1000-userspace-ov850` |

### 4.16 整机备份压缩包（6）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `2a备份(苦力怕制作).zip` | 2.49 GiB | 2026-01-21 15:33 | `b92a80ff8063792b` | G405 | `E:/rootphone/t508n/savebrock` |
| `2a备份(苦力怕制作).zip` | 2.49 GiB | 2026-01-21 15:33 | `b92a80ff8063792b` | G405 | `E:/rootphone/t508n/savebrocka` |
| `SPD_Driver_R4.20.4201(驱动).zip` | 21.61 MiB | 2026-01-21 15:40 | `8db534f1ef654bbc` | G233 | `E:/rootphone/t508n/savebrock` |
| `SPD_Driver_R4.20.4201(驱动).zip` | 21.61 MiB | 2026-01-21 15:40 | `8db534f1ef654bbc` | G233 | `E:/rootphone/t508n/savebrocka` |
| `flash_all_spd_dump_带分区表.zip` | 1.02 MiB | 2026-01-21 15:39 | `484c02eb8738f122` | G411 | `E:/rootphone/t508n/savebrock` |
| `flash_all_spd_dump_带分区表.zip` | 1.02 MiB | 2026-01-21 15:39 | `484c02eb8738f122` | G411 | `E:/rootphone/t508n/savebrocka` |

### 4.17 解锁工具压缩包（9）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `3、Root工具TCL_T508N-Magisk-26300.zip` | 33.65 MiB | 2026-01-18 08:48 | `f4ef1fc46d8baf00` | G288 | `/mnt/e/Download` |
| `3、Root工具TCL_T508N-Magisk-26300.zip` | 33.65 MiB | 2026-01-18 08:48 | `f4ef1fc46d8baf00` | G288 | `/mnt/e/rootphone/t508n` |
| `ums9620_tcl_t508n_magic64.zip` | 11.25 MiB | 2026-10-09 15:13 | `5cbefab09145f2ee` |  | `/mnt/e/Download` |
| `ums9620_tcl_t508n_v3 (1).zip` | 5.64 MiB | 2026-10-09 08:36 | `a9be03e0fa830c60` | G289 | `/mnt/e/Download` |
| `ums9620_tcl_t508n_v3.zip` | 5.64 MiB | 2026-01-20 23:21 | `a9be03e0fa830c60` | G289 | `/mnt/e/Download` |
| `2、解BL锁工具ums9620_tcl_t508n_v2.zip` | 5.64 MiB | 2026-01-18 08:47 | `5430c76d20679f6c` | G287 | `/mnt/e/Download` |
| `2、解BL锁工具ums9620_tcl_t508n_v2.zip` | 5.64 MiB | 2026-01-18 08:47 | `5430c76d20679f6c` | G287 | `/mnt/e/rootphone/t508n` |
| `ums9620_tcl_t508n_v6 (1).zip` | 5.36 MiB | 2026-01-20 23:01 | `9bd408b911b3d24e` | G290 | `/mnt/e/Download` |
| `ums9620_tcl_t508n_v6.zip` | 5.36 MiB | 2026-01-18 08:45 | `9bd408b911b3d24e` | G290 | `/mnt/e/Download` |

### 4.18 magic64 一键包（1）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `t508n-uboot-magic64.zip` | 4.96 MiB | 2026-10-09 14:35 | `0c9921273878bebf` |  | `/mnt/c/platform-tools` |

### 4.19 启动链二进制/载荷（19）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `boot_54custom_probe_kernel.bin` | 30.16 MiB | 2026-10-05 19:11 | `9145f190e00de843` |  | `~/t508/work` |
| `trustos_a.bin` | 6.00 MiB | 2026-08-24 23:43 | `49c13177af172533` |  | `~/t508/firmware/payloads-work` |
| `mmcblk0boot0_stock_backup.bin` | 4.00 MiB | 2026-08-01 06:31 | `0e50cb3b9b83884a` | G198 | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `mmcblk0boot0_stock_backup.bin` | 4.00 MiB | 2026-08-01 06:31 | `0e50cb3b9b83884a` | G198 | `~/t508/work/srag/UnisocBypass-main/examples` |
| `mmcblk0boot0.bin` | 4.00 MiB | 2026-08-15 16:39 | `c1cec9ca55147821` | G672 | `~/t508/firmware/payloads` |
| `mmcblk0boot1.bin` | 4.00 MiB | 2026-08-15 16:39 | `c1cec9ca55147821` | G672 | `~/t508/firmware/payloads` |
| `boot_ramdisk_cpio.bin` | 3.49 MiB | 2026-08-15 16:51 | `98d6e006e19abe2c` |  | `~/t508/build/boot-artifacts` |
| `trustos_a.bin` | 2.99 MiB | 2026-08-15 16:40 | `94bb50c80f7003ef` |  | `~/t508/firmware/payloads` |
| `sml_a.bin` | 1023.5 KiB | 2026-08-24 23:43 | `b341e7ecec48345c` |  | `~/t508/firmware/payloads-work` |
| `sml_a.bin` | 137.2 KiB | 2026-08-15 16:40 | `f076239a15c4d8e6` | G266 | `~/t508/firmware/payloads` |
| `sml_a.bin` | 137.2 KiB | 2026-08-28 18:05 | `f076239a15c4d8e6` | G266 | `~/t508/work/vddusb33/decomp-handoff-full/firmware` |
| `sml_a.bin` | 137.2 KiB | 2026-08-28 18:05 | `f076239a15c4d8e6` | G266 | `~/t508/work/vddusb33/decomp-handoff/firmware` |
| `BootROM_ums512_0x100000_0x111FFF.bin` | 72.0 KiB | 2026-08-01 06:31 | `be9fc98c630a78e2` | G858 | `~/t508/work/srag-refs/UnisocBypass/analysis/bootrom` |
| `BootROM_ums512_0x100000_0x111FFF.bin` | 72.0 KiB | 2026-08-01 06:31 | `be9fc98c630a78e2` | G858 | `~/t508/work/srag/UnisocBypass-main/analysis/bootrom` |
| `vendor_bootconfig.bin` | 7.5 KiB | 2026-08-15 16:46 | `b2ae0c3c45f2f7ed` |  | `~/t508/build/boot-artifacts` |
| `rsa4096_boot_pub.bin` | 1.0 KiB | 2026-08-01 06:31 | `eb0619c347731386` | G850 | `~/t508/work/srag-refs/UnisocBypass/keys` |
| `rsa4096_boot_pub.bin` | 1.0 KiB | 2026-08-01 06:31 | `eb0619c347731386` | G850 | `~/t508/work/srag/UnisocBypass-main/keys` |
| `gnss_boot_capture.bin` | 38 B | 2026-10-07 18:43 | `3a0f8ed4504a0282` | G93 | `~/t508/work` |
| `gnss_boot_capture.bin` | 38 B | 2026-10-07 18:43 | `3a0f8ed4504a0282` | G93 | `/mnt/c/platform-tools` |

### 4.20 vbmeta 镜像（9）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `dtbo_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `407e3d4e906f18ca` |  | `~/t508/firmware/footers` |
| `l_agdsp_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `c21e47929f479fc9` |  | `~/t508/firmware/footers` |
| `nr_modem_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `50c7a323ed58c9e3` |  | `~/t508/firmware/footers` |
| `nr_phy_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `92f0a26359d0f499` |  | `~/t508/firmware/footers` |
| `pm_sys_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `d9fae90aa5f6c115` |  | `~/t508/firmware/footers` |
| `trustos_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `0c871efbd4648ad4` |  | `~/t508/firmware/footers` |
| `uboot_a.vbmeta` | 4.0 KiB | 2026-08-15 16:55 | `59308041508eabe6` |  | `~/t508/firmware/footers` |
| `boot_footer.vbmeta` | 4.0 KiB | 2026-08-15 16:54 | `9b0797598881ca9d` |  | `~/t508/firmware/stock` |
| `vendor_boot_footer.vbmeta` | 4.0 KiB | 2026-08-15 16:54 | `54c5579688750cf3` |  | `~/t508/firmware/stock` |

### 4.21 镜像备份(.bak)（1）

| 文件 | 大小 | 日期 | SHA256(前16) | 重复组 | 位置 |
|---|---:|---|---:|---:|---|
| `boot_54vendor.img.bak.1787922677` | 105.42 MiB | 2026-08-28 21:11 | `2055692d43137de8` |  | `~/t508/build/boot-images` |

## 5. 折叠目录（未逐条展开）

| 目录 | 文件数 | 体积 | 原因 |
|---|---:|---:|---|
| `~/t508/work/super_raw` | 6755 | 6.84 GiB | super 解包原始产物 |
| `~/t508/src/vendor-kernel-ums9620` | 85774 | 5.44 GiB | 上游/厂商内核源码树 |
| `~/t508/firmware/extracted-fs` | 6743 | 4.06 GiB | ext4 镜像解包产物 |
| `~/t508/work/android_stage` | 6619 | 4.05 GiB | Android staging 解包 |
| `~/t508/src/linux-6.12.103` | 108314 | 2.95 GiB | 上游/厂商内核源码树 |
| `~/t508/src/linux-5.4-custom` | 78239 | 2.87 GiB | 上游/厂商内核源码树 |
| `~/t508/src/linux-6.12-clean` | 101646 | 2.54 GiB | 上游/厂商内核源码树 |
| `~/t508/mainline-ref/src` | 105407 | 2.01 GiB | 上游参考仓库源码 |
| `~/t508/work/openwrt-sdk-25.12.5-armsr-armv8_gcc-14.3.0_musl.Linux-x86_64` | 35559 | 1.35 GiB | OpenWrt SDK 工具链 |
| `~/t508/src/linux-5.4.210` | 65708 | 846.79 MiB | 上游/厂商内核源码树 |
| `E:/rootphone/t508n/savebrocka/out/decompiled` | 60287 | 587.11 MiB | APK 反编译 smali |
| `~/t508/src/stock-5.4.210` | 62 | 192.61 MiB | 上游/厂商内核源码树 |
| `~/t508/src/lk-upstream-ref` | 4381 | 110.35 MiB | 上游/厂商内核源码树 |
| `~/t508/work/openwrt54` | 2374 | 107.14 MiB | OpenWrt 构建树 |
| `~/t508/src/fih_OT5_device_tree` | 79 | 55.15 MiB | 上游/厂商内核源码树 |
| `~/t508/src/twrp_OT5_device_tree` | 42 | 54.33 MiB | 上游/厂商内核源码树 |
| `~/t508/src/busybox-1.36.1` | 5233 | 31.27 MiB | 上游/厂商内核源码树 |
| `~/t508/mainline-ref/mu300-linux` | 1250 | 28.58 MiB | 上游参考仓库源码 |
| `~/t508/mainline-ref/f50-openwrt` | 1194 | 21.02 MiB | 上游参考仓库源码 |
| `(WSL-T508)/**/.git` | 338 | 2.50 MiB | 构建/解包中间产物 |
| `~/t508/mainline-ref/OpenWrt-CI-ZTE-F50` | 30 | 175.1 KiB | 上游参考仓库源码 |

## 6. 大文件登记（>256 MiB，共 28 个，65.76 GiB）

| 大小 | SHA256(前16) | 用途 | 位置 |
|---|---:|---|---|
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/super.img` |
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrock/Image/super.img` |
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/super.img` |
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrocka/Image/super.img` |
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/vendor_dlkm_b.img` |
| 5.47 GiB | `8738931e4f1df065` | 分区/文件系统镜像 | `~/t508/firmware/stock-images/super.img` |
| 2.73 GiB | `f436ec5dca133d4f` | 分区/文件系统镜像 | `~/t508/work/android_payload2.erofs` |
| 2.73 GiB | `1dd217108c871614` | 分区/文件系统镜像 | `C:/platform-tools/android_payload2.simg` |
| 2.73 GiB | `1dd217108c871614` | 分区/文件系统镜像 | `~/t508/work/android_payload2.simg` |
| 2.72 GiB | `86b7c5a513fe3938` | 分区/文件系统镜像 | `C:/platform-tools/android_payload.erofs` |
| 2.72 GiB | `86b7c5a513fe3938` | 分区/文件系统镜像 | `~/t508/work/android_payload.erofs` |
| 2.72 GiB | `743ea3385cc2e0cb` | 分区/文件系统镜像 | `C:/platform-tools/android_payload.simg` |
| 2.72 GiB | `743ea3385cc2e0cb` | 分区/文件系统镜像 | `~/t508/work/android_payload.simg` |
| 2.49 GiB | `b92a80ff8063792b` | 整机备份压缩包 | `E:/rootphone/t508n/savebrock/2a备份(苦力怕制作).zip` |
| 2.49 GiB | `b92a80ff8063792b` | 整机备份压缩包 | `E:/rootphone/t508n/savebrocka/2a备份(苦力怕制作).zip` |
| 1.64 GiB | `476a9e2c33163d53` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrocka/out/system_a.img` |
| 1.64 GiB | `476a9e2c33163d53` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/system_a.img` |
| 1.32 GiB | `16ca1fbc49a01f79` | 日志/证据 | `~/t508/work/lpunpack2.log` |
| 500.00 MiB | `19f6ff0da7fcec67` | stock 分区镜像 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/blackbox.img` |
| 500.00 MiB | `19f6ff0da7fcec67` | stock 分区镜像 | `E:/rootphone/t508n/savebrock/Image/blackbox.img` |
| 500.00 MiB | `19f6ff0da7fcec67` | stock 分区镜像 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/blackbox.img` |
| 500.00 MiB | `19f6ff0da7fcec67` | stock 分区镜像 | `E:/rootphone/t508n/savebrocka/Image/blackbox.img` |
| 492.66 MiB | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrocka/out/vendor_a.img` |
| 492.66 MiB | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/system_ext_b.img` |
| 492.66 MiB | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/vendor_a.img` |
| 301.53 MiB | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `E:/rootphone/t508n/savebrocka/out/system_ext_a.img` |
| 301.53 MiB | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/system_ext_a.img` |
| 301.53 MiB | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `~/t508/firmware/extracted-super/system_b.img` |

## 7. 敏感文件登记（只登记，未读取/未哈希/未复制）

| 文件 | 大小 | 日期 | 位置 |
|---|---:|---|---|
| `calinv.img` | 15.00 MiB | 2024-03-09 12:14 | `~/t508/firmware/stock-images/calinv.img` |
| `ot5_boot_key.pem` | 3.2 KiB | 2026-08-15 19:01 | `~/t508/tools/ot5_boot_key.pem` |
| `test_key.pem` | 3.2 KiB | 2026-08-15 17:42 | `~/t508/tools/test_key.pem` |
| `rsa2048_0.pem` | 1.6 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa2048_0.pem` |
| `rsa2048_0_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa2048_0_pub.pem` |
| `rsa2048_1.pem` | 1.6 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa2048_1.pem` |
| `rsa2048_1_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa2048_1_pub.pem` |
| `rsa2048_devkey_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa2048_devkey_pub.pem` |
| `rsa4096_boot.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_boot.pem` |
| `rsa4096_modem.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_modem.pem` |
| `rsa4096_product.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_product.pem` |
| `rsa4096_recovery.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_recovery.pem` |
| `rsa4096_system.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_system.pem` |
| `rsa4096_vbmeta.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_vbmeta.pem` |
| `rsa4096_vendor.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag-refs/UnisocBypass/keys/rsa4096_vendor.pem` |
| `rsa2048_0.pem` | 1.6 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa2048_0.pem` |
| `rsa2048_0_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa2048_0_pub.pem` |
| `rsa2048_1.pem` | 1.6 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa2048_1.pem` |
| `rsa2048_1_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa2048_1_pub.pem` |
| `rsa2048_devkey_pub.pem` | 451 B | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa2048_devkey_pub.pem` |
| `rsa4096_boot.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_boot.pem` |
| `rsa4096_modem.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_modem.pem` |
| `rsa4096_product.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_product.pem` |
| `rsa4096_recovery.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_recovery.pem` |
| `rsa4096_system.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_system.pem` |
| `rsa4096_vbmeta.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_vbmeta.pem` |
| `rsa4096_vendor.pem` | 3.2 KiB | 2026-08-01 06:31 | `~/t508/work/srag/UnisocBypass-main/keys/rsa4096_vendor.pem` |
| `t508n_key` | 399 B | 2026-10-07 07:00 | `C:/platform-tools/t508n_key` |
| `calinv.img` | 15.00 MiB | 2024-03-09 12:14 | `E:/rootphone/t508n/savebrock/Image/calinv.img` |
| `calinv.img` | 15.00 MiB | 2024-03-09 12:14 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/calinv.img` |
| `calinv.img` | 15.00 MiB | 2024-03-09 12:14 | `E:/rootphone/t508n/savebrocka/Image/calinv.img` |
| `calinv.img` | 15.00 MiB | 2024-03-09 12:14 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/calinv.img` |

原则：私钥/密钥/校准数据一律不读取内容、不外传；如需迁移，由用户手工操作。

## 8. 校验结果

| 项 | 数值 |
|---|---:|
| SHA256 成功条目 | 8029 |
| SHA256 失败条目 | 0 |
| 未哈希（非敏感） | 0 |
| 敏感跳过 | 32 |
| CHECKSUMS-T508N.sha256 行数 | 8029 |

> 说明：CHECKSUMS 覆盖扫描时登记的 8029 个文件。本次整理**新生成**的产物
> （docs/ 下 9 份清单与报告、work/folder-audit/、T508N/ 归档、Windows README 备份）不在其中；
> 归档自身完整性见 `T508N/MANIFEST.sha256`，来源映射见 `T508N/ORIGIN-MAP.csv`。

校验命令（WSL）：

```sh
cd / && sha256sum -c --quiet /home/test/t508n/t508/docs/CHECKSUMS-T508N.sha256
```

## 9. 相关文档

| 文档 | 内容 |
|---|---|
| [DUPLICATES-T508N.md](DUPLICATES-T508N.md) | 内容完全相同组 / 冗余副本 / 可回收体积 |
| [CONFLICTS-T508N.md](CONFLICTS-T508N.md) | 同名不同内容、boot/uboot/misc 谱系冲突 |
| [ORGANIZE-PLAN-T508N.md](ORGANIZE-PLAN-T508N.md) | 建议目录结构 + 复制/归档方案（先计划后执行） |
| [reports/REPORT-T508N-FOLDER-ORGANIZATION.md](reports/REPORT-T508N-FOLDER-ORGANIZATION.md) | 整理总报告 |
| [reports/REPORT-T508N-ARTIFACT-MANIFEST.md](reports/REPORT-T508N-ARTIFACT-MANIFEST.md) | 关键构件清单报告 |

