#!/usr/bin/env python3
import sys, os, struct, re
fn, outdir = sys.argv[1], sys.argv[2]
d = open(fn,'rb').read()
os.makedirs(outdir, exist_ok=True)
off = 0; n = len(d); entries=0; archives=0
while off + 110 <= n:
    if d[off:off+6] != b'070701':
        # skip forward to next 4-aligned 070701
        nxt = d.find(b'070701', off)
        if nxt < 0: break
        off = nxt
        archives += 1
        continue
    hdr = d[off:off+110]
    vals = [int(hdr[6+i*8:14+i*8],16) for i in range(13)]
    ino,mode,uid,gid,nlink,mtime,filesize,devmaj,devmin,rdevmaj,rdevmin,namesize,check = vals
    name = d[off+110:off+110+namesize-1].decode('utf-8','replace')
    ds = (off+110+namesize+3)&~3
    data = d[ds:ds+filesize]
    entries += 1
    if name == 'TRAILER!!!':
        off = (ds+filesize+3)&~3
        continue
    if filesize and not name.endswith('/'):
        p = os.path.join(outdir, name.lstrip('/'))
        os.makedirs(os.path.dirname(p) or outdir, exist_ok=True)
        try: open(p,'wb').write(data)
        except Exception as e: print('ERR', name, e)
    off = (ds+filesize+3)&~3
print(f'entries={entries} archives~={archives}')
