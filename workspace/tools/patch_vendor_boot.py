#!/usr/bin/env python3
"""把自定义 DTB 替换进 vendor_boot.img 的 DTB 段 (解锁态使用, footer 校验由 LK 容忍)
注意: DTB 段开头是 vendor ramdisk table + 填充, FDT 魔数在其后, 必须保留前缀。
"""
import struct, sys
src, newdtb, out = sys.argv[1:4]
d = bytearray(open(src, 'rb').read())
hsize = struct.unpack('<I', d[2096:2100])[0]
vrs = struct.unpack('<I', d[24:28])[0]
dts = struct.unpack('<I', d[2100:2104])[0]
seg_off = hsize + vrs
seg = d[seg_off:seg_off+dts]
fdt = seg.find(b'\xd0\x0d\xfe\xed')
assert fdt >= 0, 'DTB 段内未找到 FDT 魔数'
nd = open(newdtb, 'rb').read()
assert fdt + len(nd) <= dts, f'新 DTB {len(nd)} 放不下 (可用 {dts-fdt} 字节)'
d[seg_off+fdt:seg_off+fdt+len(nd)] = nd
d[seg_off+fdt+len(nd):seg_off+dts] = b'\x00' * (dts - fdt - len(nd))
open(out, 'wb').write(d)
print(f'patched: {src} -> {out} (FDT @{seg_off+fdt:#x}, 新 DTB {len(nd)}B, 前缀 {fdt}B 保留)')
