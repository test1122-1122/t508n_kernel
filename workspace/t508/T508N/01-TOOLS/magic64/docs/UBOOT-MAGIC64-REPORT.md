# T508N U-Boot magic64 report

## Result: SUCCESS

The T508N-specific magic64 U-Boot was built, verified offline, flashed to
`uboot_a` and `uboot_b`, and booted on hardware.

Hardware proof:

```text
Linux T508N-OpenWrt 5.4.210-android12-9-ga1d5c173d86b-ab1+
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
androidboot.slot_suffix=_a
```

## Input

```text
stock:  firmware/stock-images/uboot_b.img
size:   0x300000
DHTB data_size: 0x1ced10
SIMGHDR offset: 0x1cef10
LOAD_BASE:      0xb5000000
```

## Patch table

| Code offset | Stock | Target | Meaning |
|---:|---:|---:|---|
| `0xb068` | `0x54000180` | `0xd503201f` | remove unlock warning / power wait |
| `0xb0e8` | `0xb946c813` | `0x52800033` | force `get_lock_status` = UNLOCK |
| `0xb324` | `0x9402ff42` | `0xd503201f` | remove SKIP VERIFY UART print |
| `0xb32c` | `0x94044594` | `0xd503201f` | remove SKIP VERIFY screen print |

## Build

```sh
python3 UnisocBypass/tools/magic_pack_ums9620.py uboot_b.img out.img \
  --patch 0xb068=0xd503201f \
  --patch 0xb0e8=0x52800033 \
  --patch 0xb324=0xd503201f \
  --patch 0xb32c=0xd503201f
```

## Offline verification

```text
[PASS] stock/patched DHTB
[PASS] patched size == partition size
[PASS] data_size += 0x100
[PASS] entry jump -> shellcode
[PASS] payload byte-identical at 0x210
[PASS] SIMGHDR relocated, payload_offset 0x200 -> 0x210
[PASS] cert blob byte-identical, relocated only
[PASS] shellcode writes exactly 4 target words
[PASS] control returns to real _start at 0xb5000000
```

The single verifier padding warning is a false positive from its cert offset
arithmetic; both stock and patched cert tails are zero.

## Artifacts

| Artifact | Size | MD5 |
|---|---:|---|
| `uboot_t508n_magic64_v1.img` | 3145728 | `39960804d6cc18cadacf5fc239b08225` |
| `uboot_t508n_magic64_probe.img` | 3145728 | `fa4a30e4cec556eeb1ba263f76f6141c` |
| stock `uboot_a/b.img` | 3145728 | `7fe2b5eaeb68b9b7792ef806ce553e61` |

## Windows package

```text
E:\Download\ums9620_tcl_t508n_magic64\
E:\Download\ums9620_tcl_t508n_magic64.zip
```

## Follow-up

- Verify slot B mainline using `06-切换到B槽-mainline.bat`.
- Use `05-恢复原厂uboot.bat` for rollback.
