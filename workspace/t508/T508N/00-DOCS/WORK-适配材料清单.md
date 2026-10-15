# T508N mainline 适配：材料清单与反编译获取任务

> 本文是给"另一个 session"的分工任务书。目标：把 T508N（Unisoc T770/UMS9620）
> 的 mainline 6.12 内核适配到可用状态。重点列明：适配需要什么材料、哪些已就绪、
> 哪些需要从原厂镜像反编译/反汇编获取。

---

## 0. 背景与当前状态

- 设备：TCL T508N (OT5)，Unisoc T770/UMS9620，3GB LPDDR4x，Android 13，原厂内核 5.4.210。
- mainline 源码：`/home/test/t508n/linux-6.12.103/`（6.12.103）。
- 当前卡点：`do_basic_setup → driver_init → memory_dev_init → device_register`，
  间歇性漂移（之前也卡过 of_core_init 的 sysfs、stop_machine）。
- **最新观察**：加了 LSE-atomics 提前 patch 后，进度条回退到 9 格（之前 10 格），
  待排查（见 §6 待办）。

### 关键文件位置速查

| 材料 | 路径 | 状态 |
|---|---|---|
| 正确原厂符号表（116,704 符号） | `work/stock_kallsyms.map` | ✅ 已恢复 |
| kallsyms 恢复脚本 | `work/recover_kallsyms.py` | ✅ |
| 原厂完整 DTS（7731 行） | `work/stock_ums9620.dts` | ✅ |
| 原厂 .config | `work/kernel.config` | ✅ |
| 原厂内核 Image（可反汇编） | `work/stock_Image_full.bin`（38MB）/`stock_Image.bin`（8MB） | ✅ |
| 原厂 DTBO / vendor DTB | `work/dtbo0~6.dtb`、`work/vendor_boot.dtb` | ✅ |
| 固件 | `payloads/sml_a.bin`(TF-A)、`spl_a.bin`、`uboot_a.bin`、`trustos_a.bin`、`teecfg_a.bin` | ✅ |
| stock 分析报告 | `work/REPORT-T508N-stock-analysis.md` | ✅ |

---

## 1. 适配需要的东西（全景）

### 1.1 必须（当前阻塞点）
1. **正确识别并修复根因**：LSE atomics 未 patch → 间歇性卡死（§6）。
2. **打通 USB**（无串口，只能屏幕调试，急需 adb）：
   - SPRD USB PHY 驱动（mainline `drivers/phy/sprd/` 缺失）
   - SPRD DWC3 glue（mainline `drivers/usb/dwc3/dwc3-sprd` 缺失）
   - DTS USB 控制器 + PHY 节点
   - configfs + f_fs/ADB gadget（已有 CONFIG_USB_CONFIGFS=m）

### 1.2 重要（后续）
3. 显示（DSI/DPU）—— mainline 有 `drivers/gpu/drm/sprd/`，需确认 T770 支持。
4. 时钟树（`drivers/clk/sprd/` 已有，需 T770 的 clk 数据）。
5. pinctrl（`drivers/pinctrl/sprd/` 已有，需 T770 的 pin 数据）。
6. 各种外设：mmc/sdhci、i2c、spi、watchdog、ufs（mainline 已有 sprd 版本，需核对寄存器）。

---

## 2. 能从原厂镜像反编译获取的东西（交给另一个 session）

### 2.1 符号表（✅ 已完成，直接用）
- `work/stock_kallsyms.map`，116,704 个符号。
- 反汇编映射：`file_off = vaddr - 0xffffffc010080000`
  ```bash
  aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 \
    --adjust-vma=0xffffffc010080000 \
    --start-address=<addr> --stop-address=<+n> work/stock_Image_full.bin
  ```

### 2.2 原厂 DTS 节点（✅ 已反编译，待提取）
从 `work/stock_ums9620.dts` 提取并移植到 mainline DTS：
- **USB**：usb/dwc3/xhci/ehci + USB PHY 节点（地址、中断、时钟、vbus/regulator）。
- **clk**：T770 的时钟节点（与 `drivers/clk/sprd` 的驱动匹配）。
- **pinctrl**：pin 配置节点。
- 其他按需。

### 2.3 原厂驱动反汇编（🔴 需做，当前 mainline 缺 USB）
用 `stock_kallsyms.map` + objdump 反汇编以下原厂驱动，恢复其寄存器/逻辑，作为移植依据：
- **USB PHY**：搜 `sprd.*phy`、`usb_phy`、`phy_ops` 相关符号。
- **DWC3 glue**：搜 `dwc3`、`dwc3_sprd`、`sprd.*dwc`。
- 其他 mainline 缺失的 sprd 外设驱动。

### 2.4 原厂 .config 差异（✅ 已做 B 节，见报告）
- 已对比 errata 开关，建议开 `CONFIG_ARM64_ERRATUM_2441007=y`。
- 待执行：把 mainline `.config` 对齐原厂关键项（非 errata 部分）。

### 2.5 固件初始化逻辑（可选，🔴 需做）
- `sml_a.bin`（TF-A/BL31）：电源、DVFS、cluster 初始化字符串已扫过（无 CCI/CMN）。
- 如需深入，反汇编 TF-A 看有没有关键的 cluster/SCU 初始化被 mainline 漏掉。

---

## 3. 分工（并行，不是串行！）

> ⚠️ 两个 session **并行推进**，不要按 A→B→C 串行做：
> - **主 session（有设备）**：任务 C 根因修复（在线活，要反复刷机读屏验证）。
> - **另一个 session（无设备）**：任务 A+B（离线活，反汇编原厂 + 写 patch），**立即启动**。
>   USB 移植工作量最大、杠杆最高（打通 adb 后所有调试效率上一个台阶），所以要最早开工，
>   但它是"尽早并行"，不是"先于根因修复"。

### 任务 A：USB 驱动移植（最早启动、并行推进，杠杆最大）
1. 从 `stock_kallsyms.map` 找原厂 USB PHY / DWC3 glue 符号。
2. 反汇编，理清 probe 流程、寄存器、时钟、PHY 初始序列。
3. 对比 mainline 是否有可复用的驱动骨架（如 `drivers/usb/dwc3/dwc3-*-glue`、`drivers/phy/` 下的类似 SoC）。
4. 从 `stock_ums9620.dts` 提取 USB 节点，补进 `ums9620.dtsi` + `ums9620-ot5.dts`。
5. 输出：USB PHY/DWC3 glue 移植 patch + DTS 节点 + 寄存器映射说明。

### 任务 B：DTS 补全（配合 A，与 A 同步做）
1. 从 `stock_ums9620.dts` 提取 USB/clk/pinctrl 等节点。
2. 与 mainline `arch/arm64/boot/dts/sprd/ums9620.dtsi`（253 行骨架）对比，列出缺失。
3. 输出：DTS 补丁 + 缺失清单。

### 任务 C：根因修复（归主 session，另一个 session 只做离线配合）
1. 读 `work/REPORT-T508N-stock-analysis.md`，确认根因结论（LSE atomics）。
2. 离线配合：分析 `t508n_apply_lse_early()` 的 patch 时机/范围问题，给出
   "恢复完整 alternatives patch + 保留 icache no-op"的正确修法建议。
3. 在线验证由主 session 刷机读屏完成。

---

## 4. 已知结论（避免重复劳动）

1. **不要加 CCI/CMN/DSU 初始化**（原厂没有，材料不支持该方向）。
2. **普通 cache 维护只用 `ic ivau`**，不要无条件 `ic ialluis`（T770 上会 hang）。
3. **work/System.map 是错的**（mainline 构建产物），一律用 `stock_kallsyms.map`。
4. **ums9620-ref/ 不是 T508N 内容**，是 UMS9620 参考开发板资料（arch timer patch），仅移植参考。

---

## 5. 工具与环境

- 交叉工具链：`/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin/`
- `aarch64-none-linux-gnu-objdump`、`/usr/bin/dtc`、python3。
- mainline 编译：见 `SKILL-T508N-framebuffer-debug.md` 的编译命令。

---

## 6. 待办 / 待排查

- [ ] **LSE patch 后进度条回退到 9 格**：排查 `t508n_apply_lse_early()` 是否 patch 时机/范围有问题
      （patch 了 LSE 后，某处提前崩/卡）。可能需改成"恢复完整 apply_alternatives_all +
      保留 icache no-op"，或按报告 §可操作步骤逐项验证。
- [ ] 打开 `CONFIG_ARM64_ERRATUM_2441007=y` 并测试。
- [ ] USB 驱动移植（任务 A）。
- [ ] DTS 补全（任务 B）。
