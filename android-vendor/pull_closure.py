"""Pull an Android binary and its shared-library closure from the device, preserving paths.
Search order approximates the vendor linker namespace: vendor, VNDK apex, system, bionic."""
import struct, subprocess, sys, os
from pathlib import Path
ROOT=Path(os.environ.get('MU300_CLOSURE_ROOT','rootfs'))
SEARCH=['/vendor/lib64','/vendor/lib64/hw','/vendor/lib64/egl','/odm/lib64','/apex/com.android.vndk.v33/lib64',
        '/system/lib64','/apex/com.android.runtime/lib64/bionic','/apex/com.android.runtime/lib64',
        '/apex/com.android.i18n/lib64','/system_ext/lib64']
import time
def adb(cmd):
    # adb drops out when the device reboots; never treat that as "file missing"
    for attempt in range(30):
        r=subprocess.run(['adb','exec-out',f"su -c '{cmd}; echo __RC$?'"],capture_output=True,stdin=subprocess.DEVNULL)
        out=r.stdout
        i=out.rfind(b'__RC')
        if r.returncode==0 and i>=0:
            return out[:i] if out[i+4:].strip()==b'0' else b''
        time.sleep(5)
    raise SystemExit('adb unavailable')
def pull(path):
    dst=ROOT/path.lstrip('/')
    if dst.exists(): return dst
    real=adb(f'readlink -f {path}').decode().strip()
    data=adb(f'cat {real}')
    if not data.startswith(b'\x7fELF') and path.endswith('.so'): return None
    dst.parent.mkdir(parents=True,exist_ok=True); dst.write_bytes(data); return dst
def needed(p):
    b=p.read_bytes()
    if b[:4]!=b'\x7fELF': return []
    shoff,=struct.unpack_from('<Q',b,0x28); phoff,=struct.unpack_from('<Q',b,0x20)
    phnum,=struct.unpack_from('<H',b,0x38)
    dyn=None; loads=[]
    for i in range(phnum):
        t,f,off,va,pa,fs,ms,al=struct.unpack_from('<IIQQQQQQ',b,phoff+i*56)
        if t==2: dyn=(off,fs)
        if t==1: loads.append((va,off,fs))
    if not dyn: return []
    ents=[struct.unpack_from('<qQ',b,dyn[0]+j) for j in range(0,dyn[1],16)]
    strtab=[v for t,v in ents if t==5][0]
    stroff=[o+strtab-va for va,o,fs in loads if va<=strtab<va+fs][0]
    out=[]
    for t,v in ents:
        if t==1:
            e=b.index(b'\0',stroff+v); out.append(b[stroff+v:e].decode())
    return out
todo=list(sys.argv[1:]); seen=set(); missing=[]
while todo:
    path=todo.pop()
    if path in seen: continue
    seen.add(path)
    p=pull(path)
    if not p: missing.append(path); continue
    for lib in needed(p):
        for d in SEARCH:
            cand=f'{d}/{lib}'
            if cand in seen: break
            if adb(f'[ -e {cand} ] && echo y').strip().endswith(b'y'):
                todo.append(cand); break
        else: missing.append(lib)
print('pulled',len(seen)-len(missing)); print('missing',sorted(set(missing)))
