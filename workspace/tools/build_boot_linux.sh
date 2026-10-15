#!/bin/bash
# 一键: 构建 mainline 内核 + initramfs + 紫光 boot.img + 替换 DTB 的 vendor_boot
set -e
cd "$(dirname "$0")/.."
bash tools/build_kernel.sh
bash tools/build_initramfs.sh
python3 tools/sprd_mkbootimg.py \
    --kernel work/mainline_Image \
    --ramdisk initramfs.cpio \
    --output boot_linux.img
python3 tools/patch_vendor_boot.py \
    Image/vendor_boot_a.img work/ot5.dtb vendor_boot_linux.img
echo "==> 刷入:"
echo "    fastboot flash boot_a boot_linux.img"
echo "    fastboot flash vendor_boot_a vendor_boot_linux.img"
echo "    fastboot reboot"
