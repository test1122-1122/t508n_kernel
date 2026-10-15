# T508N mainline 6.18.55 build report

Built with:

```
tools/build-mainline.sh
KV=6.18.55
CROSS_COMPILE=aarch64-none-linux-gnu-
config/t508n-mainline.config
kernel source: mirrors.aliyun.com / linux-6.18.55
```

## Outputs

| File | Size |
|---|---:|
| Image | 17713160 bytes |
| Image.lk | 19286024 bytes |
| ums9620-ot5.dtb | 4588 bytes |
| modules.builtin | 11559 bytes |
| modules.builtin.modinfo | 314570 bytes |

Kernel release: `6.18.55`

## Config final status

Set as intended:

```
CONFIG_ARCH_SPRD=y
CONFIG_SPRD_UMS9620_CLK=y
CONFIG_MMC_SDHCI_SPRD=y
CONFIG_USB_MUSB_HDRC=y
CONFIG_USB_CONFIGFS_RNDIS=y
CONFIG_KEYBOARD_MATRIX=y
CONFIG_DRM_SIMPLEDRM=y
CONFIG_BACKLIGHT_PWM=y
CONFIG_PWM_SPRD=y
CONFIG_GNSS=y
CONFIG_NF_TABLES=y
CONFIG_MODULES=y
```

Removed and confirmed absent:

```
CONFIG_PCI
CONFIG_PCI_MSI
CONFIG_PCIEPORTBUS
CONFIG_PCIE_SPRD
CONFIG_CHARGER_BQ256XX
CONFIG_LEDS_ZTE_LDO
CONFIG_LEDS_SC27XX_KPLED
```

## Requested symbols not present in final .config

```
CONFIG_DMIID=n          (x86 only, irrelevant)
CONFIG_REGMAP_I2C=y     (hidden; selected by an I2C driver when ported)
CONFIG_FB_SIMPLE=y      (excluded by CONFIG_DRM_SIMPLEDRM; choose one)
```

## Next

1. Package a one-shot boot image using the T508N stock vendor DTB.
2. Test boot_b only; do not touch boot_a.
3. Build the out-of-tree modules (wcn_bsp/sprd_wlan_combo/sprdbt_tty/sprd_modem).
4. Add T508N-specific drivers: sprd_keypad, panel/backlight, SDIO WLAN/BT, GNSS.
5. Build OpenWrt rootfs and integrate via f50-openwrt.


---

## Update: C and A completed

### C. Framebuffer choice

Chosen:

```text
CONFIG_DRM_SIMPLEDRM=y
CONFIG_DRM_FBDEV_EMULATION=y
```

Dropped:

```text
CONFIG_FB_SIMPLE
```

`FB_SIMPLE` is excluded by `DRM_SIMPLEDRM`; DRM path kept for the future
vendor MIPI panel/DRM port, with fbdev emulation for `/dev/fb0`.

### A. Out-of-tree modules

Built:

```text
wcn_bsp.ko              (BUILD_WCN_PCIE=false)
sprd_wlan_combo.ko      (SDIO + PCIe objects linked for symbol completeness)
sprd_modem              22 modules
sprd_pmic_wdt.ko
mali_kbase.ko
```

Not built:

```text
sprdbt_tty.ko           MU300 only has tty-pcie; T508N needs SDIO mtty port
```

In-tree modules:

```text
modules built: 331
modules_install: ok
flat module directory: out/modules
```

See `MODULES-REPORT.md` and `modules/LIST.txt`.
