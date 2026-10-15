#!/bin/busybox sh
BB=/bin/busybox
D=/persist
LD=$D/lib/ld-musl-aarch64.so.1
[ -x $LD ] || exit 1
$BB echo "T508N-WIFIUP start" > /dev/kmsg
$LD --library-path $D/lib $D/bin/iw reg set CN 2>/dev/null
if ! $BB lsmod | $BB grep -q wcn_bsp; then
    $BB insmod $D/modules/wcn_bsp.ko 2>/tmp/wcn.err
fi
$BB sleep 3
if ! $BB lsmod | $BB grep -q sprd_wlan_combo; then
    $BB insmod $D/modules/sprd_wlan_combo.ko 2>/tmp/wlan.err
fi
i=0
while [ $i -lt 20 ]; do
    [ -d /sys/class/net/wlan0 ] && break
    $BB sleep 1
    i=$((i+1))
done
$BB timeout 10 $BB ifconfig wlan0 up 2>/dev/null
$BB killall wpad 2>/dev/null
$BB sleep 1
$LD --library-path $D/lib $D/sbin/wpad wpa_supplicant -B -i wlan0 -c $D/wpa.conf 2>/tmp/wpa.err
$BB sleep 10
$BB udhcpc -i wlan0 -q -n -t 10 -s $D/udhcpc.script 2>/tmp/dhcp.err
$BB echo "T508N-WIFIUP done" > /dev/kmsg

