# 任务：干净 6.12 树上验证 kick_ilb 补丁（隔离魔改干扰）

## 背景
T508N（Unisoc T770/UMS9620）mainline 6.12.103 bring-up。
**复刻原厂 5.4 内核已 boot 到 initramfs（exec /init）**——证明原厂式路径在 T770 可行。
mainline 6.12 在魔改树上应用 kick_ilb 补丁（PATCH-nohz-kick.md）后**回归**（SM#2 卡、9 格）。
需要**在干净 6.12 树**（无我们的探针/魔改代码）上应用该补丁编译，验证：
① 补丁本身能否干净编译；② 补丁的正确性（尤其 scheduler_ipi 在 stop_machine 上下文的
nohz_csd_func 调用是否安全——这是 9 格回归的最大嫌疑）。

## 材料
- /home/test/t508n/dl/linux-6.12.103.tar.xz  ← 干净 6.12.103 源码
- /home/test/t508n/work/PATCH-nohz-kick.md   ← kick_ilb 补丁（3 文件 diff + 设计说明）
- /home/test/t508n/linux-6.12.103/.config    ← 我们的 config（可复制作为基线，排除探针代码干扰）
- 工具链：/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin
- openssl：CPATH/LIBRARY_PATH（/home/test/t508n/openssl-local/）

## 任务

### 1. 干净树
- 解包 dl/linux-6.12.103.tar.xz 到 /home/test/t508n/work/linux-6.12-clean/
- 复制 .config（从 linux-6.12.103/.config）——**保持 config 基线**（含我们的 config 设置：
  DEVTMPFS=n/NUMA=n/SCHED_SMT=n/NR_CPUS=32/NO_HZ_IDLE=n/INITRAMFS_SOURCE 等）
- olddefconfig 处理

### 2. 应用 kick_ilb 补丁
- 从 PATCH-nohz-kick.md 提取 3 个 diff（fair.c / sched.h / core.c）应用
- **注意**：fair.c 需要补 `#include <trace/events/ipi.h>`（我们在魔改树遇到的编译错误）
- 编译修复任何错误

### 3. 编译
```bash
export PATH=.../bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
```

### 4. 补丁正确性分析（重点！）
- 读补丁后的 scheduler_ipi（core.c）与 kick_ilb（fair.c）完整代码
- **分析 nohz_csd_func() 在什么上下文被调用**：
  - scheduler_ipi 在 IRQ 上下文（RESCHEDULE 中断处理）——nohz_csd_func 里做了什么
    （锁？spin_lock？softirq raise？）——在 stop_machine 期间（7 核关中断 spin、
    CPU0 执行 multi_cpu_stop）RESCHEDULE 到达时调用是否安全？
  - **对比 5.4/原厂 scheduler_ipi**（work/DISASM-B-sched-调度器.md §1.1/§4）：原厂在
    scheduler_ipi 里怎么处理 nohz kick（idle_balance 自触发 + SCHED_SOFTIRQ +
    唤醒 ksoftirqd——**没有直接调 nohz_csd_func**？）
  - **结论**：补丁的"RESCHEDULE → nohz_csd_func"设计在 stop_machine 上下文是否有
    死锁/异常风险（比如 spinlock 在关中断上下文、softirq raise 时机）？给出修正建议
- 如果分析发现补丁有 bug，**给出修正版补丁**

### 5. 输出
- 干净树 + 补丁编译结果（成功/失败）
- 报告：/home/test/t508n/work/REPORT-kick-patch-clean-verify.md
  - 编译结果
  - 补丁正确性分析（重点：scheduler_ipi 的 nohz_csd_func 调用上下文安全性）
  - 修正建议/修正版补丁

## 注意
- 只写 REPORT-kick-patch-clean-verify.md + 在 linux-6.12-clean/ 操作（不碰魔改树）
- 重点分析 scheduler_ipi → nohz_csd_func 的上下文安全性（vs 原厂做法）
