# T508N / UMS9620 boot chain and verification

This document records the measured Unisoc UMS9620 (T508N / OT5) boot chain,
the verification performed at each stage, and the exact failure mode that
causes `no valid os found`.

## 1. Chain overview

```text
BootROM
  -> splloader / spl_a / spl_b
       DHTB SHA256, plus SIMGHDR RSA when secure-boot efuse is blown

SPL
  -> sml_a/b, trustos_a/b, uboot_a/b
       SIMGHDR RSA-2048 signature verification

U-Boot
  -> boot_a/b, vendor_boot_a/b, dtbo_a/b, vbmeta*
       Android Verified Boot (AVB)

Linux / Android / OpenWrt
```

## 2. Per-stage verification

| Stage | Loader | Image | Verification | Bypass |
|---|---|---|---|---|
| 0 | BootROM | `splloader`, `spl_a/b` | DHTB SHA256; SIMGHDR RSA on fused parts | none; keep stock SPL |
| 1 | SPL | `sml_a/b`, `trustos_a/b`, `uboot_a/b` | SIMGHDR RSA-2048 | patch runtime U-Boot with signature-preserving magic64 |
| 2 | U-Boot | `boot_a/b`, `vendor_boot_a/b`, `dtbo_a/b`, `vbmeta*` | AVB using OEM key | unlock state -> U-Boot skips AVB |
| 3 | Linux | rootfs | dm-verity for Android system/vendor | disable verity or AVB skip |

## 3. T508N measured image facts

From `firmware/stock-images/uboot_b.img`:

```text
DHTB magic       = "DHTB"
payload size     = 0x1ced10
DHTB hash        = valid SHA256(payload)
SIMGHDR offset   = 0x1cef10
RSA exponent     = 0x10001
RSA modulus      = b5c9d6f4ccf8485d7613850c7c9ba005...
partition size   = 0x300000
```

From the real device `uboot_log` (serial `T6PA03CJ3EH02ZW`):

```text
target: OT5
project: OT5
buildid: 2024-01-16-17:41:42_LOCAL
load_buf      = 0xb4fffe00
sechdr_addr   = 0xb51ced10
sechdr_offset = 0x1cef10
cert_addr     = 0xb51ced70
sysdump-uboot = 0xb5000000 + 0x1000000
```

Therefore:

```text
LOAD_BASE = 0xb4fffe00 + 0x200 = 0xb5000000
```

## 4. Unlock state and AVB

U-Boot reads the lock state from:

```text
miscdata offset 0x2000, 64 bytes
+ RPMB authoritative copy
```

Observed states:

```text
locked:
  androidboot.flash.locked=1
  androidboot.vbmeta.device_state=locked
  androidboot.verifiedbootstate=green
  AVB enforced

unlocked:
  androidboot.flash.locked=0
  androidboot.vbmeta.device_state=unlocked
  androidboot.verifiedbootstate=orange
  AVB skipped
```

When AVB is enforced, custom `boot_a` / `boot_b` images are rejected and LK
shows `no valid os found`.

## 5. T508N current result

After flashing the T508N magic64 U-Boot:

```text
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
androidboot.slot_suffix=_a
```

OpenWrt 5.4 booted from slot A, and `/dev/mmcblk0p75` was mounted with the
`/etc /usr /www /lib /root` bind layout.

## 6. Related documents

- `UBOOT-MAGIC64.md` — T508N-specific U-Boot patch points and packaging.
- `SLOT-CONTROL.md` — A/B slot metadata in `misc`.
- `RECOVERY.md` — rescue procedures and artifact locations.
