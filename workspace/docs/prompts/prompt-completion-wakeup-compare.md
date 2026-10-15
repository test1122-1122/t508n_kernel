# 任务 A：原厂 vs mainline 的 completion 唤醒机制对比

## 背景
T508N（TCL OT5，Unisoc T770/UMS9620，原厂内核 5.4.210）上 mainline 6.12.103
bring-up。当前卡点已精确收敛到 **stop_machine 的 completion 不放行**：

- `apply_alternatives_all()` → `stop_machine()` 卡住不返回（探针 x=44 不亮）
- 8 核全部进入 `__apply_alternatives_multi_stop`、全部完成 fn、`multi_cpu_stop`
  状态机走完（thread_ack 归零、EXIT 达成）、8 核全部执行了 `cpu_stop_signal_done`
- 但 `wait_for_completion(&done.completion)` 没有返回
- 推断：要么 `atomic_dec_and_test(&nr_todo)` 没把 8 减到 0（complete 没被调用），
  要么 `complete()` 调用了但唤醒 waiter 失败

**已确认**：mainline 6.12 的 completion 用 **swait（简单等待队列）**——
`complete()` → `done++` → `swake_up_locked()` → `try_to_wake_up()`。
原厂 5.4 那个年代 completion 大概率还是传统 waitqueue（`__wake_up`），
这是最可疑的差异点。

## 材料（都在 /home/test/t508n/ 下）
- work/stock_kallsyms.map        ← 正确原厂符号表（116,704 符号），不要用 work/System.map！
- work/stock_Image_full.bin      ← 原厂内核 Image（38MB），可 objdump
- work/kernel.config             ← 原厂 .config
- linux-6.12.103/                ← mainline 6.12.103 源码（直接读 C 对比）
- work/REPORT-stock-vs-mainline-locks.md  ← 已完成：spinlock/mutex/rwsem 与原厂一致
- work/REPORT-stock-sync-mechanism.md     ← 已完成：原厂 __apply_alternatives 裸协议

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<stock_kallsyms.map 里的符号地址> \
  --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。
链接基址：_text=0xffffffc010080000，_stext=0xffffffc010081000。

## 任务（逐项做）

### 1. 确认原厂 5.4 completion 的实现类型（waitqueue 还是 swait）
- 在 stock_kallsyms.map 找：complete、complete_all、wait_for_completion、
  wait_for_common、__wait_for_common、__wake_up、__wake_up_common、
  default_wake_function、autoremove_wake_function、swake_up 等
- 反汇编 complete() 和 wait_for_completion()，判断：
  - complete 用的是 `__wake_up_common`（waitqueue）还是 `swake_up_locked`（swait）？
  - wait 侧用的 wait_queue_entry 链表（waitqueue）还是 swait_queue（swait 单链表）？
- 明确回答：原厂 5.4 completion = waitqueue 还是 swait？

### 2. 对比 complete 的唤醒语义
- 原厂 complete()：唤醒一个 waiter 还是所有？唤醒路径（__wake_up_common →
  default_wake_function → try_to_wake_up）？
- mainline 6.12 complete()：swake_up_locked 唤醒一个还是所有？
- 差异点：waitqueue 的 __wake_up_common 会遍历整个 waitqueue（可能唤醒多个），
  swait 的 swake_up_locked 只唤醒队头一个。这个差异在 stop_machine 场景
  （waiter 只有 1 个，即 CPU0 的 kernel_init）里是否致命？

### 3. 对比 cpu_stop_signal_done / cpu_stop_init_done
- 反汇编原厂 cpu_stop_signal_done、cpu_stop_init_done、stop_cpus、__stop_cpus
- 对比 mainline 6.12 的 kernel/stop_machine.c 同函数
- 回答：
  - 原厂 nr_todo 用什么原子（atomic_t + dec_and_test 还是别的）？
  - 原厂 completion 初始化（init_completion）与 mainline 的差异？
  - 原厂 wait_for_completion 在哪个上下文等待（中断状态、CPU0？）

### 4. 对比 try_to_wake_up 的跨核唤醒路径
- 反汇编原厂 try_to_wake_up 关键部分（或 wake_up_process），看跨核唤醒时
  发 reschedule IPI（smp_send_reschedule / ipi）的逻辑
- 对比 mainline 6.12 try_to_wake_up 的 arm64 部分
- 回答：跨核唤醒时，原厂与 mainline 的 IPI 发送/接收机制有无差异？

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-vs-mainline-completion.md
- 每任务一节：原厂反汇编片段 + 地址 + mainline 对应实现 + 差异结论
- 最后总结：**原厂 vs mainline 的 completion 唤醒机制最关键的差异是什么**，
  是否足以解释"8 核 signal_done 但 waiter 不醒"，并给出可移植/可修复建议

## 注意
- 符号表一律用 work/stock_kallsyms.map（work/System.map 是 mainline 构建产物，错！）
- 原厂 5.4 符号命名可能与 mainline 不同，找不到就 grep 近似名
- completion 相关符号可能是 inline/static，从调用方（stop_cpus/__stop_cpus）
  反汇编反查其引用的函数地址
