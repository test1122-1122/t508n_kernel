# T508N rescue and recovery notes

## 1. Hard-won rules

1. BROM is mask ROM and remains available.  Even after a bad U-Boot or
   unbootable OS, BROM/SPD can always be used to restore known-good images.
2. Back up the device's own `splloader`, `uboot_a/b`, `misc`, `miscdata`
   before any unlock or bootloader experiment.
3. Never flash another unit's `miscdata` unless you understand the
   device-specific serial / calibration fields.
4. Custom boot images require either an unlocked U-Boot or the magic64
   runtime patch; otherwise AVB rejects them and LK prints
   `no valid os found`.
5. Do not trust fastboot mode when slot metadata marks the target slot
   unbootable; switch slot metadata through `misc` instead.

## 2. Working tool and image locations

| Purpose | Path |
|---|---|
| Full stock Android dump | `E:\rootphone\t508n\savebrock\flash_all_spd_dump\` |
| Stock Android image folder | `E:\rootphone\t508n\savebrock\Image\` |
| Original unlock tool | `E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2\` |
| Original SPL/U-Boot/boot backups | `E:\rootphone\t508n\2unlock\` |
| Original `boota.img` / `bootb.img` | `E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2\` |
| Magic64 Windows package | `E:\Download\ums9620_tcl_t508n_magic64\` |
| Host workspace | `/home/test/t508n/t508/mainline-ref/t508n-mainline/` |

## 3. Rescue: restore Android boot chain

Use BROM/SPD and the original Android boot images:

```text
write_part boot_a boota.img
write_part boot_b bootb.img
write_part dtbo_a dtbo_a.img
write_part dtbo_b dtbo_b.img
write_part vbmeta_a vbmeta_a.img
write_part vbmeta_b vbmeta_b.img
write_part misc misc.img
reset
```

If LK still shows `no valid os found`, use the matched full stock dump
`E:\rootphone\t508n\savebrock\flash_all_spd_dump\` and write all boot-chain
partitions, including `vendor_boot`, `init_boot`, `dtb`, and the chained
`vbmeta_*` images.

## 4. Rescue: restore U-Boot

Stock signed U-Boot images:

```text
uboot_a_stock.img
uboot_b_stock.img
```

or the original device backups:

```text
E:\rootphone\t508n\2unlock\uboota.bin
E:\rootphone\t508n\2unlock\ubootb.bin
```

Flash with SPD:

```text
write_part uboot_a uboot_a_stock.img
write_part uboot_b uboot_b_stock.img
reset
```

The magic64 package provides `05-恢复原厂uboot.bat` for this.

## 5. Rescue: switch slot without fastboot

Write the repacked misc image:

```text
06-切换到B槽-mainline.bat  -> misc_slot_b.img
07-切换到A槽-openwrt.bat   -> misc_slot_a.img
```

This avoids the U-Boot fastboot/download reset loop.

## 6. Current known-good layout

As of 2026-10-09:

```text
uboot_a / uboot_b = magic64 v1
boot_a            = OpenWrt 5.4 auto15
boot_b            = mainline 6.18.55 test5 (written, ANDROID! verified)
userdata p75      = OpenWrt ext4 rootfs
misc              = slot A metadata; BCB area may contain boot-fastboot
```

Verified current state:

```text
OpenWrt 25.12.5 on Linux 5.4.210
/dev/mmcblk0p75 -> /persist
/etc /usr /www /lib /root bind-mounted from p75
uhttpd / dnsmasq / hostapd / ubus running
t508n-gnssd running, /dev/data0_gnss present
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
```

## 7. Recovery commands that worked

```text
SPD write  boot_a / boot_b / misc
SPD write  vbmeta_a/b, dtbo_a/b, dtb_a/b, init_boot_a/b
SPD write  spl/uboot from device backups
SPD write  magic64 uboot_a/b
misc bootloader_control slot switch
```

## 8. Things that did not work or are unsafe

- `fastboot --set-active=b` while B is marked unbootable: U-Boot resets back to A.
- Writing another unit's `miscdata`: changes serial / device-specific flags.
- Flashing custom boot images while U-Boot is locked: `no valid os found`.
- Relying on `/persist` commits across `reboot -f` without `sync`.
