# 任务：T508N bring-up 全部反编译/分析进展汇总（供后续问题分析使用）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
历经多轮子 agent 分析（原厂反汇编、锁对比、同步机制、completion、唤醒链、LSE、
内存布局、driver_init、NOHZ 唤醒等）。**本任务：把全部成果汇总成一份结构化文档**，
供下一个"当前问题分析"子 agent 使用。**只汇总，不新分析**（不要写新结论）。

## 材料（全部读取）
/home/test/t508n/work/ 下的所有 REPORT-*.md 和 prompt-*.md：
- REPORT-T508N-stock-analysis.md
- REPORT-stock-vs-mainline-locks.md
- REPORT-stock-sync-mechanism.md
- REPORT-stock-vs-mainline-completion.md
- REPORT-mainline-completion-wakeup-analysis.md
- REPORT-LSE-atomic-completion-analysis.md
- REPORT-AGENTS-GUIDE-analysis.md
- REPORT-stock-wakeup-chain-analysis.md
- REPORT-stock-alternatives-waiter-analysis.md
- REPORT-alt-table-integrity.md
- REPORT-stock-crosscluster-visibility.md
- REPORT-stock-page-alloc-init.md
- REPORT-stock-zone-memory.md
- REPORT-stock-driver-init.md
- REPORT-stock-wakeup-nohz.md
- AGENTS-GUIDE.md（入口文档）
- work/HANDOFF-3-SESSIONS.md、work/SESSION-CONTEXT-COMPACT.md（背景）
也读一下 /home/test/t508n/linux-6.12.103 的 git diff（当前代码改动清单）：
```bash
cd /home/test/t508n/linux-6.12.103 && git status --short && git diff --stat
```

## 汇总内容（结构化输出）

### 1. 已确认事实清单（硬件/启动链）
- 设备特性（T770 双 cluster、3GB 内存、DIC=0、A76 r0p0? A55 r0p0、无 CCI/CMN 等）
- 原厂行为（multi_stop 协议、icache 维护、completion waitqueue、LSE static_key、
  pfn_valid NOMAP 拒绝等）

### 2. 反汇编成果清单（符号 + 地址 + 结论）
- 所有已反汇编的原厂符号（地址 + 用途），按报告来源组织

### 3. 已修复的问题（代码改动）
- LSE static_key（替代 asm-goto NOP-落入）→ 修了什么
- NOMAP 检查（__pageblock_pfn_to_page）→ 修了什么
- 其他改动（KVM 关闭、errata 关闭、探针系统、ttwu_queue_cond 实验、idle-states 删除实验）

### 4. 已排除的假设（结论 + 依据报告）
- 原子计数、丢唤醒、ic ialluis、errata 覆盖、alt 表写坏、唤醒链代码差异、
  cpuidle PD、ttwu_queue_cond 两种形态等

### 5. 当前未解决问题（竞态）
- 竞态 A：devtmpfs wait_for_completion(setup_done) 间歇卡（kdevtmpfs 未 complete）
- 竞态 B：memory_dev_init → device_register 前间歇卡
- 已知观测：y=325 8 格（A76 收 IPI 正常）、y=310 A55 前 4 格（A76 NOHZ 空闲）、
  两种唤醒路径（CSD+CALL_FUNC / 直接入队+RESCHEDULE）竞态相同
- 当前代码状态（git diff 清单）

### 6. 探针布局速查（当前固件）
- 所有 y 坐标探针的含义（y=150~380）

## 输出
单文件：/home/test/t508n/work/SUMMARY-ALL-ANALYSIS.md（结构清晰、自包含，
下一个子 agent 只读这一份就能工作）

## 注意
- 只汇总已有成果，不要新增分析/结论
- 文件冲突：本任务只写 SUMMARY-ALL-ANALYSIS.md 一个文件
