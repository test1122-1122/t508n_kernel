# T508N U-Boot magic64 reverse engineering

## 1. Why magic64

T508N is a fused UMS9620 device:

```text
BootROM verifies SPL
SPL RSA-verifies SML / TrustOS / U-Boot
```

The stock SPL cannot be replaced without the vendor private key.  The
signature-preserving magic64 method keeps the original signed U-Boot payload
byte-for-byte, appends a small AArch64 shellcode and patch table after the
signed payload, and patches RAM after the SPL has verified the image.

```text
0x000  DHTB header (0x200)
0x200  entry jump -> shellcode
0x210  original signed payload (unchanged)
...    shellcode + patch table
...    SIMGHDR footer moved by +0x100
...    RSA cert moved by +0x100
```

## 2. T508N load base

```text
LOAD_BASE = 0xb5000000
```

Confirmed by the device's own `uboot_log`:

```text
load_buf      = 0xb4fffe00
sechdr_addr   = 0xb51ced10
sechdr_offset = 0x1cef10
```

## 3. T508N patch points

All offsets are code offsets relative to the DHTB payload
(`file offset = code offset + 0x200`).

| Code offset | Stock word | Target | Function | Meaning |
|---:|---:|---:|---|---|
| `0xb068` | `0x54000180` | `0xd503201f` | boot lock-warning branch | remove `INFO: LOCK FLAG IS : UNLOCK!!!` and power-button wait |
| `0xb0e8` | `0xb946c813` | `0x52800033` | `get_lock_status` load | force `g_DeviceStatus = 1` (UNLOCK) |
| `0xb324` | `0x9402ff42` | `0xd503201f` | SKIP VERIFY UART print | remove UART `WARNNING: ... SKIP VERIFY!!!` |
| `0xb32c` | `0x94044594` | `0xd503201f` | SKIP VERIFY screen print | remove screen `WARNNING: ... SKIP VERIFY!!!` |

Related global:

```text
g_DeviceStatus code offset = 0x1d88c8
runtime                    = 0xb51d88c8
read sites                 = 0xb05c, 0xb0e8
```

## 4. Build

Tool:

```text
/home/test/t508n/t508/work/srag-refs/UnisocBypass/tools/magic_pack_ums9620.py
```

Command:

```sh
python3 magic_pack_ums9620.py uboot_b.img uboot_t508n_magic64_v1.img \
  --patch 0xb068=0xd503201f \
  --patch 0xb0e8=0x52800033 \
  --patch 0xb324=0xd503201f \
  --patch 0xb32c=0xd503201f
```

Offline verification (`magic64_verify.py`):

```text
DHTB valid
payload byte-identical at 0x210
SIMGHDR moved, payload_offset 0x200 -> 0x210
cert blob byte-identical, relocated only
shellcode writes exactly the 4 intended words
control returns to real _start at 0xb5000000
```

One verifier padding check reports a false positive caused by its own cert
offset arithmetic; the stock and patched cert tails are both zero.

## 5. Artifacts

| Artifact | Size | MD5 |
|---|---:|---|
| `uboot_t508n_magic64_v1.img` | 3145728 | `39960804d6cc18cadacf5fc239b08225` |
| `uboot_t508n_magic64_probe.img` | 3145728 | `fa4a30e4cec556eeb1ba263f76f6141c` |
| stock `uboot_a/b.img` | 3145728 | `7fe2b5eaeb68b9b7792ef806ce553e61` |

Probe image changes the boot log string `welcome to lk/MP` to
`MAGIC64 PROBE OK`, used to prove magic64 execution on hardware.

## 6. Hardware result

After flashing the patched U-Boot to `uboot_a` and `uboot_b`, OpenWrt booted
with:

```text
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
```

This confirms that:

- the stock SPL accepted the relocated SIMGHDR/payload offset;
- the magic64 shellcode executed;
- AVB was skipped for the custom `boot_a` image.

## 7. Flash package

Windows one-click package:

```text
E:\Download\ums9620_tcl_t508n_magic64\
E:\Download\ums9620_tcl_t508n_magic64.zip
```

Scripts:

```text
01-一键解锁并写magic64.bat
02-仅写magic64-uboot.bat
03-探针验证.bat
04-备份uboot.bat
05-恢复原厂uboot.bat
```

## 8. Rollback

```text
05-恢复原厂uboot.bat
```

or restore from a full BROM backup with the original signed U-Boot.
