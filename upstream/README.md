# Mainline (LTS) kernel on the MU300 — boots to userspace

Mainline **Linux 6.18.52** boots on the ZTE F50 / MU300 (Unisoc UMS9620): all 8 CPUs (4×A55, 4×A76), GICv3, arch
timer, PSCI 1.0, 1.5 GiB RAM, pstore/ramoops, initramfs `/init`, and reboot through the UMP9620 PMIC.

Reference: Unisoc's UMS9620 DT series (LKML, 2023-12-15, "arm64: dts: sprd: Add support for Unisoc's UMS9620", not
merged) describes the same GIC/UART/timer layout; this device is derived from their ums9620-2h10 reference board.

## How it boots
* **Device tree:** the stock vendor DTB in `vendor_boot` is used unchanged. Mainline ignores the vendor-only nodes and
  uses the standard ones (cpus, psci, GIC, timer, memory, reserved-memory, ramoops, ADI). A minimal custom DTB
  (`dts/ums9620-mu300.dts`, installed with `mkvendorboot.py`) was *rejected*: after LK's dtbo merge and fixups the kernel
  hung in `setup_machine_fdt`.
* **Load address:** LK always copies the kernel to 0x80080000 (old text_offset); `wrap-image.py` prepends a branch stub
  so the Image runs from 0x80200000 (2 MiB aligned).
* **Reset:** PSCI SYSTEM_RESET never returns on this firmware. `patches/0001-spi-sprd-adi-add-UMS9620-restart.patch`
  adds the UMS9620/UMP9620 variant to `spi-sprd-adi` (also matching the vendor compatible `sprd,qogirn6pro-adi`) and
  resets through the PMIC software reset, registered above the PSCI handler.
* **Logs:** with a working reset the console survives in ramoops and Android shows it as
  `/sys/fs/pstore/console-ramoops-0` (`tools/collect-logs.sh`).

## Build and test
```sh
docker build -t mu300-mainline-build upstream/
docker volume create mu300-mainline   # unpack linux-6.18.52 into /src of this volume
docker run --rm -v mu300-mainline:/src -v "$PWD/upstream":/work mu300-mainline-build bash /work/build.sh
python3 upstream/wrap-image.py upstream/out/Image upstream/out/Image.lk
python3 boot/build-boot-image.py --kernel upstream/out/Image.lk --init upstream/init-bringup ... --out boot-mainline.img
boot/flash-trial.sh boot-mainline.img      # slot b only, falls back to Android
```

## Debugging without a console
* `stub/pmic-reset.c`: bare-metal PMIC reset used as the kernel entry — proved that LK reaches our code and that the
  PMIC reset works (30 s cycle instead of the ~320 s PM power cut).
* `debug/install-probe.py` (`MU300_PROBE_STAGE=N` for `build.sh`): resets at a chosen boot stage; the cycle time tells
  whether the stage was reached. This located the custom-DTB hang in `setup_machine_fdt`.

## Missing for a usable system
No mainline drivers yet for UMS9620 clocks, pinctrl, power domains, USB (DWC3 glue + PHY), eMMC clocking, PCIe (Wi-Fi/BT),
or the modem/PM (SIPC) stack. Without `modem_control` the PM co-processor powers the board off after ~290 s.
Next: USB gadget (console/network) and eMMC, which need clock/PHY drivers ported from the vendor 5.4 tree.

## Status (2026-09-17): OpenWrt runs on 6.18.52

The installed OpenWrt 25.12.5 boots on the mainline kernel through the normal `boot/init`
(multi-OS switch_root): SSH, LuCI, `br-lan` over the USB 3.1 gadget, fw4/nftables, zram.

Fixes needed on top of the port:

- `sdhci-sprd`: UMS9620 has the r11p3 controller; the vendor driver programs DLL phase `0x2`
  (mainline `0x3`). With `0x3` reads work in HS400ES but every write fails with data CRC errors.
- `sdhci-sprd`: only the non-removable eMMC is probed (the SD slot is unpopulated and floods the log).
- UMP9620 PMIC watchdog, armed by LK for 300 s, is disabled by `ump9620-pmic-wdt-off`.
- Userspace config: cgroups/namespaces/seccomp, bridge, nftables, IPv6, zram.
- Thermal: vendor `sprd_thermal_r5p0` (19 on-die zones) with calibration from the UMS9620 eFuse; the eFuse
  provider is built read-only.
- cpufreq: vendor `sprd_sip_svc` + `sprd-cpufreq-v2`; ATF does the DVFS, the kernel asks through SIP SMC calls
  (3 policies: 4 little / 3 mid / 1 big, schedutil). PSCI cpuidle (WFI, core sleep, cluster power-down).
  Idle temperature dropped from ~54 C to ~48 C.
- The stock DT has trip points only on the vendor virtual zone, so `/opt/mu300/bin/thermal-guard` caps cpufreq
  above 85 C and powers off above 105 C on kernels without SoC trip points.

- Watchdog: the UMP9620 PMIC watchdog that LK arms is taken over (`ump9620-pmic-wdt`), 60 s, pinged by the
  core until procd/systemd opens it; a hard hang resets the board back to Android.
- LEDs: UMP9620 RGB status LEDs (vendor `sc27xx-bltc`).
- PCIe: vendor `pcie-sprd` ported to the 6.18 DWC host API. `num-vectors` from the DT is honoured, the
  Marlin3 driver needs a contiguous block of 32 MSIs.
- Wi-Fi/BT (SC2355 Marlin3): `modules/` holds the vendor `wcn_bsp`, `sprd_wlan_combo` (SC2355 PCIe only) and
  `sprdbt_tty` ported to 6.18 as out-of-tree modules (`build-modules.sh`; `tools/port54.py` does the mechanical
  5.4 API changes, `wcn_bsp/kinclude/mu300_compat.h` the rest). On OpenWrt the chip boots its firmware and the
  5 GHz VHT80 AP beacons. The factory Wi-Fi MAC is read from `androidboot.wifimac` in `/chosen/bootargs`.
  The WCN modules cannot be unloaded and reloaded (same as on 5.4).

Not yet on mainline: modem (sipc/sipa/modem loader), GPU, audio; Bluetooth is built but untested.
