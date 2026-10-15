#!/bin/sh
# From rooted Android: copy the current Android hotspot SSID/passphrase into the Linux rootfs
# (/etc/mu300/hotspot.conf, mode 0600). The passphrase never leaves the device.
# requires tools/android-mount-mu300root.sh pushed to /data/local/tmp
# The Linux filesystem is the installation that boots: the mu300sd filesystem on the SD card when the card holds one
# (boot/init looks there first), mu300root in internal storage otherwise. MU300_SD_DEV=<card device> names the card
# by hand (the mount helper refuses any card without that label).
set -eu
TOP=$(cd "$(dirname "$0")/.." && pwd)
SD=${MU300_SD_DEV:-}
case $SD in
    '') ;;
    */mmcblk0|*/mmcblk0p*) echo "MU300_SD_DEV=$SD is the internal eMMC, not the SD card" >&2; exit 1 ;;
    /dev/block/mmcblk[1-9]|/dev/block/mmcblk[1-9]p[0-9]|/dev/block/mmcblk[1-9]p[0-9][0-9]) ;;
    *) echo "MU300_SD_DEV=$SD is not an SD card device" >&2; exit 1 ;;
esac
if [ -z "$SD" ]; then
    # the same search reset-password.sh does (tools/storage.sh; this script is English only)
    su_do() { adb shell "su -c '$1'" </dev/null | tr -d '\r'; }
    t() { printf '%s' "$1"; }
    . "$TOP/tools/storage.sh"; sd_probe
    if [ -n "$SD_DEV" ] && [ "$(sd_existing)" = yes ]; then
        SD=$SD_DEV
        echo "SD card $SD (mu300sd): the installation that boots"
    fi
fi
adb shell "su -c '
set -e
X=/data/misc/apexdata/com.android.wifi/WifiConfigStoreSoftAp.xml
R=/data/local/tmp/mu300root
ssid=\$(sed -n \"s/.*<string name=\\\"WifiSsid\\\">&quot;\\(.*\\)&quot;<\\/string>.*/\\1/p; s/.*<string name=\\\"WifiSsid\\\">\\([^&<]*\\)<\\/string>.*/\\1/p\" \$X | head -1)
psk=\$(sed -n \"s/.*<string name=\\\"Passphrase\\\">\\(.*\\)<\\/string>.*/\\1/p\" \$X | head -1 | sed \"s/&amp;/\\&/g; s/&lt;/</g; s/&gt;/>/g; s/&quot;/\\\"/g; s/&apos;/'\"'\"'/g\")
[ -n \"\$ssid\" ] && [ \${#psk} -ge 8 ] || { echo \"no usable SoftAP config in \$X\"; exit 1; }
${SD:+MU300_SD_DEV=$SD }sh /data/local/tmp/android-mount-mu300root.sh \$R >/dev/null
# unmounted again however this ends, also when a step below fails
trap \"sh /data/local/tmp/android-mount-mu300root.sh -u \$R >/dev/null 2>&1\" EXIT
mkdir -p \$R/etc/mu300
umask 077
printf \"SSID=%s\\nPSK=%s\\nBAND=5\\nCHANNEL=auto\\nCOUNTRY=TR\\n\" \"\$ssid\" \"\$psk\" > \$R/etc/mu300/hotspot.conf
chown 0:0 \$R/etc/mu300/hotspot.conf; chmod 600 \$R/etc/mu300/hotspot.conf
sync
echo \"imported SSID \$ssid (passphrase \${#psk} chars) into \$R/etc/mu300/hotspot.conf\"
'" | tr -d '\r'
