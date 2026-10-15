# Findings: running Linux on the ZTE F50 / MU300 (Unisoc T760)

Everything below was verified on a ZTE F50 (hardware MU300, firmware `MU300_ZYV1.0.0B09`,
Android 13, stock kernel `5.4.254-android12-9-g9c6342244991`) during September 2026.
Each item lists the symptom, the root cause and the fix, so it can be reused for other
UMS9620 devices (for example the ZTE U30 Air).

## Hardware and firmware facts

| Item | Value |
|---|---|
| SoC | Unisoc T760 (UMS9620, "qogirn6pro"), board `ums9620_2h10_feimao` |
| RAM | 2 GiB (about 1.4 GiB visible to Linux, the rest is reserved for modem/TEE) |
| Storage | eMMC, about 58.25 GiB (122159104 sectors) |
| PMIC | UMP9620 (+ UMP9621), charger IC bq2560x/sgm41513, fuel gauge sc27xx-fgu |
| Wi-Fi/BT | SC2355 "Marlin3" on PCIe, firmware `MARLIN3_20A_RLS2_W24.45.4` |
| USB | DWC3 (`25100000.dwc3`) + MUSB (`musb-hdrc.1.auto`), Type-C on the PMIC |
| Bootloader | Unisoc LK ("sprdlk"), Trusty TEE, A/B slots |
| Boot images | boot and vendor_boot are Android header v4, page 4096, LZ4 legacy ramdisks |

## Boot chain and safe testing

### 1. Slot b one-shot trial (never touch boot_a)
* Linux is written to **boot_b** only, and the 32-byte AOSP `bootloader_control` block at
  offset `0x800` of `misc` is set so that slot b has the highest priority.
* **Unisoc LK treats `tries_remaining == 1 && successful_boot == 0` as an already failed boot**
  (`ANDROID: slot 1 booted fail, rolling back spl and reboot into normal`). A one-shot trial therefore
  needs `tries_remaining = 2`: LK decrements it to 1 and boots slot b; if that boot fails, the next boot
  rolls back to slot a.
* Linux `init` writes the original slot-a block back to `misc` as one of its first steps, so any later
  reboot returns to Android.
* The `uboot_log` partition contains LK's ring log. It is the best source for "which slot was chosen" and
  "why did it reset" (`rst_mode`, `charge first poweron reset`, watchdog flags).

### 2. Boot ramdisk must be LZ4 legacy
* Symptom: `RAMDISK: lz4 image found at block 0`, `RAMDISK: incomplete write (-28 != 8388608)`, then
  `VFS: Unable to mount root fs on unknown-block(1,0)`.
* Cause: vendor_boot's ramdisk is LZ4 legacy; a gzip boot ramdisk appended to it is not unpacked as an
  initramfs by this 5.4 kernel, which then falls back to the legacy `/dev/ram0` image path.
* Fix: compress the boot ramdisk with `lz4 -l`. Also add a `dev/console` node to the cpio.

### 3. Logs without a serial console
* The stock cmdline has `loglevel=1`, so nothing reaches `console-ramoops`.
* pstore only survives a warm reset. Most failures on this device end in a power cut, so init also
  persists its stage list and `dmesg` into unused space inside **boot_b at 48 MiB (8 MiB)**; it is read
  back from Android.
* A USB CDC-ACM function (`acm.GS0`) next to ECM gives a login console on the host
  (`/dev/cu.usbmodem*` on macOS) that works even when networking does not.

## Hardware bring-up with the vendor modules

### 4. USB gadget dependency chain
The UDC only appears when the whole chain probes, in this order:
`extcon-usb-gpio` (the PHY node references `extcon-gpio`) → PHYs → `sc27xx_adc` → `sprd_battery_info`
→ `sprd-charger-manager` → `sc27xx_fuel_gauge` → `bq2560x-charger` (provides the `otg-vbus` regulator used as
`vbus-supply` by DWC3 and MUSB) → `dwc3-sprd` / `musb_sprd`.
* **`sc27xx_adc` must be loaded before the fuel gauge.** The vendor `sc27xx-fgu` driver returns a hard error
  (not `-EPROBE_DEFER`) when its IIO channel is missing and is never probed again.
* The exact working order of the 86 modules is in `boot/module-order.txt`.

### 5. The ~290 s power cut (PM co-processor watchdog)
* Symptom: Linux runs normally, then the device loses power about 290 s after boot. LK shows
  `charge first poweron reset`, not a watchdog reset.
* Cause: the PM co-processor (CM4, `pm_sys`) runs its own watchdog. `sprd_pmic_wdt` disarms it by sending
  `watchdog rstoff` over an SIPC sbuf channel, but that channel only becomes ready after Android's
  `modem_control` reloads `pm_sys` (and the modem) through Trusty (`kernelbootcp` TA).
* Fix: run Android's own `/vendor/bin/modem_control` in a chroot with Android's bionic linker and
  libraries. Requirements that were each discovered by failure:
  1. Copy `/dev/__properties__` from a running Android so property reads work.
  2. Create `/dev/block/by-name` links and apply Android's node ownership (`ueventd.rc` plus `chown`/`chmod`
     from vendor `init*.rc`), because `modem_control` drops to uid `system` (1000).
  3. Bind-mount a copy of `/proc/cmdline` with `androidboot.slot_suffix=_a` so it loads the `_a` modem images
     (LK passes `_b` when booting the trial slot).
  4. **Exec the binary directly.** `sprd_modem_loader` rejects every ioctl/write unless `current->comm` is
     exactly `modem_control` (`drivers/unisoc_platform/modem_loader`); running it as
     `linker64 /vendor/bin/modem_control` makes the task name `linker64`.
  5. Provide a sink for liblog (`tools/logdw`, listens on `/dev/socket/logdw`), otherwise the daemon's logs vanish.
* Result: `kbc_verify_all_avb2() ret = 0`, `SEC_KBC_START_CP() ret = 0`,
  `sprd-pmic-wdt: sbuf ready for pmic wdt init!`, `Modem Alive`, and no more power cut.

### 6. Load average of about 12 is not CPU usage
Vendor kernel threads (`sprd-rotation/N`, `sipa-*`, `slog`) wait in `D` state; Linux counts them in the load
average. `top` shows about 98 % idle.

## Custom kernel

### 7. Matching source
* The ZTE U30 Air kernel (`github.com/Enceka/android_kernel_zte_ums9620_mifi_u30air`, "downloaded from ZTE
  opensource") is exactly 5.4.254, covers 137 of the 150 F50 vendor modules and all but one derived config
  symbol of the F50 stock config.
* Missing from that tree: camera/display/GPU/touch modules (not used on this device) and the Wi-Fi driver
  `sprd_wlan_combo`, which is taken from the realme C51/C53 AndroidT kernel_modules drop.
* An older Unisoc 5.4.147 tree lacks the whole qogirn6pro USB stack and is not usable.

### 8. Build notes
* Full LTO needs more than 8 GiB in the linker step; ThinLTO keeps CFI and links fine.
* Stock config + `kernel/mu300-linux.fragment` adds devtmpfs, fhandle, autofs, SysV IPC, namespaces, nftables,
  btrfs/xfs/squashfs, NFS/CIFS, USB serial/modem/audio host drivers, crypto user API, CDC-ACM gadget, and removes
  `STATIC_USERMODEHELPER` and forced module signatures.
* All vendor modules must be rebuilt from the same tree (symbol CRCs change).

## Root filesystem on free eMMC space

### 9. Unused space after userdata
* `userdata` is 20 GiB and ends at sector 54218752; the GPT's last usable LBA is 122155007 and the backup GPT
  is at the very end. About **32.4 GiB between them belongs to no partition** and read back as zeros.
* The rootfs is an ext4 filesystem (`mu300root`) at byte offset `27762098176` (sector 54222848), 32.39 GiB,
  accessed through a loop device with an offset. **The GPT, userdata and all Android partitions are unchanged.**
* `userdata` uses metadata encryption (`dm-default-key`, `inlinecrypt`), so it cannot be shrunk or shared.

### 10. Mounting gotchas
* busybox `mount -o loop,offset=` only creates a loop for regular files; for a block device the options go to
  ext4 and fail with `EINVAL`. Use `losetup -o` and verify `/sys/block/loopN/loop/offset`.
* Android's `losetup -f` can return a loop index whose node does not exist yet; allocate, wait for the node and
  refuse to attach the same offset twice (two loops mounting one ext4 corrupted it once).
* Toybox `od` on 64 MiB is extremely slow; never interrupt a safety check, an interrupted pipeline "passed" once.

## Ubuntu 26.04 LTS on kernel 5.4

### 11. systemd 259 works on 5.4
5.4 is systemd's minimum baseline; the system boots to `running` with the `old-kernel` taint.

### 12. USB Ethernet must be up before the host activates ECM
* Symptom: macOS shows the "MU300 Linux USB Ethernet" interface as `inactive` forever, no DHCP, although on the
  device `usb0` is UP with carrier.
* Cause: `usb0` was only brought up by a systemd unit a few seconds after the UDC was bound. f_ecm reports the link
  in its first CONNECT notification and macOS' `AppleUserECM` does not pick up a later "connected" notification.
* Fix: `ifconfig usb0 up` immediately after binding the UDC in the initramfs.

### 13. Userland needing newer syscalls
* Ubuntu 26.04's GNU `tar` fails with `Function not implemented` when extracting (it relies on `openat2`,
  Linux 5.6). Use busybox `tar` on the device or extract from another system.
* `ssh.service` is socket-activated on 26.04; enable `ssh.socket`, not only `ssh.service`.
* Extracting an archive that contains a `lib/` directory over Ubuntu replaces the `/lib -> usr/lib` symlink with a
  directory (systemd then disappears). Always ship files under `usr/lib/...`.
* `docker export` leaves `/etc/hostname` empty.

## Wi-Fi (SC2355 / Marlin3)

### 14. Bring-up
* Modules: `pcie-sprd-misc`, `pcie-sprd`, `wcn_bsp`, `sprd_wlan_combo`.
* Firmware and board config come from Android's `/odm/firmware`: `wcnmodem.bin`, `gnssmodem.bin`,
  `wifi_board_config.ini`, `wifi_board_config_ab.ini`. Place them in `/usr/lib/firmware`.
* The realme `wlan_combo` driver falls back to `wifi_board_config_hulk.ini` for unknown projects; the firmware then
  asserts with `CMD_DOWNLOAD_INI / LOAD_INI_DATA_FAILED` and the chip stays in "card dump" state, so `wlan0` cannot
  be brought up (`RTNETLINK answers: No such device`). `kernel/patches/wlan_combo-default-board-config.patch` fixes it.
* `rmmod wcn_bsp` crashes the kernel; reboot instead of reloading the Wi-Fi stack.
* The driver logs `API version not match` for a few command IDs (the realme driver is slightly older than the ZTE
  firmware) and a `WARNING` in `sc2355_free_cmd_buf` (spin_unlock_bh in IRQ context).
* Verified after the fix: the driver parses `wifi_board_config.ini`, `wlan0` comes up, `iw dev wlan0 scan` lists
  nearby networks and `hostapd` (nl80211, WPA2) reaches `AP-ENABLED`. The MAC address is randomized on each load.
* A crash right after installing a module can leave a 0-byte `.ko` on ext4; run `sync` after installing files.

## Mobile data without Android RIL

### 15. Radio, registration and the data bearer
* AT channels: `/dev/stty_nr0` carries unsolicited results (URCs); `/dev/stty_nr1` is a clean command channel.
* After `modem_control` boots the modem the radio is off (`+CFUN: 0`). Android's RIL (`libimpl-ril.so`) uses the Unisoc
  commands `AT+SFUN=2` (SIM on) and `AT+SFUN=4` (protocol stack on); afterwards `+CFUN: 1` and the modem registers
  (`+CEREG: 2,1,...,13` = E-UTRA-NR dual connectivity, i.e. 5G NSA).
* The network activates the default EPS bearer (CID 1, IPv4v6) by itself. `AT+CGCONTRDP=1` returns the address as
  `a.b.c.d.m.m.m.m` plus DNS servers. `AT+CGDATA="M-ETHER",1` answers `CONNECT` and binds the bearer to
  `sipa_eth0` (`sipa_eth<cid-1>`, raw IP, `NOARP`). Assign the address as /32 and route `default dev sipa_eth0`.
* Measured on a Turkcell 5G NSA SIM: about 9.3 MB/s download and 1.3 MB/s upload. `rootfs/.../mobile-data` implements
  up/down/status/sim-reset and NAT (`nftables masquerade` + MSS clamping) so USB/Wi-Fi clients can share the link.
* AT responses can arrive after a short read timeout and then show up as answers to the next command. Drain the channel
  before sending and read until the final result code (`OK`, `ERROR`, `+CME ERROR`, `CONNECT`).
* Hot-swapping the SIM leaves it busy (`+CME ERROR: 14`) and registration stays at emergency-only (`+CEREG: 2,8`);
  reboot after changing the SIM.
* A SIM without an active data plan still registers and gets an address; TCP handshakes may even succeed, but no data
  flows. Check the plan before debugging the data path.

### 16. Rootfs details found while testing data
* `docker export` leaves an empty `/etc/resolv.conf`: `resolvectl query` works but glibc programs cannot resolve names.
  Link it to `../run/systemd/resolve/stub-resolv.conf`.
* busybox/toybox tar drop xattrs, so `ping` loses `cap_net_raw`; `mu300-fixups.service` restores it.
* Android's uid `system` (1000) is also Ubuntu's first user, so modem device nodes show up as owned by `ubuntu`.

## Default boot

### 17. Linux as default without losing the Android fallback
* LK decrements `tries_remaining` before booting a slot and rolls back when it finds `tries == 1 && !successful`.
  The one-shot trial arms slot b with `tries = 2`.
* Default-Linux mode never sets `successful_boot`. Instead:
  1. init skips the slot-a restore when `/etc/mu300/default-boot` in the rootfs says `linux` (slot b is left at `tries = 1`);
  2. `mu300-boot-ok.service` runs 30 s after `multi-user.target` and writes the `tries = 2` block again.
* A boot that never reaches `mu300-boot-ok` therefore leaves `tries = 1`, and the next boot rolls back to Android.
* Verified: `mu300-next-boot linux` + reboot returned to Linux and re-armed slot b; `mu300-next-boot android` + reboot
  booted slot a.

## Parity with Android

### 18. Services and drivers Android runs that the minimal port lacked
* `cp_diskserver` persists modem NV (`nr_fixnv*`, `nr_runtimenv*`); on first start it immediately wrote pending
  "dirty" NV data, so without it modem NV changes are lost. `refnotify` handles modem reference-clock requests.
  Both run from the same chroot; `srtd` needs Android's binder radio HAL and is skipped.
* Drivers that load cleanly after the modem is up: `sprd_soc_thm`, `thermal-generic-adc`, `sprd_cpu_cooling` (binds
  cpufreq and CPU hotplug cooling to `soc-thmzone` with trips at 70/85/110 °C), `leds-sc27xx-bltc` (RGB status LED),
  `zte_card_holder_det`, `sc27xx-vibra`, `sprd_cp_dvfs`, `sprd_ddr_dvfs`. `zte_sar` loads but the aw9610x SAR sensor
  is not populated on this board (`-201`).
* Android disables audio entirely (`ro.audioserver.disabled=true`, no sound cards) although the device tree has an
  enabled sound card (`unisoc,vbc-v4-codec-sc2730`), the UMP9620 codec and an AW883xx smart amplifier that answers on I2C.
* Android's hotspot (`WifiConfigStoreSoftAp.xml`) lives on the metadata-encrypted `/data`, so Linux cannot read it; it
  has to be copied while Android runs. No factory default Wi-Fi credential is stored in a readable partition.
* RAM: Android uses about 960 MiB of the 1.4 GiB; Ubuntu with all services about 480 MiB, of which ~100 MiB is
  unreclaimable vendor-driver slab. journald is capped (`RuntimeMaxUse=16M`).

### 19. systemd ordering pitfall
* A unit `Before=ssh.socket` that keeps default dependencies is ordered after `basic.target`, while `ssh.socket` is
  before `sockets.target` (before `basic.target`). systemd silently drops `ssh.socket` from the boot transaction and SSH
  never starts. Units that must run before sockets need `DefaultDependencies=no`.

### 20. Diagnosing early power cuts
* The initramfs log loop ends at `switch_root` and journald flushes only after local filesystems are up, so a power cut in
  the first seconds of systemd leaves no log. `mu300-early-recorder.service` keeps writing `dmesg` to the boot_b log area
  for the first five minutes.

## LAN, Wi-Fi bands and regulatory

### 21. One LAN for USB and Wi-Fi
* `br-lan` (192.168.77.1/24) bridges `usb0` and `wlan0`; one dnsmasq serves both.
* cfg80211 refuses to bridge `wlan0` ("Device does not allow enslaving to a bridge") because `IFF_DONT_BRIDGE` stays set:
  the SC2355 driver marks the interface as AP but returns an error from `change_virtual_intf` when tearing down the
  previous firmware mode fails, so cfg80211 skips clearing the flag. `kernel/patches/wlan_combo-allow-bridging-ap.patch`.
* After moving modules, delete old copies: `depmod` indexes every subdirectory and `modprobe` loaded stale drivers from
  an `extra.old/` directory for several boots.

### 22. Regulatory database
* This 5.4 kernel only has the `sforshee` regdb certificate; current `wireless-regdb` is signed by `wens`, so
  `iw reg reload` fails with `-ENODATA`, the domain stays `00` and 5 GHz is `NO-IR`. Android's own `regulatory.db` is
  signed by a different certificate and is rejected too. `kernel/patches/regdb-wens-certificate.patch` adds mainline's
  `wens.hex`. cfg80211 tries to load the database before the rootfs is mounted, so userspace runs `iw reg reload` first.

### 23. Only one AP; 5 GHz AP needs the DS Parameter Set element
* `iw list`: `#{ managed, AP } <= 1` — only one AP interface, so 2.4 and 5 GHz cannot be served simultaneously.
* Symptom: with hostapd on channel 36 the firmware answered `CMD_START_AP` with `SPRD_CMD_STATUS_NOT_SUPPORT_ERROR`
  (HT/VHT), or accepted it and never sent beacons (non-HT), for every country, channel, rate set and HT/VHT/PMF setting
  tried. Android's SoftAP on the same firmware runs on 5180 MHz with 80 MHz.
* Stock (ZTE) and realme `sc2355_start_ap` are identical, so the command payload was captured on Android with a kprobe
  on `sc2355_send_cmd_recv_rsp` (`msg->data` at +24, command id at data-11). Android's beacon contains a DS Parameter Set
  element (`03 01 24`) on 5 GHz — Unisoc's hostapd adds it — while upstream hostapd only adds it on 2.4 GHz. The
  firmware takes the AP channel from that element.
* `kernel/patches/wlan_combo-5ghz-ap-ds-params.patch` inserts the element when hostapd omits it. Verified: 802.11a/n/ac
  AP on channel 36 at 80 MHz (seen by a client, disappears when hostapd stops). The firmware offers 5 GHz AP only on
  36-48 and 149-165 (its ACS channel list); `hotspot-start` uses HT40/VHT80 there, and `hotspot-verify` still falls back
  to 2.4 GHz if the firmware refuses.
* The wiphy rate table lists HT MCS rates as legacy bitrates, so hostapd advertises odd "extended rates" (some with the
  basic-rate bit); Android does the same and the firmware ignores them.
* hostapd's 20/40 MHz coexistence scan (`HT_SCAN`) completes on this driver; the log line after it can lag because
  hostapd's stdout is block-buffered.
* The stock driver prints the SoftAP passphrase to the kernel log (`vendor_softap_convert_para`); Android `dmesg`
  captures contain it.

### 26. Modem resets and mobile data recovery
* The modem firmware occasionally resets: it sends an smsg (type 12, "channel 0 not opened" on the AP side),
  `sipa_delegate: modem_reset`, and `modem_control` stops and reloads the modem through Trusty. It comes back with
  `+CFUN: 0`, no registration and no PDP context, while `sipa_eth0` keeps its stale address, so clients lose internet.
  Android's RIL reconnects silently.
* `mobile-data watch` (`mu300-mobile-data-watch.service`) checks `AT+CGACT?` and the interface address every 30 s and
  runs `up` again after two failed checks; `mobile-data down` sets `/run/mu300-mobile-data-down` so a manual disconnect
  is respected. Verified by switching the radio off: data returned without intervention.

### 27. Where the missing ~550 MiB of RAM goes
* `Memory: 1430144K/2097084K available ... 617788K reserved`. The device tree reserves 464 MiB for the modem firmware
  (`cp-modem@88000000`, needed for 4G/5G), 24 MiB for Trusty (`tos-mem`), 8 MiB SIPC shared memory, 3 MiB DDR training
  data and a few small areas; the kernel image (~35 MiB) and the page tables for 2 GiB (~32 MiB) make up the rest.
  `cma_share` (48 MiB) is still usable for movable pages.
* Not needed on Linux: `logobuffer` (9 MiB, there is no display) and `sysdump-uboot` (16 MiB, bootloader crash dumps).
  `kernel/patches/of-reserved-mem-skip.patch` adds `CONFIG_OF_RESERVED_MEM_SKIP` (the boot image command line is not
  passed on by LK, so a cmdline option alone would not work); MemTotal grew from 1447 to 1473 MiB.
* `mu300-zram.service` adds lz4 zram swap of half the RAM (swappiness 100).


### 28. Mali-G57 GPU (OpenCL) without Android
* The stock `mali_kbase.ko` (DDK r40p0) does not load: 166 of its 335 imported symbol CRCs differ from this kernel.
  realme's `kernel_modules` master branch has the same DDK (`gpu/natt/mali`, r40p0-01eac0, UK 11.36, platform
  `qogirn6pro`); an older checkout of the tree has r34p0 (UK 11.31), which Android's r40p0 userspace would not accept.
  `kernel/build-mali.sh` builds it; it probes `23140000.gpu` as "arch 9.0.9 r0p1" and creates `/dev/mali0`.
* Userspace is Android's `libGLES_mali.so` (it is also `libOpenCL.so` and the Vulkan ICD), a bionic library with a
  37-library closure (VNDK apex, bionic, gralloc/mapper HIDL stubs). It runs in the existing vendor chroot with
  `LD_LIBRARY_PATH` covering vendor, egl, VNDK and bionic; `/dev/ion` is enough for OpenCL buffers.
* Test programs are built for bionic with plain clang (`--target=aarch64-linux-android29`, linked against the device's
  `libc.so`/`libdl.so`/`libOpenCL.so`) and a small `_start` that calls `__libc_init` (`tools/gpu/`), so no NDK is needed.
  `android-gpu-run /system/bin/cltest`: "OpenCL 3.0 v1.r40p0-01eac0", device "Mali-G57 r0p1", 4M-element kernel
  verified correct.
* There is no display, so GLES/Vulkan are only usable off-screen. The driver adds about 45 MiB of memory use.

## OpenWrt

### 29. OpenWrt 25.12 next to Ubuntu
* Layout: the ext4 area holds `/openwrt` (and `/ubuntu`, or Ubuntu directly in the root on first installs);
  `/.mu300/boot-os` selects the system and `init` starts `/lib/systemd/systemd` or procd's `/sbin/init`. The disk stays
  mounted at `/mnt/mu300-disk`; `mu300-os ubuntu|openwrt` switches. `openwrt/build-rootfs.sh` builds the rootfs from the
  official armsr/armv8 tarball (checksum-verified) with apk, our kernel modules flat in `/lib/modules/<release>`
  (ubox kmodloader), the vendor chroot and procd services (`mu300-vendor`, `mu300-hw`, `mu300-post`).
* Cellular WAN is a netifd protocol (`proto mu300cell`, option `apn`), so LuCI/fw4 handle routing, DNS and NAT;
  `mobile-data watch` calls `ifup wan` after modem resets.
* Pitfalls found on the device:
  * procd mounts `/dev` as a 512 KiB tmpfs: copying the 85 MiB Android property area gives empty files and
    `modem_control` never boots the modem (power cut at ~290 s). The property area is now bind-mounted (also on Ubuntu,
    saving the RAM).
  * procd preloads `/lib/libsetlbf.so` into services; the chroot runners unset `LD_PRELOAD` or the bionic linker fails.
  * `ujail` drops capability 38 (CAP_PERFMON), unknown to 5.4, so jailed services crash-loop; `procd-ujail` is removed.
  * OpenWrt's busybox lacks `od`, `timeout`, `losetup`, `telnetd`; the static busybox provides them, plus a tiny
    `mountpoint` script (neither busybox has it).
  * GNU `stty` fails on the modem tty ("unable to perform all requested operations"); it is non-fatal now.
  * `wifi`/netifd wireless handlers are in `wifi-scripts` (with `iwinfo`, `wireless-regdb`), not pulled in by `wpad`.
  * macOS keeps the ECM link inactive after netifd reconfigures `usb0`; a hotplug hook re-enumerates the gadget on every
    LAN ifup.
  * The kernel had no nftables sets (`CONFIG_NF_TABLES_SET`), so fw4's ruleset (`ct state vmap {...}`) was rejected as a
    whole and there was no NAT; the fragment now enables sets, objref, flow offload, redirect, quota and friends.
  * OpenWrt's `regulatory.db` is unsigned; this kernel wants the signed one (Debian's `wireless-regdb`).
  * cfg80211 requests `regulatory.db` at 1.6 s, before the rootfs exists; the direct load fails and the request sits in the
    sysfs firmware fallback for 60 s (no userspace helper answers it, neither on OpenWrt nor with systemd-udevd), and
    `iw reg reload` returns ENOENT meanwhile. `regdb-load` answers the pending requests through `/sys/class/firmware`,
    then reloads; the country is applied before netifd starts hostapd.
  * Attended sysupgrade ("Check online for firmware upgrades") and `sysupgrade` would flash a whole-disk armsr image
    (own GPT, kernel 6.12) over the eMMC and brick the device. The packages are removed and `/sbin/sysupgrade` only allows
    configuration backups.
  * `wifi-scripts` generates an open "OpenWrt" network on first boot; `openwrt-wifi-config` replaces it once (marker
    `/etc/mu300/wifi-configured`) with the imported SSID/WPA2 settings.
* Verified after reboots: 5 GHz AP (channel 36), cellular WAN, a USB client's traffic leaves with the modem's public IP;
  memory use about 140 MiB.
* The early recorder runs from preinit, so a failed OpenWrt boot leaves dmesg, `ps`, `logread` and the Android logcat in
  boot_b for `tools/collect-logs.sh`.


### 30. Installer notes
* Android's `/system/bin/sh` (mksh) does 32-bit arithmetic: byte offsets of the Linux region (27 GiB) overflow, so the
  device script works in sectors. mksh also lets a failing EXIT trap replace the exit status; the installer checks for an
  explicit success line instead.
* `adb shell`/`exec-out` read stdin and swallow answers piped into a script; every call uses `</dev/null`.
* Android restarts once shortly after booting back from a Linux fallback; start the installer when Android has settled.
* A first-generation install (Ubuntu directly in the filesystem root) is replaced by `/ubuntu`; `init` still boots the
  old layout if no `/ubuntu` exists.

## Audio

### 24. No internal audio hardware
* The DT enables a sound card, the UMP9620 codec and an AW883xx amplifier at `6-0034`, and Android disables audio.
  With `i2c-dev`, nothing answers at 0x34 (nor at the bq2560x address 0x6b), and there is no AGDSP firmware partition.
  The board has no speaker path; only Bluetooth or USB-host audio devices are possible.
* Update: community Android modules show the audio DSP itself is usable. The F50 DT has `audiocp_boot` and `sound@0` but
  no `audio-mem`/`audiodsp-mem` reserved memory (another UMS9620 device uses 0xaf700000 3 MiB and 0xafa00000 6 MiB).
  Their flow loads an AGDSP image taken from a different device into `/sys/devices/platform/audiocp_boot/agdsp`
  (`stop`, write, `start`), binds `sound@0` to `vbc-rxpx-codec-sc27xx`, and gets a `sprdphone-sc2730` card with Bluetooth
  SCO call audio. The Unisoc ASoC/AGDSP driver sources are in the realme `unisoc-5.4` kernel_modules tree; porting this
  to Linux is future work (A2DP over BlueZ does not need the DSP).

## Bluetooth

### 25. SC2355 Bluetooth on BlueZ
* Transport: `sprdbt_tty` (realme `wcn/bluetooth/driver/tty-pcie`, built with `BSP_BOARD_UNISOC_WCN_SOCKET=pcie`) exposes
  an H4 tty `/dev/ttyBT0` over the WCN PCIe link and registers a `bluetooth` rfkill that powers the BT function.
  `btattach -B /dev/ttyBT0 -P h4` creates `hci0`.
* Vendor init (Android `libbt-sprd_suite`, Marlin3): before the stack starts, send `0xFCA0` with the 176-byte PSKey block
  from `bt_configure_pskey.ini` (BD address at bytes 20..25, little endian), `0xFCA2` with the 252-byte RF block from
  `bt_configure_rf.ini`, then `0xFCA1 00 00 01` (dual mode, enable). `tools/bt-init/mu300-bt-init.c` does this. Without the
  PSKey upload the controller reports a fixed placeholder address.
* The firmware advertises Hold Mode and Park State in its LMP features, but `Write Default Link Policy Settings` returns
  `Invalid HCI Command Parameters` for any value that includes them (0x0000/0x0001/0x0004/0x0005 accepted, 0x0007 rejected).
  The 5.4 kernel sets every advertised mode, so the init sequence aborts and `hciconfig hci0 up` fails with `EINVAL`.
  `kernel/patches/bluetooth-marlin3-link-policy.patch` requests only Role Switch and Sniff (Bluetooth is built in, so this
  needs the kernel rebuild).
* Android keeps the real BT address in `/data/vendor/bluetooth/btmac.txt` on encrypted `/data`; Linux uses
  `BDADDR=` from `/etc/mu300/bluetooth.conf` or a stable locally administered address derived from `machine-id`.
* Verified: `bluetoothd` powers the adapter and LE/BR-EDR scanning lists nearby devices.
* Rebuilding with a modified tree appends `-dirty` to the kernel release and breaks module loading; `.scmversion` with
  `-gb50db5b6224c` in the source tree keeps the release string stable.
