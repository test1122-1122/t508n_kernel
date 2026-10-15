T508N 一键解锁 + magic64 U-Boot 包

===================================



一、包内文件



01-一键解锁并写magic64.bat

    完整解锁流程，最后写入 magic64 U-Boot。

    推荐第一次使用这个。



02-仅写magic64-uboot.bat

    设备已经解锁，只更新 uboot_a / uboot_b。



03-探针验证.bat

    写入探针版，用于验证 magic64 是否真的执行。

    启动后日志里会出现 MAGIC64 PROBE OK。



04-备份uboot.bat

    备份本机 uboot_a / uboot_b 到 backup\。



05-恢复原厂uboot.bat

    写回原厂 signed U-Boot，救砖用。



uboot_t508n_magic64_v1.img

    T508N 专属 magic64 解锁 U-Boot。



uboot_t508n_magic64_probe.img

    探针验证版。



uboot_a_stock.img / uboot_b_stock.img

    原厂 signed U-Boot 备份。



spd_dump.exe / Channel9.dll / fdl1-dl.bin / fdl2-dl.bin

    SPD/BROM 刷写工具。



fdl2-cboot.bin / spl-emmc-unlock.bin

    解锁流程使用。



misc-fastbootd.bin

    解锁完成后写入 misc。



二、使用流程



1. 手机关机。

2. 按音量键组合插 USB，进入 BROM / COM5。

3. 双击 01-一键解锁并写magic64.bat

4. 按脚本提示操作。

5. 如果出现：

       find port failed

   关闭脚本，重新进 BROM，再运行。

6. 如果解锁步骤第一次不成功，重新进 BROM，再运行第 4 步。

7. 最后写入：

       uboot_a = uboot_t508n_magic64_v1.img

       uboot_b = uboot_t508n_magic64_v1.img



三、验证



1. 先运行 03-探针验证.bat

2. 启动一次

3. 读取 uboot_log，搜索：

       MAGIC64 PROBE OK

4. 出现后，再运行 02-仅写magic64-uboot.bat 或 01。



四、回滚



运行：

    05-恢复原厂uboot.bat



五、重要提示



- 本包只写 splloader / uboot_a / uboot_b / misc。

- 不写 boot_a / boot_b / userdata / vbmeta。

- 写入过程中不要拔 USB。

- 如果脚本提示 find port failed，按提示重新进 BROM 后重跑即可。


六、槽位切换（不用 fastboot）

U-Boot 通过 misc 分区 0x800 处的 bootloader_control 选择 A/B 槽。
本包已生成：

    misc_slot_a.img  -> 选择 A 槽 OpenWrt
    misc_slot_b.img  -> 选择 B 槽 mainline

用法：

    06-切换到B槽-mainline.bat
    07-切换到A槽-openwrt.bat

说明：
- U-Boot 日志显示 B 槽 unbootable / tries_remaining 0 时，fastboot
  会回退到 A 槽。必须先写 misc_slot_b.img，B 槽才会被选中。
- 不需要依赖 fastboot 模式。


七、详细文档

docs\BOOT-CHAIN.md            启动校验链
docs\UBOOT-MAGIC64.md         T508N U-Boot 逆向与 patch
docs\SLOT-CONTROL.md          A/B 槽 misc 控制
docs\RECOVERY.md              救砖与恢复
docs\UBOOT-MAGIC64-REPORT.md  构建/验证报告
docs\SLOT-CONTROL-REPORT.md   槽位切换报告


========================================================================
八、归档与索引（2026-10-09 文件整理）
========================================================================

1) 本包已纳入 T508N 离线归档（只读复制 + SHA256 校验，原文件未移动）：
      /home/test/t508n/t508/T508N/01-TOOLS/magic64/    本包整套副本
      /home/test/t508n/t508/T508N/05-MANIFESTS/        全量清单与校验和
   归档共 253 个关键文件 / 1.48 GiB，逐文件 SHA256 校验 253/253 通过。

2) v2 与 v3 的区别（重要，避免白忙一场）：
   - v2 工具（E:\\rootphone\\t508n\\2、解BL锁工具ums9620_tcl_t508n_v2\\）里 spd_dump.exe 是 36352 字节的老版本，
     其 unlock_emmc.bat 第 2 行缺少 exec_addr 0x65012f48，
     用它做 BROM exploit 会失败。
   - v3（E:\\Download\\ums9620_tcl_t508n_v3 (1)\\）为 40448 字节且带 exec_addr。
   => 推荐直接使用本 magic64 包（spd_dump.exe 40448 字节，实测可用），
      或 C:\\platform-tools\\srag-v6\\spd_dump.exe（80896 字节，最新版）。

3) B 槽切换（不依赖 fastboot）：
      06-切换到B槽-mainline.bat  ->  写 misc_slot_b.img（B 槽 mainline 6.18.55 test5）
      07-切换到A槽-openwrt.bat   ->  写 misc_slot_a.img（A 槽 OpenWrt 5.4 auto15）
   注意：01 脚本结尾会写 misc-fastbootd.bin 清掉 BCB，
         所以顺序必须是「先跑 01，再写 misc_slot_*.img」。

4) 索引位置：
      E:\\Download\\ums9620_tcl_t508n_magic64\\docs\\                                    本包文档
      /home/test/t508n/t508/docs/INVENTORY-T508N.csv       全量清单（8767 行）
      /home/test/t508n/t508/docs/CONFLICTS-T508N.md        同名冲突与风险
      /home/test/t508n/t508/docs/reports/                  整理总报告
      /home/test/t508n/t508/T508N/ORIGIN-MAP.csv           归档 <-> 原始位置映射

5) 安全说明：本次整理没有删除、移动或覆盖任何文件；本 README 的原版
   已备份为 README-中文.txt.bak-20261009-162210，可随时回滚。
