# T508N 冲突项分析报告

> 生成时间：2026-10-09 16:22:52 CST
> 定义：**同名但内容不同（SHA256 不同）或大小不同**。同族版本迭代（如 boot_54openwrt_* 200 多个变体）
> 不是冲突；本报告只列同名同用途却互不一致、会导致「刷错镜像」的项。

## 0. 结论摘要

| 指标 | 数值 |
|---|---:|
| 同名但内容/大小不一致的组 | 284 |
| 其中大小 >=64 KiB 或涉及关键构件 | 60 |

### 必须知道的 6 个冲突（先看这张表）

| 项 | 冲突内容 | 正确用法 |
|---|---|---|
| boota.img / bootb.img | 2unlock/ 是**真机原始 A/B 备份**（A!=B）；v2/、v3/、C:/platform-tools 里 A=B 同一文件 | 救砖优先用 2unlock/boota.img + 2unlock/bootb.img |
| uboota.bin / ubootb.bin | 存在 3 套互不相同的备份（2unlock 真机 A!=B、v3 A=B、magic64/backup A=B） | 恢复原厂 U-Boot 用 magic64/uboot_a_stock.img，.bin 仅作交叉验证 |
| spd_dump.exe | 5 个版本：36352(v2) / 39936 / 40448(v3,magic64) / 71168(savebrock) / 80896(srag,v6) | v2 第一条命令缺 exec_addr 0x65012f48，BROM exploit 失败；用 v3 及以上 |
| misc 家族 | misc.img(A 槽元数据)、miscdata.img、misc-fastbootd.bin(84B)、misc-wipe.bin、misc_slot_a/b.img | 切槽用 misc_slot_a/b.img；misc-fastbootd.bin 会覆盖 BCB，不要与 misc_slot 混用 |
| vendor_dlkm_b.img | firmware/extracted-super/ 下 5.47 GiB，与 super.img 同量级 | 疑为命名错误（见第 6 节），使用前核对 SHA256 |
| 文档双份 | t508/docs/reports/* 与 t508/src/t508n-repo/docs/* 部分同名不同步 | 以 t508/docs/reports/ 为准 |

## 1. 全部值得注意的同名冲突（60 组）

| # | 文件名 | 副本 | 不同大小 | 不同 SHA256 数 | 关键 |
|---:|---|---:|---|---:|---|
| 1 | `adb.exe` | 12 | 5916440, 5920544, 8485016 | 3 |  |
| 2 | `adbwinapi.dll` | 14 | 97792, 108184, 108312, 108320 | 4 |  |
| 3 | `adbwinusbapi.dll` | 14 | 62976, 73368, 73496, 73504 | 4 |  |
| 4 | `archive-20260823.md` | 5 | 28145, 34613 | 2 | 是 |
| 5 | `boot-chain.md` | 2 | 2887, 3000 | 2 | 是 |
| 6 | `boot_a.img` | 8 | 67108864 | 2 | 是 |
| 7 | `boot_b.img` | 8 | 67108864 | 2 | 是 |
| 8 | `boot_linux.img` | 3 | 67108864 | 3 | 是 |
| 9 | `boota.img` | 4 | 67108864 | 2 | 是 |
| 10 | `bootb.img` | 4 | 67108864 | 2 | 是 |
| 11 | `channel9.dll` | 14 | 229376, 252928 | 2 |  |
| 12 | `chsize.exe` | 9 | 10240 | 2 | 是 |
| 13 | `e2fsck` | 2 | 259144, 348144 | 2 |  |
| 14 | `fastboot.exe` | 3 | 1630208, 2199704 | 2 | 是 |
| 15 | `fdl1-dl.bin` | 16 | 54168, 60068, 62024 | 3 | 是 |
| 16 | `fdl2-dl.bin` | 16 | 895364, 1035432, 1897416 | 3 | 是 |
| 17 | `final-bringup-index.md` | 2 | 42434, 42961 | 2 | 是 |
| 18 | `flash-stock-android.cmd` | 2 | 724 | 2 | 是 |
| 19 | `flash_all bat版.bat` | 4 | 2655, 2677 | 2 | 是 |
| 20 | `fsck.f2fs` | 2 | 285880 | 2 |  |
| 21 | `libc.so` | 4 | 873872, 1079248 | 2 |  |
| 22 | `libc_malloc_debug.so` | 2 | 151640, 227288 | 2 |  |
| 23 | `libm.so` | 4 | 131056, 233376 | 3 |  |
| 24 | `libz.so` | 2 | 114376 | 2 |  |
| 25 | `linker64` | 3 | 1823392, 1824000 | 3 |  |
| 26 | `mali_kbase_hwcnt_backend_csf.c` | 2 | 63106, 70428 | 2 |  |
| 27 | `manifest.md` | 2 | 424, 1104 | 2 | 是 |
| 28 | `mke2fs` | 2 | 74744, 133304 | 2 |  |
| 29 | `modified.dts` | 50 | 173885, 173890, 173891, 173916, 173949 | 24 |  |
| 30 | `noop_dtbo.img` | 3 | 8388608 | 2 |  |
| 31 | `rdavcom.cat` | 9 | 63501, 65920 | 2 |  |
| 32 | `readme.md` | 90 | 125, 141, 185, 226, 317, 358, 464, 473, 488, 528, 592, 597,  | 53 | 是 |
| 33 | `recovery.md` | 2 | 3811, 3937 | 2 | 是 |
| 34 | `report-usb-consolidated.md` | 3 | 14951, 19493 | 2 | 是 |
| 35 | `report.md` | 45 | 2612, 2626, 3017, 3119, 3192, 4845, 4847, 4848, 4883, 4888,  | 23 | 是 |
| 36 | `restore_stock.bat` | 3 | 272, 630 | 2 | 是 |
| 37 | `sha256sums` | 2 | 249, 401 | 2 | 是 |
| 38 | `slot-control-report.md` | 2 | 1260, 1307 | 2 | 是 |
| 39 | `slot-control.md` | 2 | 3068, 3179 | 2 | 是 |
| 40 | `sml_a.bin` | 4 | 140528, 1048064 | 2 | 是 |
| 41 | `spd_dump.exe` | 13 | 36352, 39936, 40448, 71168, 80896 | 5 | 是 |
| 42 | `spl.bin` | 3 | 67096 | 2 | 是 |
| 43 | `sprdadb.cat` | 9 | 63545, 66007 | 2 |  |
| 44 | `sprdvcom.cat` | 9 | 63507, 65932 | 2 |  |
| 45 | `sprdvmdm.cat` | 9 | 63507, 65930 | 2 |  |
| 46 | `trustos_a.bin` | 2 | 3137536, 6290944 | 2 | 是 |
| 47 | `uartcomm_com_5_2026_10_08.log` | 4 | 1142, 2249, 2181718, 3043875 | 4 |  |
| 48 | `uartcomm_com_8_2026_10_06.log` | 4 | 124, 494, 905500 | 3 |  |
| 49 | `uboot-magic64-report.md` | 2 | 2222, 2304 | 2 | 是 |
| 50 | `uboot-magic64.md` | 2 | 3675, 3820 | 2 | 是 |
| 51 | `uboot_log.img` | 7 | 16777216, 16928034 | 3 | 是 |
| 52 | `uboota.bin` | 3 | 1896464, 1897416 | 3 | 是 |
| 53 | `ubootb.bin` | 3 | 1896464, 1897416 | 3 | 是 |
| 54 | `unisoc_ums9620.md` | 2 | 0, 6257 | 2 | 是 |
| 55 | `unlock_emmc.bat` | 5 | 1587, 1608 | 2 | 是 |
| 56 | `vendor_boot.img.signed.sha256` | 56 | 65, 89 | 25 | 是 |
| 57 | `vendor_boot_linux.img` | 4 | 104857600 | 2 | 是 |
| 58 | `wdfcoinstaller01009.dll` | 18 | 1461992, 1471304, 1721576, 1730888 | 4 |  |
| 59 | `winusbcoinstaller2.dll` | 18 | 851176, 860488, 1002728, 1012040 | 4 |  |
| 60 | `无串口验证指南.md` | 3 | 4747, 7374 | 2 | 是 |

## 2. boota.img / bootb.img 逐份证据

| 路径 | 大小 | 时间 | SHA256(前16) | 判定 |
|---|---:|---|---|---|
| `C:/platform-tools/boota.img` | 64.00 MiB | 2026-10-08 21:25 | `f252428edfe0c512` | 同一文件复制（A=B） |
| `C:/platform-tools/bootb.img` | 64.00 MiB | 2026-10-08 21:25 | `f252428edfe0c512` | 同一文件复制（A=B） |
| `E:/Download/ums9620_tcl_t508n_v3 (1)/boota.img` | 64.00 MiB | 2026-10-09 08:39 | `f252428edfe0c512` | 同一文件复制（A=B） |
| `E:/Download/ums9620_tcl_t508n_v3 (1)/bootb.img` | 64.00 MiB | 2026-10-09 08:39 | `f252428edfe0c512` | 同一文件复制（A=B） |
| `E:/rootphone/t508n/2unlock/boota.img` | 64.00 MiB | 2026-01-20 22:53 | `5901a6e5e54248d2` | 真机原始备份 |
| `E:/rootphone/t508n/2unlock/bootb.img` | 64.00 MiB | 2026-01-20 22:53 | `75d2347389ff2d82` | 真机原始备份 |
| `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/boota.img` | 64.00 MiB | 2026-10-08 20:39 | `f252428edfe0c512` | 同一文件复制（A=B） |
| `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/bootb.img` | 64.00 MiB | 2026-10-08 20:39 | `f252428edfe0c512` | 同一文件复制（A=B） |

## 3. uboota.bin / ubootb.bin 逐份证据

| 路径 | 大小 | 时间 | SHA256(前16) |
|---|---:|---|---|
| `E:/Download/ums9620_tcl_t508n_magic64/backup/uboota.bin` | 1.81 MiB | 2026-10-09 14:41 | `a2641056c560545f` |
| `E:/Download/ums9620_tcl_t508n_magic64/backup/ubootb.bin` | 1.81 MiB | 2026-10-09 14:41 | `a2641056c560545f` |
| `E:/Download/ums9620_tcl_t508n_v3 (1)/uboota.bin` | 1.81 MiB | 2026-10-09 08:38 | `39b1616566ca3a95` |
| `E:/Download/ums9620_tcl_t508n_v3 (1)/ubootb.bin` | 1.81 MiB | 2026-10-09 08:38 | `39b1616566ca3a95` |
| `E:/rootphone/t508n/2unlock/uboota.bin` | 1.81 MiB | 2026-10-08 13:57 | `403a9a89cf418674` |
| `E:/rootphone/t508n/2unlock/ubootb.bin` | 1.81 MiB | 2026-10-08 13:57 | `f7fe95924f49c7e1` |

## 4. U-Boot / SPL 谱系（去重后）

| 镜像 | 大小 | SHA256(前16) | 所在目录 |
|---|---:|---|---|
| `uboot_ev.img` | 16.00 MiB | `230c51ce7e4c3674` | `~/t508/work/evidence/evidence_203711` |
| `uboot_log.img` | 16.00 MiB | `2c2541173510dac4` | `/mnt/e/rootphone/t508n` |
| `uboot_log_new.img` | 16.00 MiB | `2c2541173510dac4` | `~/t508/work/uboot/images` |
| `uboot_log3.img` | 16.00 MiB | `2c85d8eecd131c4e` | `E:/rootphone/t508n/linux_support` |
| `uboot_now.img` | 16.00 MiB | `2c85d8eecd131c4e` | `~/t508/firmware/device-dumps` |
| `uboot_base.img` | 16.00 MiB | `2c85d8eecd131c4e` | `~/t508/firmware/device-dumps` |
| `uboot_log_3.img` | 16.00 MiB | `2c85d8eecd131c4e` | `~/t508/work/uboot/images` |
| `uboot_log.img` | 16.00 MiB | `c2b558ca76b251eb` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_log2.img` | 16.00 MiB | `f889ecb36af5c2dc` | `E:/rootphone/t508n/linux_support` |
| `uboot_log_2.img` | 16.00 MiB | `f889ecb36af5c2dc` | `~/t508/work/uboot/images` |
| `uboot_modified.img` | 3.00 MiB | `068ab9e6eae2a698` | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_patched_usbon_v2.img` | 3.00 MiB | `6934b05548751873` | `C:/platform-tools/srag` |
| `uboot_patched_usbon.img` | 3.00 MiB | `7917ea86ace761f3` | `C:/platform-tools/srag` |
| `uboot_unlocked.img` | 3.00 MiB | `7c2da3da88f684cd` | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_a_stock.img` | 3.00 MiB | `8b5a6131946b7e91` | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboot_b_stock.img` | 3.00 MiB | `8b5a6131946b7e91` | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboot.bin` | 3.00 MiB | `8b5a6131946b7e91` | `C:/platform-tools/srag` |
| `uboot_t508n_lk_orig.bin` | 3.00 MiB | `8b5a6131946b7e91` | `C:/platform-tools/srag` |
| `uboot_a.img` | 3.00 MiB | `8b5a6131946b7e91` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_b.img` | 3.00 MiB | `8b5a6131946b7e91` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump` |
| `uboot_patched_usbon_v3w2.img` | 3.00 MiB | `b8735dcaf79cec24` | `C:/platform-tools/srag` |
| `uboot_stock.img` | 3.00 MiB | `cd6c48aa59948f94` | `~/t508/work/srag-refs/UnisocBypass/examples` |
| `uboot_t508n_magic64_v1.img` | 3.00 MiB | `d4961f76747369a2` | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboot_patched_v5.img` | 3.00 MiB | `dcaca8d2ed9405b7` | `C:/platform-tools/srag` |
| `uboot_patched_v4.img` | 3.00 MiB | `ef28912bda6677d2` | `C:/platform-tools/srag` |
| `uboot_t508n_magic64_probe.img` | 3.00 MiB | `fde6b307b86cc946` | `E:/Download/ums9620_tcl_t508n_magic64` |
| `uboota.bin` | 1.81 MiB | `39b1616566ca3a95` | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `ubootb.bin` | 1.81 MiB | `39b1616566ca3a95` | `E:/Download/ums9620_tcl_t508n_v3 (1)` |
| `uboota.bin` | 1.81 MiB | `403a9a89cf418674` | `E:/rootphone/t508n/2unlock` |
| `ubootb.bin` | 1.81 MiB | `f7fe95924f49c7e1` | `E:/rootphone/t508n/2unlock` |
| `uboota.bin` | 1.81 MiB | `a2641056c560545f` | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `ubootb.bin` | 1.81 MiB | `a2641056c560545f` | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `spl.bin` | 65.5 KiB | `354dadd5f23bcf4d` | `E:/rootphone/t508n/2unlock` |
| `spl.bin` | 65.5 KiB | `682e17e3485eb827` | `E:/Download/ums9620_tcl_t508n_magic64/backup` |
| `u-boot-spl-16k-emmc-sign.bin` | 65.5 KiB | `682e17e3485eb827` | `E:/Download/ums9620_tcl_t508n_magic64` |
| `spl-emmc-unlock.bin` | 65.5 KiB | `e0846c50271cfa8b` | `E:/Download/ums9620_tcl_t508n_magic64` |

## 5. spd_dump.exe 版本矩阵

| 大小 | SHA256(前16) | 时间 | 位置 | 备注 |
|---:|---|---|---|---|
| 36352 | `2d2864b2a557c74a` | 2024-02-27 17:55 | `E:/rootphone/t508n/2unlock/spd_dump.exe` | v2 老版：第一条命令缺 exec_addr（BROM exploit 失败） |
| 36352 | `2d2864b2a557c74a` | 2024-02-27 17:55 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/spd_dump.exe` | v2 老版：第一条命令缺 exec_addr（BROM exploit 失败） |
| 39936 | `366b0c05beee1bf2` | 2024-02-27 17:56 | `E:/rootphone/t508n/3、Root工具TCL_T508N-Magisk-26300/spd_dump.exe` | Root 工具自带 |
| 40448 | `5c75c4ea64d61226` | 2026-10-09 14:38 | `E:/Download/ums9620_tcl_t508n_magic64/spd_dump.exe` | v3 / magic64 包：带 exec_addr 0x65012f48 |
| 40448 | `5c75c4ea64d61226` | 2024-03-16 21:35 | `E:/Download/ums9620_tcl_t508n_v3 (1)/spd_dump.exe` | v3 / magic64 包：带 exec_addr 0x65012f48 |
| 40448 | `5c75c4ea64d61226` | 2024-03-16 21:35 | `C:/platform-tools/srag-v3/spd_dump.exe` | v3 / magic64 包：带 exec_addr 0x65012f48 |
| 40448 | `5c75c4ea64d61226` | 2024-03-16 21:35 | `~/t508/work/srag/t508n-v3/spd_dump.exe` | v3 / magic64 包：带 exec_addr 0x65012f48 |
| 71168 | `d0ab6cb48e418ca7` | 2025-02-21 18:03 | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/spd_dump.exe` | savebrock dump 自带 |
| 71168 | `d0ab6cb48e418ca7` | 2025-02-21 18:03 | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/spd_dump.exe` | savebrock dump 自带 |
| 80896 | `2e4c117eb22ff800` | 2026-10-06 18:38 | `C:/platform-tools/srag/spd_dump.exe` | srag / srag-v6：最新 |
| 80896 | `2e4c117eb22ff800` | 2025-04-25 10:39 | `C:/platform-tools/srag-v6/spd_dump.exe` | srag / srag-v6：最新 |
| 80896 | `2e4c117eb22ff800` | 2026-10-09 14:35 | `C:/platform-tools/t508n-uboot-magic64/spd_dump.exe` | srag / srag-v6：最新 |
| 80896 | `2e4c117eb22ff800` | 2025-04-25 10:39 | `~/t508/work/srag/t508n-v6/spd_dump.exe` | srag / srag-v6：最新 |

## 6. misc 家族与 super.img / vendor_dlkm_b.img

| 文件 | 大小 | SHA256(前16) | 位置 |
|---|---:|---|---|
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `~/t508/work/srag/t508n-v3/misc-fastbootd.bin` |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `C:/platform-tools/srag-v3/misc-fastbootd.bin` |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `E:/Download/ums9620_tcl_t508n_magic64/misc-fastbootd.bin` |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `E:/Download/ums9620_tcl_t508n_v3 (1)/misc-fastbootd.bin` |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `E:/rootphone/t508n/2unlock/misc-fastbootd.bin` |
| `misc-fastbootd.bin` | 84 B | `0a23c55ca21259d9` | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/misc-fastbootd.bin` |
| `misc-wipe.bin` | 2.0 KiB | `bd6b67e852d6072e` | `~/t508/work/srag/t508n-v6/misc-wipe.bin` |
| `misc-wipe.bin` | 2.0 KiB | `bd6b67e852d6072e` | `C:/platform-tools/srag-v6/misc-wipe.bin` |
| `misc-wipe.bin` | 2.0 KiB | `bd6b67e852d6072e` | `C:/platform-tools/srag/misc-wipe.bin` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `~/t508/firmware/stock-images/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `C:/platform-tools/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `C:/platform-tools/stock/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `E:/rootphone/t508n/savebrock/Image/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `E:/rootphone/t508n/savebrocka/Image/misc.img` |
| `misc.img` | 1.00 MiB | `7db67f82f318dc58` | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/misc.img` |
| `misc_slot_a.img` | 1.00 MiB | `82bc63d7f0bb9907` | `E:/Download/ums9620_tcl_t508n_magic64/misc_slot_a.img` |
| `misc_slot_b.img` | 1.00 MiB | `4948bee107cb8b3e` | `E:/Download/ums9620_tcl_t508n_magic64/misc_slot_b.img` |
| `misc_sprd_uid.ko` | 181.3 KiB | `0da5585080d78f18` | `~/t508/firmware/initramfs_root/lib/modules/5.4.210-android12-9-ga1d5c173d86b-ab1/misc_sprd_uid.ko` |
| `misc_sprd_uid.ko` | 13.6 KiB | `66b1baa5a74af06b` | `~/t508/firmware/vendor_ramdisk_out/lib/modules/misc_sprd_uid.ko` |
| `misc_sprd_uid.ko` | 181.3 KiB | `0da5585080d78f18` | `~/t508/work/vddusb33/decomp-handoff-full/chain_modules_full/misc_sprd_uid.ko` |
| `misc_sprd_uid.ko` | 13.6 KiB | `66b1baa5a74af06b` | `~/t508/work/vddusb33/decomp-handoff-full/stock_modules_full/misc_sprd_uid.ko` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `~/t508/firmware/stock-images/miscdata.img` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `C:/platform-tools/stock/miscdata.img` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `E:/rootphone/t508n/savebrock/Image/miscdata.img` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/miscdata.img` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `E:/rootphone/t508n/savebrocka/Image/miscdata.img` |
| `miscdata.img` | 1.00 MiB | `288619bd309dab10` | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/miscdata.img` |
| `stock-miscdata.img` | 1.00 MiB | `288619bd309dab10` | `C:/platform-tools/stock/stock-miscdata.img` |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | `~/t508/firmware/stock-images/super.img` |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | `E:/rootphone/t508n/savebrock/Image/super.img` |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/super.img` |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | `E:/rootphone/t508n/savebrocka/Image/super.img` |
| `super.img` | 5.47 GiB | `8738931e4f1df065` | `E:/rootphone/t508n/savebrocka/flash_all_spd_dump/super.img` |
| `vendor_dlkm_b.img` | 5.47 GiB | `8738931e4f1df065` | `~/t508/firmware/extracted-super/vendor_dlkm_b.img` |

## 7. 风险提示

1. 任何写 misc 的操作都会改变 A/B 槽选择；01-一键解锁并写magic64.bat 结尾会写 misc-fastbootd.bin，
   会清掉 bootloader_control。**必须先跑 01，之后再写 misc_slot_a/b.img**。
2. v2 工具链的 unlock_emmc.bat 第 2 行缺少 exec_addr 0x65012f48，用它做 BROM exploit 会失败；
   实机验证通过的是 v3 / magic64 包内的 spd_dump.exe(40448) 与 srag-v6(80896)。
3. boota.img 与 bootb.img 在多数目录里是同一文件；若按名字刷回，会把 A 槽 stock 镜像写到 B 槽。
4. .pem / 校准类文件未读取、未哈希、未复制，清单见 INVENTORY-T508N.md 第 7 节。

