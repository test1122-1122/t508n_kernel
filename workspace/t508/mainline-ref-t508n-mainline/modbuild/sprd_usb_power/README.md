# sprd_usb_power.ko

Minimal UMP9620 PMIC helper for T508N mainline USB bring-up.

Writes:

- `UMP9620_LDO_VDDUSB33_PD` (0x21e8) bit0 = 0  (enable, inverted)
- `UMP9620_LDO_VDDUSB33_VOL` (0x21ec) = 0xD2   (3.3 V)
- `CHGR_DET_FGU_CTRL` (0x23a0) bit14 | bit0 = 1 (DPDM -> USB PHY)

Build:

```sh
make -C ../../src/out-6.18.55 M=$PWD ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
```

Use: `insmod sprd_usb_power.ko` before DWC3 retry.

See `../../USB-DWC3-ACM.md`.
