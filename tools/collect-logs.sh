#!/bin/sh
# After a trial falls back to Android: collect LK log, pstore and the init log persisted in boot_b.
set -eu
OUT=${1:-logs-$(date +%Y%m%d-%H%M%S)}
mkdir -p "$OUT"
adb shell "su -c 'getprop ro.boot.slot_suffix; ls -la /sys/fs/pstore'" | tee "$OUT/state.txt"
for f in $(adb shell "su -c 'ls /sys/fs/pstore'" | tr -d '\r'); do adb exec-out "su -c 'cat /sys/fs/pstore/$f'" > "$OUT/$f"; done
adb exec-out "su -c 'cat /dev/block/by-name/uboot_log'" > "$OUT/uboot_log.raw"
strings -n 6 "$OUT/uboot_log.raw" > "$OUT/uboot_log.txt"
# init writes stages + dmesg to boot_b at 48 MiB (8 MiB)
adb exec-out "su -c 'dd if=/dev/block/by-name/boot_b bs=4096 skip=12288 count=2048 2>/dev/null'" | tr -d '\000' > "$OUT/linux-persist.txt"
sed -n '/MU300-PERSIST-BEGIN/,/--- lsmod/p' "$OUT/linux-persist.txt" || true
echo "saved: $OUT"
