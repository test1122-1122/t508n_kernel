#!/bin/sh
# MU300 / ZTE F50 Linux installer. Run on a macOS/Linux host with the device booted in rooted Android (adb + su).
#
#   ./install.sh                 interactive install of Ubuntu, OpenWrt or both
#
# Needs: adb, docker, python3, lz4, and the kernel build outputs in $MU300_KERNEL_OUT (default: out/):
#   Image, modules/*.ko, modules.builtin, modules.builtin.modinfo   (see README "Build and run")
# Proprietary vendor files are pulled from *your* device into $MU300_WORK (default: work/) and never leave the host.
set -eu
TOP=$(cd "$(dirname "$0")" && pwd)
KOUT=${MU300_KERNEL_OUT:-$TOP/out}
WORK=${MU300_WORK:-$TOP/work}
OWRT_VER=25.12.5
T=/data/local/tmp

say() { printf '\n==> %s\n' "$*"; }
die() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
ask() { # ask VAR "question" default
    printf '%s [%s]: ' "$2" "$3"; read -r _a; [ -n "$_a" ] || _a=$3; eval "$1=\$_a"; }
# adb shell/exec-out read stdin; never let them eat the answers typed (or piped) into this script
su_do() { adb shell "su -c '$1'" </dev/null | tr -d '\r'; }

# ---------------------------------------------------------------- preflight
say "Checking host tools and device"
for c in adb docker python3 lz4; do command -v $c >/dev/null || die "$c not found"; done
[ -f "$KOUT/Image" ] && ls "$KOUT"/modules/*.ko >/dev/null 2>&1 || die "kernel outputs missing in $KOUT (build the kernel first)"
adb get-state </dev/null >/dev/null 2>&1 || die "no adb device (boot Android, enable USB debugging)"
[ "$(su_do 'id -u')" = 0 ] || die "su does not work on the device"
model="$(su_do 'getprop ro.product.model') / $(su_do 'getprop ro.product.device')"
echo "device: $model"
case "$model" in *MU300*|*F50*|*mu300*) ;; *) ask go "This does not look like a ZTE F50/MU300. Continue anyway? (yes/no)" no; [ "$go" = yes ] || exit 1 ;; esac
[ "$(su_do 'getprop ro.boot.slot_suffix')" = _a ] || die "Android must be running from slot a"

# ---------------------------------------------------------------- free eMMC region
say "Locating free eMMC space after the last partition"
set -- $(su_do 'e=0; for p in /sys/block/mmcblk0/mmcblk0p*; do x=$(( $(cat $p/start) + $(cat $p/size) )); [ $x -gt $e ] && e=$x; done; echo $e $(cat /sys/block/mmcblk0/size)')
last_end=$1; disk=$2
start=$(( (last_end / 4096 + 1) * 4096 ))
end=$(( ((disk - 34) / 4096 - 1) * 4096 ))
OFF=$((start * 512)); SIZE=$(( (end - start) * 512 ))
[ $SIZE -gt $((4 * 1024 * 1024 * 1024)) ] || die "less than 4 GiB of unpartitioned space ($((SIZE / 1048576)) MiB); nothing is changed"
# an existing installation defines the region (it may have been created with a slightly different size)
existing=no
for cand in $OFF 27762098176; do
    m=$(su_do "dd if=/dev/block/mmcblk0 bs=1 skip=$((cand + 1080)) count=2 2>/dev/null | od -An -tx1" | tr -d ' ')
    l=$(su_do "dd if=/dev/block/mmcblk0 bs=1 skip=$((cand + 1144)) count=16 2>/dev/null" | tr -d '\000')
    if [ "$m" = 53ef ] && [ "$l" = mu300root ]; then
        blocks=$(su_do "dd if=/dev/block/mmcblk0 bs=1 skip=$((cand + 1028)) count=4 2>/dev/null | od -An -tu4" | tr -d ' ')
        OFF=$cand; SIZE=$((blocks * 4096)); existing=yes; break
    fi
done
echo "last partition ends at sector $last_end, disk has $disk sectors"
echo "Linux region: offset $OFF, $((SIZE / 1048576)) MiB, existing mu300root filesystem: $existing"

# ---------------------------------------------------------------- choices
say "What should be installed?"
echo "  1) Ubuntu 26.04 LTS (full distribution, apt, ~500 MiB RAM in use)"
echo "  2) OpenWrt $OWRT_VER (router, LuCI web UI, ~140 MiB RAM in use)"
echo "  3) both (switch later with: mu300-os ubuntu|openwrt)"
ask choice "Choice" 3
case $choice in 1) OSES=ubuntu ;; 2) OSES=openwrt ;; 3) OSES="ubuntu openwrt" ;; *) die "invalid choice" ;; esac
BOOT_OS=${OSES%% *}
[ "$choice" = 3 ] && { ask BOOT_OS "Which one should boot (ubuntu/openwrt)" ubuntu; case $BOOT_OS in ubuntu|openwrt) ;; *) die "invalid system" ;; esac; }
ask dl "Boot Linux by default instead of Android (falls back to Android if Linux fails)? (yes/no)" yes
DEFAULT_LINUX=0; [ "$dl" = yes ] && DEFAULT_LINUX=1
ask hs "Copy Android's hotspot name and password to Linux? (yes/no)" yes
IMPORT_HOTSPOT=0; [ "$hs" = yes ] && IMPORT_HOTSPOT=1
ask gpu "Include the Mali GPU (OpenCL) userspace (~90 MiB)? (yes/no)" yes
FORMAT=0; WIPE_LEGACY=0
if [ $existing = no ]; then
    FORMAT=1
else
    ask fmt "Keep the existing Linux filesystem (other installed systems stay)? (yes = keep / format)" yes
    [ "$fmt" = format ] && FORMAT=1
    # a new /ubuntu replaces an Ubuntu installed directly in the filesystem root (first-generation layout)
    case " $OSES " in *" ubuntu "*) [ $FORMAT = 0 ] && WIPE_LEGACY=1 ;; esac
fi
printf 'Password for the "ubuntu" user (Ubuntu) and "root" (OpenWrt): '
[ -t 0 ] && stty -echo; read -r pw1; printf '\nRepeat: '; read -r pw2; [ -t 0 ] && stty echo; echo
[ "$pw1" = "$pw2" ] && [ ${#pw1} -ge 6 ] || die "passwords differ or are shorter than 6 characters"

# ---------------------------------------------------------------- pull vendor data from the device
mkdir -p "$WORK/dumps" "$WORK/firmware"
say "Pulling device data into $WORK (stays on this computer)"
adb exec-out "su -c 'cat /dev/block/by-name/boot_a'" </dev/null > "$WORK/dumps/boot_a.img"
adb exec-out "su -c 'dd if=/dev/block/by-name/misc bs=4096 count=1 2>/dev/null'" </dev/null > "$WORK/dumps/misc-head.bin"
[ -d "$WORK/android-subset" ] || sh "$TOP/android-vendor/extract-subset.sh" "$WORK/android-subset"
for f in wcnmodem.bin gnssmodem.bin wifi_board_config.ini wifi_board_config_ab.ini bt_configure_pskey.ini bt_configure_rf.ini; do
    for d in /odm/firmware /vendor/firmware /vendor/etc; do
        if [ "$(su_do "[ -f $d/$f ] && echo y")" = y ]; then adb exec-out "su -c 'cat $d/$f'" </dev/null > "$WORK/firmware/$f"; break; fi
    done
done
if [ "$gpu" = yes ] && [ ! -d "$WORK/android-gpu-subset" ]; then
    sh "$TOP/android-vendor/extract-gpu-subset.sh" "$WORK/android-gpu-subset"
fi

# ---------------------------------------------------------------- build
say "Building helper binaries"
mkdir -p "$WORK/out" "$WORK/tools/logdw" "$WORK/tools/bt-init" "$WORK/tools/gpu"
rm -rf "$WORK/out/modules" && cp -R "$KOUT/modules" "$WORK/out/modules"
cp "$KOUT/modules.builtin" "$KOUT/modules.builtin.modinfo" "$WORK/out/" 2>/dev/null || true
docker build -q -t mu300-ubuntu:26.04 "$TOP/rootfs" >/dev/null
docker run --rm mu300-ubuntu:26.04 cat /bin/busybox > "$WORK/busybox"; chmod +x "$WORK/busybox"
docker run --rm -v "$TOP/tools":/src:ro -v "$WORK/tools":/o mu300-kbuild sh -c '
  gcc -O2 -static -o /o/logdw/logdw /src/logdw/logdw.c &&
  gcc -O2 -static -o /o/bt-init/mu300-bt-init /src/bt-init/mu300-bt-init.c'
if [ -d "$WORK/android-gpu-subset" ]; then
    L=$(mktemp -d "$WORK/cllibs.XXXX")
    cp "$WORK/android-gpu-subset/vendor/lib64/libOpenCL.so" "$WORK/android-subset/apex/com.android.runtime/lib64/bionic/libc.so" \
       "$WORK/android-subset/apex/com.android.runtime/lib64/bionic/libdl.so" "$L/"
    docker run --rm -v "$TOP/tools/gpu":/w -v "$L":/l:ro -v "$WORK/tools/gpu":/o mu300-kbuild sh -c \
      'ln -sf /usr/bin/clang-12 /usr/local/bin/clang; cp -r /w /tmp/gpu && sh /tmp/gpu/build.sh /l && cp /tmp/gpu/cltest /o/'
    rm -rf "$L"
fi

# MU300_REUSE_BUILD=1 keeps rootfs tarballs from an earlier run of this script
reuse() { [ "${MU300_REUSE_BUILD:-0}" = 1 ] && [ -s "$WORK/mu300-$1.tar.gz" ] && echo "reusing $WORK/mu300-$1.tar.gz"; }
case " $OSES " in *" ubuntu "*) reuse ubuntu || {
    say "Building the Ubuntu root filesystem"
    B=$(mktemp -d "$WORK/ubuntu-build.XXXX")
    tar -C "$TOP/rootfs" --exclude ./base.tar --exclude './*.tar.gz' -cf - . | tar -xf - -C "$B"
    cid=$(docker create mu300-ubuntu:26.04 /bin/true); docker export "$cid" > "$B/base.tar"; docker rm "$cid" >/dev/null
    gpuargs=""
    [ -d "$WORK/android-gpu-subset" ] && gpuargs="-v $WORK/android-gpu-subset:/android-gpu-subset:ro -v $WORK/tools/gpu/cltest:/cltest:ro"
    # shellcheck disable=SC2086
    docker run --rm -v "$B":/w -v "$WORK/out/modules":/kmods:ro -v "$WORK/out":/kout:ro -v "$WORK/firmware":/firmware:ro \
      -v "$WORK/android-subset":/android-subset:ro -v "$WORK/tools/logdw/logdw":/logdw:ro \
      -v "$WORK/tools/bt-init/mu300-bt-init":/bt-init:ro $gpuargs mu300-ubuntu:26.04 bash /w/assemble.sh >/dev/null
    mv "$B/mu300-ubuntu-26.04-rootfs.tar.gz" "$WORK/mu300-ubuntu.tar.gz"; rm -rf "$B"; } ;;
esac
case " $OSES " in *" openwrt "*) reuse openwrt || {
    say "Building the OpenWrt root filesystem"
    MU300_INPUTS="$WORK" sh "$TOP/openwrt/build-rootfs.sh" mu300-openwrt-rootfs.tar.gz >/dev/null
    mv "$TOP/openwrt/mu300-openwrt-rootfs.tar.gz" "$WORK/mu300-openwrt.tar.gz"; } ;;
esac

say "Building the boot image"
sed "s/^ROOT_OFFSET=[0-9]*/ROOT_OFFSET=$OFF/" "$TOP/boot/init" > "$WORK/init"
python3 "$TOP/boot/build-boot-image.py" --stock-boot "$WORK/dumps/boot_a.img" --misc-head "$WORK/dumps/misc-head.bin" \
  --kernel "$KOUT/Image" --modules "$WORK/out/modules" --init "$WORK/init" --busybox "$WORK/busybox" \
  --logdw "$WORK/tools/logdw/logdw" --ueventd-perms "$TOP/android-vendor/ueventd-perms.sh" \
  --android-subset "$WORK/android-subset" --out "$WORK/boot-linux-slotb.img" >/dev/null
PWHASH=$(printf '%s' "$pw1" | docker run --rm -i mu300-ubuntu:26.04 openssl passwd -6 -stdin)

# ---------------------------------------------------------------- confirm and install
say "Ready to install"
echo "  systems:        $OSES (boots: $BOOT_OS)"
echo "  default boot:   $([ $DEFAULT_LINUX = 1 ] && echo Linux || echo Android, Linux on demand)"
echo "  filesystem:     $([ $FORMAT = 1 ] && echo "CREATE new ext4 (erases the Linux region)" || echo "keep existing")"
[ $WIPE_LEGACY = 1 ] && echo "  note:           the chosen systems are installed fresh; their previous files and settings are replaced"
echo "  writes:         Linux region at offset $OFF, boot_b, 32 bytes of misc (boot_a, GPT and userdata are not touched)"
ask confirm "Type INSTALL to continue" no
[ "$confirm" = INSTALL ] || die "cancelled"

say "Copying to the device"
adb push "$TOP/tools/android-mount-mu300root.sh" "$TOP/tools/android-install.sh" $T/ >/dev/null
for os in $OSES; do adb push "$WORK/mu300-$os.tar.gz" $T/mu300-$os.tar.gz >/dev/null; done
env=$(mktemp)
printf 'OFF=%s\nSIZE=%s\nOFF_S=%s\nSIZE_S=%s\nFORMAT=%s\nOSES="%s"\nWIPE_LEGACY=%s\nBOOT_OS=%s\nDEFAULT_LINUX=%s\nIMPORT_HOTSPOT=%s\nPWHASH='"'"'%s'"'"'\n' \
  "$OFF" "$SIZE" "$((OFF / 512))" "$((SIZE / 512))" "$FORMAT" "$OSES" "$WIPE_LEGACY" "$BOOT_OS" "$DEFAULT_LINUX" "$IMPORT_HOTSPOT" "$PWHASH" > "$env"
adb push "$env" $T/mu300-install.env >/dev/null; rm -f "$env"
su_do "sh $T/android-install.sh" | tee "$WORK/device-install.log"
grep -q MU300-INSTALL-OK "$WORK/device-install.log" || die "installation on the device failed; boot_b and misc were not changed"

say "Writing boot_b and arming slot b"
EXP=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['sha256'])" "$WORK/boot-linux-slotb.json")
adb push "$WORK/boot-linux-slotb.img" $T/mu300-boot.img >/dev/null
adb push "$WORK/boot-linux-slotb.misc-slot-b-trial.bin" $T/mu300-bc-b.bin >/dev/null
[ "$(su_do "sha256sum $T/mu300-boot.img" | cut -d' ' -f1)" = "$EXP" ] || die "pushed boot image hash mismatch"
su_do "dd if=$T/mu300-boot.img of=/dev/block/by-name/boot_b bs=4M && sync"
[ "$(su_do 'sha256sum /dev/block/by-name/boot_b' | cut -d' ' -f1)" = "$EXP" ] || die "boot_b verify failed (slot a still active, Android keeps booting)"
su_do "dd if=$T/mu300-bc-b.bin of=/dev/block/by-name/misc bs=1 seek=2048 conv=notrunc && sync && rm $T/mu300-boot.img $T/mu300-bc-b.bin"

say "Done. Rebooting into $BOOT_OS"
echo "  USB network: 192.168.77.1   SSH: $([ "$BOOT_OS" = ubuntu ] && echo ubuntu@192.168.77.1 || echo root@192.168.77.1, LuCI http://192.168.77.1)"
echo "  switch systems: mu300-os ubuntu|openwrt   back to Android: mu300-next-boot android"
adb reboot </dev/null
