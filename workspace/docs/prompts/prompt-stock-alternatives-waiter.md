# 任务：原厂镜像深度分析——alternatives patch 与 waiter 唤醒的交互（针对当前卡点）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
当前卡点（已探针钉死）：

- `apply_alternatives_all()` 的 stop_machine（SM#2）卡住：8 核全部完成 fn + signal_done，
  `complete()` 被调用（探针 y=450 亮），但 CPU0 的 kernel_init 的 `wait_for_completion` 永不返回
  （y=440 完成计数只有 1 格 = 只有 SM#1 完成）。
- **最新关键探针事实**：SM#2 阶段有任务被 ttwu 直接入队（y=460 x=296 亮、y=470 x=152/188 亮），
  但 **kernel_init（pid==1）从未被 ttwu_queue 处理**（y=470 x=224/x=296 不亮）。
  → 推导：complete() 被调用（y=450 亮）→ swake_up_locked → 若 swait 队列有 kernel_init 必调
  try_to_wake_up → 必进 ttwu_queue → pid==1 探针必亮。**不亮 ⟹ 要么 swait 队列空
  （kernel_init 没入队），要么 try_to_wake_up 提前返回（on_rq=1 / state 异常）**。
- SM#1（enable_cpu_capabilities 的 stop_machine）正常完成（kernel_init 被成功唤醒）。
  SM#1→SM#2 之间唯一大事件 = SM#2 的 12,000+ 条 alternatives patch（LSE 12,074 +
  repeat-TLBI 99 + speculative-AT 24）+ `ic ialluis` 8 核广播（已实测 DIC=0，ic ialluis 真实执行）。
- 综合假设：**SM#2 的 patch 收尾后，某核执行了半新半旧指令（I-cache 广播不完整/patch 与执行
  交错），破坏了 kernel_init 的调度器状态（swait 队列项被摘/on_rq 残留/__state 异常）→
  complete 的唤醒失效**。需要从原厂镜像找对照：原厂同样做系统级 alternatives + ic ialluis，
  为什么原厂能跑。

## 材料
- /home/test/t508n/work/stock_kallsyms.map        ← 正确原厂符号表（116,704 符号）
- /home/test/t508n/work/stock_Image_full.bin      ← 原厂内核，可反汇编
- /home/test/t508n/work/kernel.config             ← 原厂 5.4 config
- /home/test/t508n/work/stock-5.4.210/            ← 已拉取的 5.4.210 上游源码（对照用）
- /home/test/t508n/linux-6.12.103/                ← mainline 6.12.103 源码
- 已有报告（先读，避免重复）：
  - /home/test/t508n/work/REPORT-stock-wakeup-chain-analysis.md（唤醒链逐环节等价结论）
  - /home/test/t508n/work/REPORT-stock-sync-mechanism.md（multi_stop 协议）
  - /home/test/t508n/work/REPORT-stock-vs-mainline-completion.md（completion 原语对比）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。链接基址 _text=0xffffffc010080000。

## 任务（逐项做）

### 1. 原厂 wait_for_completion 完整反汇编（0xffffffc0116a0d30）
- waiter 侧精确序列：查 done → 入队（waitqueue）→ set_current_state → schedule → 唤醒后重查
- 与 mainline 6.12 swait 版 do_wait_for_common（kernel/sched/completion.c:80-116）逐行对比
- **重点：原厂 waiter 入队与 state 设置的顺序、锁的使用、唤醒后摘链时机**

### 2. 原厂 complete 完整反汇编（0xffffffc010227a70）
- complete_with_flags 等价物：done++ → __wake_up_common → autoremove_wake_function 的精确序列
- exclusive waiter 的唤醒语义（nr_exclusive=1 如何实现）
- 唤醒后 waiter 摘链时机（在 try_to_wake_up 前还是后？）
- 对比 mainline swake_up_locked（kernel/sched/swait.c）

### 3. 原厂系统级 alternatives 的调用链与规模（核心对比点）
- 从 stock_kallsyms.map 找：apply_alternatives_all、setup_system_capabilities、
  setup_system_features、smp_cpus_done、enable_cpu_capabilities、__apply_alternatives
- 反汇编确认：原厂 apply_alternatives_all 的调用上下文（单核还是 stop_machine？8 核在线？）
- **原厂 alternatives 规模**：分析原厂 __alt_instructions 段（在 vmlinux 布局中找
  __alt_instructions/__alt_instructions_end 符号或从反汇编估算），数一下原厂有多少条
  alternatives（LSE/errata 等）？对比 mainline 的 12,000+ 条
- **原厂 kernel.config 的 CONFIG_ARM64_ERRATUM_* 与 LSE 相关配置**（读 work/kernel.config）
- **结论：原厂系统级 patch 的量级 vs mainline（12,000+ 条）差距多大？**

### 4. 原厂 patch 期间的核状态与 I-cache 一致性
- 原厂系统级 patch 时（8 核在线），其他 7 核在干什么（spin？中断开？）
- 原厂 ic ialluis 之后有没有额外措施（每个核的 isb？patch 后 sync？）
- 原厂 patch 收尾后到"第一个跨核唤醒"之间的指令流（有没有原厂独有的 barrier/sync）

### 5. 原厂调度器数据结构 vs patch 的交互
- 原厂 5.4 的 waitqueue/swait 队列项（wait_queue_entry）在 task_struct 里的位置
- 原厂 alternatives 的替代点是否可能覆盖调度器/唤醒相关代码（对比 mainline 12K 条中
  落在 kernel/sched/ 的替代点数量——从 mainline 源码 alt 表统计）
- **结论：mainline 大规模 patch 是否更可能"碰到"调度器/唤醒代码？**

### 6. 综合回答
- **原厂与 mainline 在"系统级 alternatives patch + waiter 唤醒"场景的最关键差异**是什么？
- 原厂有没有任何机制（显式或隐式）避免"patch 影响调度器状态/唤醒"？
- 对当前卡点（complete 被调但 waiter 不醒、pid==1 探针不亮）给出最可能的解释和验证方向

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-alternatives-waiter-analysis.md
- 每任务一节：原厂反汇编片段 + 地址 + mainline 对应 + 差异结论
- 最后：最可疑差异 + 可操作验证建议

## 注意
- 符号表用 work/stock_kallsyms.map（不要用 work/System.map）
- 已有报告覆盖的内容（唤醒链等价、completion 原语、multi_stop 协议）直接引用，不要重复反汇编
- 本任务的价值增量：**原厂 alternatives 规模对比 + 原厂 waiter 侧精确序列 + patch 与调度交互**
