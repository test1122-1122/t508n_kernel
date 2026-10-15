#!/bin/bash
# 构建 mainline 最小内核 (T508N/OT5)
set -e
cd "$(dirname "$0")/.."
# 优先用已解压的 arm-gnu-toolchain, 否则用 musl 工具链(内核为 freestanding, 两者皆可)
TC=""
for d in arm-gnu-toolchain-*/bin aarch64-linux-musl-cross/bin; do
  if [ -d "$d" ]; then TC="$PWD/$d"; break; fi
done
export PATH="$TC:$PATH"
CC=$(ls "$TC"/aarch64-*-gcc 2>/dev/null | head -1)
CROSS=$(basename "$CC" | sed 's/gcc$//')
K=linux-6.12.103
cd "$K"
# 宿主编译 certs/extract-cert 需要 openssl 头文件(系统无 libssl-dev, 用本地提取的)
export HOST_EXTRACFLAGS="-I$OLDPWD/work/host-openssl/usr/include -I$OLDPWD/work/host-openssl/usr/include/x86_64-linux-gnu"
export LIBRARY_PATH="$OLDPWD/work/host-libs"
if [ ! -f .config ]; then
  make ARCH=arm64 CROSS_COMPILE=$CROSS defconfig
fi
./scripts/config -e CONFIG_SERIAL_SPRD -e CONFIG_SERIAL_EARLYCON \
  -e CONFIG_SERIAL_SPRD_CONSOLE \
  -e CONFIG_BLK_DEV_INITRD -e CONFIG_RD_GZIP -e CONFIG_RD_LZ4 \
  -e CONFIG_DEVTMPFS -e CONFIG_DEVTMPFS_MOUNT -e CONFIG_PRINTK_TIME \
  -e CONFIG_PSTORE -e CONFIG_PSTORE_RAM -e CONFIG_PSTORE_CONSOLE -e CONFIG_PSTORE_PMSG \
  --set-val CONFIG_LOG_BUF_SHIFT 18 \
  -d CONFIG_RANDOMIZE_BASE   # 关 KASLR 便于调试
make ARCH=arm64 CROSS_COMPILE=$CROSS olddefconfig
make ARCH=arm64 CROSS_COMPILE=$CROSS Image \
     dtbs -j$(nproc) 2>&1 | tail -3
cp arch/arm64/boot/Image ../work/mainline_Image
mkdir -p ../work/dtb
cp arch/arm64/boot/dts/sprd/ums9620.dtsi ../tools/dts/ 2>/dev/null || true
cp ../tools/dts/ums9620-ot5.dts arch/arm64/boot/dts/sprd/
make ARCH=arm64 CROSS_COMPILE=$CROSS \
     sprd/ums9620-ot5.dtb 2>&1 | tail -2
cp arch/arm64/boot/dts/sprd/ums9620-ot5.dtb ../work/ot5.dtb
cd ..
echo "==> Image: work/mainline_Image ; DTB: work/ot5.dtb"
