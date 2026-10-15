# REPORT-T508N-FOLDER-ORGANIZATION：T508N 项目文件夹整理总报告

> 设备：TCL T508N / OT5，Unisoc UMS9620（T770），3GB LPDDR4x + eMMC
> 日期：2026-10-09
> 阶段：**阶段一（只读盘点 + 分析 + 方案）已完成；阶段二（复制/归档）未执行，等待确认**
> 一句话结论：全部指定路径已盘清并逐文件 SHA256（8029 个成功 / 0 失败）；发现 1695 个内容重复组（58.02 GiB 理论可回收）与 284 组同名冲突；**没有删除、没有移动、没有覆盖任何文件**。

---

## 1. 盘点范围

| 用户指定路径 | WSL 对应 | 状态 | 文件数 | 体积 |
|---|---|---|---:|---:|
| `E:\rootphone\t508n` | `/mnt/e/rootphone/t508n` | 存在 | 791 | 39.11 GiB |
| `E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2` | 同左 | 存在（v2 工具） | 24 | 140 MiB |
| `E:\rootphone\t508n\2unlock` | 同左 | 存在（真机原始备份） | 24 | 154 MiB |
| `E:\rootphone\t508n\savebrock` | 同左 | 存在（stock dump #1） | 266 | 18 GiB |
| `E:\rootphone\t508n\savebrocka` | 同左 | 存在（stock dump #2） | 60628 | 22 GiB |
| `E:\Download\ums9620_tcl_t508n_v3 (1)` | 同左 | 存在（v3 工具） | 23 | 143.40 MiB |
| `E:\Download\ums9620_tcl_t508n_magic64` | 同左 | 存在（magic64 一键包） | 41 | 29.43 MiB |
| `E:\Download\ums9620_tcl_t508n_magic64.zip` | 同左 | 存在 | — | 11.25 MiB |
| `E:\Download\ums9620_tcl_t508n_v2 或类似 v2 工具目录` | `E:/Download` | **未找到目录** | — | — |
| `C:\platform-tools` | `/mnt/c/platform-tools` | 存在 | 441 | 15.67 GiB |
| `/home/test/t508n/t508` | 同左 | 存在 | 6776 | 71.35 GiB |
| `/home/test/t508n/t508/docs` | 同左 | 存在 | 107 | 12 MiB |
| `/home/test/t508n/t508/work` | 同左 | 存在 | — | — |
| `/home/test/t508n/t508/mainline-ref/t508n-mainline` | 同左 | 存在 | 1918 | 113 MiB |
| `/home/test/t508n/t508/build` | 同左 | 存在 | 231 | 7.4 GiB |

**未找到（如实记录，不猜测）**

- `E:\Download\ums9620_tcl_t508n_v2` 目录不存在；v2 以 `E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2\` 与 `E:\Download\2、解BL锁工具ums9620_tcl_t508n_v2.zip` 形式存在。
- `rootfs-vendor-usb` 未找到同名文件/目录；相关物证是 `boot_54openwrt_vendor_usbserial*.img` 与 `work/build-vendor-usb-modules.log`。
- 无 v4/v5 工具目录；额外发现 `ums9620_tcl_t508n_v6.zip` / `v6 (1).zip`（同一文件，两处）。

覆盖：登记文件 **8061** 个 / **126.3 GiB**；已计算 SHA256 **8029** 个 / **92.14 GiB**；折叠的 21 个大体积源码/解包目录（34.09 GiB）以目录摘要登记（见 `INVENTORY-T508N.md` 第 5 节）。本次产物目录 `work/folder-audit/` 已从清单中排除。

---

## 2. 关键文件清单

完整清单：`INVENTORY-T508N.md`（人读）/ `INVENTORY-T508N.csv`（机读，8767 行）/ `TREE-T508N.txt`（路径树）。

| 类别 | 数量 | 代表构件与权威副本 |
|---|---:|---|
| 解锁工具/载荷 | 66 | `spd_dump.exe`、`unlock_emmc.bat`、`fdl1/fdl2*.bin`、`spl-emmc-unlock.bin` |
| SPD/BROM 工具 | 50 | `spd_dump.exe`（5 个版本）、`chsize.exe`、`Channel9.dll` |
| magic64 U-Boot | 7 | `uboot_t508n_magic64_v1.img`（`d4961f76…`）、`…_probe.img`（`fde6b307…`） |
| stock U-Boot 镜像 | 4 | `uboot_a_stock.img` / `uboot_b_stock.img`（`8b5a6131…`） |
| misc 槽位控制 | 11 | `misc_slot_a.img`（`82bc63d7…`）、`misc_slot_b.img`（`4948bee1…`）、`misc-fastbootd.bin` |
| stock 分区镜像 | 513 | `boot_a/b`（`f252428e…`）、`vbmeta_*`、`dtbo/dtb`、`init_boot`、`misc/miscdata`、`super.img` |
| 原始设备备份 | 19 | `2unlock/` 下 `boota/bootb.img`、`uboota/b`.bin、`spl.bin`、`m.bin` |
| mainline 镜像 | 9 | `boot_mainline_6.18.55_t508n_test5.img`（`0c6011ea…`）、`out/Image` |
| OpenWrt / p75 | 236+2 | `boot_54openwrt_hotspot_luci_rndis_auto15_hotspot_net.img`（`73a61f5c…`）、`openwrt-p75-112m/256m.img` |
| 文档 | 1389 | `INDEX.md`、`HANDOFF.md`、`reports/*`、`t508n-mainline/*.md`、magic64 包 `docs/` |

---

## 3. 重复 / 冲突项

- **内容完全相同**：1695 组，2995 个冗余副本，理论可回收 **58.02 GiB**（`DUPLICATES-T508N.md`）。
  最大一组是 `super.img`（5.47 GiB × 6 份，含被错误命名为 `vendor_dlkm_b.img` 的那一份）。
- **同名不同内容/大小**：284 组，值得注意的 60 组（`CONFLICTS-T508N.md`）。要点：
  1. `boota.img` / `bootb.img`：只有 `2unlock/` 是真机原始 A≠B 备份；v2/v3/`C:\platform-tools` 中 A=B 同一文件。
  2. `uboota.bin` / `ubootb.bin`：三套互不相同（2unlock 真机 A≠B、v3 A=B、magic64/backup A=B）。
  3. `spd_dump.exe`：5 个版本（36352 / 39936 / 40448 / 71168 / 80896）；v2 的 36352 版本第一条命令缺 `exec_addr 0x65012f48`。
  4. `misc` 家族：`misc.img`（A 槽元数据）、`miscdata.img`、`misc-fastbootd.bin`（84 B，会覆盖 BCB）、`misc_slot_a/b.img`。
  5. `firmware/extracted-super/vendor_dlkm_b.img` 与 `super.img` 字节完全相同 → 命名错误。
  6. 文档双份：`t508/docs/reports/*` 与 `t508/src/t508n-repo/docs/*` 部分同名不同步。

---

## 4. 建议目录结构

见 `ORGANIZE-PLAN-T508N.md` 第 3 节。要点：**默认原地保留 + manifest 索引**，新建 `/home/test/t508n/t508/T508N/`（00-DOCS / 01-TOOLS / 02-FIRMWARE / 03-ROOTFS / 04-REPORTS / 05-MANIFESTS / 06-ARCHIVE），Windows 侧目录与文件名**完全不动**（保护 `unlock_emmc.bat`、`spd_dump`、magic64 包的相对路径）。

---

## 5. 已执行的操作

| # | 操作 | 影响面 |
|---|---|---|
| 1 | 只读扫描（`find -printf`）五大根 + 两个顶层 | 无写入 |
| 2 | 计算 8029 个文件的 SHA256（92.14 GiB） | 只读 |
| 3 | 新建过程产物目录 `/home/test/t508n/t508/work/folder-audit/` | 新增，位于 WSL |
| 4 | 生成 6 份盘点产物到 `/home/test/t508n/t508/docs/` | 新增文件，未覆盖既有文档 |
| 5 | 生成 2 份报告到 `/home/test/t508n/t508/docs/reports/` | 新增文件 |
| 6 | 更新 `docs/INDEX.md`、`docs/HANDOFF.md` 登记本次产物 | 追加小节 |

### 5.1 阶段二（B2-精简归档，2026-10-09 已执行）

| # | 操作 | 结果 |
|---|---|---|
| 7 | 新建归档根 `/home/test/t508n/t508/T508N/`（00-DOCS / 01-TOOLS / 02-FIRMWARE / 03-ROOTFS / 04-REPORTS / 05-MANIFESTS / 06-ARCHIVE） | 已建 |
| 8 | 复制关键集 **253 个文件 / 1.48 GiB**（文档 + 工具链 + stock 启动链 + 真机备份 + magic64 + misc 槽位 + mainline test5 + 当前 A 槽 OpenWrt + p75 rootfs + 精选 OpenWrt） | 逐文件 SHA256 校验 **253/253 OK**，0 失败 |
| 9 | 生成 `ORIGIN-MAP.csv`（253 条来源映射）与 `MANIFEST.sha256`（261 条） | `sha256sum -c` **261/261 OK** |
| 10 | 生成归档 `README.md` + 5 份说明（00-DOCS / unlock-v2 / spd-dump-versions / 06-ARCHIVE） | 新增 |
| 11 | 生成 `05-MANIFESTS/BIGFILES-MANIFEST.md`（>256 MiB 只登记）与 `OPENWRT-VARIANTS-MANIFEST.md`（历史变体只登记） | 新增 |
| 12 | 更新 `docs/INDEX.md`（§8）、`docs/HANDOFF.md`、`t508n-mainline/README.md`（Archive 小节） | 追加 |
| 13 | 备份并更新 Windows `README-中文.txt`（先备份 → 哈希核对 → 追加第八节，保持 CRLF） | 原 2984 B → 新 5111 B；备份 `README-中文.txt.bak-20261009-162210` |

**未执行（红线）**：没有删除任何文件；没有移动/重命名任何原始文件；没有覆盖任何镜像；没有连接手机；没有运行 adb / fastboot / spd_dump / dd 写操作；没有复制私钥与校准数据。

**被修改的原始文件仅有 1 个**：`E:\Download\ums9620_tcl_t508n_magic64\README-中文.txt`（用户批准，已备份，原内容前缀逐字节保留）。

---

## 6. 未动 / 待确认项

| 项 | 状态 | 说明 |
|---|---|---|
| `T508N/` 归档（B2-精简归档） | **已执行** | 253 文件 / 1.48 GiB，253/253 校验 OK |
| Windows 侧目录 | **零改动** | 仅 README-中文.txt 按批准更新（含备份） |
| `README-中文.txt` | **已更新** | 备份 `README-中文.txt.bak-20261009-162210` |
| `t508n-mainline/README.md` | **已更新** | 追加 Archive (2026-10-09) 小节 |
| `FINAL-BRINGUP-INDEX.md` | **未改** | 可选后续追加（不影响使用） |
| OpenWrt 200+ 历史变体 | **只登记** | 仅归档 3 个：auto15 / stable_emmc_rndis / boot_54vendor |
| 大文件（>256 MiB） | **只登记** | 未复制（见 05-MANIFESTS/BIGFILES-MANIFEST.md） |
| 敏感文件（32 个） | **只登记** | 未读取/未哈希/未复制 |

---

## 7. 校验结果

| 项 | 数值 |
|---|---:|
| SHA256 计算成功 | **8029** |
| SHA256 计算失败 | **0** |
| 未哈希（非敏感） | 0 |
| 敏感文件跳过 | 32 |
| `CHECKSUMS-T508N.sha256` 行数 | 8029 |
| 登记条目 / 文件 | 8767 / 8061 |
| 扫描期间读错误 | 0 |

复核命令：`cd / && sha256sum -c --quiet /home/test/t508n/t508/docs/CHECKSUMS-T508N.sha256`

---

## 8. 风险提示

1. **写 `misc` 会改变 A/B 槽选择**：`01-一键解锁并写magic64.bat` 结尾会写 `misc-fastbootd.bin`（清 BCB），必须先跑 01、之后再写 `misc_slot_a/b.img`。
2. **v2 工具链不可用于 BROM exploit**：`unlock_emmc.bat` 第 2 行缺 `exec_addr 0x65012f48`。
3. **boota/bootb 同文件陷阱**：多数副本 A=B，按文件名写回会把 A 槽 stock 镜像写到 B 槽。
4. **命名错误**：`firmware/extracted-super/vendor_dlkm_b.img` 实为 `super.img`。
5. **不要从 WSL UNC 运行 Windows 工具**；`spd_dump.exe` 请在 Windows 本地目录执行。
6. **不删除重复副本**：`savebrock/`、`savebrocka/`、`2unlock/` 互为灾备，BROM 救砖链依赖其可用性。
7. 私钥/校准类文件（`.pem`、`calinv.img`、`t508n_key` 等 32 个）未读取，未纳入哈希，迁移需人工。
