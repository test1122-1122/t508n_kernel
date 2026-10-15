# T508N 归档（B2-精简归档）

> 生成：2026-10-09　设备：TCL T508N / OT5，Unisoc UMS9620（T770）
> 本归档是**只读盘点 + 精简复制**的产物：`253` 个关键文件 / `1.48 GiB`，每个文件都有 SHA256 校验。
> **原始文件全部保持原位、未删除、未移动、未覆盖**；Windows 侧目录结构完全未改。

## 1. 本归档是什么

- 目的：把「救砖 + 解锁 + 当前运行集 + 关键文档」集中成一份可校验、可携带的最小集合。
- 原则：关键小文件复制；大文件（>256 MiB）与历史变体只登记；重复副本不复制。
- 校验：`ORIGIN-MAP.csv`（每个归档文件的来源与哈希）+ `MANIFEST.sha256`（归档自身完整性）。

```text
T508N/
  README.md                  本文件
  ORIGIN-MAP.csv             归档路径 -> 原始路径 映射（含 SHA256）
  MANIFEST.sha256            归档内容校验和（cd T508N && sha256sum -c MANIFEST.sha256）
  00-DOCS/                   索引、交接、报告、mainline 文档
  01-TOOLS/                  unlock-v2（对比样本）/ unlock-v3（整套）/ magic64（整套）/ spd-dump-versions（5 版）
  02-FIRMWARE/               stock-critical / device-backup / magic64 / misc-slots / boot-mainline / boot-openwrt
  03-ROOTFS/openwrt-p75/     p75 ext4 rootfs（112 MiB / 256 MiB）
  04-REPORTS/                本次整理报告
  05-MANIFESTS/              全量清单：INVENTORY / CHECKSUMS / DUPLICATES / CONFLICTS / BIGFILES / OPENWRT-VARIANTS
  06-ARCHIVE/                openwrt-known-good / historical-selected（精选，非全量）
```

## 2. 关键入口文档

| 文档 | 用途 |
|---|---|
| `00-DOCS/HANDOFF-COPY.md` | 交接入口（新会话从这里开始） |
| `00-DOCS/INDEX-COPY.md` | 资料索引 |
| `00-DOCS/FINAL-BRINGUP-INDEX.md` | mainline bring-up 综合索引 |
| `00-DOCS/REPORTS/REPORT-T508N-UBOOT-magic64.md` | magic64 校验链/patch 点/实机结果 |
| `00-DOCS/REPORTS/REPORT-T508N-BOOT-SLOT-与救砖.md` | A/B 槽 misc 控制与救砖 |
| `04-REPORTS/REPORT-T508N-ARTIFACT-MANIFEST.md` | 关键构件清单与出处 |
| `05-MANIFESTS/INVENTORY-T508N.csv` | 全量逐条清单（8767 行） |

## 3. 从零恢复 `no valid os found` 的最短路径

```text
根因：U-Boot AVB 强制校验 + 自定义 boot 无原厂签名 + B 槽元数据 unbootable。

【A】恢复 stock 启动链（BROM/SPD write_part；镜像在 02-FIRMWARE/stock-critical/）
    boot_a <- boot_a.img        boot_b <- boot_b.img
    init_boot_a/b, vendor_boot_a/b, dtbo_a/b, dtb_a/b,
    vbmeta_a/b 及 vbmeta_system/vendor/product/system_ext/odm_a/b, misc, miscdata, metadata
    仍失败时：spl_a/b, uboot_a/b, sml_a/b, trustos_a/b, teecfg_a/b, avbmeta_rs_a/b
【B】写 magic64 U-Boot（02-FIRMWARE/magic64/ 或 01-TOOLS/magic64/）
    uboot_a/b <- uboot_t508n_magic64_v1.img     回滚：uboot_a_stock.img / uboot_b_stock.img
    LOAD_BASE=0xb5000000；patch 0xb068/0xb0e8/0xb324/0xb32c
【C】切槽（02-FIRMWARE/misc-slots/）
    misc <- misc_slot_b.img（B 槽 mainline 6.18.55 test5）
    misc <- misc_slot_a.img（A 槽 OpenWrt auto15）
【D】顺序红线
    先跑 01-一键解锁并写magic64.bat（会写 misc-fastbootd.bin 清 BCB），再写 misc_slot_*.img。
    真机原始备份在 02-FIRMWARE/device-backup/（boota/bootb 唯一 A!=B 的真备份）。
```

## 4. 只登记、未复制的文件

| 类别 | 规模 | 位置 |
|---|---:|---|
| 大文件（>256 MiB） | 28 个 / 65.76 GiB | `05-MANIFESTS/BIGFILES-MANIFEST.md`；原件在原位置 |
| OpenWrt boot 历史变体 | 234 个 | `05-MANIFESTS/OPENWRT-VARIANTS-MANIFEST.md`（只归档 3 个） |
| 内容重复副本 | 2995 个 / 58.02 GiB | `05-MANIFESTS/DUPLICATES-T508N.md` |
| 敏感文件（密钥/校准） | 32 个 | 只登记路径，未读取/未哈希/未复制 |
| 折叠的构建源码树 | 21 个目录 / 34.09 GiB | `05-MANIFESTS/INVENTORY-T508N.md` 第 5 节 |

## 5. 校验

```sh
cd /home/test/t508n/t508/T508N
sha256sum -c --quiet MANIFEST.sha256        # 归档自身完整性
```

全量原始文件校验（8029 条，含未复制文件）：

```sh
cd / && sha256sum -c --quiet /home/test/t508n/t508/docs/CHECKSUMS-T508N.sha256
```

## 6. 安全声明

- 本归档由 `/home/test/t508n/t508/work/folder-audit/` 下的脚本生成，全部为**读源 + 新增**动作。
- 原始文件未删除、未移动、未重命名、未覆盖；Windows 侧（`E:\rootphone\t508n`、`E:\Download\ums9620_tcl_t508n_magic64`、`C:\platform-tools`）目录结构保持原样。
- 私钥/校准类文件不在归档内。
