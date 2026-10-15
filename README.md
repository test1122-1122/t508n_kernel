# Linux on the ZTE F50 / MU300 (Unisoc T760)

Ubuntu 26.04 LTS with systemd, SSH, telnet, a USB serial console, a working modem/PM co-processor and
Wi-Fi on the ZTE F50 5G mobile hotspot (hardware MU300, Unisoc T760 / UMS9620). It runs a custom
5.4.254 kernel built from ZTE's GPL source and boots next to Android without changing the partition table.

> **Türkçe özet:** ZTE F50 / MU300 (Unisoc T760) üzerinde, Android'e ve bölüm tablosuna dokunmadan,
> ZTE'nin GPL kaynağından derlenmiş 5.4.254 kernel ile Ubuntu 26.04 LTS (systemd, SSH, telnet, USB seri
> konsol, modem ve Wi-Fi) çalıştırmak için gereken her şey. Ayrıntılar İngilizce; teknik bulguların tamamı
> [`docs/FINDINGS.md`](docs/FINDINGS.md) içinde.

> **Warning.** This writes to `boot_b` and to the `misc` partition, and creates a filesystem in unused eMMC space.
> It requires an unlocked/verification-bypassed device with root on Android and a working SPD download-mode
> recovery path. You can brick your device. Nothing here is endorsed by ZTE or Unisoc.

## Status

| Feature | State |
|---|---|
| Custom kernel (5.4.254, ZTE U30 Air source + fragment) | ✅ boots, 86 vendor modules rebuilt from source |
| Boot without touching `boot_a`, GPT or `userdata` | ✅ slot b one-shot trial, init restores slot a |
| USB Ethernet (ECM) + DHCP | ✅ `192.168.77.1`, host gets `192.168.77.2-9` |
| USB serial console (CDC-ACM) | ✅ `serial-getty@ttyGS0` |
| Ubuntu 26.04.1 LTS, systemd 259 | ✅ `running` (kernel 5.4 = systemd minimum baseline) |
| SSH and telnet | ✅ |
| PM co-processor watchdog / no power cut after ~290 s | ✅ via Android `modem_control` in a chroot |
| Modem firmware boot (`Modem Alive`) | ✅ |
| Wi-Fi (SC2355 / Marlin3) | ✅ station scan and access-point mode (`hostapd` AP-ENABLED) |
| Mobile data (5G NSA/LTE) | ✅ AT commands + `sipa_eth0`, about 75 Mbit/s down / 10 Mbit/s up measured |
| Internet sharing to USB (NAT) | ✅ host behind `usb0` reaches the internet through the modem; a watchdog reconnects after modem resets |
| Wi-Fi hotspot out of the box | ✅ hostapd on `wlan0` bridged with USB into one LAN (192.168.77.0/24); SSID/password imported from Android or generated; `BAND=5` (802.11ac, 80 MHz, channels 36-48/149-165) or `BAND=2.4`; one band at a time |
| Modem NV persistence (`cp_diskserver`), `refnotify` | ✅ Android daemons in the chroot |
| Thermal throttling, status LEDs, SIM tray, DVFS drivers | ✅ `mu300-extra-modules` (blue LED = mobile data up) |
| Default boot to Linux with automatic fallback | ✅ `mu300-next-boot linux\|android`; a Linux boot that never completes rolls back to Android |
| RAM | ✅ 1473 MiB usable (464 MiB is the modem firmware's, unavoidable); unused logo/sysdump reservations freed, zram swap |
| OpenWrt 25.12 (selectable next to Ubuntu, `mu300-os`) | ✅ procd boot, modem, cellular WAN (`proto mu300cell`, APN in LuCI), firewall4 NAT, 5 GHz Wi-Fi (UCI/LuCI), USB LAN, SSH; ~140 MiB RAM used |
| Internal audio | ✗ no speaker/mic path; the AW883xx amplifier does not answer on I2C. The AGDSP can be booted with firmware from another device (Android community modules), Linux port pending |
| Bluetooth (SC2355) | ✅ BlueZ `hci0` powered, scanning works: `sprdbt_tty` (PCIe H4) + `mu300-bt-init` vendor PSKey/RF upload + link-policy kernel patch |
| GPU (Mali-G57) | ✅ OpenCL 3.0 (headless): `mali_kbase` r40p0 built from source + Android's Mali userspace in the vendor chroot (`android-gpu-run`) |

## How it works

```
LK (slot b, tries=2) ─► custom 5.4 kernel + vendor_boot DTB
   └─► initramfs /init (boot/init)
         ├─ load 86 modules in a fixed order (boot/module-order.txt)
         ├─ misc: restore slot a, unless the rootfs says default-boot=linux
         ├─ bind USB gadget: ECM (usb0 up immediately) + ACM console
         ├─ losetup -o 27762098176 /dev/mmcblk0 → ext4 "mu300root" (free space after userdata)
         └─ switch_root → systemd
               ├─ mu300-vendor   : Android modem_control in a chroot (disarms PM watchdog, boots modem)
               ├─ mu300-lan      : br-lan (usb0 + wlan0) 192.168.77.1 + dnsmasq DHCP/DNS
               ├─ mu300-wifi     : pcie-sprd, wcn_bsp, sprd_wlan_combo
               ├─ mu300-mobile-data : AT on /dev/stty_nr1, sipa_eth0, nftables NAT
               ├─ mu300-bluetooth : sprdbt_tty, mu300-bt-init (PSKey/RF), btattach → bluetoothd
               └─ ssh.socket, telnetd, serial-getty@ttyGS0
```

Read [`docs/FINDINGS.md`](docs/FINDINGS.md) for the reasoning behind each step (LK slot rules, LZ4 ramdisk,
USB dependency chain, the PM watchdog, the `modem_control` process-name check, macOS ECM link state, …).

## Repository layout

| Path | Contents |
|---|---|
| `kernel/` | Docker build env, stock F50 config, `mu300-linux.fragment`, build scripts, Wi-Fi driver patch |
| `boot/` | `init`, `module-order.txt`, `build-boot-image.py`, `flash-trial.sh` |
| `rootfs/` | Ubuntu 26.04 `Dockerfile`, `assemble.sh`, systemd units and helper scripts (`overlay/`) |
| `android-vendor/` | Scripts to extract the Android runtime subset from *your* device, permission generator |
| `tools/` | `logdw` (liblog sink), SSH/SCP/telnet/serial helpers, log collector, Android-side mount helper |
| `docs/` | Findings and notes |

Related: the kernel source used here is mirrored at
[`dikeckaan/zte-ums9620-kernel-5.4.254`](https://github.com/dikeckaan/zte-ums9620-kernel-5.4.254)
(ZTE U30 Air GPL release, 5.4.254).

**Not included** (proprietary or device-specific): stock partition images, Android vendor binaries and libraries,
`/dev/__properties__`, Wi-Fi/modem firmware, device dumps and identifiers. The scripts extract these from your own device.

## Requirements

* A rooted MU300/F50 with the boot verification bypass (Android must already boot a modified `boot` image),
  `adb` access, and a tested SPD/BROM recovery path.
* macOS or Linux host with Docker (arm64 native or emulation), Python 3, `lz4`, `adb`.
* Your own dumps: `boot_a.img` and the first 4 KiB of `misc` (`dd if=/dev/block/by-name/misc bs=4096 count=1`).

## Quick install

With the kernel built (step 1 below; outputs in `out/`: `Image`, `modules/`, `modules.builtin*`), the device in rooted
Android and Docker running:

```sh
./install.sh
```

The installer
* checks the device and finds the unpartitioned eMMC space after the last partition (or the existing `mu300root` ext4),
* asks which systems to install (Ubuntu, OpenWrt or both), which one boots, whether Linux is the default boot, whether to
  copy Android's hotspot name/password and whether to include the GPU userspace, and asks for a password
  (`ubuntu` user on Ubuntu, `root` on OpenWrt),
* pulls the vendor files it needs from the device into `work/` (never into the repository), builds both root filesystems
  and the boot image, and shows a summary that must be confirmed with `INSTALL`,
* unpacks the systems to `/ubuntu` and `/openwrt` on the Linux filesystem, writes `boot_b` and arms slot b, and reboots.

Only the Linux region, `boot_b` and 32 bytes of `misc` are written; `boot_a`, the GPT and `userdata` stay untouched. After
installation: `mu300-os ubuntu|openwrt` switches systems, `mu300-next-boot android` returns to Android. Hotspot, cellular
data and USB sharing are configured on first boot and stay editable (`/etc/mu300/hotspot.conf` on Ubuntu, UCI/LuCI on
OpenWrt). `MU300_REUSE_BUILD=1` reuses the root filesystems built by a previous run.

**Never flash OpenWrt firmware images or use sysupgrade on this device** (they target generic arm64 disks and would
overwrite the eMMC); the installed OpenWrt blocks it. Updating packages with `apk upgrade` is fine, except `kernel`/`kmod-*`
(this device runs its own 5.4 kernel).

## Build and run

### 1. Kernel
```sh
git clone https://github.com/dikeckaan/zte-ums9620-kernel-5.4.254   # or the Enceka U30 Air repo
docker build -t mu300-kbuild kernel/
docker volume create mu300-kernel
docker run --rm -v mu300-kernel:/src -v "$PWD/../zte-ums9620-kernel-5.4.254":/tree mu300-kbuild cp -a /tree /src/zte-u30air
cp kernel/f50-stock-B09.config kernel/device.config
docker run --rm -v mu300-kernel:/src -v "$PWD/kernel":/work mu300-kbuild bash /work/build-linux.sh
```
`build-linux.sh` merges `mu300-linux.fragment` into the stock config, switches to ThinLTO and builds `Image` + modules
into `/src/out-linux`. Copy `Image`, `modules.builtin*` and all `*.ko` (flattened, `llvm-strip --strip-debug`) to `out/`.

Wi-Fi driver: extract `kernel_modules/kernel5.4/wcn/wlan/wlan_combo` from the realme C51/C53 AndroidT kernel source into
the volume as `/src/ext-wlan_combo`, then run `kernel/build-wlan.sh` (applies the `patches/wlan_combo-*.patch` files).

Kernel patches: apply `kernel/patches/bluetooth-marlin3-link-policy.patch`, `of-reserved-mem-skip.patch` and `regdb-wens-certificate.patch` to the kernel tree
before building, and `echo -gb50db5b6224c > .scmversion` so the release string does not get a `-dirty` suffix.

GPU: copy `kernel_modules/kernel5.4/gpu/natt/mali` from the realme tree (master branch) to `/src/ext-mali` and run
`kernel/build-mali.sh`; pull the userspace with `android-vendor/extract-gpu-subset.sh` and build `tools/gpu/cltest` with
`tools/gpu/build.sh <dir with libc.so libdl.so libOpenCL.so>`.

Bluetooth: build `sprdbt_tty.ko` from the same realme tree (`wcn/bluetooth/driver/tty-pcie`, `BSP_BOARD_UNISOC_WCN_SOCKET=pcie`)
and the vendor-init tool: `docker run --rm -v "$PWD/tools/bt-init":/w mu300-kbuild gcc -O2 -static -o /w/mu300-bt-init /w/mu300-bt-init.c`.

### 2. Android vendor subset (from your device)
```sh
android-vendor/extract-subset.sh android-subset
python3 android-vendor/gen-ueventd-perms.py android-subset/vendor/etc/ueventd.rc <vendor init *.rc> > android-vendor/ueventd-perms.sh
```

### 3. Boot image
```sh
docker build -t mu300-ubuntu:26.04 rootfs/
docker run --rm mu300-ubuntu:26.04 cat /bin/busybox > busybox && chmod +x busybox   # static, has mdev/losetup/switch_root/telnetd
docker run --rm -v "$PWD/tools/logdw":/w mu300-kbuild gcc -O2 -static -o /w/logdw /w/logdw.c
python3 boot/build-boot-image.py --stock-boot dumps/boot_a.img --misc-head dumps/misc-head.bin \
  --kernel out/Image --modules out/modules --busybox busybox --logdw tools/logdw/logdw \
  --ueventd-perms android-vendor/ueventd-perms.sh --android-subset android-subset --out boot-linux-slotb.img
```

### 4. Root filesystem on the free eMMC region
1. Check on *your* device that the space after `userdata` is really unallocated (compare the last partition end with
   the GPT's last usable LBA) and adjust `ROOT_OFFSET` in `boot/init` and `tools/android-mount-mu300root.sh`.
2. On Android (root), create the filesystem through a bounded loop device and verify offset/size first.
3. Assemble and deploy:
```sh
cid=$(docker create mu300-ubuntu:26.04); docker export $cid > rootfs/base.tar; docker rm $cid
docker run --rm -v "$PWD/rootfs":/w -v "$PWD/out/modules":/kmods:ro -v "$PWD/out":/kout:ro \
  -v "$PWD/firmware":/firmware:ro -v "$PWD/android-subset":/android-subset:ro -v "$PWD/tools/logdw/logdw":/logdw:ro \
  -v "$PWD/tools/bt-init/mu300-bt-init":/bt-init:ro mu300-ubuntu:26.04 bash /w/assemble.sh
```
Push `mu300-ubuntu-26.04-rootfs.tar.gz` to the device and extract it with `tools/android-mount-mu300root.sh`.
`firmware/` holds `wcnmodem.bin`, `gnssmodem.bin` and `wifi_board_config*.ini` from the device's `/odm/firmware`, plus
`bt_configure_pskey.ini` and `bt_configure_rf.ini` from `/vendor/etc`.

### 5. Boot Linux
```sh
boot/flash-trial.sh boot-linux-slotb.img
```
After about 50 s: `ssh ubuntu@192.168.77.1` (password `ubuntu`, **change it**), `telnet 192.168.77.1`, or
`screen /dev/cu.usbmodem* 115200`. `sudo /opt/mu300/bin/mobile-data status|up [APN]|down|sim-reset` controls the modem (`mu300-mobile-data-watch` reconnects automatically unless you ran `down`). `sudo reboot` returns to Android. If a trial fails, collect logs from Android with
`tools/collect-logs.sh`.

Make Linux the default (inside Linux):
```sh
sudo mu300-next-boot linux     # every successful boot re-arms slot b (mu300-boot-ok.service)
sudo mu300-next-boot android   # next reboot goes to Android and stays there
sudo mu300-next-boot status
```
Hotspot settings live in `/etc/mu300/hotspot.conf` (`SSID=`, `PSK=`, `BAND=5|2.4`, `CHANNEL=auto|n`, `COUNTRY=`); `tools/android-import-hotspot.sh`
copies the current Android hotspot into it before the first boot, otherwise a random password is generated and shown at login.

From Android, `boot/android-boot-linux.sh boot-linux-slotb.img` boots the image already on `boot_b` again without reflashing.
If Linux ever fails before `mu300-boot-ok` runs, LK sees `tries_remaining=1` on the next boot and falls back to Android.

## Credits and licenses

* Kernel source: ZTE GPL release for the U30 Air (mirrored by Enceka), Unisoc drivers therein — GPL-2.0.
* Wi-Fi driver source: realme C51/C53 AndroidT kernel release (`wlan_combo`) — GPL-2.0; the patch in `kernel/patches` is GPL-2.0.
* Scripts, tools and documentation in this repository: MIT (see `LICENSE`).
* Stock firmware, Android vendor components and bootloaders belong to their owners and are not distributed here.
