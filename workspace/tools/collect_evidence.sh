#!/bin/bash
# 一键取证: 拉取 pstore + uboot_log 并做初步判读
set -e
cd "$(dirname "$0")/.."
ADB=./tools/winadb/adb.exe
TS=$(date +%H%M%S)
mkdir -p work/evidence_$TS
$ADB shell su -c "cp -r /sys/fs/pstore /sdcard/pstore_ev 2>/dev/null; ls /sys/fs/pstore/ 2>&1" | head -10
$ADB pull /sdcard/pstore_ev work/evidence_$TS/pstore >/dev/null 2>&1 || true
$ADB shell su -c "dd if=/dev/block/by-name/uboot_log of=/sdcard/uboot_ev.img" >/dev/null 2>&1
$ADB pull /sdcard/uboot_ev.img work/evidence_$TS/ >/dev/null 2>&1
echo "=== pstore 文件:"
ls -la work/evidence_$TS/pstore/ 2>/dev/null | head
echo "=== uboot_log 关键行:"
strings -a work/evidence_$TS/uboot_ev.img | grep -E 'start_linux|merge_dtbo|fdt_overlay|FDT_ERR|exception_kernel|watchdog rst|kernel size is|ramdisk size is|Booting slot' | tail -25
echo "=== 取证目录: work/evidence_$TS"
