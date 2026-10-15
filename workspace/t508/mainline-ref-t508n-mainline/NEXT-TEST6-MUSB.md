# T508N mainline 6.18.55 test6 — MUSB bring-up plan

> ⚠️ **2026-10-11: MUSB plan abandoned.** The MUSB glue reached
> UDC/gadget/usb0, but the host stayed at "device descriptor request
> failed" because the vendor 5.4 SPRD MUSB core/DMA EP0 behavior is not
> in mainline 6.18. The working route is DWC3 + SSPHY + ACM:
> `USB-DWC3-ACM.md` and
> `../../docs/reports/REPORT-USB-MAINLINE-DWC3-ACM-SUCCESS.md`.
> Keep this file only as historical evidence of the MUSB attempt.


> Date: 2026-10-11
> Goal: make B-slot mainline 6.18.55 observable over USB by using the
> stock T508N USB2 channel (MUSB), not DWC3.

## 1. Evidence from test5

`/persist/mainline-test5-dmesg.txt`:

```text
Linux version 6.18.55
mmc0: new HS400 Enhanced strobe MMC card
mmcblk0: p1 ... p75
EXT4-fs (mmcblk0p75): mounted filesystem r/w
MAINLINE_TEST5_PERSIST_MOUNTED
platform 25100000.dwc3: deferred probe pending: dwc3: failed to initialize core
sprd-ssphy 25310000.ssphy: UMS9620 USB 3.1 PHY registered
```

Conclusion:

```text
mainline 6.18.55 boots to userspace
eMMC / p75 work
USB fails because DWC3 core cannot initialize
```

## 2. Correct USB path

Stock T508N DTS:

```dts
usb@a00000 {
    compatible = "sprd,qogirn6pro-musb";
    reg = <0xa00000 0x2000>;
    interrupts = <0 8 4>;
    interrupt-names = "mc";
    clocks = <&aonapb_gate 0x1a>;
    clock-names = "core_clk";
    usb31pllv_frc_on = <&...>;
    usb-phy = <&hsphy>;
    phy-names = "usb";
    dr-mode = "peripheral";
    multipoint = "true";
    wakeup-source;
};
```

Stock adb uses this MUSB controller. DWC3 is SS-only and disabled in
the T508N stock DTS.

## 3. Missing mainline pieces

Mainline 6.18.55 has:

```text
drivers/usb/musb/musb_core.c
drivers/usb/musb/musb_gadget.c
drivers/usb/musb/musb_host.c
```

but no SPRD glue.

Existing vendor source:

```text
src/vendor-kernel-ums9620/drivers/usb/musb/musb_sprd.c   (1974 lines)
```

It depends on vendor-only frameworks:

```text
sprd_usbm
sprd_commonphy
sprd_typec
sprd_musbhsdma
sprd_usbpinmux
```

Mainline cannot use it as-is.

## 4. test6 MUSB design

Write a minimal mainline glue:

```text
drivers/usb/musb/musb_sprd_mainline.c
```

Responsibilities:

1. Match `sprd,qogirn6pro-musb`.
2. Map controller registers / IRQ.
3. Get `core_clk`.
4. Get hsphy via `devm_phy_get(dev, "usb")` or skip initially.
5. Fill `struct musb_hdrc_platform_data`:
   - mode = MUSB_PERIPHERAL
   - config = sprd_musb_hdrc_config (copy FIFO config from vendor)
   - platform_ops = minimal SPRD ops
6. Register a `musb-hdrc` platform device.
7. Force peripheral:
   - ignore extcon
   - set vbus_active = 1
   - `.set_vbus` returns 0
   - `.set_mode` accepts peripheral
8. Optional first step: no PHY driver, rely on LK/PHY defaults.
9. If needed, add minimal `sprd,qogirn6pro-phy` driver later.

## 5. Build steps

1. Add `CONFIG_USB_MUSB_SPRD`.
2. Add source to `drivers/usb/musb/Makefile`.
3. Build against 6.18.55 out tree:
   ```sh
   make -C src/out-6.18.55 M=drivers/usb/musb modules
   ```
4. If module symbols are not exported, build glue into vmlinux instead.
5. Rebuild `Image` / `Image.lk` if built-in.

## 6. test6 initramfs

Reuse test4 USB gadget init with changes:

```text
- load musb_hdrc + musb_sprd module if modules
- wait for /sys/class/udc
- configfs RNDIS + ACM
- telnetd 23
- keep p75 logs: /persist/mainline-test6-*
- hold 300s instead of 60s
- add pstore/ramoops dump
- if possible add framebuffer console
```

## 7. Flash B without fastboot

From A OpenWrt SSH:

```sh
scp boot_mainline_6.18.55_t508n_test6.img root@192.168.42.1:/tmp/
ssh root@192.168.42.1 'dd if=/tmp/boot_mainline_6.18.55_t508n_test6.img of=/dev/mmcblk0p37 bs=1M; sync'
scp misc_slot_b.img root@192.168.42.1:/tmp/
ssh root@192.168.42.1 'dd if=/tmp/misc_slot_b.img of=/dev/mmcblk0p3 bs=1M; sync; reboot'
```

## 8. Success criteria

```text
/sys/class/udc contains musb-hdrc.0.auto
configfs gadget binds
usb0 appears
host sees RNDIS / ACM
telnet 192.168.42.1:23 or COM reaches mainline shell
```

## 9. Blockers / fallback

- If MUSB core still does not register UDC:
  - check `vddusb33` rail (LK turns it off on normal boot)
  - port minimal hsphy init
  - compare with 5.4 self-built USB chain reports
- If MUSB fails completely, enable DWC3 + ssphy using MU300 port as fallback.
