#!/bin/bash
# Build the out-of-tree vendor modules (WCN) against the mainline tree built by build.sh.
# Run inside the mu300-mainline-build container: bash /work/build-modules.sh [module-dir...]
set -eo pipefail
K=/src/linux-6.18.52
O=/src/out-6.18.52
mods=${*:-wcn_bsp}
mkdir -p /work/out/modules
# Module.symvers for the built-in exports (pcie-sprd etc.)
make -C $K O=$O ARCH=arm64 -j"$(nproc)" modules > $O/modules.log 2>&1 || { tail -20 $O/modules.log; exit 1; }
extra=
for m in $mods; do
    rm -rf /src/mod-build/$m && mkdir -p /src/mod-build && cp -r /work/modules/$m /src/mod-build/$m
    # wlan/bt use wcn_bsp's exports and its vendor headers (../wcn_bsp/kinclude)
    [ -d /src/mod-build/wcn_bsp ] || cp -r /work/modules/wcn_bsp /src/mod-build/wcn_bsp
    make -C $O ARCH=arm64 M=/src/mod-build/$m KBUILD_EXTRA_SYMBOLS="$extra" -j"$(nproc)" modules 2>&1 | tee /work/out/modules/$m.log
    [ -f /src/mod-build/$m/Module.symvers ] && extra="$extra /src/mod-build/$m/Module.symvers"
    find /src/mod-build/$m -name '*.ko' -exec cp {} /work/out/modules/ \;
done
ls -la /work/out/modules/*.ko
