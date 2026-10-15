# 任务：原厂 stop_machine/alternatives 同步机制分析 + 跨 cluster cache 一致性方案

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12 bring-up。
当前卡点：`apply_alternatives_all()` 里的 `stop_machine()` 卡住——
CPU0 的 `__apply_alternatives` 已完成、`all_alternatives_applied=1` 已置位，
但**部分二级核（推测 A55 cluster）读不到该标志，卡在
`while(!all_alternatives_applied)` 循环**。y=400（fn 返回）探针只有一半 CPU 亮。

我们已试过：
1. CPU0 置位后 `dcache_clean_poc(&all_alternatives_applied) + dsb(ishst)`
2. 二级核循环里每轮 `dcache_inval_poc`
结果待验证（刷机中），但按 T770 的 A76/A55 异构双 cluster 特性，
很可能 PoC 一致性本身有硬件层面问题（DSU/SCU 或互联），
dcache 指令可能不够。

## 材料
- /home/test/t508n/work/stock_kallsyms.map   （正确原厂符号表）
- /home/test/t508n/work/stock_Image_full.bin （原厂内核，objdump）
- /home/test/t508n/work/REPORT-stock-vs-mainline-locks.md （已完成的锁对比，含 __apply_alternatives 部分结论）
- /home/test/t508n/work/REPORT-T508N-stock-analysis.md （A.4 节有原厂 __apply_alternatives 末尾 ic ialluis）
- /home/test/t508n/linux-6.12.103/ （mainline 源码）

## 反汇编方法
```bash
aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<kallsyms地址> --stop-address=<+n> work/stock_Image_full.bin
```

## 任务

### 任务 1：原厂 __apply_alternatives_multi_stop 的同步机制（核心）
1. 在 stock_kallsyms.map 找：__apply_alternatives_multi_stop、apply_alternatives_all、
   stop_machine、stop_cpus、cpu_stop_signal_done、multi_cpu_stop、
   all_alternatives_applied（如果符号存在）
2. 反汇编 __apply_alternatives_multi_stop，回答：
   - 原厂二级核等 all_alternatives_applied 的循环里，有没有 dcache/dsb 操作？
   - 原厂 CPU0 置位 all_alternatives_applied 后，有没有 dcache_clean / dsb(ishst) /
     sev / 其他唤醒机制？
   - 原厂的循环是 `cpu_relax()` 还是 `wfe/sev` 事件机制？
3. 对比 mainline 6.12 的 __apply_alternatives_multi_stop：两者同步机制差异是什么？

### 任务 2：原厂 stop_machine 完成同步
1. 反汇编原厂 stop_cpus / __stop_cpus / cpu_stop_signal_done / multi_cpu_stop 状态机
2. 回答：原厂的完成同步（completion/atomic）与 mainline 是否有差异？
   原厂 atomic_dec_and_test 用 LSE 还是 LL/SC？

### 任务 3：原厂对跨 cluster 一致性的处理（关键）
1. 在原厂 kallsyms 搜索以下类别符号，判断原厂是否初始化/使用了一致性机制：
   - CCI/CCN/CMN/DSU/SCU/snoop：cci_*、ccn_*、cmn_*、dsu_*、scu_*、snoop_*
   - 架构 cache 维护：dcache_clean_poc、dcache_clean_all、icache_inval_all、
     flush_all 等
   - CPU power/cluster 唤醒相关：psci、cpu_suspend、cluster 相关
2. 重点：原厂 boot 早期（sml/TF-A 之外）有没有对 cluster/DSU 做初始化？
   还是完全依赖 SML（BL31）？
3. 原厂的 cache 维护函数（caches_clean_inval_pou 等）实现细节——
   有没有 T770 特有的 workaround（比如对 A55 cluster 的额外 clean）？

### 任务 4：SML（BL31）的一致性相关字符串/逻辑（可选的深度分析）
sml_a.bin 在 /home/test/t508n/payloads/。用 strings 搜：
- "snoop"、"SCU"、"DSU"、"cluster"、"coherency"、"cortex"、"power"、"cluster power"
- 判断 SML 是否初始化了 cluster/DSU 电源或一致性。

### 任务 5：我们当前方案的可行性评估
基于以上分析，回答：
1. `dcache_clean_poc + dsb(ishst)`（CPU0 置位后）+ 二级核 `dcache_inval_poc` 循环，
   理论上能否解决"读不到 all_alternatives_applied"？有什么不足？
2. 原厂有没有更优雅/更可靠的跨核唤醒机制（wfe/sev、IPI、或 stop_machine 的
   multi_stop 状态机本身）可以直接移植到 mainline？
3. 如果 PoC 一致性在硬件上就是不可靠，原厂如何保证？是否用了
   "每个 CPU 自己执行 __apply_alternatives"（而不是等 CPU0 完成）的方案？

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-sync-mechanism.md
- 每任务一节：原厂反汇编片段 + 地址 + 结论
- 最后给出：**推荐的可移植方案**（具体到函数/指令级），以及**还需要哪些分析**
- 特别回答：原厂 __apply_alternatives_multi_stop 的 while 循环里到底有没有
  dcache/dsb/wfe，这是当前最关键的对比点

## 注意
- 符号表用 stock_kallsyms.map（不要用 work/System.map，它是 mainline 构建产物）
- 原厂 5.4 符号命名可能不同，搜不到就 grep 近似名（如 "apply_alternatives"、"multi_stop"）
- 若 all_alternatives_applied 没有独立符号（static），通过 __apply_alternatives_multi_stop
  的反汇编上下文确认它引用的地址和同步逻辑
