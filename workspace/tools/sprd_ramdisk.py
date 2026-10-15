#!/usr/bin/env python3
"""
紫光展锐 ramdisk 容器 打包/解包工具
容器格式: [0x600 零填充] + magic 0x184C2102 (u32 LE) + size_field (u32 LE)
          + LZ4 原始块流 + [4B 零尾]
  size_field = 0x604 + len(lz4)   (实测 stock: ramdisk_size = size_field + 8)
解码/编码均使用标准 LZ4 原始块格式 (与内核 LZ4 解压器兼容)。
用法:
  unpack: python3 sprd_ramdisk.py unpack <boot.img> <kernel_end_off> <ramdisk_size> <out.cpio>
  pack:   python3 sprd_ramdisk.py pack <in.cpio> <out.bin>      # 输出容器(raw, 不含0x600填充)
  self-test: python3 sprd_ramdisk.py test
"""
import sys, struct

MAGIC = 0x184C2102

def lz4_block_decode(data, maxout=1<<30):
    out = bytearray(); i = 0; n = len(data)
    while i < n:
        token = data[i]; i += 1
        lit_len = token >> 4
        if lit_len == 15:
            while True:
                b = data[i]; i += 1
                lit_len += b
                if b != 255: break
        if i + lit_len > n:
            out += data[i:]; break
        out += data[i:i+lit_len]; i += lit_len
        if i >= n: break
        offset = struct.unpack('<H', data[i:i+2])[0]; i += 2
        if offset == 0: break
        match_len = token & 0xF
        if match_len == 15:
            while True:
                b = data[i]; i += 1
                match_len += b
                if b != 255: break
        match_len += 4
        src = len(out) - offset
        if src < 0: raise ValueError('bad offset')
        for k in range(match_len):
            out.append(out[src + k])
    return bytes(out)

def lz4_block_encode(data):
    """LZ4 原始块编码: 哈希表匹配器 (标准格式, 兼容内核 LZ4 解码器)"""
    n = len(data)
    out = bytearray()
    ht = {}
    MASK = 0xFFFFF
    def h(p):
        return ((data[p] | data[p+1]<<8 | data[p+2]<<16 | data[p+3]<<24) ^ 0x9E3779B9) & MASK
    def emit_ext(v):
        while v >= 255:
            out.append(255); v -= 255
        out.append(v)
    i = 0; anchor = 0
    while i + 4 <= n:
        cands = ht.get(h(i))
        best = 0; bestlen = 0
        if cands:
            for p in cands[-8:]:
                if i - p > 65535: continue
                if data[p] != data[i] or data[p+1] != data[i+1] or data[p+2] != data[i+2] or data[p+3] != data[i+3]:
                    continue
                l = 4
                while i + l < n and data[p + l] == data[i + l]:
                    l += 1
                    if l > 131070: break
                if l > bestlen: bestlen = l; best = p
        if bestlen >= 4:
            lit_len = i - anchor
            mlen = bestlen - 4
            out.append((min(lit_len, 15) << 4) | min(mlen, 15))
            if lit_len >= 15: emit_ext(lit_len - 15)
            out += data[anchor:anchor + lit_len]
            out += struct.pack('<H', i - best)
            if mlen >= 15: emit_ext(mlen - 15)
            for q in range(i, min(i + bestlen, n - 4)):
                ht.setdefault(h(q), []).append(q)
            i += bestlen
            anchor = i
            continue
        ht.setdefault(h(i), []).append(i)
        i += 1
    lit_len = n - anchor
    out.append(min(lit_len, 15) << 4)
    if lit_len >= 15: emit_ext(lit_len - 15)
    out += data[anchor:]
    return bytes(out)

def unpack_container(img, ramdisk_off, ramdisk_size):
    """从 boot.img 中按 stock 布局解出 cpio: [0x600 pad][magic][size][lz4][4B]"""
    d = open(img, 'rb').read()
    reg = d[ramdisk_off: ramdisk_off + ramdisk_size]
    p = reg.find(struct.pack('<I', MAGIC))
    if p < 0:
        raise SystemExit('ramdisk 容器 magic 未找到')
    size_field = struct.unpack('<I', reg[p+4:p+8])[0]
    lz4 = reg[p+8: p+8 + (size_field - 0x604)]
    cpio = lz4_block_decode(lz4)
    return cpio

def pack_container(cpio_path, out_path):
    cpio = open(cpio_path, 'rb').read()
    lz4 = lz4_block_encode(cpio)
    size_field = 0x604 + len(lz4)
    blob = struct.pack('<II', MAGIC, size_field) + lz4 + b'\x00' * 4
    open(out_path, 'wb').write(blob)
    return len(lz4)

if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'unpack':
        img, koff, rsize, out = sys.argv[2:6]
        cpio = unpack_container(img, int(koff, 0), int(rsize, 0))
        open(out, 'wb').write(cpio)
        print(f'unpacked {len(cpio)} bytes -> {out}')
    elif cmd == 'pack':
        n = pack_container(sys.argv[2], sys.argv[3])
        print(f'packed (lz4={n}, size_field={0x604+n:#x}, ramdisk_size={0x604+n+8:#x})')
    elif cmd == 'test':
        # 回环验证: 解码 stock boot ramdisk -> 重新编码 -> 再解码, 断言一致
        cpio = unpack_container('Image/boot_a.img', 0x2489a00, 0x1f79b9)
        open('/tmp/rt.cpio', 'wb').write(cpio)
        lz4 = lz4_block_encode(cpio)
        blob = struct.pack('<II', MAGIC, 0x604 + len(lz4)) + lz4 + b'\x00' * 4
        back = lz4_block_decode(blob[8: 8 + len(lz4)])
        assert back == cpio, 'round-trip MISMATCH'
        print(f'round-trip OK: cpio {len(cpio)} bytes, lz4 {len(lz4)} bytes '
              f'(stock lz4 was 2061229), ratio {len(lz4)/len(cpio):.2f}')
    else:
        raise SystemExit('用法: unpack/pack/test')
