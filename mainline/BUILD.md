# Build

## Kernel

```sh
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-
make O=out-6.18.55 defconfig
# merge mainline/kernel/config.fragment, then:
make O=out-6.18.55 olddefconfig
make O=out-6.18.55 -j$(nproc) Image
python3 tools/wrap-image.py out-6.18.55/arch/arm64/boot/Image out-6.18.55/Image.lk
```

Apply `mainline/kernel/mu300-port` and `mainline/kernel/overlay` to a
6.18.55 tree before building.

## Modules

Build `wcn_bsp` first, then modules that depend on its symbols:

```sh
make -C out-6.18.55 M=$PWD/modules/wcn_bsp \
  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

make -C out-6.18.55 M=$PWD/modules/sprd_wlan_combo \
  KBUILD_EXTRA_SYMBOLS=$PWD/modules/wcn_bsp/Module.symvers \
  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

make -C out-6.18.55 M=$PWD/modules/sprdbt_tty \
  KBUILD_EXTRA_SYMBOLS=$PWD/modules/wcn_bsp/Module.symvers \
  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
```

Other modules are independent unless their README says otherwise.

## Boot image

```sh
python3 tools/build_boot_v50align.py \
  --stock-boot stock-boot_b.img \
  --kernel out-6.18.55/Image.lk \
  --ramdisk-cpio initramfs.cpio \
  --out boot_mainline.img
```
