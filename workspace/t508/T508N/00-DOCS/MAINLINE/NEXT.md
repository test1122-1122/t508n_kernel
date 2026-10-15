# T508N mainline staged plan

## Current status (2026-10-09)

Already done:

- mainline 6.18.55 Image + modules built (`out/BUILD-REPORT.md`, `out/MODULES-REPORT.md`).
- mainline reached userspace once; pstore proof in `out/MAINLINE-BOOT-RESULT.md`.
- T508N magic64 U-Boot built and confirmed on hardware (`UBOOT-MAGIC64.md`).
- Slot A vendor 5.4 OpenWrt is running; slot B has mainline test5 written.
- `misc_slot_a.img` / `misc_slot_b.img` allow A/B switching without fastboot
  (`SLOT-CONTROL.md`).

Immediate next actions:

1. Enter BROM and run `06-切换到B槽-mainline.bat` to select slot B.
2. Verify mainline 6.18.55 test5 boot path and capture pstore/console logs.
3. Bring up mainline USB (DWC3 or MUSB) and RNDIS; test host connectivity.
4. Port SDIO WLAN/BT and GNSS modules; compare with vendor 5.4 OpenWrt.
5. Only after B-slot bring-up, consider OpenWrt rootfs integration and sysupgrade.

## Stage 0 — prepare


1. Use `mu300-linux` mainline 6.18.55 as baseline.
2. Use T508N stock vendor DTB (do not force custom DTB first).
3. Keep the current vendor 5.4 boot image as daily/fallback.
4. Test only with one-shot `boot_b`; never overwrite `boot_a`.

## Stage 1 — boot

- memory & reserved-memory from live T508N stock DTB
- PSCI / GIC / timer / UART
- ADI + PMIC watchdog reset
- eMMC / SDHCI
- boot to userspace

## Stage 2 — base I/O

- USB role / PHY
- RNDIS gadget via configfs `f_rndis`
- PMIC regulators
- reboot/poweroff
- thermal guard

## Stage 3 — communication

- SIPC/SIPA/modem core
- 5G data path
- Wi-Fi SDIO (`wcn_bsp/sdio` + `sprd_wlan_combo/sc2355/sdio.c`)
- Bluetooth SDIO mtty
- GNSS (`/dev/data0_gnss`, NMEA, gpsd)

## Stage 4 — T508N-specific

- `sprd_keypad`
- MIPI panel / backlight (or simplefb first)
- charger/battery/thermal parameters

## Stage 5 — standard OpenWrt

- reuse `mu300cell` netifd proto
- reuse `luci-app-mu300` adapted to T508N
- reuse `fw4-sipa-offload`
- adopt `f50-openwrt` squashfs + overlay + factory reset
- sysupgrade path

## First concrete task

Generate a T508N device tree delta against the stock DTB, then run a one-shot
mainline 6.18 boot with:

```text
stock vendor DTB
+ T508N memory/reserved-memory
+ MU300 mainline patches
+ no custom DTB replacement
```
