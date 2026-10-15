# 任务：原厂 vs mainline 并发基础设施对比（锁/原子/多核同步）

## 背景一句话
T508N（TCL OT5，Unisoc T770/UMS9620，原厂内核 5.4.210）上 mainline 6.12.103 内核
bring-up 当前卡点：**多核并发下 sysfs/device model 初始化偶发卡死**（of_core_init 的
sysfs attach、memory_dev_init 的 device_register 间歇性卡住，时好时坏）。
已确认 LSE atomics 提前 patch 后大问题解决，但仍有偶发竞态。

## 材料（都在 /home/test/t508n/ 下）
- work/stock_kallsyms.map        ← 正确原厂符号表（116,704 符号），不要用 work/System.map！
- work/stock_Image_full.bin      ← 原厂内核 Image，可 objdump
- work/kernel.config             ← 原厂 .config
- linux-6.12.103/                ← mainline 6.12.103 源码（直接读 C 对比）
- work/REPORT-T508N-stock-analysis.md ← 之前的分析报告（含 LSE/errata 结论）

## 反汇编方法
```bash
aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<stock_kallsyms.map 里的符号地址> --stop-address=<+n> \
  work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务（按价值排序，逐项做）

### 任务 1：锁实现对比（最高价值）
1. 在 stock_kallsyms.map 找：arch_spin_lock、queued_spin_lock_slowpath、
   queued_spin_unlock、mutex_lock/unlock、mutex_lock_slowpath、
   down_write/up_write、rwsem_down_read_slowpath、rwsem_down_write_slowpath
2. 反汇编原厂实现，读 mainline 6.12 同函数 C/汇编
3. 逐项回答：
   - 原厂 spinlock 用 LSE（ldadd/cas）还是 LL/SC（ldxr/stxr）？
   - mainline 6.12 用哪种？（重点：mainline 若依赖 LSE 原子做 spinlock，
     而我们的 LSE patch 有遗漏，会导致锁偶发失效）
   - 锁顺序、有无额外屏障/WFE 超时机制差异

### 任务 2：LSE 原子在锁路径的使用细节
- 对比原厂 vs mainline 的 queued_spin_lock_slowpath / unlock 的原子指令
- 结论：LSE 使用是否一致；mainline 是否有"依赖 LSE 但可能未 patch"的锁代码

### 任务 3：多核同步基础设施
- 原厂 secondary 上线时 GIC/IPI/timer 初始化时机（gic_cpu_init、gic_raise_softirq、
  ipi_setup、arch timer）与 mainline 对比
- 结论：多核并发基础（中断、IPI）是否一致

### 任务 4：启动时序
- 原厂 smp_cpus_done → do_basic_setup → driver_init 的调用顺序/中断状态 vs mainline
- 结论：原厂是否在单核/关中断状态下做更多 device model 初始化

### 任务 5：config 并发/调试选项
对比原厂 kernel.config 与 linux-6.12.103/.config：
CONFIG_QUEUED_SPINLOCKS、CONFIG_DEBUG_SPINLOCK、CONFIG_PROVE_LOCKING、
CONFIG_DEBUG_ATOMIC_SLEEP、CONFIG_PREEMPT_*、CONFIG_RCU_*、CONFIG_HZ
列出所有差异，标出可能影响锁行为/时序的项。

### 任务 6：原厂 sysfs 创建锁路径
- 反汇编原厂 kobject_add_internal、sysfs_create_dir_ns、device_add 的锁获取顺序
- 对比 mainline 6.12 同函数
- 结论：锁顺序是否一致（锁顺序差异 → 死锁窗口）

## 输出
一份 Markdown 报告（放 /home/test/t508n/work/REPORT-stock-vs-mainline-locks.md）：
- 每任务一节，给出：原厂反汇编片段 + 地址 + mainline 对应实现 + 差异结论
- 最后总结：**哪些差异最可能导致当前偶发卡死**，给出可操作的修复建议
- 特别标出"mainline 有、原厂没有"的差异（最可疑）

## 注意
- 符号表一律用 work/stock_kallsyms.map（work/System.map 是 mainline 构建产物，错！）
- 原厂 5.4 的符号命名可能与 mainline 6.12 不同，找不到时用 kallsyms grep 近似名
- 若某符号原厂没有（如 qspinlock 相关），明确说明"原厂无此符号"并给出原厂替代实现
