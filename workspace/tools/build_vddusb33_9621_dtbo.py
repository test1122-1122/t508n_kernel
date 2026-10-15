#!/usr/bin/env python3
"""构建 F1 变体矩阵修复的 DTBO 分区镜像 (dt_table 大端, 1 条目, 8MB 分区格式).

用法 (需要 dtc 在 PATH):
  python3 tools/build_vddusb33_9621_dtbo.py [--overlay work/dtbo-overlay-vddusb33-9621.dts] [--out /tmp/vddusb33-9621.img]

匹配规则与 stock/noop 一致: board_id 0x2700, sc-id "ums9620 OT5 1000" (见 tools/build_noop_dtbo.py)。
输出: /tmp/vddusb33-9621.img (dt_table 头 + overlay, 供 fastboot/dd 刷入 dtbo_a)。

⚠️ 仅当 T1 机上证实 pmic@0 compatible == "sprd,ump9621" 时使用;
   若 booted DTB 为库存 (pmic@0="sprd,ump9620"), 不要刷本镜像 (见
   work/REPORT-vddusb33-variant-matrix-offline.md §4 决策树)。
"""
import struct, subprocess, sys, os, argparse

def build(overlay_dts: str, out_img: str) -> None:
    tmp_dtbo = '/tmp/vddusb33-9621.dtbo'
    subprocess.run(['dtc', '-@', '-I', 'dts', '-O', 'dtb',
                    '-o', tmp_dtbo, overlay_dts], check=True)
    dtb = open(tmp_dtbo, 'rb').read()
    page = 2048
    dt_offset = page
    total = dt_offset + ((len(dtb) + page - 1) // page) * page
    # dt_table header: magic, total_size, header_size, dt_entry_size, entry_count,
    #                  dt_entry_offset, page_size, version
    hdr = struct.pack('>8I', 0xd7b7ab1e, total, 32, 32, 1, 32, page, 0)
    # dt_entry: dt_size, dt_offset, id, rev, custom0-3 (board_id 0x2700 放 id)
    entry = struct.pack('>8I', len(dtb), dt_offset, 0x2700, 0, 0, 0, 0, 0)
    img = hdr + entry + b'\x00' * (dt_offset - 32 - 32) + dtb
    img = img[:total]
    with open(out_img, 'wb') as f:
        f.write(img)
    print(f'{out_img}: {len(img)} bytes, 1 entry (board_id 0x2700), overlay={overlay_dts}')
    print(f'  overlay dtb = {len(dtb)} bytes -> dtc OK')

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--overlay', default='work/dtbo-overlay-vddusb33-9621.dts')
    ap.add_argument('--out', default='/tmp/vddusb33-9621.img')
    args = ap.parse_args()
    build(args.overlay, args.out)
