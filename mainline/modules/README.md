# Modules

Pre-built modules target mainline `6.18.55 SMP mod_unload aarch64`.

| Module | Load order / notes |
|---|---|
| `sprd_usb_power` | early, before DWC3 |
| `sprd_dummy_phy` | early, after usb_power |
| `wcn_bsp` | before wlan/bt |
| `sprd_wlan_combo` | after wcn_bsp |
| `sprdbt_tty` | after WiFi is up; creates `/dev/ttyBT0` |
| `sprd_keypad` | independent |
| `sc27xx_fast_charger` | manual, PMIC fast charger |
| `sprd_pmic_wdt` | manual, after SIPC/sbuf stack |
| `mali/` | binary only; source in MU300 |
| `sprd_modem-bin/` | binary set; source in MU300 |

The `.ko` files are provided for convenience.  Build from source for
kernel-version changes.
