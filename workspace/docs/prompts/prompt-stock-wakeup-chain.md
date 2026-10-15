# 任务：原厂 5.4 唤醒链反汇编对比（为什么原厂 SM#2 后 CPU0 能正常调度 waiter）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12 bring-up。
当前卡点（已精确定位到最后一环）：

- `apply_alternatives_all()` 的 stop_machine（SM#2）卡住：8 核全部完成 fn + signal_done，
  `complete()` 被调用（探针 y=450），但 CPU0 的 kernel_init 的 `wait_for_completion` 不返回。
- 已实测：SM#2 的唤醒**没有走 IPI_CALL_FUNC**（wakelist），而是 **IPI_RESCHEDULE 到达 CPU0**
  （探针 y=460 x=188 亮）→ 推导：waker（SM#2 complete 的核，与 CPU0 同为 A55 簇、共享 L2）
  走 **ttwu 直接入队**路径 → `resched_curr` 跨核发 `smp_send_reschedule` → IPI_RESCHEDULE。
- 中断链活着：SGI/PPI 都能到 CPU0（do_handle_IPI、timer_handler 探针都亮），CPU0 DAIF 正常。
- 但 kernel_init 始终不被调度。需要知道：**原厂 5.4 在这个场景（stop_machine 完成后
  waiter 唤醒）是怎么做的，与 mainline 6.12 的差异在哪**。

## 材料
- /home/test/t508n/work/stock_kallsyms.map  ← 正确原厂符号表（116,704 符号）
- /home/test/t508n/work/stock_Image_full.bin ← 原厂内核，可反汇编
- /home/test/t508n/linux-6.12.103/           ← mainline 6.12.103 源码
- /home/test/t508n/work/REPORT-stock-vs-mainline-completion.md  ← 已完成：completion 原语对比
- /home/test/t508n/work/REPORT-stock-sync-mechanism.md          ← 已完成：multi_stop 协议对比
- /home/test/t508n/work/REPORT-mainline-completion-wakeup-analysis.md ← 已完成：mainline 唤醒链

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。链接基址 _text=0xffffffc010080000。

## 原厂符号地址（已查好，直接用）
```
cpu_stopper_thread    0xffffffc01030adc8
smpboot_thread_fn     0xffffffc0101e3dc8
resched_curr          0xffffffc0101f3b30
try_to_wake_up        0xffffffc0101f6084
scheduler_ipi         0xffffffc0101fc658
smp_send_reschedule   0xffffffc010117470
smp_cross_call        0xffffffc0101169b4
ipi_handler           0xffffffc010116cac
complete              0xffffffc010227a70
```
（do_handle_IPI 可能无独立符号，从 ipi_handler 反汇编内找；wait_for_completion 从
try_wait_for_completion 或 complete 附近找）

## 任务（逐项做，每项给出反汇编片段 + mainline 对应源码 + 差异结论）

### 1. 原厂 cpu_stopper_thread（0xffffffc01030adc8）的退出路径
- signal_done 之后的指令：是否 goto repeat（取下一个 work）？work 为空时返回？
- 返回后 smpboot_thread_fn（0xffffffc0101e3dc8）如何 schedule（schedule_idle? schedule_preempt_disabled? 直接 schedule?）
- 对比 mainline 6.12 kernel/stop_machine.c cpu_stopper_thread + kernel/smpboot.c smpboot_thread_fn
- **重点：原厂 stopper 线程在 preempt_count 上的处理（有没有 preempt_count_inc？）vs mainline（mainline 有）**

### 2. 原厂 resched_curr（0xffffffc0101f3b30）
- 跨核 reschedule：set_nr_and_not_polling / set_nr_if_polling / smp_send_reschedule 的指令序列
- 对比 mainline 6.12 resched_curr（kernel/sched/core.c:1083）
- **重点：arm64 原厂是否定义 TIF_POLLING_NRFLAG？idle 用 wfi 还是 polling？**（影响 set_nr_if_polling 分支）

### 3. 原厂 smp_send_reschedule（0xffffffc010117470）+ smp_cross_call（0xffffffc0101169b4）
- SGI 发送：GIC 哪个寄存器/接口？IPI 编号（IPI_RESCHEDULE 是多少？）
- 对比 mainline arch/arm64/kernel/smp.c

### 4. 原厂 ipi_handler（0xffffffc010116cac）→ IPI_RESCHEDULE 处理
- RESCHEDULE 的 case 调什么（scheduler_ipi? 直接 preempt_fold?）
- scheduler_ipi（0xffffffc0101fc658）的实际指令：是否 preempt_fold_need_resched？
- 对比 mainline：mainline 的 scheduler_ipi 是 include/linux/sched.h:1968 的 inline preempt_fold_need_resched

### 5. 原厂 try_to_wake_up（0xffffffc0101f6084）的跨核路径
- 5.4 时代有没有 ttwu_queue_wakelist？cpus_share_cache / ttwu_queue_cond 判断？
- 同 cluster（共享 L2）唤醒：直接入队 + resched_curr？
- 对比 mainline 6.12 try_to_wake_up（kernel/sched/core.c:4142）

### 6. 综合回答
- **原厂 SM#2 完成后，CPU0 的唤醒/调度链与 mainline 的关键差异是什么？**
- 原厂有没有什么 mainline 没有的"保证 waiter 被调度"机制（比如 stopper 退出路径不同、
  idle 处理不同、preempt_count 处理不同）？
- 最可能解释"原厂能跑、mainline 卡"的差异点（按可能性排序）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-wakeup-chain-analysis.md
- 每任务一节：原厂反汇编片段 + 地址 + mainline 对应 + 差异结论
- 最后：最可疑差异 + 可操作的移植/修复建议

## 注意
- 符号表用 work/stock_kallsyms.map（不要用 work/System.map）
- 原厂 5.4 函数内部可能有 inline 展开，反汇编时注意区分
- 结论要具体到指令/寄存器/函数级
