# T508N mainline 6.18.55 one-shot boot_b package

## Image

```text
boot_mainline_6.18.55_t508n_test.img
size   = 67108864 bytes
sha256 = a43b50303225bcb2148cf545aca2b5703c80046ac4de8a2b5eb3dcef29ff20f3
```

Built from:

```text
stock base: firmware/stock-images/boot_b.img
kernel:     t508n-mainline/out/Image.lk
ramdisk:    t508n-mainline/out/mainline-initramfs.cpio
```

## Ramdisk behavior

The minimal initramfs:

1. mounts proc/sys/devtmpfs;
2. prints `MAINLINE_BOOT_START` to `/dev/kmsg`;
3. waits for `/dev/mmcblk0p75` (userdata/persist);
4. mounts it ext4 rw;
5. writes:

```text
/persist/mainline-boot-ok
/persist/mainline-dmesg.txt
```

6. prints `MAINLINE_BOOT_PERSIST_OK` to `/dev/kmsg`;
7. sleeps 8 s, then tries `reboot -f`.

If mainline reaches userspace and SDHCI/ext4 work, the marker appears on
the normal vendor system at:

```text
/persist/mainline-boot-ok
/persist/mainline-dmesg.txt
```

## Safe flash plan

Never touch boot_a.

### 1. Flash only boot_b

From bootloader/fastboot:

```text
fastboot flash boot_b boot_mainline_6.18.55_t508n_test.img
```

This does not change the active slot.

### 2. Boot slot B

Option A (persistent switch, recoverable):

```text
fastboot --set-active=b
fastboot reboot
```

Recovery back to the normal system:

```text
fastboot --set-active=a
```

Option B (if a one-shot mechanism is found):

Use the U-Boot/LK `misc` / `bootonce` command for a single boot, without
changing the persistent active slot.

The T508N U-Boot strings show:

```text
slot-count
current-slot
set_active
get partition slot
download slot
bootonce-bootloader
```

so `--set-active` is supported. A true one-shot slot command still needs
confirmation from the bootloader.

## Recovery

- boot_a is untouched.
- If slot B hangs, force bootloader/fastboot with the usual key combo and run:

```text
fastboot --set-active=a
```

- Existing vendor 5.4 + OpenWrt system remains on boot_a.
