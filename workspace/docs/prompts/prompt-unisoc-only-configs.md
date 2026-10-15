# 任务：找出紫光（Unisoc/SPRD）特有的、标准内核无法复刻的 config

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210 fork）。原厂 config 在
/home/test/t508n/work/kernel.config。需要找出：**原厂 config 中哪些选项在标准内核
（v5.4.210 和 v6.12.103）的 Kconfig 树里不存在**（= 紫光 fork 特有，标准内核无法直接复刻）。

## 材料
- /home/test/t508n/work/kernel.config      ← 原厂 .config（全部选项）
- /home/test/t508n/work/linux-5.4.210/     ← 标准 5.4.210（git 仓库，可 git archive 提取完整树）
- /home/test/t508n/linux-6.12.103/         ← 标准 6.12.103（完整树）

## 任务

### 1. 提取两边的选项集
- 从 kernel.config 提取所有 `CONFIG_XXX`（去掉 =y/=m/值；注意 `# CONFIG_XXX is not set` 也计入）
- 从标准 5.4.210 提取所有 Kconfig 定义的选项：
  `git -C linux-5.4.210 archive v5.4.210 | tar -x -C /tmp/k54` 后
  `grep -rhoE "^config [A-Z0-9_]+" /tmp/k54 --include=Kconfig* | awk '{print $2}'` +
  `grep -rhoE "config [A-Z0-9_]+" /tmp/k54 --include=Kconfig*`（含菜单项）→ 去重
  （注意有些选项在 Kconfig 用 `config FOO` 定义；menuconfig 项也算；另外有些选项
  可能是 `def_bool`/`tristate` 直接定义）
- 同样提取 6.12.103 的选项集（6.12 已有完整树）

### 2. 差集计算
- **A = kernel.config 选项 − 5.4.210 Kconfig 选项** = 紫光特有（5.4 标准无法复刻）
- **B = kernel.config 选项 − 6.12.103 Kconfig 选项** = 连 6.12 都没有的（彻底特有）
- C = kernel.config 选项 − (5.4 ∪ 6.12) = 完全无法复刻

### 3. 分类清单
对 A/B/C 的每个选项分类：
- **驱动类**（SPRD 外设驱动：USB/PHY/时钟/PMIC/thermal/DMA 等）——理论上可从原厂
  驱动源码移植（但我们没有原厂源码，只有反汇编）
- **编译层**（KFENCE/CFI/LTO/SCS/PAuth 等——标注在哪个主线版本加入）
- **设施/调试类**（hang-monitor/sysdump/ipc/ims 等 Unisoc 设施）
- **与 bring-up 直接相关**（标注：是否影响 boot 到 initramfs 的目标）

### 4. 输出
报告：/home/test/t508n/work/REPORT-UNISOC-ONLY-CONFIGS.md
- 差集清单（A/B/C 三层）
- 分类表（每项：选项、类型、复刻可行性、与 bring-up 的相关性）
- 结论：哪些是"真正无法复刻"（无驱动源码+无主线对应）、哪些只是"需要移植"

## 注意
- 只写 REPORT-UNISOC-ONLY-CONFIGS.md
- 5.4.210 完整树提取到 /tmp/k54（临时，不污染工作区）
- 选项提取要全面（config/menuconfig/def_bool 等）
