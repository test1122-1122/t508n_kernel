#!/usr/bin/env python3
"""构建空操作 DTBO 分区镜像 (8MB, 大端 dt_table, 1 条目)
匹配规则与 stock 一致 (board_id 0x2700, sc-id ums9620 OT5 1000),
但 overlay 为空 -> LK 合并必然成功, 不破坏 mainline DTB。
"""
import struct, subprocess, sys, os

OVERLAY = """/dts-v1/;
/plugin/;
/ {
\tsprd,sc-id = "ums9620 OT5 1000";
\tsprd,board_id = <0x2700>;
\tfragment@0 {
\t\ttarget-path = "/";
\t\t__overlay__ {
\t\t};
\t};
};
"""
open('/tmp/noop_overlay.dts','w').write(OVERLAY)
subprocess.run(['dtc','-@','-I','dts','-O','dtb','-o','/tmp/noop_overlay.dtbo','/tmp/noop_overlay.dts'], check=True)
dtb = open('/tmp/noop_overlay.dtbo','rb').read()
# dt_table: 大端
page = 2048
dt_offset = page
total = dt_offset + ((len(dtb)+page-1)//page)*page
hdr = struct.pack('>8I', 0xd7b7ab1e, total, 32, 32, 1, 32, page, 0)
entry = struct.pack('>8I', len(dtb), dt_offset, 0x2700, 0, 0,0,0,0)
img = hdr + entry + b'\x00'*(dt_offset - 32 - 32) + dtb
img = img[:total]                      # 只保留表+DTB, 供 fastboot 刷入(分区 8MB)
out = '/tmp/noop_unsigned.img'
open(out,'wb').write(img)
open(out,'wb').write(img)
print(f'{out}: {len(img)} bytes, 1 entry (board_id 0x2700, overlay 空)')
