# sprd_dummy_phy.ko

Minimal legacy USB2 PHY for the stock DTB node:

```dts
dummyphy {
    compatible = "sprd,usb-dummy-phy";
    status = "okay";
};
```

DWC3 uses `usb-phy = <&dummyphy>, <&ssphy>`.  Without this driver the
first `devm_usb_get_phy_by_phandle()` defers forever and
`dwc3_core_init()` returns `-EPROBE_DEFER`.

Registers a no-op `usb_phy` of type `USB_PHY_TYPE_USB2`.

Build:

```sh
make -C ../../src/out-6.18.55 M=$PWD ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
```

See `../../USB-DWC3-ACM.md`.
