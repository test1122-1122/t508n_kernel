#!/usr/bin/env python3
"""Parse Unisoc super.img LP metadata and extract logical partition images."""
import struct, sys, os
d = open('Image/super.img','rb').read()
def find_geometry(d):
    for off in range(0, 0x8000, 0x1000):
        if d[off:off+4] == b'gDla':
            return off
    raise SystemExit('no geometry magic found')
goff = find_geometry(d)
print('geometry at', hex(goff))
magic, smax, mmax, mslot, lbs = struct.unpack('<4sIIII', d[goff:goff+20])
print('geom: struct_max=%#x metadata_max=%#x slots=%d logical_block=%#x' % (smax, mmax, mslot, lbs))
# metadata header right after two geometry slots?
for cand in (goff+0x1000, goff+0x2000, 0x3000, 0x4000):
    if d[cand:cand+4] == b'0LPA':
        moff = cand; break
print('metadata at', hex(moff))
hdr = d[moff:moff+64]
magic2, major, minor, hsize, hcsum, tsize, tcsum = struct.unpack('<4sHHIIII', hdr[:24])
poff, pnum, eoff, enum, groff, gnum, bdoff, bdnum = struct.unpack('<8I', hdr[24:56])
print(f'metadata: ver={major}.{minor} header_size={hsize} partitions={pnum} extents={enum} groups={gnum} blockdevs={bdnum}')
base = moff
groups = []
for i in range(gnum):
    g = d[base+groff+i*48: base+groff+i*48+48]
    name = g[:36].split(b'\x00')[0].decode()
    flags, maxsize = struct.unpack('<IQ', g[36:48])
    groups.append((name, flags, maxsize))
print('groups:', groups)
bds = []
for i in range(bdnum):
    b = d[base+bdoff+i*64: base+bdoff+i*64+64]
    fls, align, aoff, bdsz = struct.unpack('<QIIQ', b[:24])
    pname = b[24:60].split(b'\x00')[0].decode()
    flags = struct.unpack('<I', b[60:64])[0]
    bds.append((fls, align, aoff, bdsz, pname, flags))
    print(f'blockdev: {pname} size={bdsz:#x} first_logical_sector={fls}')
parts = []
for i in range(pnum):
    p = d[base+poff+i*56: base+poff+i*56+56]
    name = p[:36].split(b'\x00')[0].decode()
    attr, fei, ne = struct.unpack('<III', p[36:48])
    gi = struct.unpack('<I', p[48:52])[0]
    parts.append((name, attr, fei, ne, gi))
    print(f'partition: {name} attrs={attr:#x} extents[{fei}..{fei+ne}) group={gi}')
os.makedirs('work/super_raw', exist_ok=True)
for name, attr, fei, ne, gi in parts:
    extents = []
    for i in range(fei, fei+ne):
        e = d[base+eoff+i*24: base+eoff+i*24+24]
        nsect, ttype, tdata, tsrc = struct.unpack('<QIQI', e)
        extents.append((nsect, ttype, tdata, tsrc))
    total = sum(ns for ns,_,_,_ in extents)*512
    print(f'  {name}: {total} bytes, {ne} extents')
    with open(f'work/super_raw/{name}.img','wb') as f:
        for nsect, ttype, tdata, tsrc in extents:
            if ttype == 0:
                f.write(d[tdata*512:(tdata+nsect)*512])
            elif ttype == 1:
                f.write(b'\x00'*(nsect*512))
            else:
                print(f'   unknown extent type {ttype}')
