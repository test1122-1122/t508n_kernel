import re, os, sys
files = ['README.md','docs/INDEX.md','docs/WORKSPACE.md','docs/PATH-MAPPING.md','docs/cross/README.md',
 'shared/README.md',
 't508/README.md','t508/docs/INDEX.md','t508/src/README.md','t508/firmware/README.md',
 't508/build/README.md','t508/work/README.md','t508/tools/README.md',
 'v50/README.md','v50/docs/INDEX.md','v50/src/README.md','v50/firmware/README.md',
 'v50/work/README.md','v50/build/README.md','v50/tools/README.md']
bad = 0
for f in files:
    if not os.path.isfile(f):
        print('MISSING FILE', f); bad += 1; continue
    base = os.path.dirname(f)
    for m in re.finditer(r'\]\(([^)]+)\)', open(f, encoding='utf-8').read()):
        t = m.group(1).strip()
        if t.startswith(('http://','https://','#','mailto:')): continue
        t = t.split('#')[0]
        if not t: continue
        p = os.path.normpath(os.path.join(base, t))
        if not os.path.exists(p):
            print(f'BROKEN  {f}  ->  {t}'); bad += 1
print('broken links:', bad)
