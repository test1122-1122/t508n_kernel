# T508N mainline workdir

This directory is the T508N-specific delta workspace layered on top of:

- `../mu300-linux` — mainline 6.18.55 / 7.2 reference for UMS9620 (ZTE F50 / MU300 / U30 Air)
- `../f50-openwrt` — clean OpenWrt squashfs/overlay derivative
- `../OpenWrt-CI-ZTE-F50` — OpenWrt build + 7.2 kernel reference

## What is here

- `dts/ums9620-ot5.dts` — reference minimal DTS using T508N live memory/reserved-memory.
  **Important:** MU300 found that LK/dtbo rejects custom DTB. Prefer the T508N stock
  vendor DTB, like MU300 does. This DTS is a reference/fallback only.
- `DELTA.md` — T508N vs MU300 board delta.
- `DRIVER-MAP.md` — T508N driver / module reuse and port map.
- `NEXT.md` — staged bring-up plan.
- `USB-DWC3-ACM.md` — ★ mainline 6.18.55 DWC3 + SSPHY + USB ACM 成功记录（test28）。
- `NEXT-TEST6-MUSB.md` — MUSB 路线历史记录；2026-10-11 已放弃，改走 DWC3。
- `BOOT-CHAIN.md` — UMS9620/T508N 启动链与校验链联合分析（DHTB、SIMGHDR、AVB、magic64、槽位）。
- `UBOOT-MAGIC64.md` — T508N U-Boot magic64 reverse engineering and package.
- `SLOT-CONTROL.md` — A/B slot metadata in `misc` and fastboot fallback behaviour.
- `RECOVERY.md` — brick rescue procedures and current known-good state.

## First principle

Do not start from the local 6.12 custom-DTB bring-up.

Use:

```text
mainline 6.18.55 (MU300 baseline)
+ T508N stock vendor DTB
+ T508N memory / reserved-memory from live DTB
+ MU300 mainline modules and patches
```

## Live device facts

```text
model      = Spreadtrum UMS9620 SoC
compatible = sprd,ums9620
sprd,sc-id = ums9620 1000 1000
```

USB / WCN / GNSS:

```text
wlan0  -> /sys/devices/platform/sprd-marlin3/sprd-marlin3:wlan
SDIO   -> mmc2:8800:1
usb0   -> MUSB gadget (Remote NDIS)
GNSS   -> gnss_common_ctl_all + /dev/data0_gnss
```


## Tools

- `tools/build-mainline.sh` — build wrapper reusing MU300 mainline patches/config.
  Usage:
  ```sh
  cd t508n-mainline
  CROSS_COMPILE=aarch64-none-linux-gnu- KV=6.18.55 ./tools/build-mainline.sh
  ```
- `tools/wrap-image.py` — LK load-address wrapper (same as MU300).
- `tools/mkvendorboot.py` — vendor_boot DTB replacement reference.

Note: T508N boot image layout differs from MU300's vendor_boot; use our existing
`tools/build_boot_v50align.py` for the actual T508N boot image once the mainline
Image is built.


## Config

- `config/t508n-mainline.config` — T508N config derived from MU300:
  - removed PCIe RC
  - removed U30 charger/LED symbols
  - added MUSB/RNDIS, matrix keypad, simplefb/simpledrm, PWM backlight, GNSS
- `config/README.md` — change list and symbol sanity result.

`tools/build-mainline.sh` now merges this config and rejects the removed
MU300-only symbols.


## Current status (2026-10-09)

- T508N magic64 U-Boot built, verified offline, and confirmed on hardware.
- Slot A runs vendor 5.4 OpenWrt (`linux 5.4.210-android12`, OpenWrt 25.12.5).
- Slot B holds mainline 6.18.55 test5 (written, `ANDROID!` verified).
- Repacked `misc_slot_a.img` / `misc_slot_b.img` switch A/B without fastboot.
- Windows one-click package: `E:\Download\ums9620_tcl_t508n_magic64\`.

See `BOOT-CHAIN.md`, `UBOOT-MAGIC64.md`, `SLOT-CONTROL.md`, `RECOVERY.md`.

## Archive (2026-10-09)

A verified offline archive of the T508N toolchain and boot-chain artifacts lives at
`/home/test/t508n/t508/T508N/` (253 key files / 1.48 GiB, per-file SHA256 verified):

* `README.md` — what the archive contains + the shortest `no valid os found` recovery path
* `ORIGIN-MAP.csv` — archive path -> original path + SHA256
* `MANIFEST.sha256` — archive self-check (`cd T508N && sha256sum -c MANIFEST.sha256`)
* `05-MANIFESTS/` — full inventory, checksums, duplicates, conflicts, big-file and OpenWrt-variant manifests

Full audit reports: `t508/docs/INVENTORY-T508N.md`, `DUPLICATES-T508N.md`, `CONFLICTS-T508N.md`,
`ORGANIZE-PLAN-T508N.md`, and `t508/docs/reports/REPORT-T508N-{FOLDER-ORGANIZATION,ARTIFACT-MANIFEST}.md`.
