@echo off
setlocal
cd /d "%~dp0"
title T508N 仅写入 magic64 U-Boot
echo ============================================================
echo  仅写 uboot_a / uboot_b = uboot_t508n_magic64_v1.img
echo  适用于已经解锁、只需更新 U-Boot 的设备
echo ============================================================
echo.
echo 手机关机，进 BROM / COM5，然后继续。
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part uboot_a uboot_t508n_magic64_v1.img write_part uboot_b uboot_t508n_magic64_v1.img reset
echo.
echo 成功标志：Write Part Done: uboot_a / uboot_b
pause
