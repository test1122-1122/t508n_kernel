# T508N mainline 6.18.55

TCL T508N / OT5 (UNISOC UMS9620 / Qogirn6Pro, T770)
mainline Linux bring-up archive.

Author: deepseek
Target repository: https://github.com/test1122-1122/t508n_kernel

---

## Status

| Feature | State |
|---|---|
| Boot chain | stock LK/SPL/TrustOS + magic64 U-Boot |
| A/B slots | A: vendor OpenWrt 5.4, B: mainline 6.18.55 |
| eMMC | works, p75 userdata used as persistent OpenWrt rootfs |
| USB | DWC3 gadget + ACM serial console (`COM18`) |
| WiFi | SC2355 SDIO, scan / WPA2 station / DHCP / 5 GHz AP |
| Regulatory | `regulatory.db` embedded, `iw reg set CN` works |
| Bluetooth | `sprdbt_tty` + H4 attach, `hci0` UP |
| BlueZ userspace | 5.83 offline packages, `bluetoothd` + `bluetoothctl` |
| Keypad | `sprd_keypad.ko`, input device registered |
| PMIC | UMP9620 regulator, RTC, ADC, eFuse, thermal, cpufreq |
| Watchdog | `sprd_pmic_wdt.ko` (manual load after SIPC stack) |
| Modem / GPU | modules built, untested on T508N |
| Display / audio / camera / VPU / NPU | not ported |

---

## Layout

```text
mainline/
  docs/                 reports and bring-up notes
  kernel/
    mu300-port/         UMS9620 mainline driver ports from MU300
    overlay/            T508N-specific modified kernel files
    config.fragment     relevant kernel config options
  modules/              out-of-tree module sources and .ko binaries
  userspace/            WiFi/BT helper scripts and BlueZ packages
  images/               self-built boot image + wrapped Image.lk
  tools/                build/flash helpers
```

---

## Persistent p75

The mainline initramfs is deliberately small.  Modules, firmware and
userspace live on the eMMC `userdata` partition (`/dev/mmcblk0p75`,
mounted as `/persist`):

```text
/persist/modules/       *.ko
/persist/firmware/      wcnmodem.bin, gnssmodem.bin, wifi_board_config*.ini,
                        regulatory.db, regulatory.db.p7s
/persist/bin/           iw, apk, btattach_mini
/persist/sbin/          wpad
/persist/usr/           BlueZ files
```

The p75 filesystem can be restored/recreated with `mke2fs`, then the files
from this repository copied into it.

---

## Build notes

### Kernel

The kernel is mainline 6.18.55 plus:

- MU300 UMS9620 ports (`kernel/mu300-port`)
- T508N-specific overlay (`kernel/overlay`)
- T508N config fragment (`kernel/config.fragment`)

### Modules

Each module directory contains source, Makefile/Kbuild, and a pre-built
`.ko` for `6.18.55 SMP mod_unload aarch64`.

Example:

```sh
make -C /path/to/out-6.18.55 M=$PWD \
  KBUILD_EXTRA_SYMBOLS=/path/to/wcn_bsp/Module.symvers \
  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
```

### Boot image

`images/boot_mainline_6.18.55_t508n_test40.img` is a 64 MiB Android boot
image made from a stock boot header, `images/Image.lk`, and a small
initramfs containing:

- DWC3 ACM setup first
- embedded `regulatory.db` / `regulatory.db.p7s`
- `wifi-up.sh` / `bluetooth-up.sh` callouts

Flash with fastboot:

```sh
fastboot flash boot_b images/boot_mainline_6.18.55_t508n_test40.img
fastboot flash misc  tools/misc/misc_slot_b.img
fastboot reboot
```

or use the SPD batch in `tools/flash/`.

---

## WiFi

```sh
/persist/wifi-up.sh
```

This loads `wcn_bsp` + `sprd_wlan_combo`, brings `wlan0` up, sets `CN`,
starts `wpa_supplicant` and runs `udhcpc`.

## Bluetooth

```sh
/persist/bluetooth-up.sh
```

This loads `sprdbt_tty`, attaches H4 via `btattach_mini`, starts
`dbus-daemon` + `bluetoothd`, and brings `hci0` up.

Then:

```sh
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket
LD=/persist/lib/ld-musl-aarch64.so.1
LIB=/persist/lib:/persist/usr/lib
$LD --library-path $LIB /persist/usr/bin/bluetoothctl
```

---

## Known issues

- WCN modules cannot be unloaded/reloaded reliably.
- Load Bluetooth after WiFi; it can disturb WCN power state.
- BD address is currently the default `27:93:31:14:22:11`.
- `btmgmt info` reports `Unable to send index_list cmd`; use `bluetoothctl`.
- Display, audio, camera, video codec and NPU are not ported.
- Vendor firmware and stock boot images are not included.

---

## License

- Code, patches and kernel modifications: GPL-2.0-only
- Documentation: CC-BY-4.0

See `LICENSE` and `LICENSE.docs`.
