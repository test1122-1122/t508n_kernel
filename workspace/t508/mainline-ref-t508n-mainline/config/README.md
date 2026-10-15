# T508N mainline config

Base:

```text
mu300-linux/upstream/mu300-mainline.config
```

Derived file:

```text
config/t508n-mainline.config
```

## Removed for T508N

```text
CONFIG_PCI=y
CONFIG_PCI_MSI=y
CONFIG_PCIEPORTBUS=y
CONFIG_PCIE_SPRD=y

CONFIG_CHARGER_BQ256XX=y          # U30 Air charger
CONFIG_LEDS_ZTE_LDO=y             # U30 Air LEDs
CONFIG_LEDS_SC27XX_KPLED=y        # U30 Air red LED
```

T508N WCN is SDIO, not PCIe; T508N charger is bq2560x / sc27xx, not the U30 bq256xx path.

## Added for T508N

```text
CONFIG_USB_MUSB_HDRC=y
CONFIG_USB_CONFIGFS_RNDIS=y

CONFIG_KEYBOARD_MATRIX=y

CONFIG_FB=y
CONFIG_DRM_FBDEV_EMULATION=y
CONFIG_DRM=y
CONFIG_DRM_SIMPLEDRM=y

CONFIG_BACKLIGHT_CLASS_DEVICE=y
CONFIG_BACKLIGHT_PWM=y
CONFIG_PWM=y
CONFIG_PWM_SPRD=y

CONFIG_GNSS=y
```

## Still to port

```text
sprd_keypad                 -> mainline input driver
sprd DRM / MIPI panel       -> display
T508N SDIO WLAN bindings    -> wcn_bsp/sdio + sprd_wlan_combo/sc2355/sdio.c
T508N SDIO Bluetooth mtty   -> sprdbt_tty / wcn_bsp
T508N GNSS transport        -> gnss_common_ctl or user-space daemon
T508N bq2560x charger       -> mainline power_supply driver
```

## Note

The config is a starting point, not final. It is merged on top of `allnoconfig`
by `tools/build-mainline.sh`. If a symbol does not exist in the target kernel,
`merge_config.sh`/`olddefconfig` will drop it; the build script currently does
not fail on ignored symbols, unlike the MU300 upstream script.


## Config symbol sanity check

Against the local Linux 6.12 Kconfig tree:

```text
requested symbols: 644
not found:        8
```

The 8 are the expected MU300 port / out-of-tree symbols:

```text
SPRD_UMS9620_CLK
SPRD_UMS9620_IPA_PD
REGULATOR_UMP9620
USB_SPRD_UMS9620_SSPHY
SPRD_THERMAL_R5P0
SPRD_SIP_SVC
ARM_SPRD_CPUFREQ_V2
OF_RESERVED_MEM_SKIP
```

These are added by `mu300-linux/upstream/port/install.py` and the MU300
patches, so they remain in the T508N config.


## Framebuffer choice (C)

Chosen:

```text
CONFIG_DRM=y
CONFIG_DRM_SIMPLEDRM=y
CONFIG_DRM_FBDEV_EMULATION=y
```

Dropped:

```text
CONFIG_FB_SIMPLE
```

Reason: `FB_SIMPLE` and `DRM_SIMPLEDRM` are mutually exclusive
(`depends on !DRM_SIMPLEDRM`). DRM_SIMPLEDRM keeps the DRM path for the later
vendor MIPI panel/DRM port, while `DRM_FBDEV_EMULATION` can still provide
`/dev/fb0` for the current fb-based UI.
