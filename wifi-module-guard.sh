#!/bin/sh
# Boot-time guard while testing a rebuilt sprdwl_ng: if the gateway is not
# reachable 3 min after boot, dump diagnostics to /root (persistent; /var/log
# is tmpfs here), put the pristine module back and reboot.
M=/lib/modules/$(uname -r)/kernel/drivers/net/wireless/uwe5622/unisocwifi/sprdwl_ng.ko.xz
LOG=/root/wifi-guard.log
GW=${GW:-192.168.1.1}
sleep 180
SUM=$(md5sum "$M" | cut -c1-8)
if ping -c3 -W3 $GW >/dev/null 2>&1; then
	echo "$(date '+%F %T') boot OK on $SUM $(wpa_cli -i wlan0 status 2>/dev/null | grep -E '^(bssid|freq)=' | tr '\n' ' ')" >> $LOG
	exit 0
fi
D=/root/wifi-guard-$(date '+%Y%m%d-%H%M%S')-$SUM
mkdir -p "$D"
dmesg > "$D/dmesg.txt" 2>&1
journalctl -b --no-pager > "$D/journal.txt" 2>&1
cat /proc/modules > "$D/modules.txt"
ip -br link > "$D/iplink.txt" 2>&1; ip -br addr >> "$D/iplink.txt" 2>&1
/usr/sbin/iw reg get > "$D/reg.txt" 2>&1
/usr/sbin/iw dev > "$D/iwdev.txt" 2>&1
wpa_cli -i wlan0 status > "$D/wpa-status.txt" 2>&1
timeout 20 /usr/sbin/iw dev wlan0 scan 2>&1 | grep -E "^BSS|freq:|SSID:" > "$D/scan.txt"
echo "$(date '+%F %T') NO NETWORK on $SUM: diagnostics in $D, restoring original module and rebooting" >> $LOG
if [ -f "$M.orig" ]; then cp -a "$M.orig" "$M" && depmod -a && sync; fi
reboot
