#!/usr/bin/env python3
"""Walk a newc (070701/070702) cpio stream starting at a given offset."""
import sys, os, struct
def parse(fn, start, outdir, limit=None):
    d = open(fn,'rb').read()
    end = limit if limit else len(d)
    off = start
    os.makedirs(outdir, exist_ok=True)
    count = 0
    while off + 110 <= end:
        magic = d[off:off+6]
        if magic not in (b'070701', b'070702'):
            print(f'  stop at {off:#x}: magic={magic!r}')
            break
        hdr = d[off:off+110]
        # newc: 6 magic + 13 fields of 8 hex chars
        vals = [int(hdr[6+i*8:14+i*8], 16) for i in range(13)]
        ino, mode, uid, gid, nlink, mtime, filesize, devmaj, devmin, rdevmaj, rdevmin, namesize, check = vals
        name = d[off+110:off+110+namesize-1].decode('utf-8', 'replace')
        data_start = off + 110 + namesize
        data_start = (data_start + 3) & ~3
        data = d[data_start:data_start+filesize]
        count += 1
        if name == 'TRAILER!!!':
            print(f'  trailer at {off:#x}; entries={count}')
            break
        path = os.path.join(outdir, name.lstrip('/'))
        if filesize > 0 and not name.endswith('/'):
            os.makedirs(os.path.dirname(path) or outdir, exist_ok=True)
            open(path,'wb').write(data)
        off = data_start + filesize
        off = (off + 3) & ~3
    else:
        print(f'  ran to end at {off:#x}, entries={count}')
if __name__ == '__main__':
    fn, start, outdir = sys.argv[1], int(sys.argv[2], 0), sys.argv[3]
    limit = int(sys.argv[4], 0) if len(sys.argv) > 4 else None
    parse(fn, start, outdir, limit)
