# Kernel integration

- `mu300-port/` contains the UMS9620 driver ports used by this device
  (clocks, PM domains, UMP9620 regulator, USB SS PHY, SPI-ADI restart,
  thermal, cpufreq, PCIe, SIP SVC, watchdog).
- `overlay/` contains T508N-specific modified files:
  - `drivers/mmc/host/sdhci-sprd.c` — only eMMC + SDIO WiFi hosts
  - `drivers/input/misc/sc27xx-vibra.c` — UMP9620 vibrator data
  - `drivers/usb/dwc3/*` — SPRD compatibles and role switch
  - `arch/arm64/boot/dts/sprd/ums9620*.dts*` — reference DTS
- `config.fragment` lists the relevant options.

MU300 is the upstream source of most UMS9620 mainline ports:
https://github.com/dikeckaan/mu300-linux
