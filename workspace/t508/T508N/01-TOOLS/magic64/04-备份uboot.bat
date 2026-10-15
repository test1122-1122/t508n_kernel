@echo off
setlocal
cd /d "%~dp0"
title T508N 备份本机 U-Boot
echo ============================================================
echo  备份本机 uboot_a / uboot_b 到 backup\
echo ============================================================
echo.
echo 手机关机，进 BROM / COM5，然后继续。
pause
if not exist backup mkdir backup
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 read_part uboot_a 0 3m backup\uboota.bin read_part uboot_b 0 3m backup\ubootb.bin reset
echo.
echo 完成：backup\uboota.bin backup\ubootb.bin
pause
