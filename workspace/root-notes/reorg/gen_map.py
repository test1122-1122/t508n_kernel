import collections
rows = [l.rstrip('\n').split('\t') for l in open('.reorg/mapping.tsv') if l.strip()]
top, wk = [], []
for r in rows:
    if len(r) != 2: continue
    s, d = r
    if s.startswith('work/'):
        wk.append(('/home/test/t508n/' + s, '/home/test/t508n/' + d))
    elif '/' not in s:
        top.append(('/home/test/t508n/' + s, '/home/test/t508n/' + d))
def table(pairs):
    out = ['| 旧路径 | 新路径 |', '|---|---|']
    for a, b in sorted(pairs):
        out.append(f'| `{a}` | `{b}` |')
    return '\n'.join(out)

head = '''# 路径映射表（旧 → 新）

> 2026-09-29 工作区重组后生成。**历史报告/文档中的旧绝对路径一律保留原文**（不篡改溯源），
> 按本表换算即可。脚本类的硬编码路径已同步修复（见文末）。

## 1. 顶层路径映射

除下表外，下列两项是“内容合并”而非整体改名：

| 旧路径 | 新路径 | 说明 |
|---|---|---|
| `/home/test/t508n/dl/` | `/home/test/t508n/shared/dl/` | 下载缓存，4 个压缩包并入既有目录 |
| `/home/test/t508n/tools/` | `/home/test/t508n/t508/tools/` | 构建工具并入，`restore_stock.bat`、`fetch_ums9620_ref.sh` 也归入此处 |

'''
tail = '''

## 3. 旧路径兼容符号链接

以下**顶层**符号链接被创建，使旧的顶层入口路径继续可用（均指向真实新位置，可安全删除）：

'''
gen = head + table(top) + '\n\n## 2. `work/` 内条目映射\n\n原 `work/` 同时混放两个项目的资料。下表为逐项归位结果。\n\n' + table(wk) + tail
open('docs/PATH-MAPPING.md','w').write(gen)
print('wrote', len(gen), 'bytes; top=', len(top), 'work=', len(wk))
