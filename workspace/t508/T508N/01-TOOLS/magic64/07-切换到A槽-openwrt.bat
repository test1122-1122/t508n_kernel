@echo off
setlocal
cd /d "%~dp0"
title T508N 切换到 A 槽 OpenWrt
echo ============================================================
echo  写 misc_slot_a.img 到 misc
echo  U-Boot 将选择 A 槽 OpenWrt
echo ============================================================
echo.
echo 手机关机，进 BROM / COM5，然后继续。
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part misc misc_slot_a.img reset
echo.
echo 成功标志：Write Part Done: misc
pause
