# 任务 C：LSE atomic 的 dec_and_test 语义 + stop_machine 双原子（thread_ack vs nr_todo）

## 背景
T508N（Unisoc T770/UMS9620）上 mainline 6.12.103 bring-up。当前卡点：
`stop_machine()` 不返回。

已确认现象（关键矛盾）：
- 8 核 `multi_cpu_stop` 状态机**走完了**（thread_ack 从 8 归零 → EXIT 达成，
  8 核都从 multi_cpu_stop 返回了）
- 8 核也都执行了 `cpu_stop_signal_done(done)`（nr_todo 应从 8 归零 → complete()）
- 但 `wait_for_completion` 不返回

**核心疑点**：multi_cpu_stop 的 `thread_ack` 和 completion 的 `nr_todo` 都是
`atomic_t`，都用 `atomic_dec_and_test`。为什么 thread_ack 能正确归零走完 EXIT，
nr_todo 却疑似没归零（或归零了但 complete 唤醒失败）？两者实现相同，
如果 atomic 有问题，thread_ack 也应该卡——但它没卡。

## 材料（都在 /home/test/t508n/ 下）
- linux-6.12.103/   ← mainline 源码
  - arch/arm64/include/asm/atomic*.h、atomic_ll_sc.h、atomic_lse.h  ← atomic 实现
  - kernel/stop_machine.c  ← multi_cpu_stop（thread_ack）、cpu_stop_signal_done（nr_todo）
  - kernel/sched/completion.c  ← completion（swait）
- work/stock_kallsyms.map       ← 原厂符号表
- work/stock_Image_full.bin     ← 原厂内核，可反汇编
- work/kernel.config            ← 原厂 config
- work/REPORT-stock-vs-mainline-locks.md  ← 已完成：锁对比（结论：锁实现一致）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<addr> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务

### 1. mainline 6.12 的 atomic_dec_and_test 在 LSE 下的实现
- 读 arch/arm64/include/asm/atomic_lse.h 和 atomic.h 的
  `atomic_dec_and_test` / `arch_atomic_dec_and_test` 定义
- 回答：
  - LSE 下 dec_and_test 用哪条指令（`ldaddal`? `staddl`? `casa`?）
  - 内存序（acquire/release/relaxed）是什么？
  - 返回值判断（是否 ==0）如何实现？
- 对比 LL/SC 版本（atomic_ll_sc.h）：语义是否等价？

### 2. 对比 thread_ack 和 nr_todo 的使用方式，找出"为什么一个能过一个不能"
- multi_cpu_stop 里 thread_ack：`set_state` 里 `atomic_set(thread_ack, num_threads)`，
  `ack_state` 里 `atomic_dec_and_test(&thread_ack)`
- cpu_stop_signal_done 里 nr_todo：`cpu_stop_init_done` 里 `atomic_set(nr_todo, n)`，
  `atomic_dec_and_test(&nr_todo)`
- 两者使用方式几乎一致。逐字对比：
  - 初值设置时机（set_state vs cpu_stop_init_done）
  - 是否有并发写、是否有额外屏障（set_state 里有 `smp_wmb()` + WRITE_ONCE(state)）
  - thread_ack 归零后触发 `set_state`（又 atomic_set），nr_todo 归零后触发 `complete`
  - **complete 内部有 raw_spin_lock + swake_up，而 set_state 只有 atomic_set + smp_wmb**：
    这是唯一实质差异。分析 complete 的锁/唤醒是否可能是卡点（而非 atomic 本身）。

### 3. LSE 原子在 T770 上的正确性
- 之前已确认 LSE early patch 让锁/原子走 LSE（见 REPORT-stock-vs-mainline-locks.md）
- 反汇编原厂 5.4 的 atomic_dec_and_test（如果符号存在）或 dec_and_test 的内联实现，
  对比 mainline 的 LSE 版本
- 回答：原厂 LSE 的 dec_and_test 与 mainline 是否一致？有无 T770 errata 相关
  workaround（原厂有、mainline 没有的）？

### 4. 结论
- 判断：nr_todo 归零失败，更可能是（a）atomic 本身问题，还是（b）complete 的
  raw_spin_lock/swake_up 在 stop_machine 关中断上下文里的问题？
- 给出验证方法（具体探针位置）和修复建议

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-LSE-atomic-completion-analysis.md
- 每任务一节，给出源码片段/反汇编 + 结论
- 最后：明确回答"thread_ack 能归零、nr_todo 疑似不能"最可能的原因

## 注意
- 符号表用 work/stock_kallsyms.map（不要用 work/System.map）
- atomic_dec_and_test 大概率是 inline，stock 里可能无独立符号，从调用方反查
- 结论要具体到指令/内存序/屏障级别
