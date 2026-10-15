@echo off
setlocal
cd /d "%~dp0"
title T508N 刷 B 槽 mainline test40（CN 法规 + 自动 WiFi）
echo ============================================================
echo  test40 = test39 + 开机自动 iw reg set CN
echo.
echo  boot_b md5 = 234d57947e9a606cc03faa73fc7c3fdd
echo ============================================================
echo.
echo 准备：
echo   1. 长按电源键 10~15 秒，强制关机
echo   2. 按住音量键 + 插 USB，进 BROM / COM5
echo   3. 保持 USB，然后继续
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part boot_b boot_mainline_6.18.55_t508n_test40.img write_part misc misc_slot_b.img reset
echo.
echo 成功标志：Write Part Done: boot_b / misc
pause
