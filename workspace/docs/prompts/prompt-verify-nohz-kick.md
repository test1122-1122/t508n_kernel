# 任务：验证未验证项 1 — NOHZ 恢复 + kick_ilb 改 IPI_RESCHEDULE

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**先读**：
- /home/test/t508n/work/UNISOC-MODS-AND-CONFIG.md（第 4 节未验证项）
- /home/test/t508n/work/DISASM-B-sched-调度器.md（原厂 scheduler_ipi/kick_ilb 反汇编）
- /home/test/t508n/work/REPORT-stock-wakeup-nohz.md（唤醒/NOHZ 分析）

当前：memory_dev_init 的 device_register 间歇竞态仍现（偶发）。已对齐：DEVTMPFS=n、
NUMA/SMT=n、NR_CPUS=32、ttwu_queue_cond 恒 false（P1）、CONFIG_NO_HZ_IDLE=n（诊断关闭）。

## 本任务：设计"恢复 NOHZ + kick_ilb 改 smp_send_reschedule"的完整方案

原厂 5.4 的 nohz idle balance kick 走 **smp_send_reschedule（IPI_RESCHEDULE/SGI#0）**；
6.12 的 kick_ilb（kernel/sched/fair.c:12374）走 **smp_call_function_single_async（CSD+IPI_CALL_FUNC）**。
当前树 P1 后 ttwu 已无 CSD，但 kick_ilb 的 CSD 是我们关闭 NO_HZ_IDLE 才绕开的（原厂开着 NOHZ 且 kick 走 RESCHEDULE）。

## 任务
1. 读 6.12 kick_ilb（fair.c:12374）与 nohz_csd_func（core.c:1236）完整实现
2. 设计"kick_ilb 改 smp_send_reschedule"的具体代码改动：
   - smp_call_function_single_async(ilb_cpu, &rq->nohz_csd) → smp_send_reschedule(ilb_cpu)
   - **关键问题**：RESCHEDULE 到达 ilb_cpu 后，谁触发 rebalance（nohz_csd_func 的回调）？
     6.12 的 scheduler_ipi（sched.h inline）只 fold，不处理 nohz kick——需要补什么？
   - 参考原厂 scheduler_ipi（DISASM-B：fold + wake_list 处理 + 自触发 idle balance kick +
     唤醒 ksoftirqd）——设计 6.12 的等价实现（或最小化：RESCHEDULE 处理里触发
     nohz_csd_func / rebalance_domains）
3. 评估影响：nohz idle balance 功能、性能、与其他路径的交互
4. 给出**可直接实施的补丁**（具体 diff 形式）到 /home/test/t508n/work/PATCH-nohz-kick.md

## 输出
单文件：/home/test/t508n/work/PATCH-nohz-kick.md（含设计分析 + 具体补丁 + 风险）

## 注意
- 只写自己的输出文件
- 补丁要具体到函数/行，可直接编译
