# T508N mainline 6.18.55 boot result

## Result: SUCCESS

Mainline 6.18.55 reached userspace on T508N.

Proof from ramoops/pstore:

```text
[    1.106340] mmc0: Host Software Queue enabled
[    1.110660] mmc0: new HS400 Enhanced strobe MMC card at address 0001
[    1.117997] mmcblk0: mmc0:0001 A3A551 28.9 GiB
[    1.128402]  mmcblk0: p1 ... p75
[    1.257618] EXT4-fs (mmcblk0p75): recovery complete
[    1.262638] EXT4-fs (mmcblk0p75): mounted filesystem ... r/w
[    1.285403] MAINLINE_BOOT_PERSIST_OK
[    1.293605] MAINLINE_BOOT_FASTBOOT_BCB
[    3.437642] kvm: exiting hardware virtualization
[    3.442789] reboot: Restarting system
```

## What worked

```text
CPU
GIC / timer / PSCI
memory / reserved-memory
eMMC / SDHCI
EXT4 / p75 userdata
initramfs
userspace init script
reboot path
```

## Boot path used

```text
fastboot flash boot_b boot_mainline_6.18.55_t508n_test2.img
fastboot --set-active=b
fastboot reboot
```

Mainline booted from slot B, ran the initramfs, mounted eMMC, wrote
`MAINLINE_BOOT_PERSIST_OK`, then rebooted.

U-Boot/LK rolled back to slot A and the vendor 5.4 OpenWrt system returned.

## Notes

- `boot_a` was never touched.
- `boot_b` currently holds the mainline test image.
- `misc` was restored to a clean `_a` state after the test, with the command
  area zeroed.
- The `/persist/mainline-boot-ok` file was not visible after reboot because
  the mainline ext4 write was not journal-committed before `reboot -f`.
  pstore is the authoritative log.
