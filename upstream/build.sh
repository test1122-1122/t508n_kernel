#!/bin/bash
# Build mainline for the MU300 inside mu300-mainline-build: Image + DTB. /work = this directory, /src = kernel tree volume
set -eo pipefail
KV=${KV:-6.18.52}
cd /src/linux-$KV
# MU300 patches (idempotent); MU300_PROBE_STAGE=N adds the boot-stage reset probe (debug)
python3 /work/port/install.py .
[ -n "${MU300_PROBE_STAGE:-}" ] && python3 /work/debug/install-probe.py . "$MU300_PROBE_STAGE"
for p in /work/patches/*.patch; do patch -p1 -N -s --dry-run < "$p" >/dev/null 2>&1 && patch -p1 -N -s < "$p"; done
O=/src/out-$KV
mkdir -p $O
make O=$O ARCH=arm64 allnoconfig >/dev/null
./scripts/kconfig/merge_config.sh -m -O $O $O/.config /work/mu300-mainline.config >/dev/null
make O=$O ARCH=arm64 olddefconfig >/dev/null
# report options Kconfig did not take
while IFS= read -r l; do
  case "$l" in CONFIG_*=*) k=${l%%=*}; v=${l#*=}; g=$(grep -E "^$k=" $O/.config | cut -d= -f2- || true);
    [ "$v" = n ] && { grep -q "^$k=y" $O/.config && echo "NOT DISABLED: $k" || true; continue; }
    [ "$g" = "$v" ] || echo "NOT SET: $k want $v got ${g:-unset}";; esac
done < /work/mu300-mainline.config
make O=$O ARCH=arm64 -j"$(nproc)" Image > $O/build.log 2>&1 || { grep -E "error|Error|ERROR" $O/build.log | head -20; exit 1; }
cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp -I include -I scripts/dtc/include-prefixes \
  /work/dts/ums9620-mu300.dts | dtc -I dts -O dtb -o $O/ums9620-mu300.dtb -
cp $O/arch/arm64/boot/Image $O/ums9620-mu300.dtb /work/out/
ls -la /work/out
