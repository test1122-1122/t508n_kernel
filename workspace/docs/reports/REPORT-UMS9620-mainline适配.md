# UMS9620 mainline 内核适配信息整理

> 整理时间：本次 bring-up 调试期间（联网搜索 + lore.kernel.org 下载）
> 下载资料目录：`~/ums9620-ref/`（含完整 patch 系列 `series-v3.mbox`）

---

## 1. UMS9620 mainline 支持时间线

| 内核版本 | 时间 | 内容 | 提交者 | 说明 |
|---|---|---|---|---|
| 6.7 | 2023-12 | DTS 基础支持 | Chunyan Zhang | `arm64: dts: sprd: Add support for Unisoc's UMS9620`（commit `bb8551c19da0`），新增 `arch/arm64/boot/dts/sprd/ums9620.dtsi` |
| 6.8 | 2024-01 | clk 驱动 | Chunyan Zhang | `clk: sprd: add support for UMS9620` |
| 6.8+ | 2024-04 | PMU 节点拆分 | Rob Herring | `arm64: dts: sprd: Split PMU nodes for heterogeneous CPUs`（A55/A76 异构 PMU 拆分） |
| **6.15+** | **2026-05** | **EL2 virtual timer** | **Marc Zyngier** | 17 patch 系列，VHE 下改用 EL2 虚拟定时器，**含 UMS9620 专用 patch** |

> **关键结论**：UMS9620 的 DTS/clk 在 6.7/6.8 就已合入，但 **EL2 虚拟定时器（hyp-virt）支持直到 2026 年 5 月才合入**。6.12.103（本项目使用的版本）正好缺这一块。

---

## 2. EL2 virtual timer 系列（Marc Zyngier, 2026-05）

### 动机（cover letter 原文要点）

> 在 VHE 模式下，host 内核运行于 EL2。此时架构提供了 **EL2 虚拟定时器（CNTHV）**，arch timer 驱动应该改用它，而不是 EL2 物理定时器（CNTHP）。
>
> 好处：深层嵌套虚拟化（KVM NV）场景下显著性能提升，对 host 零开销。

### patch 清单（v3，共 17 个）

| 编号 | 内容 |
|---|---|
| 01/17 | ACPI: GTDT: 处理 GTDTv3 大小 |
| 02/17 | ACPI: GTDT: 解析 EL2 虚拟定时器信息 |
| **03/17** | **clocksource/drivers/arm_arch_timer: Default to EL2 virtual timer when running VHE** |
| 04/17 | dt-bindings: timer: arm,arch_timer: 修正 interrupt 描述要求 |
| 05~17/17 | 各 SoC DTS 加 EL2 虚拟定时器中断（allwinner/amlogic/bst/exynos/freescale/intel/mediatek/nvidia/qcom/realtek/rockchip/**sprd**/xilinx） |

### 核心 patch 03/17 的关键改动

`drivers/clocksource/arm_arch_timer.c`：

1. **`arch_timer_select_ppi()`**：
   ```c
   if (is_kernel_in_hyp_mode()) {
       if (arch_timer_ppi[ARCH_TIMER_HYP_VIRT_PPI])
           return ARCH_TIMER_HYP_VIRT_PPI;   // 优先 EL2 虚拟定时器
       return ARCH_TIMER_HYP_PPI;             // 无 hyp-virt 则回退
   }
   ```
2. **3 处 switch 加 `case ARCH_TIMER_HYP_VIRT_PPI:`**（复用 virt 的 shutdown/set_next_event/handler，因为 VHE 下 `cntv_ctl_el0` 自动重定向到 `cnthv_ctl_el2`）
3. **`arch_counter_register()`**：`arch_timer_uses_ppi == ARCH_TIMER_HYP_VIRT_PPI` 时用虚拟计数器（cntvct）

### patch 16/17（sprd/UMS9620 专用）diff

```diff
--- a/arch/arm64/boot/dts/sprd/ums9620.dtsi
+++ b/arch/arm64/boot/dts/sprd/ums9620.dtsi
@@ timer {
 		interrupts = <GIC_PPI 13 IRQ_TYPE_LEVEL_HIGH>, /* Physical Secure PPI */
 			     <GIC_PPI 14 IRQ_TYPE_LEVEL_HIGH>, /* Physical Non-Secure PPI */
 			     <GIC_PPI 11 IRQ_TYPE_LEVEL_HIGH>, /* Virtual PPI */
-			     <GIC_PPI 10 IRQ_TYPE_LEVEL_HIGH>; /* Hipervisor PPI */
+			     <GIC_PPI 10 IRQ_TYPE_LEVEL_HIGH>, /* Hypervisor physical PPI */
+			     <GIC_PPI 12 IRQ_TYPE_LEVEL_HIGH>; /* Hypervisor virtual PPI */
+		interrupt-names = "sec-phys", "phys", "virt", "hyp-phys", "hyp-virt";
 	};
```

> **PPI 编号确认**：`hyp-virt = PPI 12`（EL2 虚拟定时器 CNTHV）。commit message 原文：
> "The ARMv8.2 based CPUs used in a number of Unisoc SoCs are missing the EL2 virtual timer interrupt. Add it."

---

## 3. 本项目的应用情况

| 事项 | 状态 |
|---|---|
| DTS 5 中断（PPI 12 + interrupt-names） | ✅ 已应用（`arch/arm64/boot/dts/sprd/ums9620.dtsi`） |
| arch_timer.c 的 03/17 改动 | ✅ 已应用（`arch_timer_select_ppi` + 3 处 switch + counter） |
| tick 正常性 | ✅ y=60 长条（jiffies 推进，hyp-virt 中断工作） |

**效果**：应用 EL2 virtual timer 后，之前的 workqueue_init 卡死得到推进（fork 概率性卡死消失），内核推进到 smp_init。

> 注意：之前 workqueue_init 卡死的**真正根因**是 dtbo 分区被恢复成 stock（DTBO merge 失败），刷回 noop dtbo 后即解决，与 EL2 virtual timer 无直接因果关系。

---

## 4. 当前遗留问题（与 EL2 virtual timer 无关）

**smp_init 卡死：二级核开启 MMU 后取指失败。**

已排查确认：
- 二级核从 EL2 进入（与 boot CPU 一致），`init_kernel_el` 正常返回
- 走到 `__enable_mmu` 的 `set_sctlr_el1` 前（探针 x=152 亮），开启 MMU 后死
- boot CPU 侧 cache flush（`dcache_clean_poc`）+ 二级核侧 `dcache_inval_poc` 均未解决
- 二级核用 `idmap_pg_dir`（`create_idmap()` 创建，只映射 `.idmap.text` 段），boot CPU 用 `init_idmap_pg_dir`（`create_init_idmap()`，映射整个内核）

**待查方向**：
1. `idmap_pg_dir[0]` 内容是否非空（create_idmap 映射是否真的写入）—— 需换 fbready 之后的时机验证
2. `__cpu_setup` 返回的 TCR T0SZ 是否真为 16（48 位 idmap）
3. `idmap_pg_dir` 的 4 级页表（IDMAP_VA_BITS=48）在 CONFIG_PGTABLE_LEVELS=3（VA_BITS=39）配置下的正确性

---

## 5. 参考资料 URL

- 系列 v3（含全部 17 patch + 回复）：https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-1-maz@kernel.org/
- LWN 报道：https://lwn.net/Articles/1074143/
- patch 03/17（arch_timer 驱动）：https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-4-maz@kernel.org/
- patch 16/17（sprd DTS）：https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-17-maz@kernel.org/
- UMS9620 DTS 初版（6.7）：https://lkml.org/lkml/2023/12/18/101
- UMS9620 clk（6.8）：https://lists.openwall.net/linux-kernel/2024/01/04/178
- UMS9620 PMU 拆分（6.8+）：https://patchew.org/linux/20240412222923.3873814-1-robh@kernel.org/
- arch_timer 绑定文档：https://www.kernel.org/doc/Documentation/devicetree/bindings/timer/arm%2Carch_timer.yaml

---

## 附：本地下载资料清单（~/ums9620-ref/）

| 文件 | 内容 |
|---|---|
| `series-v3.mbox` | 完整 17 patch + 全部讨论回复（424KB） |
| `ums9620.dtsi` | mainline 当前 UMS9620 DTS（4 中断版，未含 PPI 12） |
| `arm-arch-timer.yaml` | arch_timer 绑定文档 |
| `patch-03-arch-timer.txt` | 403 下载失败（内容已从 mbox 提取） |
| `patch-16-sprd-dts.txt` | 403 下载失败（内容已从 mbox 提取） |
