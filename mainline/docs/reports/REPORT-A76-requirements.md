# T508N A76 核（CPU4-7）特殊需求分析 —— stock DTS vs mainline DTS 全量对比

> 任务：`prompt-a76-requirements.md`（2026-08-18 之后）。
> 材料：`work/stock_ums9620.dts`（原厂 5.4.210 DTS，7731 行）、`linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620.dtsi` + `ums9620-ot5.dts`（我们的 DTS）、
> `work/linux-5.4.210/`、`work/stock_kallsyms.map` + `stock_Image_full.bin`、`work/kernel.config`（原厂 config）、`linux-6.12.103/.config`（我们的 config）。
> 目的：找出"含 A76（CPU4-7）时竞态再现、纯 A55（NR_CPUS=4）时竞态消失"的**缺失 DTS 配置/机制**。
> 已知排除：唤醒路径（CSD+IPI_CALL_FUNC vs 直接入队+IPI_RESCHEDULE）、cpuidle 去 idle-states、LSE casp 强制 LL/SC、
> A76 楔死（do_idle 全亮）、DTS cpu-map 单→双 cluster；已修复：LSE static_key + NOMAP。

---

## 0. 结论摘要（TL;DR）

1. **stock 与我们的 DTS 在 CPU4-7（A76）相关节点上的差异是"系统性缺失"，不是个别属性缺失**：
   stock 的 8 个 cpu 节点全部带有 `cpu-idle-states`、`capacity-dmips-mhz`、`dynamic-power-coefficient`、
   `cpufreq-data-v1`（`#cooling-cells`、`sugov_slack_timer` 只在 cluster 代表核 cpu0/4/7）；我们的 cpu 节点
   **只有 device_type/compatible/reg/enable-method 四项**，`idle-states` 定义了但**没有被任何 cpu 节点引用**。
2. **最重要的结构性差异（此前从未被测试过）**：
   - **cpu-map 拓扑**：stock = **三 cluster（4+3+1）**：cluster0=CPU0-3(A55)、cluster1=CPU4-6(A76)、
     **cluster2=CPU7 单独一簇**；我们 = 双 cluster（4+4，CPU4-7 同一簇）。此前实验只做了"单→双 cluster"，
     **从未复现 stock 的 4+3+1 三簇拓扑**。
   - **cpufreq 完全缺失**：stock 有 `cpufreq-cluster0/1/2` 三个 DVFS 域（`sprd,cluster-cpumask` =
     0x0f / 0x70 / 0x80，即 A55×4 / A76×3 / **CPU7 独立域**）+ `sprd,cpufreq-v2` 驱动节点；我们**完全没有**，
     且 mainline 无 sprd cpufreq 驱动 → **A76 没有任何调频/电压管理，始终跑在 LK 遗留频率**。
   - **capacity-dmips-mhz / dynamic-power-coefficient 缺失**：stock A55=0x1a1(417)、A76=0x3fe(1022)、
     CPU7=0x400(1024)，DPC A55=0x9b(155)、A76=0x2d1(721)、CPU7=0x380(896)；我们全无 → 8 核全部
     capacity=1024 → **调度器把 A55/A76 视为等容量，EAS/EM 不生效**，且与 cpu-map 双簇叠加产生
     与 stock 完全不同的调度域结构。
3. **排序（最可疑 → 次之）**：
   1. **cpu-map 三簇（4+3+1）+ CPU7 独立 cluster**（stock 结构，我们从未复现）——直接影响 `cluster_sibling`、
      LLC(cacheinfo) 回退路径、sched domain 结构；CPU7 在 stock 中拥有独立 DVFS 域/独立 idle-state/更高
      capacity/DPC，我们把它与 CPU4-6 混在同一簇。
   2. **capacity-dmips-mhz / dynamic-power-coefficient 缺失** → 调度器容量对称 → 与 A55-only 4 核配置在
      "容量语义"上相同（纯 A55 时本来全 1024），但含 A76 时掩盖了 A76 的 2.45× 容量 → 影响
      wake_affine/find_idlest/负载均衡，且 EAS 依赖 asym-capacity 条件被关闭。
   3. **cpufreq/cooling 域缺失** → A76 无 DVFS；stock 有 cluster0/1/2-cooling + cooling-maps 依赖 cpu 节点
      `#cooling-cells`（我们无）。
   4. **idle-states 未挂接**（我们定义了 LIT/BIG_CORE_PD 但 cpu 节点无 `cpu-idle-states` 引用；stock 的
      A76 用 `core-sleep-mid/big` + `cluster-pd`，suspend-param 0x10000/0x1010000）——cpuidle 虽已被实验排除
      为竞态根源，但这是 stock 与我们的直接差异，且 `cluster-pd`(0x1010000) 的 power-domain 位我们从未定义。
   5. **PMU**：stock 单节点 `arm,armv8-pmuv3` + 8 个 SPI(177-184)；我们拆成 pmu-a55(177-180)+pmu-a76(181-184)。
      中断号一致，等效；但 stock 无 interrupt-affinity，我们有——无竞态影响。
   6. **arch timer**：stock 4 个 PPI(13/14/11/10) + `arm,no-tick-in-suspend`；我们 5 个 PPI（多了 hyp-virt
      PPI12）且有 names。PPI 一致；`arm,no-tick-in-suspend` 我们缺失（仅影响 suspend）。
   7. **GIC**：**完全一致**（reg 0x12000000 0x20000 + 0x12040000 0x100000；stride 0x20000；#redistributor-regions 1；
      PPI9 maintenance）。GICR 0x100000/0x20000 = 8 个 redistributor，恰好覆盖 8 核，A76 无遗漏。
   8. **power-domains**：stock 的 cpu 节点**没有** power-domains 属性（原厂 CPU 电源走 PSCI + idle-states +
      cpufreq 域，不走 DT power-domain）；我们也没有 → **不是差异**，但也意味着 A76 无 DT 电源域描述。
4. **对竞态的最可能解释**（与已有排除项一致）：含 A76 时，我们的 DTS 使 8 核在调度器眼中为"对称等容量 +
   单 LLC 域（或 4+4 双簇）"，与 stock 的"4+3+1 三簇 + 非对称容量 + 独立 CPU7"语义完全不同 → 早期
   wakeup/负载均衡把任务（如 kdevtmpfs 二次唤醒）放到空闲 A76 核上的行为与 stock 不同 → 叠加唤醒路径/
   IPI 时序即复现竞态。**优先做 D4 式"按 stock 逐项补齐 DTS"实验（三簇拓扑 + capacity + idle-states 挂接）**。

---

## 1. A76 相关 DTS 节点完整对比（核心）

### 1.1 cpu-map 拓扑对比（★最重要差异）

| 项目 | stock（4799-4841 行） | 我们（ums9620.dtsi 19-51 行） |
|---|---|---|
| 簇数 | **3 簇**：cluster0=CPU0-3、cluster1=CPU4-6、**cluster2=CPU7（单独一簇）** | 2 簇：cluster0=CPU0-3、cluster1=CPU4-7 |
| 各簇核数 | 4 + 3 + 1 | 4 + 4 |
| CPU7 归属 | **独立 cluster（core0）**，与 CPU4-6 不同簇 | 与 CPU4-6 同簇 |
| 结论 | stock 是 **4+3+1 结构**，CPU7 是"独立大核簇" | 我们把它并入 A76 簇 |

- **影响链**：cpu-map 决定 `cpu_topology[].cluster_id` → `cluster_sibling`/`core_sibling` 掩码 →
  LLC(cacheinfo 回退)与 `cpu_coregroup_mask`(MC 域) → sched domain 结构、`cpus_share_cache()`、
  wake_affine/find_idlest 范围。4+3+1 与 4+4 的调度域结构不同；CPU7 在 stock 中是"孤核簇"。
- **历史**：任务背景明确"已排除 DTS cpu-map 单→双 cluster"——但那只测了 1 簇 vs 2 簇（4+4），
  **stock 的 4+3+1 三簇结构从未被复现测试过**。这是本次对比的最大缺口。

### 1.2 cpu 节点完整属性对比（stock 4843-4941 行 vs 我们 53-107 行）

| 属性 | stock CPU0-3 (A55) | stock CPU4-6 (A76) | stock CPU7 (A76) | 我们 CPU0-7 |
|---|---|---|---|---|
| compatible | "arm,cortex-a55\0arm,armv8" | "arm,cortex-a76\0arm,armv8" | 同左 | "arm,cortex-a55"/"arm,cortex-a76"（**无 arm,armv8 回退**） |
| reg | 0x0/0x100/0x200/0x300 | 0x400/0x500/0x600 | 0x700 | 同左 |
| enable-method | psci | psci | psci | psci（同） |
| **cpu-idle-states** | <0xe9 lit, 0xea cluster-pd> | <0xec mid, 0xea cluster-pd> | <0xee big, 0xea cluster-pd> | **无**（idle-states 定义但未引用） |
| **capacity-dmips-mhz** | 0x1a1 (417) | 0x3fe (1022) | 0x400 (1024) | **无** |
| **dynamic-power-coefficient** | 0x9b (155) | 0x2d1 (721) | 0x380 (896) | **无** |
| **cpufreq-data-v1** | <0xeb cluster0> | <0xed cluster1> | <0xef cluster2> | **无** |
| #cooling-cells | cpu0 有 <0x02> | cpu4 有 <0x02> | cpu7 有 <0x02> | **无** |
| sugov_slack_timer | cpu0 有 | — | — | **无**（vendor 属性，mainline 不识别） |
| power-domains | **无** | **无** | **无** | **无**（两版都无 → 非差异） |

- A76 专属要点：
  - stock **CPU4-6 与 CPU7 使用不同的 idle-state**（mid vs big）和**不同的 cpufreq 域**（cluster1 vs cluster2）——
    CPU7 是独立的"大核"配置（capacity/DPC 更高）。
  - 我们所有 CPU 节点属性相同、且几乎为空：既无容量、无 DPC、无 cpufreq 域、无 idle 挂接、无 cooling。
- 结论：**A76 相关"个性化配置"在 stock 中存在 3 套（lit/mid/big），我们 0 套**。

### 1.3 cpufreq 对比（stock 4944-4968 行 + 3293-3296 行 vs 我们无）

| 项目 | stock | 我们 |
|---|---|---|
| cpufreq-cluster0 | `sprd,voltage-step=0xb71b0; voltage-margin=0x28; transition-delay=0x3e8; temp-threshold=0x41; sprd,cluster-cpumask=0x0f`(CPU0-3) | **无** |
| cpufreq-cluster1 | 同构型，`cluster-cpumask=0x70`(CPU4-6) | **无** |
| cpufreq-cluster2 | `voltage-margin/transition-delay/temp-threshold` + `cluster-cpumask=0x80`(CPU7) | **无** |
| 驱动节点 | `/soc/aon/apcpu-dvfs@950000 { compatible = "sprd,cpufreq-v2"; }`（3293-3296） | **无**（mainline 无 sprd cpufreq 驱动） |
| 依赖的 cpu 属性 | 每核 `cpufreq-data-v1 = <域 phandle>` | 无 |

- 影响：stock 中 A76（及 CPU7）有独立 DVFS 域、电压步进、温度阈值；mainline 无 sprd cpufreq 驱动，
  即使补节点也不会被驱动 → **A76 无任何频率管理，运行在 bootloader 遗留频率**。若 LK 遗留频率/电压
  对 A76 不稳定，可能放大内存/总线错误（与"含 A76 竞态"间接相关，需实测 A76 频率）。

### 1.4 cooling 对比（stock 6620-6669 cooling-maps、6918-6997 cooling-devices vs 我们无）

- stock：`cooling-devices/cluster0/1/2-cooling`（compatible "sprd,cluster-cooling"，含 leak/temp/voltage scale、
  hotplug-period、min-cpufreq、sensor-names 等）+ `soc-thmzone/cooling-maps` 引用 cpu0/cpu4/cpu7 的
  `#cooling-cells`（phandle 0xcb/0xd3/0xd9）与 cluster0/1/2-cooling。
- 我们：**无任何 cooling 节点/属性**。thermal 子系统在 mainline 下不绑定 CPU 冷却。
- 竞态相关性：低（thermal 初始化在 driver_init 之后，且竞态点在 devtmpfs/memory_dev_init 之前），
  但属于 stock 有而我们缺失的 A76 配套机制。

### 1.5 idle-states 对比（stock 4991-5033 vs 我们 110-129）

| 状态 | stock | 我们 |
|---|---|---|
| entry-method | "arm,psci" | "psci" |
| LIT (A55) | core-sleep-lit: 1000/500/2500us, `arm,psci-suspend-param=0x10000`, local-timer-stop | LIT_CORE_PD: 1000/500/2500us, `0x00010000`, local-timer-stop（定义但未挂接） |
| A76 CPU4-6 | core-sleep-mid: 4000/4000/10000us, 0x10000 | BIG_CORE_PD: 4000/4000/10000us, 0x10000（定义但未挂接） |
| A76 CPU7 | core-sleep-big: 4000/4000/10000us, 0x10000 | （无对应 big 核独立状态） |
| cluster | cluster-pd: 10000/10000/25000us, **0x1010000**（bit16=power-domain+state1） | **无 cluster-pd** |
| 挂接 | cpu 节点 `cpu-idle-states` 引用 | **cpu 节点无任何引用** → cpuidle 无状态可注册 |

- 关键：stock 的 A76 用 mid/big 状态 + cluster-pd(0x1010000)；我们的 LIT/BIG 参数与 stock 的 lit/mid 相同，
  但**未挂接**（实验"去 idle-states"即此状态）；**cluster-pd（0x1010000，含 PD 位）我们从未定义**。

### 1.6 power-domains 对比

- stock cpu@0-7：**无 power-domains 属性**（cpu 电源经 PSCI+idle-states+cpufreq 域管理）。
- 我们：也无 → **不是差异**。任务 1 中"stock cpu@400-700 有 power-domains 吗？"答案是：**没有**。
- stock 的 power-domain phandle（0x1a 等）仅用于 GPU/DPU 等设备域，与 CPU 无关。

### 1.7 其他 A76 专属

- `sugov_slack_timer`（cpu0，vendor）：sprd 的 schedutil slack 优化属性，mainline 不识别 → 无影响。
- `compatible` 缺 "arm,armv8" 回退：mainline 6.12 对 CPU 节点不依赖该回退串（grep 内核源码无消费点）→ 无影响。

---

## 2. stock DTS cpu 节点完整属性 vs 我们（逐属性差异汇总）

见 §1.2 表格。**逐属性结论**：
- 相同：reg、enable-method(psci)、device_type、interrupt-parent。
- 缺失（我们）：cpu-idle-states、capacity-dmips-mhz、dynamic-power-coefficient、cpufreq-data-v1、
  #cooling-cells（cpu0/4/7）、sugov_slack_timer（cpu0）。
- compatible：我们少了 "arm,armv8" 回退（无实际影响）。
- **A76(CPU4-7) 与 A55(CPU0-3) 的区分度**：stock 中 A76 与 A55 在 capacity(2.45×)、DPC、idle-state、
  cpufreq 域全部不同；我们中 A76 与 A55 **除 compatible 字符串外无任何区分**。

---

## 3. A76 的 GIC/中断配置

### 3.1 GIC（stock 5035-5047 vs 我们 170-181）

| 项目 | stock | 我们 |
|---|---|---|
| compatible | arm,gic-v3 | 同 |
| reg | 0x12000000 0x20000 + 0x12040000 0x100000 | **完全相同** |
| redistributor-stride | 0x0 0x20000 | 同 |
| #redistributor-regions | 1 | 同 |
| interrupts (maintenance) | PPI9 | 同 |
| 覆盖 A76？ | GICR 0x100000 / 0x20000 = 8 个 redistributor = 8 核 | 同 → **A76 的 GICR 完全覆盖，无差异** |

### 3.2 arch timer（stock 5049-5053 vs 我们 136-144）

- stock：4 个 PPI（sec-phys=13、phys=14、virt=11、hyp-phys=10）+ `arm,no-tick-in-suspend`。
- 我们：5 个 PPI（多 PPI12 hyp-virt）+ interrupt-names；**缺 `arm,no-tick-in-suspend`**。
- PPI 对全部核（含 A76）按 CPU 私有中断处理，无核差异；`arm,no-tick-in-suspend` 仅影响 suspend。

### 3.3 PMU（stock 5055-5058 vs 我们 146-162）

- stock：单节点 `arm,armv8-pmuv3`，8 个 SPI（0xb1-0xb8 = 177-184），无 interrupt-affinity。
- 我们：pmu-a55（SPI 177-180，affinity CPU0-3）+ pmu-a76（SPI 181-184，affinity CPU4-7）。
- 中断号与 stock **完全一致**（177-184），A76 用 181-184；mainline `arm_pmuv3` 支持 arm,cortex-a55/a76-pmu
  compatible（arm_pmuv3.c:1380/1386 有 a55/a76 init）→ 功能等效，无竞态影响。

---

## 4. A76 的电源/时钟需求（PSCI 层面）

- psci 节点：stock（4970-4973）与我们的（131-134）**完全相同**：`arm,psci-0.2`, method="smc"。无差异。
- A76 的电源管理完全走 **PSCI CPU_SUSPEND 参数**（在 idle-states 里，见 §1.5）：
  - A76 CPU4-6：state1 (0x10000)；
  - A76 CPU7：state1 (0x10000)（不同 idle 状态节点，参数同）；
  - cluster-pd：0x1010000（power-domain bit16 + state1）。
- **我们**：idle-states 定义了 0x10000 状态但未挂接（cpuidle 无状态）；**cluster-pd(0x1010000) 从未定义**。
  - 但 cpuidle 已被实验排除为竞态根源；此差异意义在于"stock 的 A76 有完整 PSCI 电源状态描述，我们没有"。
- 无 DT power-domain（两版皆无）→ A76 不会走 PM_GENERIC_DOMAINS_OF 的 genpd 电源管理；
  注意我们的 config 开了 `CONFIG_PM_GENERIC_DOMAINS_OF=y`（579-582 行），stock 也开（483-486 行）——
  但两者 CPU 节点都没挂 power-domains，故该 config 差异不影响 CPU。

---

## 5. 原厂 5.4 对 A76 的运行时处理（stock_kallsyms.map）

### 5.1 A76 相关符号

| 符号 | 地址 | 含义 |
|---|---|---|
| `has_cortex_a76_erratum_1463225` | 0xffffffc010112168 | A76 SSBS errata（两版 config 都开 1463225） |
| `armv8_a55_pmu_init` / `armv8_a76_pmu_init` | 0x...11e6d0 / 0x...11eda8 | PMU init（5.4 也区分 A55/A76） |
| `sprd_cpufreq_cooling_init` / `_exit` | 0x...2198e24 / 0x...1ca508 | sprd cluster cooling（依赖 DTS cluster0/1/2-cooling） |
| `cpufreq_dvfs_module_notifier` | 0x...ea4aa8 | sprd DVFS 与 cpufreq 联动 |
| `scmi_cpufreq_probe` / `scpi_cpufreq_probe` | 0x...f136dc / 0x...f141b0 | 5.4 的 SCMI/SCPI cpufreq 框架（stock config 542-545：CPUFREQ_DT off，SCPI/SCMI cpufreq on） |

- 说明：stock 的 A76 运行时管理 = **sprd,cpufreq-v2（vendor 驱动）+ SCPI/SCMI 框架 + sprd cluster-cooling +
  PMU + errata**；mainline 全无（无 sprd cpufreq/cooling 驱动，SCMI/SCPI 无对应节点）。5.4 树
  `work/linux-5.4.210/` 为裁剪版（arch/fs/include/init/kernel/mm），无 sprd 驱动源码可核对，但符号表证实存在。

### 5.2 stock config vs 我们 config 关键差异（kernel.config vs .config）

| 选项 | stock | 我们 | 影响 |
|---|---|---|---|
| CONFIG_NR_CPUS | 32 | **512** | cpumask 尺寸、percpu 大小（NR_CPUS=4 实验 = 尺寸最小） |
| CONFIG_NUMA | **off** (376) | **on** (441) + NUMA_BALANCING | 我们引入 NUMA 域/NUMA 平衡路径（但 DTS 无 numa 节点 → 单节点） |
| CONFIG_SCHED_SMT | off (373) | **on** (438) | 我们打开 SMT 层（DTS 无 thread 层 → 退化） |
| CONFIG_SCHED_MC | on | on | 同 |
| CONFIG_SCHED_CLUSTER | （5.4 无此选项） | off (437) | 5.4 无；我们 off |
| CONFIG_ARM_PSCI_CPUIDLE_DOMAIN | （5.4 无） | on (609) | 我们开 psci domain cpuidle（无 power-domain 挂接则无效） |
| CONFIG_CPUFREQ_DT | off | **on** + PLATDEV | 我们开 DT cpufreq（但 DTS 无 operating-points → 不生效） |
| CONFIG_ENERGY_MODEL | on | on | 同，但 EM 需要 OPPs+DPC（两版 DTS 都无 OPPs；stock 有 DPC） |
| CONFIG_PM_GENERIC_DOMAINS_OF | on | on | 同 |

---

## 6. 综合：最可能导致"A76 存在 → 竞态"的缺失配置/机制（排序 + 修复）

### 6.1 排序

| 序 | 缺失/差异 | 与"A76 相关"的证据 | 为何可能致竞态 | 是否已测 |
|---|---|---|---|---|
| 1 | **cpu-map 三簇 4+3+1（CPU7 独立簇）** | stock 明确把 CPU7 放独立 cluster；CPU7 有独立 cpufreq 域/独立 idle/更高 capacity/DPC | 调度域结构、LLC 回退、wake_affine 范围与 4+4 完全不同；含 A76 时调度语义偏离 stock | **否（只测过 1 簇/2 簇 4+4）** |
| 2 | **capacity-dmips-mhz / dynamic-power-coefficient 缺失** | stock A76=1022/1024 vs A55=417（2.45×）；DPC 721/896 vs 155 | 我们 8 核全 1024 → 调度器对称 → 唤醒/均衡行为=纯 A55 语义；掩盖 A76 非对称容量；EAS 关闭 | 否（D4 建议过，未做） |
| 3 | **cpufreq 域缺失（cluster0/1/2 + sprd,cpufreq-v2）** | stock 有 3 个 DVFS 域，CPU7 单独域；符号表有 sprd cpufreq/cooling | A76 无 DVFS，频率/电压不受控；A76 高频率下总线/内存时序敏感可能放大竞态 | 否 |
| 4 | **idle-states 未挂接 + 无 cluster-pd(0x1010000)** | stock A76 用 mid/big + cluster-pd | cpuidle 已排除；但 A76 的 PSCI 电源状态描述缺失是结构性差异 | 否（"去 idle-states"实验 = 确认无状态；但从未按 stock 挂接+加 cluster-pd 测试） |
| 5 | cooling/cooling-cells 缺失 | stock 有 cluster0/1/2-cooling | 热管理后于竞态点，低相关 | 否 |
| 6 | PMU 拆两节点 / timer 缺 no-tick-in-suspend | 中断号一致 | 无 | 无关 |
| 7 | GIC | 完全一致 | 无 | 无关 |

### 6.2 竞态解释（与全部已排除项相容）

- 纯 A55（NR_CPUS=4）：只有 CPU0-3，DTS 中 CPU4-7 节点被忽略（possible_cpus=0-3）→ 调度器面对
  4 个等容量核，行为简单，稳定。
- 含 A76（8 核）：我们的 DTS 让 8 核以"对称容量 + 4+4 双簇（CPU7 混入 A76 簇）"上线，而 stock 是
  "4+3+1 三簇 + 非对称容量 + CPU7 独立"。调度器在 A76 核上做 wakeup/均衡/IPI 的**目标选择与 timing**
  偏离 stock → 二次唤醒（kdevtmpfs）落到空闲 A76 的行为与 stock 不同 → 与已排除的唤醒/IPI 机制叠加
  即复现竞态。**任何单一传输机制（CSD/直接入队）都不变，变的是"该往哪个核发、何时发"——这正是 DTS
  拓扑/容量缺失所能改变的部分。**

### 6.3 修复方案（按优先级）

1. **复现 stock 三簇拓扑（一次性，成本最低）**：cpu-map 改 cluster0=CPU0-3、cluster1=CPU4-6、
   cluster2=CPU7；每个 cpu 节点补 `capacity-dmips-mhz`（A55=417、A76=1022、CPU7=1024）与
   `dynamic-power-coefficient`（155/721/896）。观察竞态是否消失/改变（验证拓扑+容量假设）。
2. **补 idle-states 挂接**：cpu 节点加 `cpu-idle-states = <&LIT_CORE_PD>`（A55）/ `<&BIG_CORE_PD>`（A76），
   并按 stock 增加 `cluster-pd`（arm,psci-suspend-param=0x1010000）。注意：此改动会重新启用 cpuidle，
   需与"cpuidle 去 idle-states"实验区分变量。
3. **config 对齐**：`CONFIG_NR_CPUS` 降到 8~32（与 stock 32 对齐，避免 512 的 cpumask/percpu 膨胀）、
   评估关闭 `CONFIG_NUMA`/`CONFIG_SCHED_SMT`（stock 均为 off）。
4. **cpufreq**：mainline 无 sprd 驱动 → 需自行移植 `sprd,cpufreq-v2` 或提供 operating-points +
   cpufreq-dt 支持（工作量大）；短期可先验证 LK 遗留的 A76 频率/电压是否稳定（若 A76 长期跑在过高频率，
   竞态可能是电压/时序问题的放大）。
5. **观测**：读 MIDR_EL1 确认 A76 修订（决定 4118414/3194386 errata 是否真实生效）；开机参数
   `cpuidle.off=1`、`nohz=off`、`maxcpus=5..8` 二分核数触发点。

---

## 附：本次对比使用的关键行号索引（stock_ums9620.dts）

- cpu-map：4799-4841（cluster0/1/2）
- cpu@0-7：4843-4941
- cpufreq-cluster0/1/2：4944-4968
- psci：4970-4973
- idle-states：4991-5033（lit 4994-5002 / mid 5004-5012 / big 5014-5022 / cluster-pd 5024-5032）
- GIC：5035-5047
- timer：5049-5053
- pmu：5055-5058
- apcpu-dvfs@950000（sprd,cpufreq-v2）：3293-3296
- soc-thmzone cooling-maps：6620-6669
- cooling-devices（cluster0/1/2-cooling）：6918-6997
- phandle 映射表：7626-7719
