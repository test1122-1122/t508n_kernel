# 任务：原厂 5.4 的多核唤醒/调度完整处理方式（对比 mainline 6.12 竞态根源）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**已修复**：LSE static_key（stop_machine 过）、NOMAP（set_zone_contiguous 过）。

**当前问题：间歇性竞态（卡点漂移）**：
- 竞态 A：`devtmpfs_init` 的 `wait_for_completion(&setup_done)` 卡——kdevtmpfs 线程被
  kthread_run 创建（kthread_run 返回）但 **setup_done 没 complete**（线程没执行到 complete）
- 竞态 B：`memory_dev_init` → `__add_memory_block` → `device_register` 前卡
- 每次重启卡点不同（间歇漂移）——时序敏感的多核竞态
- 已试修复无效：ttwu_queue_cond 去掉 6.12 新增"目标 rq 空→wakelist"分支（同簇直接入队）——竞态仍在

**关键线索**：
- 8 核 SMP、双 cluster（CPU0-3=A55、CPU4-7=A76）
- NOHZ_IDLE 开（两版都开）——A76 核空闲停 tick（y=310 心跳只亮 A55 部分，正常）
- **kdevtmpfs 等新线程可能被调度到空闲核（A76）→ 异簇唤醒路径**：
  - 5.4：异簇 wakelist = `rq->wake_list` + **IPI_RESCHEDULE**
  - 6.12：异簇 wakelist = CSD + **IPI_CALL_FUNC**（sched_ttwu_pending）
  - **如果 T770 的 IPI_CALL_FUNC（跨簇）间歇性失效而 IPI_RESCHEDULE 可靠 → 6.12 异簇唤醒失败 = 竞态根源**

## 材料
- /home/test/t508n/linux-6.12.103/             ← mainline 6.12.103 源码
  - kernel/sched/core.c（try_to_wake_up/ttwu_queue/ttwu_queue_wakelist/sched_ttwu_pending）
  - kernel/sched/idle.c（do_idle/nohz）
  - kernel/smp.c（__smp_call_single_queue/IPI_CALL_FUNC）
  - arch/arm64/kernel/smp.c（IPI 处理）
- /home/test/t508n/work/linux-5.4.210/         ← 完整 v5.4.210 上游源码（已拉取解压）
  - kernel/sched/core.c（5.4 的 ttwu/ttwu_queue_remote）
  - kernel/sched/idle.c、kernel/smp.c
- /home/test/t508n/work/stock_kallsyms.map + stock_Image_full.bin（原厂反汇编）
- 已有报告（先读）：
  - /home/test/t508n/work/REPORT-stock-wakeup-chain-analysis.md（唤醒链等价结论 + §6.2 wakelist 传输差异）
  - /home/test/t508n/work/REPORT-stock-driver-init.md（driver_init 链分析）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务

### 1. 异簇 wakelist 唤醒路径逐环节对比（核心）
- 5.4：ttwu_queue_remote（rq->wake_list + smp_send_reschedule=IPI_RESCHEDULE）→
  CPU 收到 RESCHEDULE → scheduler_ipi（5.4 是真实函数，处理 wake_list + sched_ttwu_pending）
- 6.12：__ttwu_queue_wakelist（CSD + __smp_call_single_queue + IPI_CALL_FUNC）→
  CPU 收到 CALL_FUNC → generic_smp_call_function_interrupt → csd_do_func(sched_ttwu_pending)
- **逐行对比两条路径**（5.4 vs 6.12 源码），列出所有差异
- **重点：6.12 的 IPI_CALL_FUNC 处理链在"目标核是 NOHZ idle（wfi）"时的行为**——wfi 核被
  IPI 唤醒的机制（SGI 唤醒 wfi 是 GIC 硬件行为，两版相同？）；唤醒后 IPI_CALL_FUNC 的
  分发处理（irq_enter/exit、csd 队列）在 6.12 vs 5.4 的差异

### 2. 原厂 5.4 的 scheduler_ipi 完整实现（0x1fc658 已知）
- 反汇编原厂 scheduler_ipi：wake_list 处理 + nohz kick 逻辑
- 对比 6.12 的 scheduler_ipi（sched.h inline 纯 fold）——**5.4 的 RESCHEDULE 会处理 wake_list，
  6.12 的 RESCHEDULE 只 fold（wake_list 由 CALL_FUNC 处理）**——如果 6.12 的 CALL_FUNC 在
  T770 失效，而 5.4 的 RESCHEDULE 可靠 → 差异致命

### 3. NOHZ idle 唤醒对比
- 5.4 vs 6.12 的 do_idle / nohz_balance / wake_up_idle_cpu（或等价机制）
- **空闲核（NOHZ tickless）被跨核唤醒的完整路径在两版的差异**
- 原厂 5.4 有没有 6.12 没有的"保证空闲核被唤醒"机制

### 4. 竞态根源假设验证
- 综合任务 1-3，判断：竞态 A（kdevtmpfs 唤醒失败）是否因为 6.12 的异簇
  wakelist/IPI_CALL_FUNC 在 T770 间歇性失效
- **给出可操作的修复方案**（比如：把 6.12 的 wakelist 传输改回 5.4 式
  rq->wake_list + IPI_RESCHEDULE；或禁用 wakelist 全部走直接入队；或禁用 NOHZ_IDLE）
- 竞态 B（device_register 前卡）与竞态 A 是否同源（都是"某任务/某核的唤醒/同步失败"）

### 5. 其他差异扫描（补充）
- 5.4 vs 6.12 的 kthread 创建/唤醒（kthreadd、wake_up_process 路径）
- 5.4 vs 6.12 的 complete/swait（已排除，引用旧报告）
- 5.4 vs 6.12 的 workqueue（devtmpfs 的 kthread 相关）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-wakeup-nohz.md
- 每任务一节：对比证据（源码/反汇编）+ 结论
- 最后：竞态根源判断 + 修复方案（按优先级，具体到代码）

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码在 work/linux-5.4.210/（完整树）
- 重点：异簇/空闲核唤醒路径的 5.4 vs 6.12 差异
