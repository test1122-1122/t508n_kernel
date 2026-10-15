# T508N A/B slot control (misc bootloader_control)

## 1. Where slot selection lives

U-Boot does not choose the A/B slot from `boot_a` / `boot_b` directly.
It reads the Android bootloader control block from:

```text
partition: misc
offset:    0x800
struct:    bootloader_control
```

T508N U-Boot log:

```text
ANDROID: bootable slot 0 priority: 15, tries_remaining: 7,
         verity_corrupted: 0, successful_boot: 1
ANDROID: unbootable slot 1 tries_remaining: 0, verity_corrupted: 0
ANDROID: Booting slot_a
```

This matches the metadata that was present in `misc`:

```text
0x800: 5f 61 00 00 42 43 41 42 01 02 00 00 ff 00 0f 00
0x810: 00 00 00 00 00 00 00 00 00 00 00 00 96 b1 8d 84
```

## 2. Struct layout

Packed `bootloader_control` used by the UMS9620 tooling:

| Offset | Size | Field |
|---:|---:|---|
| `0x00` | 4 | `slot_suffix[4]` (`_a` / `_b`) |
| `0x04` | 4 | magic `BCAB` (`0x42414342`) |
| `0x08` | 1 | version (`0x01`) |
| `0x09` | 1 | `nb_slot` / recovery / merge bitfields |
| `0x0a` | 2 | reserved / padding |
| `0x0c` | 2 | `slot_info[0]` -> A |
| `0x0e` | 2 | `slot_info[1]` -> B |
| `0x10` | 8 | reserved |
| `0x18` | 4 | CRC32 over first `0x1c` bytes |

`slot_metadata` is 2 bytes:

```text
byte 0: priority:4 | tries_remaining:3 | successful_boot:1
byte 1: verity_corrupted:1 | reserved:7
```

Selection order:

```text
priority -> successful_boot -> tries_remaining
```

## 3. Reference active-slot writer

The F50 toolkit implements `set_active a|b` as:

```c
uint32_t tmp[8] = { 0x5F, 0x42414342, 0x201, 0, 0, 0, 0, 0 };
bootloader_control *abc = (bootloader_control *)tmp;
int slot = *arg - 'a';

abc->slot_info[slot].priority = 15;
abc->slot_info[slot].tries_remaining = 6;
abc->slot_info[slot].successful_boot = 0;
abc->slot_info[1 - slot].priority = 14;
abc->slot_info[1 - slot].tries_remaining = 1;
abc->slot_suffix[1] = *arg;
abc->crc32_le = crc32(0, abc, 0x1C);

w_mem_to_part_offset(io, "misc", 0x800, abc, 0x20, 0x1000);
```

## 4. Repacked misc images

The package contains two prebuilt 1 MiB `misc` images with a zeroed BCB area:

| Image | Slot suffix | A metadata | B metadata | CRC32 |
|---|---|---|---:|---:|
| `misc_slot_a.img` | `_a` | priority 15, tries 6 | priority 14, tries 1 | `0xc5eabfe6` |
| `misc_slot_b.img` | `_b` | priority 14, tries 1 | priority 15, tries 6 | `0x7010e29e` |

`misc_slot_b.img` bytes at `0x800`:

```text
5f 62 00 00 42 43 41 42 01 02 00 00 1e 00 6f 00
00 00 00 00 00 00 00 00 00 00 00 00 9e e2 10 70
```

## 5. Why fastboot reboots to A

When slot B has `tries_remaining: 0` and is marked unbootable, U-Boot enters
download/fastboot mode, fails to hand over cleanly, resets, and falls back to
the bootable slot A.  This is why plugging USB in fastboot mode reboots into
OpenWrt A.

The correct fix is to write `misc_slot_b.img`; no fastboot is required.

## 6. Switching slots from the package

```text
06-切换到B槽-mainline.bat   -> write misc_slot_b.img, slot B next boot
07-切换到A槽-openwrt.bat    -> write misc_slot_a.img, slot A next boot
```

Both scripts use SPD directly.
