# 任务 B：mainline 6.12 completion 唤醒链路在 stop_machine 上下文的分析

## 背景
T508N（Unisoc T770/UMS9620）上 mainline 6.12.103 bring-up。当前卡点：
`apply_alternatives_all()` 里的 `stop_machine()` 不返回。

已确认现象：
- 8 核全部进入/完成 `__apply_alternatives_multi_stop`（flag 可见性 OK）
- 8 核 `multi_cpu_stop` 状态机走完（thread_ack 归零 → EXIT）
- 8 核全部执行了 `cpu_stop_signal_done(done)`（探针 y=430 全亮）
- 但 `wait_for_completion(&done.completion)` 在 CPU0 不返回（探针 x=44 不亮）

纯 mainline 源码分析任务，**不需要刷机、不需要反汇编 stock**。

## 材料
- /home/test/t508n/linux-6.12.103/   ← mainline 6.12.103 源码
  - kernel/sched/completion.c        ← completion 用 swait
  - kernel/stop_machine.c            ← stop_machine / multi_cpu_stop / cpu_stopper_thread
  - kernel/sched/swait.c 或 include/linux/swait.h  ← swait 实现
  - kernel/sched/core.c              ← try_to_wake_up / wake_up_process
  - arch/arm64/kernel/smp.c          ← smp_send_reschedule / IPI
  - arch/arm64/kernel/ipi.c          ← IPI 分发
  - include/linux/atomic.h、arch/arm64/include/asm/atomic*.h  ← atomic_dec_and_test

## 任务：完整走读唤醒链路，找出可能丢唤醒/卡住的环节

### 1. 画出完整链路（务必精确到函数 + 行号）
从 `cpu_stop_signal_done()` 到 `wait_for_completion()` 返回：
```
cpu_stop_signal_done(done)
  └ atomic_dec_and_test(&done->nr_todo)   // 8→0, 最后一个是 true
      └ complete(&done->completion)
          └ complete_with_flags → done++ → swake_up_locked(&x->wait, flags)
              └ __wake_up_common? / swake_up_locked 内部
                  └ try_to_wake_up(p)
                      └ (跨核时) smp_send_reschedule(cpu) / ttwu_queue
                          └ IPI → scheduler → 唤醒 kernel_init
                              └ wait_for_completion 返回
```
每个箭头写出：函数名、文件、行号、关键条件判断。

### 2. 分析每个环节在 stop_machine 上下文里可能卡住的点
stop_machine 的特殊上下文：
- waiter（kernel_init）在 CPU0，睡眠在 swait 上
- 8 个 stopper 线程（migration/0..7）各自在自己的 CPU 上执行 multi_cpu_stop
- 执行 multi_cpu_stop 期间：local_irq_disable + hard_irq_disable（DISABLE_IRQ 状态）
- stopper 线程 preempt_count 被 inc（cpu_stopper_thread 里 preempt_count_inc）
- 第 8 个 signal_done 的核可能是 CPU7（与 waiter 的 CPU0 跨 cluster：A76/A55）

重点回答：
1. `swake_up_locked` 唤醒 waiter 时，waiter 是否已被正确挂到 swait 队列？
   （wait_for_completion → __prepare_to_swait 是否真的完成？）
2. `try_to_wake_up` 跨核唤醒时，如果 target CPU（CPU0）此刻正在跑 stopper 线程
   （migration/0，preempt_count 高、关中断），ttwu 的路径是"发 IPI 排队"
   还是"直接唤醒"？IPI 会不会因为 CPU0 关中断而**延迟到 multi_cpu_stop 结束**？
   （注意：CPU0 的 multi_cpu_stop 结束后会 local_irq_restore，IPI 应该能送达）
3. **关键怀疑点**：`swake_up_locked` 的唤醒是否依赖"当前 CPU 与 waiter 同核"？
   如果 complete 在 CPU7 执行、waiter 在 CPU0，swait 的唤醒语义是否跨核正确？
4. completion 的 `done` 字段递增（complete 里 x->done++）与 waiter 的唤醒判断
   （wait_for_completion 检查 x->done）之间的内存序/可见性是否够？
5. 有没有可能 waiter 根本没在 swait 队列上（wait_for_completion 尚未 schedule，
   或已经超时/被唤醒过一次），导致 complete 的 swake_up 空操作？

### 3. 给出可操作的诊断/修复建议
针对每个怀疑点，给出：
- 如何用探针/打印验证（不刷机也能建议加在哪一行）
- 可能的修复（如：改用 atomic_t 显式计数 + 手动 wake_up_process、
  或改用完成量 raw_spinlock 版、或给 complete 加 smp_mb）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-mainline-completion-wakeup-analysis.md
- 第 1 节：完整链路（函数+行号）
- 第 2 节：每个环节的卡点分析（编号，对应上面 5 个问题）
- 第 3 节：按可能性排序的根因假设 + 对应验证方法 + 修复建议

## 注意
- 只读 mainline 源码，不用碰 stock 镜像
- 结论要落到"具体哪一行代码可能导致问题"，不要泛泛而谈
- 如果发现 swait 在跨核唤醒上有已知限制，重点说明
