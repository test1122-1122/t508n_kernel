#!/usr/bin/env python3
import sys, struct
def lz4_block_decode(data, maxout=1<<28):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        token = data[i]; i += 1
        lit_len = token >> 4
        if lit_len == 15:
            while True:
                b = data[i]; i += 1
                lit_len += b
                if b != 255: break
        if i + lit_len > n:
            raise ValueError(f'literals overrun at {i}')
        out += data[i:i+lit_len]; i += lit_len
        if i >= n:
            break
        offset = struct.unpack('<H', data[i:i+2])[0]; i += 2
        if offset == 0 or offset > len(out):
            raise ValueError(f'bad offset {offset} at {i}')
        match_len = token & 0xF
        if match_len == 15:
            while True:
                b = data[i]; i += 1
                match_len += b
                if b != 255: break
        match_len += 4
        src = len(out) - offset
        for k in range(match_len):
            out.append(out[src + k])
        if len(out) > maxout:
            raise ValueError('output too large')
    return bytes(out)
if __name__ == '__main__':
    fn, off, out = sys.argv[1], int(sys.argv[2],0), sys.argv[3]
    d = open(fn,'rb').read()
    dec = lz4_block_decode(d[off:])
    open(out,'wb').write(dec)
    print(f'decoded {len(dec)} bytes -> {out}')
