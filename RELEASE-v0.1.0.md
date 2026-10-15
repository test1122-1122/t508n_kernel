# T508N mainline 6.18.55 bring-up v0.1.0

First released snapshot of the TCL T508N (UNISOC UMS9620 / Qogirn6Pro)
mainline Linux work.

## Highlights

- mainline Linux 6.18.55 boots on the T508N
- DWC3 USB gadget + ACM serial console (`COM18`)
- A/B slot control; A: vendor OpenWrt 5.4, B: mainline
- eMMC and persistent p75 (`userdata`) OpenWrt rootfs
- SC2355 SDIO WiFi
  - scan 2.4G / 5G
  - WPA2 station + DHCP
  - 2.4G / 5G AP mode
  - regulatory CN (`regulatory.db` embedded)
- Bluetooth
  - `sprdbt_tty` + H4 attach -> `hci0` UP
  - BlueZ 5.83 userspace (`bluetoothd`, `bluetoothctl`)
- keypad, PMIC RTC/ADC/eFuse, thermal, cpufreq
- UMP9620 PMIC watchdog and fast charger modules
- modem / GPU modules built (experimental)

## Included artifacts

```text
mainline/images/boot_mainline_6.18.55_t508n_test40.img
mainline/images/Image.lk
mainline/modules/*/*.ko
mainline/userspace/*.sh
mainline/userspace/bluez/pkgs/*.apk
```

## Flashing

Fastboot:

```sh
fastboot flash boot_b mainline/images/boot_mainline_6.18.55_t508n_test40.img
fastboot flash misc  mainline/tools/misc/misc_slot_b.img
fastboot reboot
```

SPD/BROM: `mainline/tools/flash/19-刷B槽-test40.bat`.

## Persistent p75

Modules, firmware and userspace live under `/persist`:

```text
/persist/modules/
/persist/firmware/
/persist/bin/
/persist/sbin/
/persist/usr/
```

## Known issues

- WCN modules cannot be unloaded/reloaded reliably.
- Load Bluetooth after WiFi to avoid WCN power-state conflicts.
- Default Bluetooth address is not the factory address.
- Display / audio / camera / video codec / NPU are not ported.
- Vendor firmware and stock boot images are not included.

## Links

- Repository: https://github.com/test1122-1122/t508n_kernel
- Mainline docs: `mainline/README.md`
- WiFi/BT status: `mainline/docs/WIFI-BT-5GHZ.md`
- Excluded files: `workspace/EXCLUDED-MANIFEST.md`

## License

- Code / patches: GPL-2.0-only
- Documentation: CC-BY-4.0

Author: deepseek
