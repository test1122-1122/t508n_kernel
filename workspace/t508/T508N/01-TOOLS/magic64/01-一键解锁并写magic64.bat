@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
title T508N 一键解锁并写入 magic64 U-Boot
echo ============================================================
echo  T508N / UMS9620  一键解锁 + magic64 U-Boot
echo  最终写入: uboot_t508n_magic64_v1.img
echo ============================================================
echo.
echo 准备：
echo   1. 手机关机
echo   2. 按音量键组合插入 USB，进 BROM / COM5
echo   3. 保持 USB
echo.
echo 第 1 步：adb reboot autodloader（如果 Android 已不能启动，失败可忽略）
adb.exe reboot autodloader
echo.
echo 第 2 步：备份本机 SPL / U-Boot，并擦除 splloader
echo          如果提示 find port failed，关闭脚本，重新进 BROM 再运行
pause
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 read_part splloader 0 1m backup\spl.bin read_part uboot_a 0 3m backup\uboota.bin read_part uboot_b 0 3m backup\ubootb.bin erase_part splloader erase_part splloader_bak reset
echo.
echo 如果上面出现 find port failed，请不要继续，重新进 BROM 后重新运行本脚本。
pause
echo.
echo 第 3 步：写入 fdl2-cboot.bin 到 uboot_a / uboot_b
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part uboot_a fdl2-cboot.bin write_part uboot_b fdl2-cboot.bin reset
echo.
echo 如果提示找不到端口，重新进 BROM 后单独重跑本步骤。
pause
set StorageType=emmc
set spl_bak=backup\spl.bin
set uboot_a_bak=backup\uboota.bin
set uboot_b_bak=backup\ubootb.bin
chsize.exe backup\spl.bin
if %errorlevel% equ 1 set spl_bak=u-boot-spl-16k-emmc-sign.bin
chsize.exe backup\uboota.bin
if %errorlevel% equ 1 set uboot_a_bak=fdl2-sign.bin
chsize.exe backup\ubootb.bin
if %errorlevel% equ 1 set uboot_b_bak=fdl2-sign.bin
timeout /t 10 /nobreak
echo.
echo 第 4 步：解锁 SPL（发送 spl-emmc-unlock.bin）
spd_dump.exe exec_addr 0x65012f48 fdl spl-emmc-unlock.bin 0x65000800
echo.
echo 解锁可能需要执行两次。如果第一次不成功，请重新进 BROM 再运行第 4 步。
pause
echo.
echo 第 5 步：读取 miscdata 验证解锁状态
echo         解锁成功：32 字节字符串 + 16 字节 hash + 16 字节 hash
echo         未解锁：64 字节全 0
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec verbose 2 skip_confirm 1 read_part miscdata 8192 64 m.bin reset
echo.
pause
echo.
echo 第 6 步：恢复本机 SPL，写入 magic64 U-Boot，处理 misc
spd_dump.exe exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec skip_confirm 1 write_part splloader %spl_bak% write_part uboot_a uboot_t508n_magic64_v1.img write_part uboot_b uboot_t508n_magic64_v1.img erase_part misc write_part misc misc-fastbootd.bin reset
echo.
echo ============================================================
echo  完成。
echo  成功标志：Write Part Done: uboot_a / uboot_b
echo ============================================================
pause
