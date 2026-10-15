#!/usr/bin/env python3
"""
紫光展锐 T770/UMS9620 boot.img 打包器（复刻 stock 布局，可被 LK 引导）

布局（实测 stock boot_a.img 反推）:
  0x0            ANDROID! v4 头: header_size=1580, header_version=4, cmdline 全 0
  0x1000         kernel（必须是 raw ARM64 Image，不能是 gzip! LK 日志:
                 "The kernel is in raw format!"）
  0x1000+ALIGN   ramdisk 区域（LK 按页对齐读取 kernel_size 后紧跟）:
                 [0x600 零填充][magic 0x184C2102][u32 size=0x604+len(lz4)][LZ4 原始流][4B 零尾]
                 ramdisk_size(头字段) = 0x60C + len(lz4)
  结尾(可选)     AVB0 vbmeta footer（--key 时用 avbtool add_hash_footer 生成）

用法:
  python3 sprd_mkbootimg.py --kernel Image --ramdisk initramfs.cpio \
      --output boot.img [--os_version 0x1a000178] [--key key.pem] [--partition_size 0x4000000]
"""
import argparse, struct, subprocess, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sprd_ramdisk import MAGIC, lz4_block_encode

def build(args):
    kernel = open(args.kernel, 'rb').read()
    cpio = open(args.ramdisk, 'rb').read()
    if kernel[:2] != b'MZ':
        print('!! kernel 不是 ARM64 raw Image (无 MZ/PE 头); LK 无法加载 gzip 内核', file=sys.stderr)
    lz4 = lz4_block_encode(cpio)
    # stock: ramdisk 容器紧贴页对齐内核末尾, ramdisk_size = 8 + size_field
    container = struct.pack('<II', MAGIC, 0x604 + len(lz4)) + lz4 + b'\x00' * 4
    ramdisk = container
    kernel_size = len(kernel)
    aligned = (kernel_size + 0xfff) & ~0xfff
    kernel_pad = b'\x00' * (aligned - kernel_size)
    os_version = int(args.os_version, 0)
    hdr = b'ANDROID!' + struct.pack('<IIII', kernel_size, len(ramdisk),
                                    os_version, 1580) \
        + b'\x00' * 16 + struct.pack('<I', 4) + b'\x00' * 1536
    assert len(hdr) == 1580
    img = hdr + b'\x00' * (0x1000 - len(hdr)) + kernel + kernel_pad + ramdisk
    open(args.output, 'wb').write(img)
    print(f'kernel {kernel_size} (+{len(kernel_pad)} pad) @0x1000')
    print(f'container @{0x1000+aligned:#x} (页对齐内核末尾), '
          f'lz4 {len(lz4)}, ramdisk_size {len(ramdisk):#x}')
    if args.key:
        psize = args.partition_size or str(64*1024*1024)
        cmd = ['python3', 'tools/avbtool.py', 'add_hash_footer',
               '--image', args.output, '--partition_name', 'boot',
               '--partition_size', str(int(psize, 0)),
               '--key', args.key, '--algorithm', 'SHA256_RSA4096',
               '--salt', 'a50d5d6c50a6f5c88ac88398e0c4b1e2c4a6d5e2d1c3b4a5968778695a4b3c2d', '--rollback_index', '1']
        r = subprocess.run(cmd, capture_output=True, text=True)
        print(r.stdout[-300:], r.stderr[-300:])
        if r.returncode != 0:
            print('!! avbtool footer 失败 (可先不带 --key, 解锁态 LK 允许无 footer)', file=sys.stderr)

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--kernel', required=True)
    ap.add_argument('--ramdisk', required=True)
    ap.add_argument('--output', required=True)
    ap.add_argument('--os_version', default='0x1a000178')
    ap.add_argument('--key')
    ap.add_argument('--partition_size')
    build(ap.parse_args())
