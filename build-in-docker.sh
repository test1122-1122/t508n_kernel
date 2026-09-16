#!/bin/bash
# Runs inside debian:trixie with this directory mounted at /w. Builds sprdwl_ng.ko for each
# driver source tree given as argument (e.g. build-patched) against the
# DietPi 6.18.45 headers, and xz-compresses the result next to it.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
# DietPi kernel headers matching the board's running kernel. Get the current
# filename + sha256 from https://dietpi.com/apt/dists/all/orangepizero2w/binary-arm64/Packages
HDR_DEB=${HDR_DEB:-linux-headers-current-sunxi64_26.08.0-trunk-dietpi3_arm64.deb}
HDR_SHA=${HDR_SHA:-cce68538e7a6f4a4d1746e434e77edae2ac05d29b07e2f237718d4cc44648c13}
KVER=${KVER:-6.18.45-current-sunxi64}
H=/w/hdr/usr/src/linux-headers-$KVER
KM="ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-"

if ! command -v aarch64-linux-gnu-gcc >/dev/null; then
	apt-get update -qq >/dev/null
	apt-get install -y -qq --no-install-recommends make gcc gcc-aarch64-linux-gnu \
		libc6-dev-arm64-cross flex bison bc libssl-dev libelf-dev kmod xz-utils \
		git curl ca-certificates dpkg binutils-aarch64-linux-gnu >/dev/null
fi
if [ ! -d "$H" ]; then
	[ -f /w/hdr.deb ] || curl -fsSL -o /w/hdr.deb "https://dietpi.com/apt/pool/$HDR_DEB"
	echo "$HDR_SHA  /w/hdr.deb" | sha256sum -c
	mkdir -p /w/hdr && dpkg-deb -x /w/hdr.deb /w/hdr
fi
echo "host gcc: $(gcc --version | head -1)"
echo "cross gcc: $(aarch64-linux-gnu-gcc --version | head -1)"

# kbuild host tools (fixdep, modpost). syncconfig only regenerates autoconf.h
# from the shipped .config, which is what we want.
if [ ! -x $H/scripts/mod/modpost ]; then
	# Same sequence as the Armbian headers postinst (minus resolve_btfids).
	make -C $H $KM -s olddefconfig
	make -C $H $KM -s -j"$(nproc)" scripts
	make -C $H $KM -s -j"$(nproc)" M=scripts/mod
	if [ -f $H/include/generated/.armbian-build.tar.gz ]; then
		tar -C $H -xzf $H/include/generated/.armbian-build.tar.gz
	fi
fi
ls -la $H/scripts/basic/fixdep $H/scripts/mod/modpost | awk '{print $5, $9}'

for T in "$@"; do
	D=/w/$T
	echo "=== building $T"
	rm -f $D/unisocwifi/sprdwl_ng.ko $D/unisocwifi/sprdwl_ng.ko.xz
	make -C $H $KM M=$D/unisocwifi UNISOC_BSP_INCLUDE=$D/unisocwcn/include -j"$(nproc)" modules \
		> /w/$T.log 2>&1 || { echo "BUILD FAILED, see $T.log"; grep -E "error|Error" /w/$T.log | head; exit 1; }
	grep -E "warning:|undefined!" /w/$T.log | grep -v "frame size" | head -5 || true
	aarch64-linux-gnu-strip --strip-debug $D/unisocwifi/sprdwl_ng.ko
	xz --check=crc32 --lzma2=dict=1MiB -k -f $D/unisocwifi/sprdwl_ng.ko  # same as scripts/Makefile.modinst; CONFIG_MODULE_DECOMPRESS rejects CRC64 with EINVAL
	modinfo $D/unisocwifi/sprdwl_ng.ko | grep -E "^(vermagic|depends)"
	ls -la $D/unisocwifi/sprdwl_ng.ko $D/unisocwifi/sprdwl_ng.ko.xz | awk '{print $5, $9}'
done
