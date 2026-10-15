#!/bin/bash
# T508N mainline build wrapper.
# Reuses the MU300 mainline 6.18 patches/modules/config, but builds the T508N
# reference DTB. The real boot path should use the T508N stock vendor DTB.
#
# usage:
#   CROSS_COMPILE=aarch64-none-linux-gnu- KV=6.18.55 ./tools/build-mainline.sh
set -euo pipefail

TOP=$(cd "$(dirname "$0")/.." && pwd)
MU=${MU300_DIR:-$(cd "$TOP/../mu300-linux" && pwd)}
KV=${KV:-6.18.55}
SRC=${SRC:-/src}
OUT=${OUT:-$TOP/out}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-none-linux-gnu-}
MAKE_OPTS=(-j"$(nproc)")

mkdir -p "$SRC" "$OUT"

# fetch kernel
if [ ! -d "$SRC/linux-$KV" ]; then
  base="${KERNEL_BASE_URL:-https://cdn.kernel.org/pub/linux/kernel/v${KV%%.*}.x}"
  cd "$SRC"
  if [ ! -f "linux-$KV.tar.xz" ]; then
    curl -fSL --retry 3 -o "linux-$KV.tar.xz" "$base/linux-$KV.tar.xz"
  fi
  xz -t "linux-$KV.tar.xz"
  tar -xJf "linux-$KV.tar.xz"
  rm -f "linux-$KV.tar.xz"
fi

KDIR="$SRC/linux-$KV"
cd "$KDIR"

# MU300 port + patches
python3 "$MU/upstream/port/install.py" .
for p in "$MU"/upstream/patches/*.patch "$MU"/upstream/patches/"$(echo "$KV" | cut -d. -f1,2)"/*.patch; do
  [ -e "$p" ] || continue
  if patch -p1 -N -s --dry-run < "$p" >/dev/null 2>&1; then patch -p1 -N -s < "$p"
  elif patch -p1 -R -s -f --dry-run < "$p" >/dev/null 2>&1; then :
  else echo "patch does not apply to $KV: $p" >&2; exit 1; fi
done

O="$SRC/out-$KV"
mkdir -p "$O"
make O="$O" ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" allnoconfig >/dev/null
"$KDIR/scripts/kconfig/merge_config.sh" -m -O "$O" "$O/.config" "$TOP/config/t508n-mainline.config" >/dev/null
make O="$O" ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" olddefconfig >/dev/null
make O="$O" ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" "${MAKE_OPTS[@]}" Image

# guard against MU300-only symbols removed for T508N
for sym in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCIEPORTBUS CONFIG_PCIE_SPRD CONFIG_CHARGER_BQ256XX; do
  if grep -q "^$sym=" "$TOP/config/t508n-mainline.config"; then
    echo "T508N config still contains $sym" >&2
    exit 1
  fi
done

# T508N reference DTB
cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
  -I "$KDIR/include" -I "$KDIR/scripts/dtc/include-prefixes" \
  "$TOP/dts/ums9620-ot5.dts" | \
  "$O/scripts/dtc/dtc" -I dts -O dtb -o "$OUT/ums9620-ot5.dtb" -

cp "$O/arch/arm64/boot/Image" "$OUT/Image"
python3 "$TOP/tools/wrap-image.py" "$OUT/Image" "$OUT/Image.lk"
cp "$O/modules.builtin" "$O/modules.builtin.modinfo" "$OUT/" 2>/dev/null || true
echo "T508N mainline build output: $OUT"
