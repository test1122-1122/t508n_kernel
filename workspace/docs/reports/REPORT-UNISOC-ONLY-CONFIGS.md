# REPORT：紫光（Unisoc/SPRD）特有 config 与标准内核的差集分析

## 摘要

对比原厂 `kernel.config`（4941 个 CONFIG_ 选项）与标准内核 **v5.4.210**（Kconfig 树提取 17451 个符号）和 **v6.12.103**（20571 个符号）的 Kconfig 选项集：

| 差集 | 定义 | 数量 | 含义 |
|---|---|---|---|
| **A** | kernel.config − 5.4.210 Kconfig | **394** | 紫光 5.4 fork 特有（标准 5.4 无此选项） |
| **B** | kernel.config − 6.12.103 Kconfig | **551** | 连 6.12 也没有的选项（含 5.4 曾有、6.12 移除的主线项） |
| **C** | kernel.config − (5.4 ∪ 6.12) | **312** | 彻底无对应（两个标准版本都没有）＝ 真正无法复刻的核心 |

集合关系：`C ⊂ A`（C 全部在 A 内）、`C ⊂ B`（C 全部在 B 内）；
`A−C = 82` 个选项是 5.4 没有、但 **6.12 已上游**（KFENCE/CFI_CLANG/LTO/SCS/DRM_SPRD/SPRD_IOMMU 等，可从 6.12 移植）；
`B−C = 239` 个选项是 **5.4 主线就有、6.12 移除/改名**的主线项（如 CONFIG_ANDROID/ASHMEM/64BIT_TIME），不属于紫光特有，5.4 直接可用。

### 分类统计（C 层 312 项）

| 大类 | 数量 | 说明 |
|---|---|---|
| 驱动类 | 138 | SPRD/Unisoc 外设驱动（时钟/USB PHY/PMIC/充电/UFS/MMC/WLAN/pinctrl/DVFS 等） |
| 设施/调试类 | 101 | 挂死监控/sysdump/Modem IPC(SIPC)/Trusty/thermal 等 Unisoc 设施 |
| 内核特性类 | 40 | MM/调度/fs/net 分支特性（SPF/PELT half-life/process reclaim 等） |
| 编译层 | 33 | GKI 隐藏项/Clang LTO·PGO·CFI·SCS 相关 |

## 一、C 层：彻底无对应（312 项）——真正无法复刻的核心

以下选项在 v5.4.210 与 v6.12.103 的 Kconfig 树中均不存在。按与 bring-up（boot 到 initramfs）的相关性标注：

| 选项 | 类型 | 子类 | 复刻可行性 | bring-up 相关性 |
|---|---|---|---|---|
| CONFIG_ANDROID_KABI_RESERVE | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_ANDROID_VENDOR_HOOKS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_ANDROID_VENDOR_OEM_DATA | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_ARCH_SUPPORTS_PGO_CLANG | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_ARCH_SUPPORTS_SPECULATIVE_PAGE_FAULT | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_ARCH_SUPPORTS_THINLTO | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_ARM_SPRD_CPUFREQ_PUBLIC | 驱动类 | Unisoc cpufreq/cpuidle | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_ARM_SPRD_CPUFREQ_V2 | 驱动类 | Unisoc cpufreq/cpuidle | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_ARM_SPRD_CPUIDLE | 驱动类 | Unisoc cpufreq/cpuidle | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_ARM_SPRD_SW_CPUFREQ | 驱动类 | Unisoc cpufreq/cpuidle | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_BUILD_ARM64_DTC | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_BUILD_ARM64_DTC_FLAGS | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_CFI_CLANG_SHADOW | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_CHARGER_AW32257 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_BQ2560X | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_BQ25910 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_BQ2597X | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_ETA6937 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_FAN54015 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_SC8549 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_SC8551 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_SGM41511 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CHARGER_UPM6710 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_CORESIGHT_APETB | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_CORESIGHT_PHY | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_CORESIGHT_TMC_GROUP | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_CPUFREQ_DUMMY | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_CPU_FREQ_GOV_INTERACTIVE | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_CPU_FREQ_TIMES | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_CRYPTO_BLAKE2S | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_DEVFREQ_GOV_SPRD_VOTE | 驱动类 | Unisoc devfreq/DDR DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_DEVFREQ_SPRD_DDR_DVFS | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_DEVFREQ_SPRD_DDR_DVFS_SP | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_DIRECT_SWAPPINESS | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_DM_BOW | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_DM_DEFAULT_KEY | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_DM_USER | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_DRM_SPRD_GSP_DVFS | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_DVFS_APSYS_SPRD | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_ENHANCE_SMAPS_INFO | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_E_SHOW_MEM | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_FAST_CHARGER_SC27XX | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_GKI_HACKS_TO_FIX | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_CRYPTO_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_DRM_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_GPIO_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_GPU_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_HYPERVISOR_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_IRQ_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_MEDIA_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_MMC_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_MM_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_QCOM_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_REGMAP_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_RPMSG_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_SND_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_SND_SOC_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_SOC_BUS_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_USB_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_HIDDEN_VIRTUAL_CONFIGS | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GKI_LEGACY_WEXT_ALLCONFIG | 编译层 | Android GKI 隐藏项/hook | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低(仅 GKI 内核) |
| CONFIG_GNSS_CMDLINE_SERIAL | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_GPIO_PLUS_SPRD | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_HW_RANDOM_SPRD | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_I2C_SPRD_HW | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_I2C_SPRD_HW_V2 | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_INCREMENTAL_FS | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_IOMMU_IOVA_ALIGNMENT | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_IOMMU_LIMIT_IOVA_ALIGNMENT | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_ION_SPRD | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_IP6_NF_VOWIFI_DEC_TCPDUMP | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_IP_NF_VOWIFI_DEC_TCPDUMP | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_IRQSOFF_TRACER_BYTEDANCY | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_KEYBOARD_SPRD | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_LEDS_SC27XX_KEYPAD | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_LEDS_SC27XX_VIBRA | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_LEDS_TORCH_GPIO | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_LIB_MEMNEQ | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_LRU_BALANCE_BASE_THRASHING | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_MEMCGV2_DECAYED_WATERMARK | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_MFD_UMP96XX_PMIC | 驱动类 | Unisoc PMIC | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(电源域/regulator) |
| CONFIG_MMC_SPRD_SDHCR11P3 | 驱动类 | Unisoc MMC/SD | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_MMC_SWCQ | 驱动类 | Unisoc MMC/SD | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_NETFILTER_XT_MATCH_QUOTA2 | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_NETFILTER_XT_MATCH_QUOTA2_LOG | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PCIE_SPRD | 驱动类 | Unisoc PCIe | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_PELT_UTIL_HALFLIFE_16 | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PELT_UTIL_HALFLIFE_32 | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PELT_UTIL_HALFLIFE_8 | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PGO_CLANG | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_PGO_NONE | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_PINCTRL_SPRD_PIKE2 | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_QOGIRL6 | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_QOGIRL6PRO | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_SHARKL3 | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_SHARKL5 | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_SHARKL5PRO | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PINCTRL_SPRD_SHARKLE | 驱动类 | Unisoc pinctrl | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(UART/GPIO) |
| CONFIG_PRINTK_CPUID | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PRINTK_TIME_BOOT | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PRINTK_TIME_LOCAL | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PRINTK_TIME_MONO | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PRINTK_TIME_REAL | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PRINTK_TIME_TYPE | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_PROCESS_RECLAIM | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_RCC | 驱动类 | Unisoc 杂项(regmap/EIC/PD) | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_REGMAP_HOOK | 驱动类 | Unisoc 杂项(regmap/EIC/PD) | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_REGULATOR_UMP9620 | 驱动类 | Unisoc PMIC | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(电源域/regulator) |
| CONFIG_REGULATOR_UMP9621 | 驱动类 | Unisoc PMIC | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(电源域/regulator) |
| CONFIG_RPMB_INTF_DEV | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_RPMB_SIM | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SC2332_SIPC_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355 | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_PCIE_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_SDIO_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_SIPC_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_WLAN_NAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC2355_WLAN_RTT | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC23XX | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC27XX_PD | 驱动类 | Unisoc 杂项(regmap/EIC/PD) | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SC27XX_TSENSOR_THERMAL | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SCSI_UFS_HPB | 驱动类 | Unisoc UFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(根文件系统存储) |
| CONFIG_SCSI_UFS_SPRD_QOGIRL6 | 驱动类 | Unisoc UFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(根文件系统存储) |
| CONFIG_SCSI_UFS_SPRD_QOGIRN6PRO | 驱动类 | Unisoc UFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(根文件系统存储) |
| CONFIG_SDIOHAL | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SELINUX_AVC_BACKTRACE | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_SHADOW_CALL_STACK_VMAP | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_SHOW_UREGS_WITH_PHYSICAL | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_SND_I2S_HI3660_I2S | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_SPECULATIVE_PAGE_FAULT | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_SPECULATIVE_PAGE_FAULT_DEBUG | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_SPRD_7SRESET | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_APDU_R3P0 | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_AUTOTEST | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_AUTOTEST_GPIO | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_AUTOTEST_OTG | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_AUTOTEST_PINCTRL | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_BC1P2 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_SPRD_CACHE_EFUSE | 驱动类 | Unisoc efuse | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_CMA_DEBUG | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_COMMONPHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_CORE_CTL | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_CORE_ISOLATION | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_CPU_COOLING | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_CP_DUMP | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_DEBUG | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_DEDICATE_THERMAL | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_DISP_PM_DOMAIN_SHARKL3 | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_DMC_DRV | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_EFUSE | 驱动类 | Unisoc efuse | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_EMERGENCY_RESTART | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_GPU_COOLING_DEVICE | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_HANG_DEBUG | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HANG_DEBUG_UART | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HANG_RESET | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HANG_TRIGGER | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HANG_WDF | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HANG_WDH | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_HW_DEVICE_DVFS_CP | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_IMS_BRIDGE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IMS_BRIDGE_TEST | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IMS_BRIDGE_TRANSIT | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_DELEGATE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_ETH | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_SUPPORT | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_SYS | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_USB | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IPA_V3 | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_IQ | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_KTP | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_MEMDISK | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_MEM_OVERLAY_CHECK | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_MINI_SYSDUMP | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_MIPI_LOG_QOGIRN6PRO | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R0P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R1P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R2P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R3P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R4P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R4P2 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R5P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_LOG_R5P1 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MIPI_SWITCH | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_MODEM_DEBUG | 设施/调试 | Unisoc Modem 加载 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_MODEM_POWER_MANAGER | 设施/调试 | Unisoc Modem 加载 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_NATIVE_HANG_MONITOR | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_NET_HELPER | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_NPU_COOLING_DEVICE | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_PAMU3 | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_PIKE2_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_PMIC_SMPL | 驱动类 | Unisoc PMIC | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(电源域/regulator) |
| CONFIG_SPRD_PMIC_SYSCON | 驱动类 | Unisoc PMIC | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(电源域/regulator) |
| CONFIG_SPRD_POWER_DEBUG | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_POWER_DOMAIN | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_POWER_STAT | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_QOGIRL6_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_QOGIRN6PRO_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_QOGIRN6PRO_USB31_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_REDRIVER_PTN38003A | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_SPRD_ROTATION_TASK | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_SC7731E_CLK | 驱动类 | Unisoc clock | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(时钟必需) |
| CONFIG_SPRD_SC9832E_CLK | 驱动类 | Unisoc clock | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(时钟必需) |
| CONFIG_SPRD_SENSOR_HUB | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_SENSOR_HUB_NEW | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_SFP_SUPPORT | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SFP_TEST | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SHARKL3_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_SHARKL5PRO_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_SHARKL5_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_SHARKL6PRO_CORESIGHT_PHY | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_SHARKLE_USB2_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_SIPC | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_MEM_CACHE_EN | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SBLOCK_BRIDGE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SBUF_BRIDGE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SETH | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SLOG_BRIDGE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SPIPE | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_SPOOL | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_WCN | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIPC_ZERO_COPY_SIPX | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SIP_FW | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_SOCID | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_SOC_THERMAL | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_SYSDUMP | 设施/调试 | Unisoc 崩溃/挂死监控 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中 |
| CONFIG_SPRD_SYSTIMER | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_THERMAL_DEBUG | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_THERMAL_MAX_FREQ_LIMIT | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_THERMAL_POLICY | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_THERMAL_R5P0 | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_TIME_SYNC | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_TIME_SYNC_CH | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_TIME_SYNC_CP | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_TOP_DVFS_DEVFREQ | 驱动类 | Unisoc power/DVFS | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_TSHM | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_TYPEC_DP_ALTMODE | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_SPRD_TYPEC_TCPM | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_SPRD_UID | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_SPRD_UMP518_EIC_DBNC | 驱动类 | Unisoc 杂项(regmap/EIC/PD) | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SPRD_UMP96XX_TSENSOR | 设施/调试 | Unisoc thermal/冷却 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 中(过温保护) |
| CONFIG_SPRD_UMS312_CLK | 驱动类 | Unisoc clock | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(时钟必需) |
| CONFIG_SPRD_UMS9230_CLK | 驱动类 | Unisoc clock | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(时钟必需) |
| CONFIG_SPRD_UMS9620_CLK | 驱动类 | Unisoc clock | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(时钟必需) |
| CONFIG_SPRD_USBM | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_SPRD_USBPINMUX | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_USBPINMUX_QOGIRL6 | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_USB_CONFIGFS_RNDIS | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_SPRD_USB_DUMMY_PHY | 驱动类 | Unisoc USB PHY | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(USB 调试口) |
| CONFIG_SPRD_USB_F_RNDIS | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_SPRD_USB_U_ETHER | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_SPRD_WATCHDOG_FIQ | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_WATCHDOG_PMIC | 驱动类 | Unisoc 外设驱动 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 中 |
| CONFIG_SPRD_WLAN | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SPRD_WLAN_DEBUGFS | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_SPRD_WLAN_VENDOR_SPECIFIC | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_THINLTO | 编译层 | Clang LTO/PGO/CFI/SCS | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_TRANSPARENT_HUGE_PAGECACHE | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_TRUSTY | 设施/调试 | Trusty TEE | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 高(如固件要求 TEE) |
| CONFIG_TRUSTY_LOG | 设施/调试 | Trusty TEE | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 高(如固件要求 TEE) |
| CONFIG_TRUSTY_TUI | 设施/调试 | Trusty TEE | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 高(如固件要求 TEE) |
| CONFIG_TRUSTY_VIRTIO | 设施/调试 | Trusty TEE | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 高(如固件要求 TEE) |
| CONFIG_TRUSTY_VIRTIO_IPC | 设施/调试 | Trusty TEE | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 高(如固件要求 TEE) |
| CONFIG_UBSAN_MISC | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_UID_SYS_STATS | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_UID_SYS_STATS_DEBUG | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_UMW2652 | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_UMW2653 | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_UNISOC_GNSS | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_GNSS_COMMON_CTL | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_GNSS_DBG | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_GNSS_PMNOTIFY_CTL | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_IOMMU | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_UNISOC_MAILBOX | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_MAILBOX_R1 | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_MAILBOX_R2 | 设施/调试 | Unisoc 辅助设施 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低~中 |
| CONFIG_UNISOC_MAPUSER | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_UNISOC_MODEM_LOADER | 设施/调试 | Unisoc Modem 加载 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_UNISOC_MODEM_LOADER_RAM_VMAP | 设施/调试 | Unisoc Modem 加载 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_UNISOC_SIPC_SLOG_BRIDGE_5G | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_UNISOC_SIPC_VIRT_BUS | 设施/调试 | Unisoc IPC/Modem 通信 | 无法直接复刻（Unisoc 设施，依赖原厂固件接口） | 低(boot 不需要) |
| CONFIG_UNISOC_WCN_BSP | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_USB_CONFIGFS_F_ACC | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_CONFIGFS_F_AUDIO_SRC | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_CONFIGFS_F_VSERIAL | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_CONFIGFS_UEVENT | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_DWC3_SPRD | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_DWC3_SPRD_CORE | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_F_ACC | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_F_AUDIO_SRC | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_F_VSERIAL | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_F_VSERIAL_BYPASS_USER | 驱动类 | Android 通用 USB gadget 扩展 | 需评估 | 低(非必需) |
| CONFIG_USB_MUSB_SPRD | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_PAM | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_SC27XX_TYPEC | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_USB_SPRD_DMA | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_SPRD_DMA_V3 | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_SPRD_OFFLOAD | 驱动类 | Unisoc USB controller | 无法直接复刻（需原厂驱动源码/反汇编移植） | 高(ADB/存储) |
| CONFIG_USB_VIRTUAL_TYPEC | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_USE_PROFILE | 编译层 | profile 工具 | 可复刻（属于工具链/构建层，6.12 已含同类设施） | 低 |
| CONFIG_WCN_BOOT | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_WCN_INTEG | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_WCN_RDCDBG | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_WCN_SLP | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_WIRELESS_CHARGER_NU1619 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_WIRELESS_CHARGER_SY65153 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_WIRELESS_CHARGE_PUMPS_CONVERTER_HL1506 | 驱动类 | 充电/Type-C/PMIC 配套 | 需评估 | 中 |
| CONFIG_WLAN_VENDOR_SPREADTRUM | 驱动类 | Unisoc WLAN/IOMMU/其他 | 无法直接复刻（需原厂驱动源码/反汇编移植） | 低~中 |
| CONFIG_WRITEBACK_SWAPCACHE | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_XFRM_FRAGMENT | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |
| CONFIG_ZRAM_DEDUP | 内核特性 | MM/调度/fs/net 分支特性 | 需移植（fork 内核特性，无主线对应补丁，需重实现） | 低~中 |

## 二、A−C 层：5.4 没有、但 6.12 已上游（82 项）——可从 6.12 反向移植

这些选项 5.4 标准内核没有（故在 A 中），但 6.12 的 Kconfig 已定义（故不在 C 中）。对 5.4 路线需要反向移植，对 6.12 路线可直接复刻。

| 选项 | 类型 | 子类 | 复刻可行性 | bring-up 相关性 |
|---|---|---|---|---|
| CONFIG_ARCH_SUPPORTS_LTO_CLANG | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_ARCH_SUPPORTS_SHADOW_CALL_STACK | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_ARM64_AMU_EXTN | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_BLK_CGROUP_IOPRIO | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_BLK_INLINE_ENCRYPTION | 编译层 | 6.12 已上游的内联加密 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK | 编译层 | 6.12 已上游的内联加密 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_BOOT_CONFIG | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CC_HAS_AUTO_VAR_INIT_PATTERN | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CC_HAS_AUTO_VAR_INIT_ZERO | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CC_HAS_AUTO_VAR_INIT_ZERO_ENABLER | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CC_HAVE_SHADOW_CALL_STACK | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CC_VERSION_TEXT | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CFI_CLANG | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CFI_PERMISSIVE | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_ARCH_HAVE_LIB_CHACHA | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_ARCH_HAVE_LIB_POLY1305 | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_BLAKE2B | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_CURVE25519 | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_BLAKE2S_GENERIC | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_CHACHA | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_CHACHA20POLY1305 | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_CHACHA_GENERIC | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_CURVE25519 | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_CURVE25519_GENERIC | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_POLY1305 | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_POLY1305_GENERIC | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_LIB_POLY1305_RSIZE | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_CRYPTO_POLY1305_NEON | 内核特性 | 6.12 已上游的 crypto 库 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DMABUF_HEAPS | 内核特性 | 6.12 已上游的 dmabuf heaps | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DMABUF_HEAPS_CMA | 内核特性 | 6.12 已上游的 dmabuf heaps | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DMABUF_HEAPS_SYSTEM | 内核特性 | 6.12 已上游的 dmabuf heaps | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DMABUF_SYSFS_STATS | 内核特性 | 6.12 已上游的 dmabuf heaps | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DRM_LONTIUM_LT9611 | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_DRM_SPRD | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_DYNAMIC_DEBUG_CORE | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_EROFS_FS_PCPU_KTHREAD | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_EROFS_FS_PCPU_KTHREAD_HIPRI | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_COMPRESSION | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_LZ4 | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_LZ4HC | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_LZO | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_LZORLE | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_F2FS_FS_ZSTD | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_FS_ENCRYPTION_ALGS | 内核特性 | 6.12 已上游的 fs 特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_FS_ENCRYPTION_INLINE_CRYPT | 编译层 | 6.12 已上游的内联加密 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_FW_CACHE | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HAVE_ARCH_KFENCE | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HAVE_ARCH_USERFAULTFD_MINOR | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HAVE_MOVE_PUD | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HID_NINTENDO | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HID_PLAYSTATION | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_HISI_HIKEY_USB | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_INIT_STACK_ALL_PATTERN | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_INIT_STACK_ALL_ZERO | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KFENCE | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KFENCE_NUM_OBJECTS | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KFENCE_SAMPLE_INTERVAL | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KFENCE_STATIC_KEYS | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KFENCE_STRESS_TEST_FAULTS | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_KUNIT | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_LLD_VERSION | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_LTO | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_LTO_CLANG | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_LTO_NONE | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_MMC_CRYPTO | 编译层 | 6.12 已上游的内联加密 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_MMC_HSQ | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_PLAYSTATION_FF | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_SCSI_UFS_CRYPTO | 编译层 | 6.12 已上游的内联加密 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_SCSI_UFS_SPRD | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_SECURITY_SELINUX_SIDTAB_HASH_BITS | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_SHADOW_CALL_STACK | 编译层 | 6.12 已上游的编译器/加固设施 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_SPRD_IOMMU | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_SPRD_MBOX | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_SPRD_SC9863A_CLK | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_SPRD_THERMAL | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_SPRD_UMS512_CLK | 驱动类 | 6.12 已上游的 SPRD 驱动(旧SoC) | 可复刻（6.12 已上游，可从 6.12 移植） | 中 |
| CONFIG_TRACE_GPU_MEM | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_UBSAN_BOUNDS | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_UBSAN_TRAP | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_USB_XHCI_PCI_RENESAS | 内核特性 | 6.12 已上游的其他特性 | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_WIREGUARD | 内核特性 | 6.12 已上游的 wireguard | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |
| CONFIG_WIREGUARD_DEBUG | 内核特性 | 6.12 已上游的 wireguard | 可复刻（6.12 已上游，可从 6.12 移植） | 低 |

## 三、B−C 层：5.4 主线已有、6.12 移除/改名（239 项）——非紫光特有

这些选项在标准 5.4 的 Kconfig 中存在（故不在 A、不在 C），仅在 6.12 中消失（改名或删除）。它们不是紫光特有项：**5.4 路线直接可用**；6.12 路线需要核对改名（如 `SC27XX_EFUSE`→`NVMEM_SC27XX_EFUSE`）或接受缺失。

| 选项 | 备注 |
|---|---|
| CONFIG_64BIT_TIME | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ADE7854 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ANDROID | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ANDROID_VSOC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARCH_AGILEX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARCH_HAS_DMA_COHERENT_TO_PFN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARCH_HAS_UBSAN_SANITIZE_ALL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARCH_STRATIX10 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARCH_ZX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_CONT_SHIFT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_CRYPTO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_MODULE_PLTS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_PAGE_SHIFT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_PTDUMP_DEBUGFS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_RANDOMIZE_TEXT_OFFSET | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_SSBD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_UAO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM64_VHE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ARM_GIC_V3_ITS_PCI | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ASHMEM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_AUTOFS4_FS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BACKLIGHT_GENERIC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BACKLIGHT_PM8941_WLED | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BASE_FULL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_CMDLINE_PARSER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_CRYPTOLOOP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_RSXX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_SKD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_SX8 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_THROTTLING_LOW | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_DEV_UMEM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLK_SCSI_REQUEST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BLOCK_COMPAT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC_VALUE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BPFILTER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BT_HS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_BUILDTIME_EXTABLE_SORT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CAN_ESD_USB2 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CC_HAS_ASM_GOTO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CC_HAS_STACKPROTECTOR_NONE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CEC_PLATFORM_DRIVERS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CLEANCACHE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CLKDEV_LOOKUP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CLOCK_THERMAL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CMA_DEBUG | 5.4 有 / 6.12 移除或改名 |
| CONFIG_COMMON_CLK_VERSATILE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_BLKCIPHER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_BLKCIPHER2 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_CFB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_GF128MUL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_OFB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_RMD128 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_RMD256 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_RMD320 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_SALSA20 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_CRYPTO_TGR192 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DAX_DRIVER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DEBUG_BLOCK_EXT_DEVT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DEBUG_CREDENTIALS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DEBUG_TIMEKEEPING | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DECNET | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DMA_REMAP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_DEBUG_SELFTEST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_DP_AUX_CHARDEV | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_DP_CEC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_DUMB_VGA_DAC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_GEM_CMA_HELPER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_KMS_CMA_HELPER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_LEGACY | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_LVDS_ENCODER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_DRM_PANEL_ROCKTECH_JH057N00900 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_EEPROM_LEGACY | 5.4 有 / 6.12 移除或改名 |
| CONFIG_EFI_ARMSTUB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_EFI_VARS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_EMBEDDED | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ENABLE_MUST_CHECK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_EROFS_FS_CLUSTER_PAGE_LIMIT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_FB_CMDLINE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_FORCE_MAX_ZONEORDER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_FRAME_VECTOR | 5.4 有 / 6.12 移除或改名 |
| CONFIG_FRONTSWAP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_FS_VERITY_DEBUG | 5.4 有 / 6.12 移除或改名 |
| CONFIG_GENERIC_MSI_IRQ_DOMAIN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_GPIO_ADP5588 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_GS_FPGABOOT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_GUP_BENCHMARK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HABANA_AI | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HANDLE_DOMAIN_IRQ | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HARDENED_USERCOPY_FALLBACK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HARDENED_USERCOPY_PAGESPAN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HARDEN_EL2_VECTORS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_CONTEXT_TRACKING | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_COPY_THREAD_TLS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_FAST_GUP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_HARDENED_USERCOPY_ALLOCATOR | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_KVM_ARCH_TLB_FLUSH_ALL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_KVM_EVENTFD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_KVM_IRQFD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_MEMORY_PRESENT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_NET_DSA | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HAVE_RCU_TABLE_FREE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HOLES_IN_ZONE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HTC_I2CPLD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_HTC_PASIC3 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_I2C_COMPAT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_I2C_PARPORT_LIGHT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_INPUT_GP2A | 5.4 有 / 6.12 移除或改名 |
| CONFIG_INPUT_MSM_VIBRATOR | 5.4 有 / 6.12 移除或改名 |
| CONFIG_INPUT_POLLDEV | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ION | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ION_CMA_HEAP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ION_SYSTEM_HEAP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_IP_NF_TARGET_CLUSTERIP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_IR_SIR | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KALLSYMS_BASE_RELATIVE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KEYBOARD_MCS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KEYS_COMPAT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KPC2000 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KS7010 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KVM_ARM_HOST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KVM_ARM_PMU | 5.4 有 / 6.12 移除或改名 |
| CONFIG_KVM_INDIRECT_VECTORS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_LEDS_TRIGGER_AUDIO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MANDATORY_FILE_LOCKING | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MEMCG_KMEM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MEMCG_SWAP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MEMCG_SWAP_ENABLED | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MEMORY_HOTPLUG_SPARSE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MFD_CROS_EC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MFD_RK808 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MFD_ROHM_BD70528 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MFD_SMSC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_MFD_TPS80031 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ND_BLK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_ACT_IPT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_CLS_RSVP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_CLS_RSVP6 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_CLS_TCINDEX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_SCH_CBQ | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NET_SCH_DSMARK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_NF_LOG_NETDEV | 5.4 有 / 6.12 移除或改名 |
| CONFIG_OF_NET | 5.4 有 / 6.12 移除或改名 |
| CONFIG_OPTIMIZE_INLINING | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PCIEASPM_DEBUG | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PCIE_BW | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PCI_MSI_IRQ_DOMAIN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PDA_POWER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PHY_CADENCE_DP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PI433 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_POWER_AVS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PREEMPTIRQ_EVENTS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PRINTK_NMI | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PRINTK_SAFE_LOG_BUF_SHIFT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PRISM2_USB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_842_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_COMPRESS_DEFAULT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_DEFLATE_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_DEFLATE_COMPRESS_DEFAULT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_LZ4HC_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_LZ4_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_LZO_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PSTORE_ZSTD_COMPRESS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_PWM_SYSFS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_R8188EU | 5.4 有 / 6.12 移除或改名 |
| CONFIG_R8712U | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RANDOM_TRUST_BOOTLOADER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RAW_DRIVER | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RCU_FAST_NO_HZ | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RCU_PERF_TEST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RTC_DRV_V3020 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_RTL8192U | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SC27XX_EFUSE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SCSI_GDTH | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SECURITY_SELINUX_DISABLE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SERIAL_EARLYCON_ARM_SEMIHOST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SERIAL_IFX6X60 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SIMPLE_PM_BUS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SLAB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SLOB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SLUB_MEMCG_SYSFS_ON | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SND_SOC_SIRF_AUDIO_CODEC | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SND_SOC_ZX_AUD96P22 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SND_VERBOSE_PRINTK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SPLIT_PTLOCK_CPUS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SRCU | 5.4 有 / 6.12 移除或改名 |
| CONFIG_STAGING_BOARD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_STAGING_GASKET_FRAMEWORK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_STRING_SELFTEST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SYSCTL_SYSCALL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SYSFS_DEPRECATED | 5.4 有 / 6.12 移除或改名 |
| CONFIG_SYS_SUPPORTS_HUGETLBFS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TABLET_USB_GTCO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_BITFIELD | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_HASH | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_OVERFLOW | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_STACKINIT | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_STRING_HELPERS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_STRSCPY | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TEST_USER_COPY | 5.4 有 / 6.12 移除或改名 |
| CONFIG_THERMAL_WRITABLE_TRIPS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TOUCHSCREEN_CYTTSP4_CORE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TOUCHSCREEN_MCS5000 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TOUCHSCREEN_PROPERTIES | 5.4 有 / 6.12 移除或改名 |
| CONFIG_TRACE_SINK | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UBSAN_SANITIZE_ALL | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UIO_PRUSS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UNISYSSPAR | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UNIX_SCM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UNUSED_SYMBOLS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_FTDI_ELAN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_NET_RNDIS_WLAN | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_OTG_BLACKLIST_HUB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_OTG_WHITELIST | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_STKWEBCAM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_WUSB_CBAF | 5.4 有 / 6.12 移除或改名 |
| CONFIG_USB_ZR364XX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_UWB | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_AD9389B | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_CADENCE | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_CPIA2 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_M5MOLS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_MT9M032 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_MT9T001 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_NOON010PC30 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_S5K4ECGX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_S5K6AA | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_SH_VEU | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_SMIAPP | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_SR030PC30 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_V4L2 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VIDEO_VS6624 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_VOP_BUS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_WARN_ALL_UNSEEDED_RANDOM | 5.4 有 / 6.12 移除或改名 |
| CONFIG_WIMAX | 5.4 有 / 6.12 移除或改名 |
| CONFIG_WIRELESS_WDS | 5.4 有 / 6.12 移除或改名 |
| CONFIG_WLAN_VENDOR_CISCO | 5.4 有 / 6.12 移除或改名 |
| CONFIG_XZ_DEC_IA64 | 5.4 有 / 6.12 移除或改名 |
| CONFIG_ZX_TDM | 5.4 有 / 6.12 移除或改名 |

## 四、结论

### 4.1 真正无法复刻（C 层，312 项）

1. **驱动类（138 项）**：全部是紫光自研外设驱动，主线无对应实现，且我们没有原厂源码（只有反汇编）。其中与 boot 到 initramfs **强相关**的有：
   - 时钟：`SPRD_UMS9620_CLK`（T770 主时钟）、`SPRD_UMS9230_CLK`、`SPRD_UMS312_CLK`、`SPRD_SC7731E_CLK`、`SPRD_SC9832E_CLK`
   - PMIC/电源：`MFD_UMP96XX_PMIC`、`REGULATOR_UMP9620/9621`、`SPRD_PMIC_SYSCON`、`SPRD_PMIC_SMPL`、`FAST_CHARGER_SC27XX`
   - USB（调试口/ADB）：`USB_DWC3_SPRD(_CORE)`、`SPRD_*_USB2_PHY`/`USB31_PHY`、`SPRD_COMMONPHY`、`USB_SPRD_DMA(_V3)`、`SPRD_USBPINMUX(_QOGIRL6)`、`SPRD_TYPEC_*`
   - 存储：`SCSI_UFS_SPRD_QOGIRL6/QOGIRN6PRO`（UFS，boot 根分区依赖）、`MMC_SPRD_SDHCR11P3`、`MMC_SWCQ`
   - 看门狗/复位：`SPRD_WATCHDOG_FIQ`、`SPRD_WATCHDOG_PMIC`、`SPRD_7SRESET`、`SPRD_EMERGENCY_RESTART`
   - 系统设施：`SPRD_SYSTIMER`、`SPRD_EFUSE`、`SPRD_SOCID`、`SPRD_POWER_DOMAIN`、`SPRD_DMC_DRV`
2. **设施/调试类（101 项）**：`SPRD_HANG_*`（挂死监控）、`SPRD_SYSDUMP/MINI_SYSDUMP`、`SPRD_SIPC*`（Modem IPC）、`TRUSTY*`（TEE，boot 早期若固件要求则关键）、`SPRD_THERMAL_*` 等，依赖原厂固件接口/通信协议，无法仅凭主线复刻。
3. **内核特性（40 项）**：SPF（speculative page fault）、PELT half-life、process reclaim、DM_BOW、INCREMENTAL_FS 等均为 fork 私有补丁，主线无对应，需要重实现或放弃。
4. **编译层（33 项）**：`GKI_HIDDEN_*`/`ANDROID_VENDOR_*` 等是 GKI 构建占位项（可安全忽略）；`PGO_CLANG`/`THINLTO`/`CFI_CLANG_SHADOW`/`SHADOW_CALL_STACK_VMAP` 属工具链支持项，6.12 中大部分已有对应设施。

### 4.2 只需移植（A−C 层，82 项）

KFENCE、CFI_CLANG、LTO/THINLTO、SHADOW_CALL_STACK、WIREGUARD、DMABUF_HEAPS、F2FS/EROFS 压缩、`DRM_SPRD`、`SPRD_IOMMU`、`SPRD_MBOX`、`SPRD_THERMAL`、`SCSI_UFS_SPRD`、`SPRD_*_CLK`（旧 SoC 9863A/UMS512）等——6.12 已全部上游，从 6.12 移植到 5.4 或直接采用 6.12 即可复刻。

### 4.3 关键结论（针对 T508N bring-up）

- 原厂 config 中 **312/4941（6.3%）** 为紫光 fork 特有、标准内核两版皆无；**82 项** 可从 6.12 移植；**239 项** 为 5.4 主线项（非特有）。
- **注意（改名陷阱）**：少数 C 层选项的 config 符号名是 fork 特有的，但**上游驱动代码以改名形式存在**，如 `CHARGER_BQ2560X` → 上游 `CHARGER_BQ256XX`（drivers/power/supply/bq256xx_charger.c）、`SND_I2S_HI3660_I2S` → 上游 `SND_I2S_HI6210_I2S`。此类不能按"符号不存在"一刀切判死，需按驱动功能逐一比对上游源码。
- **T770（UMS9620/QogirN6PRO）激活项**：时钟 `SPRD_UMS9620_CLK`、UFS `SCSI_UFS_SPRD_QOGIRN6PRO`（注意是 QOGIRN6PRO 而非 QOGIRL6，原厂 config 中 QOGIRL6 为 not set）、PMIC `MFD_UMP96XX_PMIC`/`REGULATOR_UMP9620` 均为 `=m` 激活且属 C 层，是 bring-up 必需、无法复刻项。
- **最关键的无法复刻项**：`SPRD_UMS9620_CLK`（时钟）、`MFD_UMP96XX_PMIC`/`REGULATOR_UMP9620`（电源）、`USB_DWC3_SPRD`+`SPRD_QOGIRL6_USB2_PHY`（USB/ADB）、`SCSI_UFS_SPRD_QOGIRN6PRO`（UFS 根分区）、`SPRD_SYSTIMER`（系统定时器）、`SPRD_WATCHDOG_*`（防复位）——这些在 boot 到 initramfs 路径上不可或缺，且无主线替代，**只能靠反汇编逆向或获取原厂源码**。
- 紫光在主线中只上游了**旧 SoC**（9860/9863A/UMS512）的驱动；**T770/UMS9620 世代（Qogirl6）的驱动全部未上游**，属于 C 层。

## 附录：方法

- 原厂选项集：`grep -oE 'CONFIG_[A-Za-z0-9_]+' kernel.config`（含 `# ... is not set`），共 4941 项。
- 5.4.210 树：`git archive v5.4.210` 提取至 /tmp/k54；选项提取 `grep -rhoE '^\s*(config|menuconfig) [A-Za-z0-9_]+' --include=Kconfig*`，去重加 `CONFIG_` 前缀，共 17451 项。
- 6.12.103 树：同法提取，共 20571 项。
- 差集：A = 原厂 − 5.4；B = 原厂 − 6.12；C = 原厂 − (5.4∪6.12)。
- 分类由脚本按选项名前缀/语义规则自动标注，关键项人工核对（如 KFENCE/CFI_CLANG/DRM_SPRD 已确认在 6.12 Kconfig 存在）。
