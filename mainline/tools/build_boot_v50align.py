#!/usr/bin/env python3
"""
T508N: build a boot image aligned with mu300-linux/V50 boot-image lessons.

Lessons applied (see docs/cross/REPORT-mu300-linux-对T508N价值评估.md §10.1):
  1. reuse the stock boot image header v4 layout and original AVB footer;
  2. signature_size = 0;
  3. ramdisk is a true LZ4 legacy frame: magic 0x184C2102, then per-8MiB
     little-endian compressed-block length + raw LZ4 block;
  4. image length == stock partition image length; assert data does not cross
     the 48 MiB persistent-log guard used by V50 (T508N boot_a has no such log,
     but keeping the guard makes the constraint explicit).

The copied AVB footer keeps the stock vbmeta blob and public key, but its hash
descriptor still describes the stock image.  This is intentional for a
boot-verification-bypassed/unlocked device (V50 does the same).  Do not use
this on a locked device that enforces AVB hashes.

Example:
  python3 tools/build_boot_v50align.py \
    --stock-boot firmware/stock-images/boot_a.img \
    --kernel work/boot_54custom_probe_kernel.bin \
    --ramdisk-cpio work/boot_54custom_probe_ramdisk.cpio \
    --out build/boot-images/boot_54custom_probe_v50align.img
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sprd_ramdisk import MAGIC, lz4_block_encode

PAGE = 4096
LEGACY_LZ4_MAGIC = 0x184C2102
BLOCK_SIZE = 8 << 20          # V50 lz4_legacy uses 8 MiB input blocks
PERSIST_LOG_OFFSET = 48 << 20  # V50 guard; T508N boot_a is 64 MiB, keep as explicit check


def lz4_legacy(data: bytes) -> bytes:
    """V50-compatible LZ4 legacy frame: magic + per-8MiB block length + block.

    Prefer the system lz4 command (same as V50's build-boot-image.py); fall
    back to the in-tree LZ4 block encoder if lz4 is unavailable.
    """
    try:
        r = subprocess.run(['lz4', '-l', '-12', '-c'], input=data,
                           capture_output=True, check=True)
        if r.stdout[:4] == struct.pack('<I', LEGACY_LZ4_MAGIC):
            return r.stdout
    except FileNotFoundError:
        pass

    out = bytearray(struct.pack('<I', LEGACY_LZ4_MAGIC))
    for i in range(0, len(data), BLOCK_SIZE):
        block = lz4_block_encode(data[i:i + BLOCK_SIZE])
        out += struct.pack('<I', len(block)) + block
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--stock-boot', required=True, type=Path)
    ap.add_argument('--kernel', required=True, type=Path)
    ap.add_argument('--ramdisk-cpio', required=True, type=Path)
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--cmdline', default='', help='optional boot header cmdline (<=1536 bytes)')
    a = ap.parse_args()

    base = a.stock_boot.read_bytes()
    if len(base) < PAGE or base[:8] != b'ANDROID!':
        sys.exit('stock image is not an Android boot image')
    hdr_ver = struct.unpack_from('<I', base, 40)[0]
    if hdr_ver != 4:
        sys.exit(f'stock image is not header v4 (got {hdr_ver})')

    kern = a.kernel.read_bytes()
    cpio = a.ramdisk_cpio.read_bytes()
    ram = lz4_legacy(cpio)
    if ram[:4] != struct.pack('<I', LEGACY_LZ4_MAGIC):
        sys.exit('internal error: bad LZ4 legacy magic')
    if len(ram) > 0xFFFFFFFF:
        sys.exit('ramdisk too large for header')

    # ---- header: stock v4 layout, replace sizes, signature_size=0 ----
    hdr = bytearray(base[:PAGE])
    struct.pack_into('<I', hdr, 8, len(kern))          # kernel_size
    struct.pack_into('<I', hdr, 12, len(ram))          # ramdisk_size
    struct.pack_into('<I', hdr, 1580, 0)               # signature_size = 0
    cmd = a.cmdline.encode()
    if len(cmd) > 1536:
        sys.exit('cmdline too long')
    hdr[44:44 + 1536] = cmd + b'\0' * (1536 - len(cmd))

    # ---- body: header + kernel + page pad + LZ4 legacy ramdisk ----
    body = bytearray(hdr)
    body += kern
    body += b'\0' * ((-len(body)) % PAGE)
    body += ram
    body += b'\0' * ((-len(body)) % PAGE)
    original_size = len(body)

    if original_size > PERSIST_LOG_OFFSET:
        sys.exit(f'body {original_size:#x} crosses V50 48 MiB guard')

    # ---- copy stock AVB footer: original vbmeta blob + footer struct ----
    # footer struct: magic/version (8), original_image_size (8), vbmeta_offset (8), vbmeta_size (8)
    vbmeta_offset, vbmeta_size = struct.unpack_from('>QQ', base, len(base) - 44)
    if vbmeta_offset + vbmeta_size > len(base) - 64:
        sys.exit('stock AVB footer points outside the image')
    body += base[vbmeta_offset:vbmeta_offset + vbmeta_size]

    max_body = len(base) - 64
    if len(body) > max_body:
        sys.exit(f'body+vbmeta {len(body)} exceeds {max_body}')
    body += b'\0' * (max_body - len(body))

    footer = bytearray(base[-64:])
    if footer[:4] != b'AVBf':
        sys.exit('stock image has no AVBf footer')
    # original_image_size, vbmeta_offset, vbmeta_size (big-endian in AVB footer)
    struct.pack_into('>QQQ', footer, 12, original_size, original_size, vbmeta_size)
    image = bytes(body) + bytes(footer)
    if len(image) != len(base):
        sys.exit(f'output size {len(image)} != stock size {len(base)}')

    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(image)
    manifest = {
        'image': a.out.name,
        'size': len(image),
        'sha256': hashlib.sha256(image).hexdigest(),
        'md5': hashlib.md5(image).hexdigest(),
        'stock_source': str(a.stock_boot),
        'stock_size': len(base),
        'stock_avb_public_key_sha1_note': 'copied from stock image; hash descriptor is stale by design',
        'kernel_size': len(kern),
        'kernel_md5': hashlib.md5(kern).hexdigest(),
        'ramdisk_cpio_size': len(cpio),
        'ramdisk_cpio_md5': hashlib.md5(cpio).hexdigest(),
        'lz4_legacy_size': len(ram),
        'lz4_legacy_magic': hex(LEGACY_LZ4_MAGIC),
        'lz4_legacy_blocks': (len(cpio) + BLOCK_SIZE - 1) // BLOCK_SIZE,
        'signature_size': 0,
        'original_image_size': original_size,
        'persist_log_guard': PERSIST_LOG_OFFSET,
    }
    mf = a.out.with_suffix('.json')
    mf.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))
    print(f'\nmanifest: {mf}')


if __name__ == '__main__':
    main()
