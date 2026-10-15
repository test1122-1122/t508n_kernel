#!/system/bin/sh
# Device side of install.sh (runs as root on Android). Settings come from /data/local/tmp/mu300-install.env:
#   OFF SIZE           free eMMC region (bytes) after the last GPT partition, as strings
#   OFF_S SIZE_S       the same in 512-byte sectors (Android's mksh has 32-bit arithmetic: never compute with bytes)
#   FORMAT=0|1         create the ext4 filesystem "mu300root" in that region
#   OSES="ubuntu openwrt"  systems to (re)install from /data/local/tmp/mu300-<os>.tar.gz
#   WIPE_LEGACY=0|1    remove a first-generation Ubuntu that lives directly in the filesystem root
#   BOOT_OS            system started by the initramfs
#   DEFAULT_LINUX=0|1  keep booting Linux (otherwise every Linux boot is one-shot and returns to Android)
#   PWHASH             SHA-512 crypt hash for the "ubuntu" (Ubuntu) and "root" (OpenWrt) accounts
#   IMPORT_HOTSPOT=0|1 copy Android's hotspot SSID/passphrase into each system
set -e
T=/data/local/tmp
. $T/mu300-install.env
M=$T/mu300root
say() { echo "[device] $*"; }

# --- the region must not overlap any partition (checked again here, on the device itself)
end=0
for p in /sys/block/mmcblk0/mmcblk0p*; do
    e=$(( $(cat $p/start) + $(cat $p/size) ))
    [ $e -gt $end ] && end=$e
done
disk=$(cat /sys/block/mmcblk0/size)
[ "$OFF_S" -ge $end ] && [ $((OFF_S + SIZE_S)) -le $((disk - 34)) ] || { say "region overlaps partitions or the backup GPT"; exit 1; }

attach() {
    for o in /sys/block/loop*/loop/offset; do
        [ "$(cat $o 2>/dev/null)" = "$OFF" ] && { say "region already attached (${o%/loop/offset})"; exit 1; }
    done
    f=$(losetup -f 2>&1 | grep -o '/dev/block/loop[0-9]*' | head -1); n=${f##*loop}
    L=/dev/block/loop$n
    [ -b "$L" ] || mknod "$L" b $(cut -d: -f1 /sys/block/loop$n/dev) $(cut -d: -f2 /sys/block/loop$n/dev) 2>/dev/null || [ -b "$L" ]
    losetup -o $OFF -S $SIZE "$L" /dev/block/mmcblk0
    [ "$(cat /sys/block/loop$n/loop/offset)" = "$OFF" ] && [ "$(blockdev --getsize64 $L)" = "$SIZE" ] || { losetup -d $L; say "loop setup mismatch"; exit 1; }
}

sb() { dd if=/dev/block/mmcblk0 bs=512 skip=$OFF_S count=4 2>/dev/null | dd bs=1 skip=$1 count=$2 2>/dev/null; }
magic=$(sb 1080 2 | od -An -tx1 | tr -d ' \n')
label=$(sb 1144 16 | tr -d '\000')
if [ "$FORMAT" = 1 ]; then
    if [ "$magic" = 53ef ] && [ "$label" != mu300root ]; then say "refusing to format: foreign ext4 ($label) in the region"; exit 1; fi
    attach
    say "creating ext4 mu300root on $L ($((SIZE_S / 2048)) MiB)"
    mke2fs -t ext4 -L mu300root -F "$L" >/dev/null
    losetup -d "$L"
elif [ "$magic" != 53ef ] || [ "$label" != mu300root ]; then
    say "no mu300root filesystem in the region (run with FORMAT=1)"; exit 1
fi

MU300_OFF=$OFF MU300_SIZE=$SIZE sh $T/android-mount-mu300root.sh $M
trap 'sync; sh $T/android-mount-mu300root.sh -u $M >/dev/null 2>&1; true' EXIT

if [ "$WIPE_LEGACY" = 1 ] && { [ -x $M/lib/systemd/systemd ] || [ -L $M/lib ]; }; then
    say "removing the root-level Ubuntu"
    for e in $M/* $M/.[!.]*; do
        case "${e##*/}" in lost+found|.mu300|ubuntu|openwrt) ;; *) rm -rf "$e" ;; esac
    done
fi

ssid=; psk=
if [ "$IMPORT_HOTSPOT" = 1 ]; then
    X=/data/misc/apexdata/com.android.wifi/WifiConfigStoreSoftAp.xml
    ssid=$(sed -n 's/.*<string name="WifiSsid">&quot;\(.*\)&quot;<\/string>.*/\1/p; s/.*<string name="WifiSsid">\([^&<]*\)<\/string>.*/\1/p' $X 2>/dev/null | head -1)
    psk=$(sed -n 's/.*<string name="Passphrase">\(.*\)<\/string>.*/\1/p' $X 2>/dev/null | head -1 | sed "s/&amp;/\&/g; s/&lt;/</g; s/&gt;/>/g; s/&quot;/\"/g; s/&apos;/'/g")
    [ -n "$ssid" ] && [ ${#psk} -ge 8 ] || { say "no usable Android hotspot config, a random password will be generated"; ssid=; psk=; }
fi

for os in $OSES; do
    tarball=$T/mu300-$os.tar.gz
    [ -f $tarball ] || { say "missing $tarball"; exit 1; }
    say "installing $os"
    rm -rf $M/$os.new && mkdir $M/$os.new
    tar -xzpf $tarball -C $M/$os.new
    rm -rf $M/$os && mv $M/$os.new $M/$os
    R=$M/$os
    mkdir -p $R/etc/mu300
    if [ -n "$ssid" ]; then
        umask 077
        printf 'SSID=%s\nPSK=%s\nBAND=5\nCHANNEL=auto\nCOUNTRY=TR\n' "$ssid" "$psk" > $R/etc/mu300/hotspot.conf
        chmod 600 $R/etc/mu300/hotspot.conf
        umask 022
    fi
    if [ "$DEFAULT_LINUX" = 1 ]; then echo linux > $R/etc/mu300/default-boot; else rm -f $R/etc/mu300/default-boot; fi
    if [ -n "$PWHASH" ]; then
        case $os in
            ubuntu) sed -i "s|^ubuntu:[^:]*:|ubuntu:$PWHASH:|" $R/etc/shadow ;;
            openwrt) sed -i "s|^root:[^:]*:|root:$PWHASH:|" $R/etc/shadow ;;
        esac
    fi
    rm -f $tarball
done
mkdir -p $M/.mu300
echo "$BOOT_OS" > $M/.mu300/boot-os
[ -n "$ssid" ] && say "hotspot: SSID $ssid imported (passphrase ${#psk} chars)"
say "installed: $(ls -d $M/ubuntu $M/openwrt 2>/dev/null | sed "s|$M/||g" | tr '\n' ' ')boot-os=$BOOT_OS default-linux=$DEFAULT_LINUX"
rm -f $T/mu300-install.env
echo MU300-INSTALL-OK   # install.sh checks for this line (set -e stops before it on any failure)
