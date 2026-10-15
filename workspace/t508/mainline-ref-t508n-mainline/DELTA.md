# T508N vs MU300 mainline delta

## 1. Memory

| Item | MU300 / F50 | T508N (live stock DTB) | Action |
|---|---|---|---|
| memory base | `0x80000000` | `0x80000000` | same |
| memory size | `0x7ffff000` (2 GiB - 4 KiB) | `0xbffff000` (3 GiB - 4 KiB) | use T508N value |
| visible MemTotal | ~1.4 GiB | ~2.5 GiB (2520900 kB) | expected |

## 2. reserved-memory

Use T508N live `/proc/device-tree/reserved-memory`.

| Node | Address | Size | MU300 comparison |
|---|---:|---:|---|
| ddrbist-mem | `0x80000000` | `0x1000` | same |
| sysdumpinfo-mem | `0x80001000` | `0x1000` | same |
| sipc-mem | `0x87800000` | `0x800000` | same |
| cp-modem | `0x88000000` | `0x1d000000` | same |
| ch_ddr | `0xae600000` | `0x300000` | same |
| audio-mem | `0xaf700000` | `0x300000` | T508N extra |
| audiodsp-mem | `0xafa00000` | `0x600000` | T508N extra |
| logobuffer | `0xb0000000` | `0x200000` | MU300 is larger (`~0x9e4000`) |
| sysdump-uboot | `0xb5000000` | live reg is 0 | MU300 `0x1000000`; re-check stock DTB |
| uboot_log-mem | `0xbbe80000` | `0x80000` | same |
| sml-mem | `0xbd000000` | `0x40000` | same |
| tos-mem | `0xbd060000` | `0x1fa0000` | MU300 `0xbd060000/0x18a0000`; use live |
| sh-mem | `0xc3000000` | `0x100000` | same |
| cma_share | `0xf0000000` | `0x3000000` | T508N extra |
| dptx_hdcp | `0xfff10000` | `0x60000` | T508N extra |
| rebootescrow | `0xfff70000` | `0x10000` | MU300 has same |
| ramoops | `0xfff80000` | `0x40000` | same layout |

## 3. Wi-Fi / WCN

| | MU300 / F50 | T508N |
|---|---|---|
| compatible | `unisoc,marlin3_pcie` | `unisoc,marlin3lite_sdio` |
| Wi-Fi child | `sprd,sc2355-pcie-wifi` | `sprd,sc2355-sdio-wifi` |
| bus | PCIe | SDIO1 |
| module | `pcie-sprd`, `pcie-sprd-misc` | `sc2355_sdio_wlan`, `sprd_wlan`, `wcn_bsp` |
| mainline reuse | `wcn_bsp/pcie` + `sprd_wlan_combo/sc2355/pcie.c` | `wcn_bsp/sdio` + `sprd_wlan_combo/sc2355/sdio.c` |
| verdict | reference | high reuse, change bus |

T508N WCN GPIOs from vendor DTS:

```text
m2-wakeup-ap-gpios  = GPIO128
reset-gpios         = GPIO172
enable-gpios        = GPIO171
m2-to-ap-irq-gpios  = GPIO173
sdhci-name          = <&sdio1>
```

## 4. Bluetooth

| | MU300 | T508N |
|---|---|---|
| node | `sprdbt_tty/tty-pcie` | `sprd-mtty` with `sprd,name="ttyBT"` |
| bus | PCIe | SDIO/Marlin3lite |
| action | reference | add SDIO mtty variant or reuse wcn_bsp mtty |

## 5. USB

| | MU300 | T508N |
|---|---|---|
| gadget | NCM / ECM | RNDIS (`usb0`) |
| controller | DWC3 | MUSB (`musb_sprd`) |
| mainline | `f_ncm` / `f_ecm` | `f_rndis` exists in mainline |
| action | reference | configfs + `f_rndis`, board USB role/PHY |

## 6. Keypad

T508N:

```text
641b0000.keypad
driver: drivers/input/keyboard/sprd_keypad.c
```

MU300 does not have this. Port `sprd_keypad.c` to mainline input subsystem.

## 7. Display / backlight

T508N vendor DT:

```text
panel (MIPI)
sprd,qogirn6pro-backlight
LCD panel files under arch/arm64/boot/dts/sprd/lcd/
```

MU300 has no panel. Plan:

```text
stage 1: simplefb / simple-framebuffer
stage 2: port vendor sprd DRM + panel + backlight
```

## 8. GNSS

T508N:

```text
gnss_common_ctl_all
/dev/data0_gnss
SIPC core@9 channel@7
NMEA
```

Options:

```text
1. port vendor gnss_common_ctl to mainline
2. keep t508n-gnssd user-space daemon over SIPC
3. adapt drivers/gnss/sprd.c from UMS9230 mainline fork
```

## 9. Modem / 5G / SIPC / SIPA

Same SoC family, same design:

```text
sprd_modem_loader
sprd_power_manager
seth/sipx/spipe/spool
sipa_v3
sipa_sys
sipa_delegate
```

MU300 `upstream/modules/sprd_modem` is the main reference.

## 10. PMIC / charger / battery

T508N:

```text
bq2560x_charger
sc27xx_fuel_gauge
sprd_charger_manager
sprd_battery_info
ump9620/ump9621 regulator
```

MU300 mainline patches include UMP9620 efuse/ADC/fuel gauge and bq256xx. Reuse framework, adapt board parameters.

## 11. OpenWrt integration

Reuse:

```text
mu300cell netifd proto
luci-app-mu300 (adapted)
fw4-sipa-offload patch
squashfs + overlay + factory reset (f50-openwrt)
sysupgrade approach
```


## 12. DTS compile check

`dts/ums9620-ot5.dts` was preprocessed with the local Linux 6.12 headers and
compiled with the local `scripts/dtc/dtc`:

```text
/tmp/ums9620-ot5.dtb = 4588 bytes
```

But per MU300 FINDINGS, the custom DTB is rejected by LK after dtbo merge.
Use the T508N stock vendor DTB for boot, and treat this DTS as reference only.
