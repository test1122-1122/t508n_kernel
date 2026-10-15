# T508N driver / module map

| T508N hardware | Vendor 5.4 module | Mainline reference | Status |
|---|---|---|---|
| CPU/GIC/timer/PSCI | built-in | MU300 `upstream/dts` + mainline | reuse |
| ADI/PMIC reset | `spi_sprd_adi`, `sprd_pmic_wdt` | MU300 patch 0001 + `sprd_pmic_wdt` port | reuse |
| eMMC | `sdhci_sprd` | mainline `sdhci-sprd` | reuse/verify |
| USB MUSB/DWC3 | `musb_sprd`, `dwc3_sprd` | MU300 patch 0007 + mainline DWC3/MUSB | port/verify |
| USB RNDIS | `sprd_u_ether`, `usb_f_vser` | mainline `f_rndis` | port config |
| WCN SDIO | `wcn_bsp`, `sc2355_sdio_wlan`, `sprd_wlan` | MU300 `wcn_bsp/sdio` + `sprd_wlan_combo/sc2355/sdio.c` | high reuse |
| Bluetooth | `sprd-mtty ttyBT` | MU300 `sprdbt_tty` (PCIe) | port SDIO mtty |
| GNSS | `gnss_common_ctl_all`, `gnss_pmnotify_ctl`, `gnss_dbg` | vendor source + UMS9230 `drivers/gnss/sprd.c` | port/adapt |
| Modem/SIPC/SIPA | `sprd_modem_loader`, `sipc_core`, `sipa_*` | MU300 `upstream/modules/sprd_modem` | high reuse |
| PMIC | `ump9620_regulator`, `sprd_pmic_syscon` | MU300 patches 0004-0006 | reuse/adapt |
| Charger | `bq2560x_charger`, `sc27xx_fuel_gauge` | MU300 upstream patches + bq256xx | reuse/adapt |
| Keypad | `sprd_keypad` | none in MU300 | port |
| Display | vendor `drivers/gpu/drm/sprd`, panel | none in MU300 | port |
| Backlight | `sprd,qogirn6pro-backlight` | none in MU300 | port |
| Thermal | tboard thermistor | MU300 thermal guard approach | reuse |
| GPU | Mali module | MU300 `upstream/modules/mali` | reuse |
| OpenWrt netifd/proto | custom scripts | MU300 `mu300cell` | reuse/adapt |
