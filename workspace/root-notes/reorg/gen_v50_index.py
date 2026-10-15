import glob, os, re
def title(p):
    try:
        for line in open(p, encoding='utf-8', errors='replace'):
            if line.startswith('#'):
                return re.sub(r'^#+\s*', '', line).strip()
    except Exception: pass
    return ''
def table(paths, link):
    rows=[]
    for p in sorted(paths):
        if os.path.isfile(p):
            b=os.path.basename(p)
            rows.append(f'| [{b}]({link(b)}) | {title(p) or "—"} |')
    return '\n'.join(['| 文档 | 说明 |','|---|---|']+rows)

reports = table(glob.glob('v50/docs/reports/*.md'), lambda b: 'reports/'+b)
rev     = table(glob.glob('v50/work/rev/*.md'),   lambda b: '../work/rev/'+b)
repo    = table(glob.glob('v50/src/mu300-linux/docs/*.md'), lambda b: '../src/mu300-linux/docs/'+b)

doc = f'''# V50 / F50 资料索引

> 本目录（`v50/docs/`）保存**仓库之外的专题分析与逆向报告**。
> 项目权威文档在[主仓库 `docs/`](../src/mu300-linux/docs/INDEX.md)，本页汇总三处。

## 1. 主仓库文档（权威、持续维护）

来源：[`v50/src/mu300-linux/docs/`](../src/mu300-linux/docs/)（原 `v50-linux/docs`）。

{repo}

## 2. 逆向分析报告（`../work/rev/`）

对 ZTE 官方 V50/MU3351 固件的逆向成果，是主仓库 `docs/` 的证据来源。
**索引说明**：[../work/rev/INDEX.md](../work/rev/INDEX.md)。

{rev}

## 3. 专题分析报告（`reports/`）

{reports}

## 4. 原始产物

| 位置 | 内容 |
|---|---|
| [`../work/rev/`](../work/rev/) | `hw-nodes.tsv`（V50 DT 提取，957 节点/408 个 compatible）、`dts-nodes.py`、`jadx*/`（APK 反编译）、`dex/`、`sml_full.dis`（SML 反汇编）、`v50-marquee/` |
| [`../work/rev/REPORT-V50-社区跑马灯插件分析.md`](../work/rev/REPORT-V50-社区跑马灯插件分析.md) | 对应的插件样本在 `../work/rev/v50-marquee/` |
| [`../firmware/v50-backup/`](../firmware/v50-backup/) | V50 分区备份 + `REPORT-V50-NFC-GPS分析.md`、`REPORT-V50-WebUI-GPS调用链.md` |
| [`../src/zte-oss/`](../src/zte-oss/) | ZTE 开源门户抓取的源码包与网页（`WebTools.js`、`*_tree.txt`、`api_samples/`） |

## 5. 相关

* 项目总览：[../README.md](../README.md)
* 源码清单：[../src/README.md](../src/README.md)
* 固件清单：[../firmware/README.md](../firmware/README.md)
* 分析工作区：[../work/README.md](../work/README.md)
'''
open('v50/docs/INDEX.md','w').write(doc)
print('wrote', len(doc))
