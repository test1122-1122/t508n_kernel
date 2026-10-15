#!/bin/busybox sh
# Offline install of the bundled OpenWrt aarch64_generic packages into /persist.
# Run on T508N mainline as root.
set -e
D=/persist
LD=$D/lib/ld-musl-aarch64.so.1
LIB=$D/lib:$D/usr/lib
[ -x $LD ] || { echo "missing $LD"; exit 1; }
mkdir -p $D/lib $D/usr/lib $D/usr/bin $D/usr/sbin $D/etc/apk $D/pkgs
cp -f "$(dirname "$0")/apk" $D/bin/apk
cp -f "$(dirname "$0")/libz.so.1" $D/lib/libz.so.1
cp -f "$(dirname "$0")/apk.arch" $D/etc/apk/arch
cp -f "$(dirname "$0")/pkgs/"*.apk $D/pkgs/
cd $D
for p in $D/pkgs/*.apk; do
    $LD --library-path $LIB $D/bin/apk extract --allow-untrusted "$p" 2>/dev/null || true
done
echo "bluez extracted into $D"
