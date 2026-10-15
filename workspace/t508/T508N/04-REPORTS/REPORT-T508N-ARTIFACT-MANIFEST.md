# REPORT-T508N-ARTIFACT-MANIFEST：T508N 关键构件清单与出处

> 设备：TCL T508N / OT5，Unisoc UMS9620（T770）
> 日期：2026-10-09
> 用途：一眼看清每个关键 img/bin/zip/bat 是什么、在哪、来自哪里、是否重复、SHA256 是多少。
> 哈希口径：SHA256 全量（8029/8029 成功）。完整清单见 `INVENTORY-T508N.csv` 与 `CHECKSUMS-T508N.sha256`。

---

## 1. 启动链与解锁：权威构件表

| 构件 | 大小 | SHA256(前16) | 权威副本 | 说明 / 出处 |
|---|---:|---|---|---|
| `uboot_t508n_magic64_v1.img` | 3 MiB | `d4961f76747369a2` | `E:/Download/ums9620_tcl_t508n_magic64/` | T508N 专属 magic64 解锁 U-Boot（实机验证） |
| `uboot_t508n_magic64_probe.img` | 3 MiB | `fde6b307b86cc946` | 同上 | 探针版，日志出现 `MAGIC64 PROBE OK` |
| `uboot_a_stock.img` / `uboot_b_stock.img` | 3 MiB | `8b5a6131946b7e91` | 同上 | 原厂 signed U-Boot（救砖回滚） |
| `misc_slot_a.img` | 1 MiB | `82bc63d7f0bb9907` | 同上 | 选 A 槽 OpenWrt（bootloader_control 已重算 CRC32） |
| `misc_slot_b.img` | 1 MiB | `4948bee107cb8b3e` | 同上 | 选 B 槽 mainline |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | 同上 | 解锁流程写入 misc（会清 BCB，勿与 misc_slot 混用） |
| `spd_dump.exe`（v3） | 40 448 B | `5c75c4ea64d61226` | `E:/Download/ums9620_tcl_t508n_v3 (1)/` | 带 `exec_addr 0x65012f48`，BROM exploit 成功 |
| `spd_dump.exe`（v6/srag 最新） | 80 896 B | `2e4c117eb22ff800` | `C:/platform-tools/srag-v6/` | 目前最新；`t508n-uboot-magic64/` 同哈希 |
| `spd_dump.exe`（v2 老版） | 36 352 B | `2d2864b2a557c74a` | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/` | **第一条命令缺 exec_addr，BROM exploit 会失败** |
| `unlock_emmc.bat`（v3） | 1 608 B | `41d1f9f905a16b6a` | `E:/Download/ums9620_tcl_t508n_v3 (1)/` | 第一条命令带 `exec_addr`（已逐行核对） |
| `unlock_emmc.bat`（v2） | 1 587 B | `27a631cd2f232b1b` | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/` | 第二条命令 `spd_dump fdl fdl1-dl.bin …` 缺 exec_addr |
| `spl-emmc-unlock.bin` | 67 096 B | `e0846c50271cfa8b` | `…/magic64/spl-emmc-unlock.bin` | 解锁 SPL 用 |
| `u-boot-spl-16k-emmc-sign.bin` | 67 096 B | `682e17e3485eb827` | `…/magic64/u-boot-spl-16k-emmc-sign.bin` | SPL 恢复用 |
| `fdl2-cboot.bin` | 1.81 MiB | `2b5ed2415d169746` | `…/magic64/fdl2-cboot.bin` | 解锁流程写 uboot_a/b |
| `spl.bin`（真机备份） | 67 096 B | `354dadd5f23bcf4d` | `E:/rootphone/t508n/2unlock/` | **唯一真机 SPL 备份**（与 v3/magic64 的同名文件不同） |

---

## 2. 原始设备备份（`E:\rootphone\t508n\2unlock\`，唯一 A≠B 的真备份）

| 文件 | 大小 | SHA256(前16) | 说明 |
|---|---:|---|---|
| `boota.img` | 64 MiB | `5901a6e5e54248d2` | 真机 boot A 原始备份 |
| `bootb.img` | 64 MiB | `75d2347389ff2d82` | 真机 boot B 原始备份 |
| `uboota.bin` | 1.81 MiB | `403a9a89cf418674` | 真机 uboot_a 备份 |
| `ubootb.bin` | 1.81 MiB | `f7fe95924f49c7e1` | 真机 uboot_b 备份 |
| `spl.bin` | 67 096 B | `354dadd5f23bcf4d` | 真机 SPL 备份 |
| `m.bin` | 64 B | `299414aeee5aafff` | miscdata 解锁状态读取结果（非全 0 = 已解锁） |

> 对照：`v2/`、`v3/`、`C:\platform-tools` 下的 `boota.img` 与 `bootb.img` 哈希完全相同（`f252428edfe0c512`），**不是**真机 A/B 备份。

---

## 3. stock 恢复链（BROM 救砖最小集）

| 分区镜像 | 大小 | SHA256(前16) | 位置 |
|---|---:|---|---|
| `boot_a.img` / `boot_b.img` | 64 MiB | `f252428edfe0c512` | `savebrock/flash_all_spd_dump/`（20 份完全相同副本） |
| `vbmeta_a.img` / `vbmeta_b.img` | 1 MiB | `3d612aa2d7c7a697` | 同上 |
| `misc.img` | 1 MiB | `7db67f82f318dc58` | 同上（A 槽元数据） |
| `miscdata.img` | 1 MiB | `288619bd309dab10` | 同上（解锁状态 + 序列号/校准字段，**勿用他机**） |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | 同上（6 份完全相同，含误命名 `vendor_dlkm_b.img`） |
| `uboot_a.img` / `uboot_b.img` | 3 MiB | `8b5a6131946b7e91` | `savebrock/Image/` |
| 完整 dump 目录 | — | — | `E:\rootphone\t508n\savebrock\flash_all_spd_dump\`（95 个文件，含分区表 XML + pgpt） |
| 整机备份包 | 2.49 GiB | `b92a80ff8063792b` | `savebrock/2a备份(苦力怕制作).zip`（savebrocka 同哈希） |

---

## 4. mainline / OpenWrt / rootfs

| 构件 | 大小 | SHA256(前16) | 位置 | 状态 |
|---|---:|---|---|---|
| `boot_mainline_6.18.55_t508n_test5.img` | 64 MiB | `0c6011ead098ea14` | `t508/build/boot-images/` + `C:/platform-tools/` | **B 槽当前镜像** |
| `out/Image`（mainline 6.18.55） | 16.9 MiB | `5451a40043937f50` | `mainline-ref/t508n-mainline/out/` | 构建产物 |
| `mainline-initramfs5.cpio` | 1.15 MiB | `ce6b21c947e254ee` | 同上 | 配套 initramfs |
| `boot_54openwrt_hotspot_luci_rndis_auto15_hotspot_net.img` | 64 MiB | `73a61f5cc410dc20` | `C:/platform-tools/` + `t508/build/boot-images/` | **A 槽当前镜像**（5.4 OpenWrt 25.12.5） |
| `openwrt-p75-112m.img` | 112 MiB | `c84c556072c70058` | `C:/platform-tools/` | p75 ext4 rootfs（已实机挂载） |
| `openwrt-p75-256m.img` | 256 MiB | `c8d075e9b18f6d70` | `C:/platform-tools/` | p75 扩容版 |
| OpenWrt boot 变体 | 64 MiB × 236 | 各异 | `C:/platform-tools/`、`t508/build/boot-images/` | 版本迭代谱系，非冲突 |

---

## 5. 必须保留的结论（整理不得丢失）

### 5.1 `no valid os found` 根因（已验证）

```text
U-Boot AVB 强制校验
  + 自定义 boot 无原厂签名
  + B 槽 bootloader_control 元数据 unbootable
=> LK 判定无有效系统，打印 no valid os found 并回退/进下载模式
```

### 5.2 解决三层

1. **恢复一致 stock 启动链**（boot_a/b、vbmeta_*、dtbo_a/b、dtb_a/b、init_boot_a/b、misc、miscdata、uboot/spl）——用 `savebrock\flash_all_spd_dump\`。
2. **magic64 绕过 AVB**——写 `uboot_t508n_magic64_v1.img` 到 `uboot_a` / `uboot_b`（保持原厂 RSA 签名，payload_offset 0x210）。
3. **misc bootloader_control 设置 A/B 槽**——写 `misc_slot_a.img` / `misc_slot_b.img`（不依赖 fastboot）。

### 5.3 T508N magic64 patch 点（实机验证）

```text
0xb068 = 0xd503201f   nop（去掉 LOCK FLAG 提示 + 等待）
0xb0e8 = 0x52800033   mov w19,#1（get_lock_status 强制 UNLOCK）
0xb324 = 0xd503201f   nop（去掉 SKIP VERIFY UART 打印）
0xb32c = 0xd503201f   nop（去掉 SKIP VERIFY 屏幕打印）
LOAD_BASE = 0xb5000000
```
产物：`uboot_t508n_magic64_v1.img` = `d4961f76747369a2`…（构建命令见 `reports/REPORT-T508N-UBOOT-magic64.md`）

### 5.4 v2 vs v3

| | v2 | v3 |
|---|---|---|
| `spd_dump.exe` | 36 352 B（2024-02-27） | 40 448 B（2024-03-16） |
| `unlock_emmc.bat` | 1 587 B，第一条命令缺 `exec_addr 0x65012f48` | 1 608 B，带 `exec_addr` |
| BROM exploit | 失败 | 成功（实机验证） |

### 5.5 当前 A/B 状态（2026-10-09）

```text
U-Boot   = magic64 v1（uboot_a / uboot_b 均为此镜像）
A 槽     = vendor 5.4 OpenWrt 25.12.5（boot_54openwrt_..._auto15_hotspot_net.img）
B 槽     = mainline 6.18.55 test5（已写入，ANDROID! 校验通过）
rootfs   = p75 ext4（openwrt-p75-*.img），/etc /usr /www /lib /root bind 到 p75
槽位控制 = misc_slot_a.img / misc_slot_b.img（不依赖 fastboot）
已解锁   = androidboot.flash.locked=0 / verifiedbootstate=orange
```

---

## 6. 从零恢复 `no valid os found` 的最短路径（按报告可复现）

```text
【A】恢复 stock 启动链（BROM/SPD，write_part）
    boot_a  <- savebrock/flash_all_spd_dump/boot_a.img
    boot_b  <- savebrock/flash_all_spd_dump/boot_b.img
    vbmeta_a/b, dtbo_a/b, dtb_a/b, init_boot_a/b, misc, miscdata
    若仍失败：写全套 stock（vendor_boot_a/b、spl_a/b、uboot_a/b）
【B】写 magic64 U-Boot
    uboot_a/b <- E:\Download\ums9620_tcl_t508n_magic64\uboot_t508n_magic64_v1.img
    回滚：05-恢复原厂uboot.bat
【C】切槽
    misc <- misc_slot_b.img（06-切换到B槽-mainline.bat）
    misc <- misc_slot_a.img（07-切换到A槽-openwrt.bat）
【D】注意
    先跑 01（会写 misc-fastbootd.bin 清 BCB），再写 misc_slot_*.img
```

---

## 7. 证据来源

| 证据 | 位置 |
|---|---|
| 逐文件 SHA256（8029 条） | `/home/test/t508n/t508/docs/CHECKSUMS-T508N.sha256` |
| 机器可读条目表 | `work/folder-audit/entries.json`、`raw/*.tsv`、`hashes.tsv` |
| U-Boot magic64 逆向/patch/实机结果 | `reports/REPORT-T508N-UBOOT-magic64.md`、`mainline-ref/t508n-mainline/UBOOT-MAGIC64.md` |
| A/B 槽 misc 结构与重打包 | `reports/REPORT-T508N-BOOT-SLOT-与救砖.md`、`…/SLOT-CONTROL.md` |
| 救砖流程 | `…/RECOVERY.md`、magic64 包 `docs/RECOVERY.md` |
| 构建历史（脚本/命令出处） | `~/.bash_history`（grep `magic_pack_ums9620` / `misc_slot` / `exec_addr`） |
