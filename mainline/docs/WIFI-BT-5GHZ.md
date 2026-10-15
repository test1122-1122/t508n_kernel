# WiFi / BT / 5 GHz status (2026-10-15)

## WiFi

Hardware: UNISOC SC2355 (Marlin3-lite) on SDIO `mmc2`.

Working:

```text
scan 2.4G / 5G
WPA2 station connect (lp1111)
DHCP via udhcpc
2.4G AP (hostapd)
5G AP channel 36 (hostapd, country CN)
regulatory domain CN via embedded regulatory.db + iw reg set CN
```

Key implementation points:

- `wcn_bsp.ko` loads firmware from `/dev/block/by-name/wcnmodem_b`.
- `sprd_wlan_combo.ko` has the SC2355 SDIO branch enabled and does not
  force the `00` regulatory domain.
- `regulatory.db` / `regulatory.db.p7s` are embedded in the initramfs,
  because cfg80211 loads them before `/persist` is mounted.
- WiFi must be brought up before Bluetooth; otherwise the next WCN
  power-on can fail with `sdiohal_scan_card` timeout.

Commands:

```sh
/persist/wifi-up.sh
iw wlan0 link
ifconfig wlan0
```

## Bluetooth

Hardware: WCN Bluetooth over SDIO, exposed as `/dev/ttyBT0`.

Working:

```text
sprdbt_tty.ko -> /dev/ttyBT0 + rfkill1
H4 attach -> hci0
hci0 UP RUNNING
BlueZ 5.83 bluetoothd + bluetoothctl
scan on
```

Bring-up:

```sh
/persist/bluetooth-up.sh
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket
LD=/persist/lib/ld-musl-aarch64.so.1
LIB=/persist/lib:/persist/usr/lib
$LD --library-path $LIB /persist/usr/bin/bluetoothctl
```

Known limitations:

- BD address is not yet read from factory data.
- `btmgmt info` fails with `Unable to send index_list cmd`; `hciconfig`,
  `hcitool` and `bluetoothctl` work.
- BT and WiFi share WCN power state; load BT after WiFi.
