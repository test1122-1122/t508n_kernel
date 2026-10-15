# 任务：原厂跨 cluster 可见性操作模式分析（普通 store vs 原子/dsb/锁）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
调试探针刚发现一个硬件行为线索：

- 探针标志 `t508n_sm2_done`（volatile bool，普通 store 置位）在 **CPU0（A55）的
  __apply_alternatives_multi_stop fn 入口** 写 true。
- 同一标志在 **waker 核**（SM#2 complete 的调用者，簇未知）的 `swake_up_locked`
  里读取——**读到 false**（探针不亮）。
- 但同一标志在**另一些核**的 `ttwu_queue` 里读取**成功**（探针亮）。
- → **T770 上跨 cluster（A55/A76 双簇）的普通 store→load 可见性不可靠/有延迟**，
  需要 dsb（inner-shareable）或原子操作或锁才能保证即时可见。
- 原厂对照：原厂 flag（all_alternatives_applied）置位在 `__apply_alternatives`
  尾部 **`dsb ish; [ic ialluis]; dsb ish; isb` 之后**——原厂的跨核置位有 dsb 保护。

需要从原厂镜像确认：**原厂在跨核通信/唤醒场景下如何保证可见性**，mainline 有没有
"裸普通 store 跨核"的地方（可能正是卡点环境）。

## 材料
- /home/test/t508n/work/stock_kallsyms.map        ← 原厂符号表（116,704 符号）
- /home/test/t508n/work/stock_Image_full.bin      ← 原厂内核，可反汇编
- /home/test/t508n/work/stock-5.4.210/            ← 5.4.210 上游源码
- /home/test/t508n/linux-6.12.103/                ← mainline 6.12.103 源码
- 已有报告（先读）：
  - /home/test/t508n/work/REPORT-stock-wakeup-chain-analysis.md（唤醒链等价结论）
  - /home/test/t508n/work/REPORT-stock-sync-mechanism.md（multi_stop 协议 + flag 置位）
  - /home/test/t508n/work/REPORT-stock-vs-mainline-completion.md（completion 对比）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务

### 1. 原厂跨核置位/唤醒点的 barrier 结构（核心）
逐个反汇编/核对原厂以下位置的**置位前 barrier**（dsb/dmb/isb/原子/锁）：
- all_alternatives_applied 置位（__apply_alternatives_multi_stop 的 CPU0 分支）
- TIF_NEED_RESCHED 置位（resched_curr 跨核分支 = set_nr_and_not_polling，已知 LSE stset）
- stop_machine 的 msdata->state 置位（set_state：atomic_set + smp_wmb）
- completion 的 done++（complete 持锁）
- 其他跨核通信点（比如 rq->nr_running、per-cpu 标志）

**结论：原厂跨核写是不是全部"原子/持锁/dsb 之一"？有没有裸普通 store 跨核？**

### 2. 原厂 waitqueue 唤醒的 barrier 链（对比 mainline swait）
- 原厂 complete（0x227a70）：raw_spin_lock(wait.lock) → done++ → __wake_up_common →
  try_to_wake_up —— 锁的 release/acquire 如何保证 waiter 看到 done？
- mainline swait 的 complete：raw_spin_lock(x->wait.lock) → done++ → swake_up_locked → try_to_wake_up
- 对比：两边都持锁，barrier 结构是否等价？**有没有哪边在锁外读/写共享状态？**

### 3. T770 跨 cluster 可见性的事实确认
- 从原厂代码/反汇编找：原厂有没有**针对双 cluster 的额外处理**（比如 GIC/DSU/互联相关的
  barrier、或特定 errata workaround 涉及 cache 维护）
- 原厂 boot 早期（SML 之外）有没有对 cluster 一致性的显式初始化（已知：无 CCI/CMN/DSU——确认）
- 原厂是否在**某处用普通 store 跨核通信且工作**（如果有，说明 T770 普通 store 跨核其实可用，
  我们的探针问题在别处；如果没有，说明原厂全用原子/锁/dsb）

### 4. mainline 的裸普通 store 跨核点（对照）
- 在 mainline 6.12 里找：跨核通信中**没有**用原子/锁/dsb 保护的普通 store（比如
  t508n_sm2_done 这种探针不算，找内核本身的：一些 per-cpu 标志、快路径标志）
- 重点：stop_machine/唤醒链（t508n 的 __apply_alternatives_multi_stop flag、
  smp_prepare_boot_cpu 的 LSE patch 标志等）
- **mainline 有没有可能依赖了"普通 store 跨核即时可见"而原厂用原子/dsb 的地方？**

### 5. 综合
- 原厂跨核通信的操作模式（原子/dsb/锁的选用规则）
- mainline 与之一致的程度；不一致的地方是否可能是卡点环境
- 对探针门控的建议（用 smp_store_release/atomic 还是 dsb）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-crosscluster-visibility.md
- 每任务一节：反汇编/源码证据 + 结论
- 最后：原厂操作模式总结 + mainline 差异点 + 建议

## 注意
- 符号表用 work/stock_kallsyms.map
- 已有报告结论直接引用，不重复反汇编
- 聚焦"跨核可见性的操作模式"，不要泛泛分析
