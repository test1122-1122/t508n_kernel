#!/bin/bash
# 构建静态 busybox initramfs (aarch64)
set -e
cd "$(dirname "$0")/.."
TC=$(ls -d arm-gnu-toolchain-*/bin | head -1)
export PATH="$PWD/$TC:$PATH"
BB=busybox-1.36.1
if [ ! -f "$BB/.config" ]; then
  cd "$BB" && make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig && cd ..
fi
cd "$BB"
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- oldconfig </dev/null >/dev/null
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- -j$(nproc) >/dev/null
cd ..
# 组装 initramfs cpio
rm -rf work/initramfs/out && mkdir -p work/initramfs/out/{bin,dev,proc,sys,tmp,root}
cp "$BB/busybox" work/initramfs/out/bin/
cd work/initramfs/out
ln -sf busybox bin/sh
for c in ls cat mount umount mdev dmesg; do ln -sf busybox bin/$c; done
cp ../init ./
chmod +x init
find . | cpio -o -H newc > ../../../initramfs.cpio 2>/dev/null
cd ../..
ls -la initramfs.cpio
