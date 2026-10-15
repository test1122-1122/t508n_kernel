@echo off
setlocal
cd /d "%~dp0"
title T508N magic64 探针验证
echo ============================================================
echo  写入探针版 U-Boot
echo  启动日志里的 welcome to lk/MP 会变成 MAGIC64 PROBE OK
echo ============================================================
echo.
echo 手机关机，进 BROM / COM5，然后继续。
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part uboot_a uboot_t508n_magic64_probe.img write_part uboot_b uboot_t508n_magic64_probe.img reset
echo.
echo 完成后启动一次，再读 uboot_log 搜索 MAGIC64 PROBE OK
pause
