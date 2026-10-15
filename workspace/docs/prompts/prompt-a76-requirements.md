# 任务：A76 核（CPU4-7）在 8 核配置下的特殊需求分析（对比原厂 stock DTS）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**关键实验事实**：
- `CONFIG_NR_CPUS=4`（只上线 CPU0-3 = 纯 A55）→ **竞态完全消失**（稳定推进）
- `CONFIG_NR_CPUS=512`（8 核，含 A76 CPU4-7）→ **竞态再现**（间歇卡在 devtmpfs
  setup_done / memory_dev_init device_register 前）
- 已修复（有效）：LSE static_key（SM#2 过）、NOMAP（set_zone_contiguous 过）
- 已排除（无效）：唤醒路径两种（CSD+CALL_FUNC vs 直接入队+RESCHEDULE）、cpuidle 去
  idle-states、LSE casp（强制 LL/SC）、A76 楔死（y=210 do_idle 全亮）、
  DTS cpu-map 单→双 cluster
- **结论：竞态与"A76 核的存在"强相关，但不是拓扑声明/唤醒路径/cpuidle/casp**

**推论**：A76 核（或 A76 cluster）的**某个配置/机制**在 mainline 6.12 上缺失或错误，
导致含 A76 时系统间歇异常（而 A55 4 核正常）。

## 材料
- /home/test/t508n/work/stock_ums9620.dts   ← 原厂完整 DTS（A76 相关节点的权威来源）
- /home/test/t508n/linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620.dtsi + ums9620-ot5.dts
  ← 我们的 DTS（基于 mainline 参考板）
- /home/test/t508n/work/linux-5.4.210/      ← 5.4.210 完整源码
- /home/test/t508n/work/stock_kallsyms.map + stock_Image_full.bin（原厂反汇编）
- /home/test/t508n/work/kernel.config        ← 原厂 config

## 任务

### 1. A76 相关 DTS 节点完整对比（核心）
逐个对比 stock_ums9620.dts vs 我们的 ums9620.dtsi/ot5.dts 中 A76（CPU4-7）相关节点：
- **cpufreq**：stock 有 cpufreq-cluster0/cluster1（4944-4953 行附近）——我们的 DTS 有吗？
  A76 的调频/电源管理节点
- **cooling-device**：stock 有 cluster0/1/2-cooling + cooling-maps——我们有吗？
- **idle-states**：stock 的 LIT_CORE_PD/BIG_CORE_PD（entry-method、power-domains、
  arm,psci-suspend-param 等）——我们删了 idle-states，原厂 A76 的 PD 参数是什么？
- **power-domains**：cpu 节点的 power-domains 属性（A76 的电源域 phandle）——stock cpu@400-700
  有吗？我们有吗？
- **PMU**：arm pmu 节点（A55/A76 的 PMU 中断）
- **arch timer**：timer 节点（A76 的 PPI）
- **GIC**：GIC 节点（redistributor 区域——A76 的 GICR）
- **其他 A76 专属**：capacity-dmips-mhz、dynamic-power-coefficient、sched-energy、cpu-idle 等

### 2. stock DTS 的 cpu 节点完整属性对比
- stock cpu@0-7 的**全部属性**（compatible、reg、enable-method、cpu-idle-states、
  power-domains、capacity 等）vs 我们的 CPU0-7
- **逐属性列出差异**（特别注意 CPU4-7 = A76 的差异）

### 3. A76 的 GIC/中断配置
- GIC 节点（gic@...）：redistributor 基址/大小是否覆盖 A76
- A76 的 PPI（timer、PMU）配置
- stock 与我们的 GIC 节点差异

### 4. A76 的电源/时钟需求（PSCI 层面）
- stock DTS 的 power-domains 引用（A76 的电源域）
- psci 节点（cpu_suspend 参数）
- **A76 cluster 的电源管理在 mainline 缺失的影响**（如果 A76 无 power-domain 配置，
  PSCI 的 CPU_SUSPEND/CPU_ON 行为差异）

### 5. 原厂 5.4 对 A76 的运行时处理
- 从 stock_kallsyms.map 搜索 A76/cluster 相关符号（cpufreq 驱动、sprd 电源管理）
- 原厂是否有 A76 专属的运行时管理（cpufreq 调频、cluster 电源切换）——mainline 缺失的影响

### 6. 综合
- **最可能导致"A76 存在 → 竞态"的缺失配置/机制**（排序）
- 修复方案（DTS 节点补齐 / config / 代码）

## 输出
单文件：/home/test/t508n/work/REPORT-A76-requirements.md

## 注意
- 符号表用 work/stock_kallsyms.map
- 重点是 A76 专属的 DTS 节点/属性对比（cpufreq/cooling/power-domain/PMU/GIC/timer）
