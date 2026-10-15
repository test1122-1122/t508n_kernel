import os, glob, re

def title(p):
    try:
        for line in open(p, encoding='utf-8', errors='replace'):
            if line.startswith('#'):
                return re.sub(r'^#+\s*', '', line).strip()
    except Exception:
        pass
    return ''

def rows(paths, link_prefix=''):
    out = []
    for p in sorted(paths):
        if not os.path.isfile(p): continue
        b = os.path.basename(p)
        out.append((b, title(p), f'{link_prefix}{b}'))
    return out

def table(items):
    lines = ['| 文档 | 说明 |', '|---|---|']
    for b, t, link in sorted(items):
        lines.append(f'| [{b}]({link}) | {t or "—"} |')
    return '\n'.join(lines)

top = [(os.path.basename(p), title(p), os.path.basename(p)) for p in glob.glob('t508/docs/*.md')]
reports = rows(glob.glob('t508/docs/reports/*.md'), 'reports/')
prompts = rows(glob.glob('t508/docs/prompts/*.md'), 'prompts/')
disasm  = [(os.path.basename(p), title(p), '../work/disasm/'+os.path.basename(p)) for p in glob.glob('t508/work/disasm/DISASM-*.md')]

doc = f'''# T508N 资料索引

> `t508/docs/` 下全部资料的逐份说明。**继续工作请先看 [FINAL-BRINGUP-INDEX.md](FINAL-BRINGUP-INDEX.md)。**
>
> 说明：本目录的历史报告保留**原文路径**（多为重组前的 `/home/test/t508n/work/...`），
> 换算见 [../../docs/PATH-MAPPING.md](../../docs/PATH-MAPPING.md)。

## 1. 入口与总纲

{table(top)}

## 2. 分析 / 实验报告（`reports/`，{len(reports)} 篇）

按主题归类：

* **启动链与 stock 分析**：`REPORT-T508N-启动流程分析.md`、`REPORT-T508N-stock-analysis.md`、`REPORT-UMS9620-mainline适配.md`
* **mainline 适配与紫光魔改**：`REPORT-T508N-mainline-bringup.md`、`REPORT-UNISOC-MODS-ON-612.md`、`REPORT-UNISOC-ONLY-CONFIGS.md`、`UNISOC-DETAILED.md`、`UNISOC-MODS-AND-CONFIG.md`
* **同步 / 竞态 / LSE 深挖**：`REPORT-stock-sync-mechanism.md`、`REPORT-stock-vs-mainline-locks.md`、`REPORT-stock-vs-mainline-completion.md`、`REPORT-mainline-completion-wakeup-analysis.md`、`REPORT-stock-wakeup-chain-analysis.md`、`REPORT-stock-wakeup-nohz.md`、`REPORT-stock-alternatives-waiter-analysis.md`、`REPORT-stock-crosscluster-visibility.md`、`REPORT-CURRENT-RACE-ANALYSIS.md`、`REPORT-LSE-atomic-completion-analysis.md`、`REPORT-alt-table-integrity.md`
* **driver_init / 内存初始化卡点**：`REPORT-stock-driver-init.md`、`REPORT-stock-memory-dev-init.md`、`REPORT-stock-of-core-init.md`、`REPORT-stock-page-alloc-init.md`、`REPORT-stock-zone-memory.md`
* **USB 专项**：`REPORT-USB-BOOT-FLOW.md`、`REPORT-USB-CONSOLIDATED.md`、`REPORT-STOCK-USB-STACK-MECHANISM.md`、`REPORT-SELF-BUILT-USB-CHAIN.md`、`TASK-A-B-USB-DTS-C-离线成果.md`、`TASK-A-deepdive-SSPHY-clock.md`
* **vddusb33 调压器**：`TASK-vddusb33-regulator-fix.md`、`GUIDE-vddusb33-stock-design.md`、`REPORT-vddusb33-variant-matrix-offline.md`
* **构建与刷机**：`T508N-VENDOR-BUILD-RECIPE.md`、`REPORT-54-custom-build.md`、`REPORT-54-probe-port.md`、`FLASH-20260821-P1.md`
* **交接与总结**：`HANDOFF-3-SESSIONS.md`、`FINAL-WRAPUP-20260821.md`、`ARCHIVE-20260823.md`、`SUMMARY-ALL-ANALYSIS.md`、`SESSION-CONTEXT-COMPACT.md`、`SESSION-SPLIT-AND-WORKFLOW.md`、`REPORT-ROOTFS-COMPARE.md`、`REPORT-A76-requirements.md`、`REPORT-SML-psci-cpu-on.md`、`REPORT-AGENTS-GUIDE-analysis.md`、`WORK-适配材料清单.md`（在上一级）

完整清单：

{table(reports)}

## 3. 任务书 prompt（`prompts/`，{len(prompts)} 篇）

这些是派发给子会话的任务书，保留以便追溯每份报告的输入与验收标准。

{table(prompts)}

## 4. 原厂内核逐子系统反汇编（`../work/disasm/`）

{table(disasm)}

同目录还有：`DISASM-C-irq-中断GIC/`、`disasm_g/` ~ `disasm_n/`、`E-devm-*.dump`、
`vmlinux_sections.txt`、`vmlinux_segments.txt`、`alt_table_records.csv` 等原始产物。

## 5. 会话归档

| 文件 | 说明 |
|---|---|
| [session/session.jsonl](session/session.jsonl) | 原始中断会话日志（20,340 行 / 9.7 MB）。**一般不要重读**，用 `FINAL-BRINGUP-INDEX.md` 与 `reports/SUMMARY-ALL-ANALYSIS.md` 代替 |

## 6. 相关目录

* 源码：[../src/README.md](../src/README.md)
* 固件：[../firmware/README.md](../firmware/README.md)
* 构建产物：[../build/README.md](../build/README.md)
* 分析工作区：[../work/README.md](../work/README.md)
'''
open('t508/docs/INDEX.md','w').write(doc)
print('wrote', len(doc))
