# T508N A/B slot control report

## Result: repacked, ready for hardware switch test

U-Boot selects the A/B slot from `misc` offset `0x800`
(`bootloader_control`), not from `boot_a` / `boot_b`.

Hardware U-Boot log:

```text
ANDROID: bootable slot 0 priority: 15, tries_remaining: 7,
         verity_corrupted: 0, successful_boot: 1
ANDROID: unbootable slot 1 tries_remaining: 0, verity_corrupted: 0
ANDROID: Booting slot_a
```

Therefore slot B could not be selected through fastboot; the device reset back
to slot A.

## Repacked images

| Image | Slot | A metadata | B metadata | CRC32 |
|---|---|---|---|---|
| `misc_slot_a.img` | `_a` | priority 15, tries 6 | priority 14, tries 1 | `0xc5eabfe6` |
| `misc_slot_b.img` | `_b` | priority 14, tries 1 | priority 15, tries 6 | `0x7010e29e` |

`misc_slot_b.img` metadata at `0x800`:

```text
5f 62 00 00 42 43 41 42 01 02 00 00 1e 00 6f 00
00 00 00 00 00 00 00 00 00 00 00 00 9e e2 10 70
```

## Usage

```text
06-切换到B槽-mainline.bat   -> misc_slot_b.img
07-切换到A槽-openwrt.bat    -> misc_slot_a.img
```

No fastboot required.

## Verification to do

1. Flash `misc_slot_b.img`.
2. Reboot; confirm U-Boot selects slot B and mainline starts.
3. Recover to slot A with `misc_slot_a.img` if needed.
