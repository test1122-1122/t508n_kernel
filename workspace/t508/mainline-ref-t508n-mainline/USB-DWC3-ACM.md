# T508N mainline 6.18.55 USB: DWC3 + ACM

Status: **working** (2026-10-11, B slot, Windows COM18).
Image: `build/boot-images/boot_mainline_6.18.55_t508n_test28.img` md5 `1771fc80b7ee8441bd60dd68a7417282`.

## Final stack

```
DWC3 25100000.dwc3
  + phy-sprd-ums9620-ssphy (mainline port, drivers/usb/phy)
  + sprd_usb_power.ko      (UMP9620 vddusb33 + DPDM)
  + sprd_dummy_phy.ko      (sprd,usb-dummy-phy USB2 PHY)
  + configfs acm.GS0 only  (no RNDIS)
  + busybox applets + p75 bind mounts
```

## Why MUSB was dropped

The stock MUSB path (`sprd,qogirn6pro-musb`) was brought up with a
minimal mainline glue (`modbuild/musb_sprd_mainline/`). It reached:

- UDC `musb-hdrc.1.auto`
- configfs gadget bound
- `usb0` up

but the host never got past USB descriptor requests:

```
Unknown USB Device (Device Descriptor Request Failed)
VID_0000 & PID_0002
```

The vendor 5.4 MUSB uses a custom SPRD MUSB core + `sprd_musbhsdma`
(FIFO/DMA/EP0 behavior). Porting that to mainline 6.18 is a core-level
port, not glue-level. The DWC3 route below is the MU300-proven path.

## Why DWC3 initially deferred

Stock vendor DTB:

```dts
usb3@25100000 {
    compatible = "sprd,qogirn6pro-dwc3";
    usb-phy = <&dummyphy>, <&ssphy>;
    ...
    dwc3@25100000 {
        compatible = "snps,sprd-dwc3";
        usb-phy = <&dummyphy>, <&ssphy>;
        phy_type = "utmi";
    };
};
dummyphy { compatible = "sprd,usb-dummy-phy"; };
ssphy@25310000 { compatible = "sprd,qogirn6pro-ssphy"; };
```

- DWC3 index 0 (USB2) expects `sprd,usb-dummy-phy`.
- Mainline had no driver for that compatible, so
  `devm_usb_get_phy_by_phandle(..., 0)` returned `-EPROBE_DEFER`
  forever and `dwc3_core_init()` failed with `-EPROBE_DEFER`:

```
devices_deferred:
  25100000.dwc3   dwc3: failed to initialize core
```

`modbuild/sprd_dummy_phy/` registers a no-op legacy USB2 `usb_phy`
for this node.

## PMIC / vddusb33

LK normal boot may turn the UMP9620 `vddusb33` LDO off and leave DPDM
on charger detection. `modbuild/sprd_usb_power/` writes:

```
UMP9620_LDO_VDDUSB33_PD  (0x21e8) bit0 = 0     # enable (inverted)
UMP9620_LDO_VDDUSB33_VOL (0x21ec)    = 0xD2    # 3.3 V
CHGR_DET_FGU_CTRL        (0x23a0) bit14|bit0=1 # DPDM -> USB PHY
```

Readback after init:

```
SPRD_USB_POWER: PD=00000000 VOL=000000d2 DPDM=00004201
```

## Bring-up sequence (initramfs)

```
1. mount /dev/mmcblk0p75 -> /persist
2. insmod sprd_usb_power.ko
3. insmod sprd_dummy_phy.ko
4. wait for /sys/class/udc/25100000.dwc3
5. configfs: idVendor 0x1d6b, idProduct 0x0107, serial T508N-ACM
   functions/acm.GS0, bind UDC
6. /bin/busybox --install -s /bin; export PATH=...
7. mkdir -p /persist/etc /persist/root
   mount --bind /persist/etc /etc
   mount --bind /persist/root /root
8. echo -1 > /sys/module/usbcore/parameters/autosuspend
   echo on > /sys/.../25100000.dwc3/power/control
9. shell loop on /dev/ttyGS0; sleep forever
```

## Verified

```
Windows: USB Composite Device VID_1D6B PID_0107 OK
         USB serial COM18 OK
COM18:   ~ # uname -a         -> Linux (none) 6.18.55 #5 SMP aarch64
         ~ # ls /sys/class/udc -> 25100000.dwc3
         ~ # cat /sys/module/usbcore/parameters/autosuspend -> -1
         ~ # mount | grep -E 'p75|/etc|/root'
              /dev/mmcblk0p75 on /persist type ext4 (rw)
              /dev/mmcblk0p75 on /etc     type ext4 (rw)
              /dev/mmcblk0p75 on /root    type ext4 (rw)
```

`/etc` and `/root` persist across reboot; `/dev/mmcblk0p75` is directly
accessible from mainline at `/persist`.

## Artifacts

```
modbuild/sprd_usb_power/sprd_usb_power.ko
modbuild/sprd_dummy_phy/sprd_dummy_phy.ko
build/boot-images/boot_mainline_6.18.55_t508n_test28.img
```

## Open items

- `mmc1` (22210000.sdio) SDHCI error spam every ~20 s (SD slot unused)
- WLAN / BT / SDIO bring-up
- optional persistent `/etc` init scripts on p75
