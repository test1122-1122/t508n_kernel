# T508N mainline 6.18.55 out-of-tree module build

## Built successfully

| Module | .ko | Status |
|---|---:|---|
| wcn_bsp (SDIO, PCIe disabled) | wcn_bsp.ko 683504 bytes | OK |
| sprd_wlan_combo (SDIO + PCIe link stubs) | sprd_wlan_combo.ko 921344 bytes | OK |
| sprd_modem (SIPC/SIPA/Trusty/modem) | 22 modules | OK |
| sprd_pmic_wdt | sprd_pmic_wdt.ko 12464 bytes | OK |
| mali | mali_kbase.ko | OK |

## Not built

| Module | Reason |
|---|---|
| sprdbt_tty | MU300 source only has tty-pcie; T508N needs SDIO mtty, port from T508N vendor tree |
| sprd_wlan_combo SDIO-only | common code calls sc2355_pcie_* unconditionally; PCIe objects included to link, SDIO path selected by DT |

## In-tree modules

- modules built: 331
- modules_install: ok
- flat module directory: out/modules

## Notes

- Kernel config has no CONFIG_PCI, but sprd_wlan_combo/sc2355/pcie.c
  compiles without direct pci_* API usage; it is linked only to satisfy
  common-code references. T508N will use the SDIO path.
- wcn_bsp was built with BUILD_WCN_PCIE=false.
