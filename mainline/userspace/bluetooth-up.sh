#!/bin/busybox sh
BB=/bin/busybox
D=/persist
LD=$D/lib/ld-musl-aarch64.so.1
LIB=$D/lib:$D/usr/lib
[ -x $LD ] || exit 1
$BB echo "T508N-BTUP start" > /dev/kmsg

# 1) BT module + tty
if ! $BB lsmod | $BB grep -q sprdbt_tty; then
    $BB insmod $D/modules/sprdbt_tty.ko 2>/tmp/bt-ko.err
fi
echo 0 > /sys/class/rfkill/rfkill1/soft 2>/dev/null
$BB sleep 1

# 2) HCI attach
$BB killall btattach_mini 2>/dev/null
$D/bin/btattach_mini /dev/ttyBT0 > /persist/btattach.log 2>&1 &
$BB sleep 3
echo 0 > /sys/class/rfkill/rfkill2/soft 2>/dev/null

# 3) dbus + bluetoothd
$BB mkdir -p /var/run/dbus /run/dbus
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket
$LD --library-path $LIB $D/usr/bin/dbus-daemon --system --fork --nopidfile 2>/tmp/dbus.err
$BB sleep 2
$BB killall bluetoothd 2>/dev/null
$LD --library-path $LIB $D/usr/bin/bluetoothd -n -d > /persist/bluetoothd.log 2>&1 &
$BB sleep 3

# 4) bring controller up
$LD --library-path $LIB $D/usr/bin/hciconfig hci0 up 2>/tmp/hciup.err
$BB echo "T508N-BTUP done" > /dev/kmsg
