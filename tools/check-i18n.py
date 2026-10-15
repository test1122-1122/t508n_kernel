#!/usr/bin/env python3
"""Check the installer translations in i18n/<lang>.tsv against the messages the scripts use.

  tools/check-i18n.py            list, per language, the messages without a translation and the stale lines
  tools/check-i18n.py --keys     print every message key once (to start a new language)

A message is the first argument of t '...' / t "..." in the shell scripts and of T '...' / T "..." in install.ps1.
The {1}.. placeholders of a translation must be the ones of its message.
"""
import re
import sys
from pathlib import Path

TOP = Path(__file__).resolve().parent.parent
SOURCES = ['install.sh', 'install.ps1', 'tools/linux-mode.sh', 'tools/self-update.sh', 'tools/storage.sh',
           'android/magisk/installer/mu300-install.sh']
# single- or double-quoted first argument; the PowerShell side writes '' for a quote inside '...'
PATTERNS = [re.compile(r"""\b[tT] '((?:[^']|'')*)'"""), re.compile(r'''\b[tT] "([^"$`]*)"''')]
# shown as defaults of yes/no questions, the answers of the region check, and the error prefix
EXTRA = ['yes', 'no', 'update', 'ERROR:']


def keys():
    found = []
    for src in SOURCES:
        text = (TOP / src).read_text()
        for pat in PATTERNS:
            for m in pat.finditer(text):
                k = m.group(1)
                if src.endswith('.ps1'):
                    k = k.replace("''", "'")
                if k and k not in found:
                    found.append(k)
    return found + [k for k in EXTRA if k not in found]


def load(lang):
    table = {}
    for line in (TOP / 'i18n' / f'{lang}.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#') or '\t' not in line:
            continue
        k, v = line.split('\t', 1)
        table[k] = v
    return table


def main():
    all_keys = keys()
    if '--keys' in sys.argv:
        print('\n'.join(all_keys))
        return 0
    bad = 0
    for f in sorted((TOP / 'i18n').glob('*.tsv')):
        table = load(f.stem)
        missing = [k for k in all_keys if k not in table]
        stale = [k for k in table if k not in all_keys]
        holes = [k for k in table if k in all_keys and
                 sorted(re.findall(r'\{\d\}', k)) != sorted(re.findall(r'\{\d\}', table[k]))]
        print(f'{f.stem}: {len(table)} translated, {len(missing)} missing, {len(stale)} stale, {len(holes)} placeholder mismatches')
        for k in missing:
            print(f'  missing: {k}')
        for k in stale:
            print(f'  stale:   {k}')
        for k in holes:
            print(f'  placeholders differ: {k}')
        bad += len(missing) + len(holes)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
