@echo off
setlocal
cd /d "%~dp0"
title T508N 恢复原厂 U-Boot
echo ============================================================
echo  恢复 uboot_a / uboot_b 为原厂 signed 镜像
echo  使用包内 uboot_a_stock.img / uboot_b_stock.img
echo ============================================================
echo.
echo 手机关机，进 BROM / COM5，然后继续。
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part uboot_a uboot_a_stock.img write_part uboot_b uboot_b_stock.img reset
echo.
echo 完成。
pause
