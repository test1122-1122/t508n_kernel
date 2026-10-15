# T508N 重复项分析报告

> 生成时间：2026-10-09 16:22:52 CST
> 方法：对扫描范围内全部已登记文件计算 SHA256（敏感文件除外），按内容分组；
> 完全相同 = SHA256 完全一致。本报告只做识别与建议，**未删除任何文件**。

## 0. 结论摘要

| 指标 | 数值 |
|---|---:|
| 已登记文件 | 8061 |
| 已计算 SHA256 | 8029（92.14 GiB） |
| 未哈希 | 32（75.06 MiB；敏感/超限） |
| 内容完全相同的组 | 1695 |
| 冗余副本（可回收） | 2995 个 |
| 冗余体积 | 58.02 GiB |

**关键判断**：T508N 现存资料的重复不是零散现象，而是「同一份 stock dump / 同一批 boot 变体」
被复制到 Windows 与 WSL 两侧共 4~6 份。**不建议删除任何副本**（BROM 救砖链依赖这些镜像的可用性），
建议改用 05-MANIFESTS/ 索引 + 每份副本登记 SHA256 的方式管理。

## 1. 大体积完全相同项（>64 MiB，按可回收体积排序）

| # | 大小 | 副本数 | 可回收 | 代表文件 | 全部位置 |
|---:|---:|---:|---:|---|---|
| 1 | 5.47 GiB | 6 | 27.34 GiB | `super.img` | `~/t508/firmware/extracted-super/vendor_dlkm_b.img`<br>`~/t508/firmware/stock-images/super.img`<br>`E:/rootphone/t508n/savebrock/Image/super.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/super.img`<br>`E:/rootphone/t508n/savebrocka/Image/super.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/super.img` |
| 2 | 2.73 GiB | 2 | 2.73 GiB | `android_payload2.simg` | `~/t508/work/android_payload2.simg`<br>`C:/platform-tools/android_payload2.simg` |
| 3 | 2.72 GiB | 2 | 2.72 GiB | `android_payload.erofs` | `~/t508/work/android_payload.erofs`<br>`C:/platform-tools/android_payload.erofs` |
| 4 | 2.72 GiB | 2 | 2.72 GiB | `android_payload.simg` | `~/t508/work/android_payload.simg`<br>`C:/platform-tools/android_payload.simg` |
| 5 | 2.49 GiB | 2 | 2.49 GiB | `2a备份(苦力怕制作).zip` | `E:/rootphone/t508n/savebrock/2a备份(苦力怕制作).zip`<br>`E:/rootphone/t508n/savebrocka/2a备份(苦力怕制作).zip` |
| 6 | 1.64 GiB | 2 | 1.64 GiB | `system_a.img` | `~/t508/firmware/extracted-super/system_a.img`<br>`E:/rootphone/t508n/savebrocka/out/system_a.img` |
| 7 | 500.00 MiB | 4 | 1.46 GiB | `blackbox.img` | `E:/rootphone/t508n/savebrock/Image/blackbox.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/blackbox.img`<br>`E:/rootphone/t508n/savebrocka/Image/blackbox.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/blackbox.img` |
| 8 | 100.00 MiB | 11 | 1000.00 MiB | `vendor_boot_a_stock.img` | `~/t508/firmware/stock-images/vendor_boot_a.img`<br>`~/t508/firmware/stock-images/vendor_boot_b.img`<br>`C:/platform-tools/vendor_boot_a_stock.img`<br>`E:/rootphone/t508n/savebrock/Image/vendor_boot_a.img`<br>`E:/rootphone/t508n/savebrock/Image/vendor_boot_b.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/vendor_boot_a.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/vendor_boot_b.img`<br>`E:/rootphone/t508n/savebrocka/Image/vendor_boot_a.img`<br>`E:/rootphone/t508n/savebrocka/Image/vendor_boot_b.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/vendor_boot_a.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/vendor_boot_b.img` |
| 9 | 492.66 MiB | 3 | 985.32 MiB | `vendor_a.img` | `~/t508/firmware/extracted-super/system_ext_b.img`<br>`~/t508/firmware/extracted-super/vendor_a.img`<br>`E:/rootphone/t508n/savebrocka/out/vendor_a.img` |
| 10 | 301.53 MiB | 3 | 603.06 MiB | `system_ext_a.img` | `~/t508/firmware/extracted-super/system_b.img`<br>`~/t508/firmware/extracted-super/system_ext_a.img`<br>`E:/rootphone/t508n/savebrocka/out/system_ext_a.img` |
| 11 | 226.29 MiB | 3 | 452.58 MiB | `odm_a.img` | `~/t508/firmware/extracted-super/odm_a.img`<br>`~/t508/firmware/extracted-super/product_b.img`<br>`E:/rootphone/t508n/savebrocka/out/odm_a.img` |
| 12 | 100.00 MiB | 4 | 300.00 MiB | `vendor_boot_linux.img` | `~/t508/build/boot-artifacts/vendor_boot_linux_new.img`<br>`~/t508/build/boot-images/t508n-linux-boot-latest/vendor_boot_linux.img`<br>`~/t508/build/boot-images/vendor_boot_linux.img`<br>`C:/platform-tools/vendor_boot_linux.img` |
| 13 | 142.52 MiB | 3 | 285.04 MiB | `product_a.img` | `~/t508/firmware/extracted-super/product_a.img`<br>`~/t508/firmware/extracted-super/vendor_b.img`<br>`E:/rootphone/t508n/savebrocka/out/product_a.img` |
| 14 | 112.32 MiB | 3 | 224.64 MiB | `termux-app_v0.118.3+github-debug_universal.apk` | `E:/rootphone/t508n/savebrocka/apk/termux-app_v0.118.3+github-debug_universal.apk`<br>`E:/rootphone/t508n/savebrocka/apk/【Termux】com.termux.apk`<br>`E:/rootphone/t508n/savebrocka/out/install/01_termux.apk` |

共 14 组，涉及 44.87 GiB。

## 2. 关键构件重复（启动链 / 工具 / 槽位）

| 构件 | 大小 | 副本数 | SHA256(前16) | 位置 |
|---|---:|---:|---|---|
| `2a备份(苦力怕制作).zip` | 2.49 GiB | 2 | `b92a80ff8063792b` | `E:/rootphone/t508n/savebrock/2a备份(苦力怕制作).zip`<br>`E:/rootphone/t508n/savebrocka/2a备份(苦力怕制作).zip` |
| `boota.img` | 64.00 MiB | 20 | `f252428edfe0c512` | `~/t508/firmware/stock-images/boot_a.img`<br>`~/t508/firmware/stock-images/boot_b.img`<br>`C:/platform-tools/boot_a_stock_android54.img`<br>`C:/platform-tools/boota.img`<br>`C:/platform-tools/bootb.img`<br>`C:/platform-tools/stock/boot_a.img`<br>`C:/platform-tools/stock/boot_b.img`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/boota.img`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/bootb.img`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/boota.img`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/bootb.img`<br>`E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300/tcl-26300.img`<br>`E:/rootphone/t508n/savebrock/Image/boot_a.img`<br>`E:/rootphone/t508n/savebrock/Image/boot_b.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/boot_a.img`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/boot_b.img`<br>`E:/rootphone/t508n/savebrocka/Image/boot_a.img`<br>`E:/rootphone/t508n/savebrocka/Image/boot_b.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/boot_a.img`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/boot_b.img` |
| `boot_mainline_6.18.55_t508n_test5.img` | 64.00 MiB | 2 | `0c6011ead098ea14` | `~/t508/build/boot-images/boot_mainline_6.18.55_t508n_test5.img`<br>`C:/platform-tools/boot_mainline_6.18.55_t508n_test5.img` |
| `fdl2-dl.bin` | 1.81 MiB | 13 | `bbd5e1d7ecdd057a` | `~/t508/work/srag/t508n-v3/fdl2-dl.bin`<br>`~/t508/work/srag/t508n-v6/fdl2-dl.bin`<br>`C:/platform-tools/srag-v3/fdl2-dl.bin`<br>`C:/platform-tools/srag-v6/fdl2-dl.bin`<br>`C:/platform-tools/srag/fdl2-dl.bin`<br>`C:/platform-tools/t508n-uboot-magic64/fdl2-dl.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/fdl2-dl.bin`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/fdl2-dl.bin`<br>`E:/rootphone/t508n/2unlock/fdl2-dl.bin`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/fdl2-dl.bin`<br>`E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300/fdl2-dl.bin`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/fdl2-dl.bin`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/fdl2-dl.bin` |
| `fdl2-cboot.bin` | 1.81 MiB | 9 | `2b5ed2415d169746` | `~/t508/work/srag/t508n-v3/fdl2-cboot.bin`<br>`~/t508/work/srag/t508n-v6/fdl2-cboot.bin`<br>`C:/platform-tools/srag-v3/fdl2-cboot.bin`<br>`C:/platform-tools/srag-v6/fdl2-cboot.bin`<br>`C:/platform-tools/srag/fdl2-cboot.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/fdl2-cboot.bin`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/fdl2-cboot.bin`<br>`E:/rootphone/t508n/2unlock/fdl2-cboot.bin`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/fdl2-cboot.bin` |
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | 4 | `d4961f76747369a2` | `~/t508/work/uboot/images/uboot_t508n_magic64_v1.img`<br>`C:/platform-tools/t508n-uboot-magic64/uboot_t508n_magic64_v1.img`<br>`C:/platform-tools/uboot_t508n_magic64_v1.img`<br>`E:/Download/ums9620_tcl_t508n_magic64/uboot_t508n_magic64_v1.img` |
| `uboot_t508n_magic64_probe.img` | 3.00 MiB | 3 | `fde6b307b86cc946` | `C:/platform-tools/t508n-uboot-magic64/uboot_t508n_magic64_probe.img`<br>`C:/platform-tools/uboot_t508n_magic64_probe.img`<br>`E:/Download/ums9620_tcl_t508n_magic64/uboot_t508n_magic64_probe.img` |
| `uboota.bin` | 1.81 MiB | 2 | `a2641056c560545f` | `E:/Download/ums9620_tcl_t508n_magic64/backup/uboota.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/backup/ubootb.bin` |
| `fdl2-dl.bin` | 874.4 KiB | 2 | `8ee9dde679aeb0e6` | `~/t508/work/srag-refs/UnisocBypass/fdl/fdl2-dl.bin`<br>`~/t508/work/srag/UnisocBypass-main/fdl/fdl2-dl.bin` |
| `fdl1-dl.bin` | 52.9 KiB | 12 | `352a013e9a010a39` | `~/t508/work/srag/t508n-v3/fdl1-dl.bin`<br>`~/t508/work/srag/t508n-v6/fdl1-dl.bin`<br>`C:/platform-tools/srag-v3/fdl1-dl.bin`<br>`C:/platform-tools/srag-v6/fdl1-dl.bin`<br>`C:/platform-tools/srag/fdl1-dl.bin`<br>`C:/platform-tools/t508n-uboot-magic64/fdl1-dl.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/fdl1-dl.bin`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/fdl1-dl.bin`<br>`E:/rootphone/t508n/2unlock/fdl1-dl.bin`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/fdl1-dl.bin`<br>`E:/rootphone/t508n/savebrock/flash_all_spd_dump/fdl1-dl.bin`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/fdl1-dl.bin` |
| `spl-emmc-unlock.bin` | 65.5 KiB | 6 | `e0846c50271cfa8b` | `~/t508/work/srag/t508n-v3/spl-emmc-unlock.bin`<br>`C:/platform-tools/srag-v3/spl-emmc-unlock.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/spl-emmc-unlock.bin`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/spl-emmc-unlock.bin`<br>`E:/rootphone/t508n/2unlock/spl-emmc-unlock.bin`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/spl-emmc-unlock.bin` |
| `spd_dump.exe` | 79.0 KiB | 4 | `2e4c117eb22ff800` | `~/t508/work/srag/t508n-v6/spd_dump.exe`<br>`C:/platform-tools/srag-v6/spd_dump.exe`<br>`C:/platform-tools/srag/spd_dump.exe`<br>`C:/platform-tools/t508n-uboot-magic64/spd_dump.exe` |
| `spd_dump.exe` | 39.5 KiB | 4 | `5c75c4ea64d61226` | `~/t508/work/srag/t508n-v3/spd_dump.exe`<br>`C:/platform-tools/srag-v3/spd_dump.exe`<br>`E:/Download/ums9620_tcl_t508n_magic64/spd_dump.exe`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/spd_dump.exe` |
| `spd_dump.exe` | 69.5 KiB | 2 | `d0ab6cb48e418ca7` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/spd_dump.exe`<br>`E:/rootphone/t508n/savebrocka/flash_all_spd_dump/spd_dump.exe` |
| `fdl1-dl.bin` | 60.6 KiB | 2 | `4bebf3d2f1961002` | `~/t508/work/srag-refs/zte-f50-toolkit/mac-f50/vendor/fdl1-dl.bin`<br>`E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300/fdl1-dl.bin` |
| `fdl1-dl.bin` | 58.7 KiB | 2 | `367f85f177e17b55` | `~/t508/work/srag-refs/UnisocBypass/fdl/fdl1-dl.bin`<br>`~/t508/work/srag/UnisocBypass-main/fdl/fdl1-dl.bin` |
| `spd_dump.exe` | 35.5 KiB | 2 | `2d2864b2a557c74a` | `E:/rootphone/t508n/2unlock/spd_dump.exe`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/spd_dump.exe` |
| `misc-wipe.bin` | 2.0 KiB | 3 | `bd6b67e852d6072e` | `~/t508/work/srag/t508n-v6/misc-wipe.bin`<br>`C:/platform-tools/srag-v6/misc-wipe.bin`<br>`C:/platform-tools/srag/misc-wipe.bin` |
| `unlock_emmc.bat` | 1.6 KiB | 3 | `41d1f9f905a16b6a` | `~/t508/work/srag/t508n-v3/unlock_emmc.bat`<br>`C:/platform-tools/srag-v3/unlock_emmc.bat`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/unlock_emmc.bat` |
| `unlock_emmc.bat` | 1.5 KiB | 2 | `27a631cd2f232b1b` | `E:/rootphone/t508n/2unlock/unlock_emmc.bat`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/unlock_emmc.bat` |
| `misc-fastbootd.bin` | 84 B | 6 | `0a23c55ca21259d9` | `~/t508/work/srag/t508n-v3/misc-fastbootd.bin`<br>`C:/platform-tools/srag-v3/misc-fastbootd.bin`<br>`E:/Download/ums9620_tcl_t508n_magic64/misc-fastbootd.bin`<br>`E:/Download/ums9620_tcl_t508n_v3 (1)/misc-fastbootd.bin`<br>`E:/rootphone/t508n/2unlock/misc-fastbootd.bin`<br>`E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/misc-fastbootd.bin` |

## 3. 重复度最高的组 TOP 40

| # | 大小 | 副本 | 可回收 | 代表文件 |
|---:|---:|---:|---:|---|
| 1 | 5.47 GiB | 6 | 27.34 GiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/super.img` |
| 2 | 2.73 GiB | 2 | 2.73 GiB | `C:/platform-tools/android_payload2.simg` |
| 3 | 2.72 GiB | 2 | 2.72 GiB | `C:/platform-tools/android_payload.erofs` |
| 4 | 2.72 GiB | 2 | 2.72 GiB | `C:/platform-tools/android_payload.simg` |
| 5 | 2.49 GiB | 2 | 2.49 GiB | `E:/rootphone/t508n/savebrock/2a备份(苦力怕制作).zip` |
| 6 | 1.64 GiB | 2 | 1.64 GiB | `E:/rootphone/t508n/savebrocka/out/system_a.img` |
| 7 | 500.00 MiB | 4 | 1.46 GiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/blackbox.img` |
| 8 | 64.00 MiB | 20 | 1.19 GiB | `E:/Download/ums9620_tcl_t508n_v3 (1)/boota.img` |
| 9 | 100.00 MiB | 11 | 1000.00 MiB | `C:/platform-tools/vendor_boot_a_stock.img` |
| 10 | 492.66 MiB | 3 | 985.32 MiB | `E:/rootphone/t508n/savebrocka/out/vendor_a.img` |
| 11 | 301.53 MiB | 3 | 603.06 MiB | `E:/rootphone/t508n/savebrocka/out/system_ext_a.img` |
| 12 | 55.00 MiB | 10 | 495.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/nr_runtimenv1.img` |
| 13 | 226.29 MiB | 3 | 452.58 MiB | `E:/rootphone/t508n/savebrocka/out/odm_a.img` |
| 14 | 50.00 MiB | 10 | 450.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/nr_fixnv1_a.img` |
| 15 | 50.00 MiB | 10 | 450.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/nr_fixnv1_b.img` |
| 16 | 50.00 MiB | 10 | 450.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/nr_modem_a.img` |
| 17 | 50.00 MiB | 10 | 450.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/nr_phy_a.img` |
| 18 | 64.00 MiB | 6 | 320.00 MiB | `C:/platform-tools/stock/metadata.img` |
| 19 | 100.00 MiB | 4 | 300.00 MiB | `C:/platform-tools/vendor_boot_linux.img` |
| 20 | 142.52 MiB | 3 | 285.04 MiB | `E:/rootphone/t508n/savebrocka/out/product_a.img` |
| 21 | 64.00 MiB | 5 | 256.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/cache.img` |
| 22 | 64.00 MiB | 5 | 256.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/prodnv.img` |
| 23 | 112.32 MiB | 3 | 224.64 MiB | `E:/rootphone/t508n/savebrocka/apk/termux-app_v0.118.3+github-debug_universal.apk` |
| 24 | 8.00 MiB | 29 | 224.00 MiB | `C:/platform-tools/stock/dtb_a.img` |
| 25 | 64.00 MiB | 4 | 192.00 MiB | `C:/platform-tools/boot_54openwrt_hotspot_luci_rndis_auto5_keypad.img` |
| 26 | 64.00 MiB | 4 | 192.00 MiB | `E:/rootphone/t508n/savebrock/boot_a.img` |
| 27 | 16.00 MiB | 10 | 144.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/ch_sys_a.img` |
| 28 | 16.00 MiB | 10 | 144.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/common_rs2_a.img` |
| 29 | 34.00 MiB | 5 | 136.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/isedata.img` |
| 30 | 64.00 MiB | 3 | 128.00 MiB | `C:/platform-tools/boot_linux_t508n_latest.img` |
| 31 | 8.00 MiB | 14 | 104.00 MiB | `C:/platform-tools/dtbo_a_stock.img` |
| 32 | 10.00 MiB | 10 | 90.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/hypervsior_a.img` |
| 33 | 4.00 MiB | 20 | 76.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/mmcblk0boot0.img` |
| 34 | 8.00 MiB | 10 | 72.00 MiB | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/fbootlogo.img` |
| 35 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_cinit_bigbar_v50align.img` |
| 36 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_clean_debug_bigbar_v50align.img` |
| 37 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_clean_debug_builtin_v50align.img` |
| 38 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_clean_debug_mknod_v50align.img` |
| 39 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_clean_debug_v50align.img` |
| 40 | 64.00 MiB | 2 | 64.00 MiB | `C:/platform-tools/boot_54custom_minshell_bigbar_v50align.img` |

完整 1695 组清单见 work/folder-audit/analysis.json（机器可读）与 INVENTORY-T508N.csv 的「重复组」列。

## 4. 处置建议（本轮不执行删除）

1. **每个重复组保留 1 份权威副本**：Windows 侧 E:/rootphone/t508n 与 magic64 包不动；
   WSL 侧被折叠的 work/* 构建中间副本可在用户确认后归档到 06-ARCHIVE/。
2. **不删除**：savebrock/、savebrocka/、2unlock/ 三套互为灾备，全部保留。
3. **用 manifest 取代物理去重**：05-MANIFESTS/ 内登记 SHA256 与权威副本指针即可，
   避免破坏 unlock_emmc.bat / spd_dump 的相对路径依赖。
4. 若确需空间，优先处理大体积完全重复（需用户二次确认）。

