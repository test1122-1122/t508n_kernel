@echo off
REM T508N 恢复官方系统脚本
REM 用法: 把本文件放到 E:\rootphone\t508n\savebrock\Image 目录下运行
cd /d %~dp0
echo [1/7] flash boot_a
fastboot flash boot_a boot_a.img
echo [2/7] flash boot_b
fastboot flash boot_b boot_b.img
echo [3/7] flash vendor_boot_a
fastboot flash vendor_boot_a vendor_boot_a.img
echo [4/7] flash vendor_boot_b
fastboot flash vendor_boot_b vendor_boot_b.img
echo [5/7] flash vbmeta_a
fastboot flash vbmeta_a vbmeta_a.img
echo [6/7] flash vbmeta_b
fastboot flash vbmeta_b vbmeta_b.img
echo [7/7] set active slot a
fastboot set_active a
echo rebooting...
fastboot reboot
echo DONE
