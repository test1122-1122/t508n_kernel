# mu300-linux 对 T508N 项目的价值评估

评估对象：<https://github.com/dikeckaan/mu300-linux>（已克隆到 `work/mu300-linux/`，6.9 MB，MIT + GPL-2.0）
评估日期：2026-08-27　评估人：DSH session
评估前提：**硬件暂时不在**，本报告为只读源码对照分析，不含实测结论。

---

## 0. 一句话结论

**有显著帮助，且是本项目目前能找到的最强同 SoC 参照。** 它同时覆盖了 T508N 的
两条战线——①当前唯一卡点（USB 主机不枚举 / vddusb33 未上电）②暂停中的 mainline 6.12 线——
并给出了 3 个**立即可验证的具体缺口**（模块加载序倒置、VDDUSB33 regulator provider、USB31 PLL 缺失），
以及一整套 mainline 移植件（`upstream/port/`）。

**但必须注意**：mu300 是 ZTE F50 热点（2 GB RAM），T508N 是 TCL 平板（3 GB RAM），硬件不同、
LK 关电点不同，**不能照搬，需按 §7 甄别**。

---

## 1. mu300-linux 是什么（关键：两条线都全通）

| 维度 | 内容 |
|---|---|
| 目标设备 | ZTE F50 / 硬件 MU300 5G 热点，固件 `MU300_ZYV1.0.0B09`，Android 13 |
| SoC | **Unisoc T760 = UMS9620 = qogirn6pro**，board `ums9620_2h10_feimao` ← **与 T508N 同 SoC** |
| PMIC | **UMP9620 (+ UMP9621)**，charger bq2560x/sgm41513，fuel gauge sc27xx-fgu ← 与 T508N 同 |
| Wi-Fi/BT | SC2355 "Marlin3" on PCIe |
| USB | **DWC3 (`25100000.dwc3`) + MUSB (`musb-hdrc.1.auto`)**，Type-C 在 PMIC 上 |
| 启动链 | Unisoc LK ("sprdlk")、Trusty TEE、**A/B slots**、boot/vendor_boot = Android header v4 / page 4096 / **LZ4 legacy ramdisk** |
| 线①（vendor 5.4） | 自定义 **5.4.254** 内核（ZTE U30 Air GPL 源）+ Ubuntu 26.04 systemd + OpenWrt 25.12 |
| 线②（mainline） | **Linux 6.18.52** 跑到用户态，8 核全起（4×A55 + 4×A76），**USB 3.1 gadget 上跑 SSH/LuCI/br-lan** |

> **对比**：T508N 是 vendor **5.4.210** + mainline **6.12.103**，暂停在"8 核 bringup 之后"。
> mu300 在**同 SoC** 上两条线都推进得更远（尤其 is mainline 外设与 USB），是天然的领先参照。

---

## 2. 为什么与 T508N 强相关

同 SoC 意味着以下**全部同构**，可直接对照而非类比：

- LK 启动契约：A/B slot、`tries_remaining` 语义、DTB 由 **vendor_boot** 提供、`sprd,sc-id` 选表
- Trusty TEE + SIPC（`pm_sys` CM4 协处理器）
- PMIC UMP9620 的 regulator / watchdog / RTC / ADC / fuel gauge 拓扑
- USB 子系统：IPA 域（`sipa-sys`）+ SS PHY + HS PHY + DWC3/MUSB + PAMU3
- 无串口时的日志回读：ramoops / pstore / pmsg（T508N 同样**无 UART**）

mu300 作者还明确写了可复用意图：

> *"Each item lists the symptom, the root cause and the fix, **so it can be reused for other UMS9620 devices** (for example the ZTE U30 Air)."* — `docs/FINDINGS.md` 第 5–6 行

---

## 3. 对当前唯一卡点（USB 主机不枚举）的直接价值 ⭐

T508N 现状（据 `t508n-repo/README.md`、`docs/ARCHIVE-20260823.md` §22–§25、
`work/REPORT-USB-CONSOLIDATED.md`、`work/GUIDE-vddusb33-stock-design.md`）：

- ✅ STEP1–4 全通：域唤醒 → MUSB 绑定（`64a00000.usb → musb-sprd`，UDC `musb-hdrc.0.auto`）→ `attached` → gadget bind
- ❌ **主机枚举失败**：`REG33: disabled`（vddusb33 rail 未上电）；`64300000.hsphy` probe 三环断在
  ② `of_find_device_by_node(regmap_np) = NULL`（`sprd,ump962x-regulator` 无平台设备）→ ③ regmap 不可及
- 定性修正：SError 真因 = **pm_genpd 域 suspended（非 TEE 防火墙）**，`echo on > .../power/control` 后零 SError

下面 4 条按"性价比"排序。

### 3.1 模块加载顺序与 mu300 实战验证的顺序**相反** ⭐⭐（最便宜、可立即验证）

mu300 `docs/FINDINGS.md` §4 的结论原文：

> *"The UDC only appears when the whole chain probes, **in this order**: `extcon-usb-gpio` → PHYs → `sc27xx_adc`
> → `sprd_battery_info` → `sprd-charger-manager` → `sc27xx_fuel_gauge` → `bq2560x-charger`
> (**provides the `otg-vbus` regulator used as `vbus-supply` by DWC3 and MUSB**) → `dwc3-sprd` / `musb_sprd`."*
>
> *"**`sc27xx_adc` must be loaded before the fuel gauge.** The vendor `sc27xx-fgu` driver returns a **hard error
> (not `-EPROBE_DEFER`)** when its IIO channel is missing and **is never probed again**."*

**行号对照（实测数据）：**

| 模块 | mu300 `boot/module-order.txt` | T508N `initramfs/modules.load-final` |
|---|---|---|
| extcon-usb-gpio | 61 | 42 |
| sc27xx_adc | **62** | **90** ❌ |
| sprd_battery_info | **63** | **101** ❌ |
| sprd_tcpm | 64 | 98 |
| sprd-charger-manager | **65** | **106** ❌ |
| sc27xx_fuel_gauge | **66** | **102** ❌ |
| bq2560x-charger（供 `otg-vbus`） | **67** | **103** ❌ |
| phy-sprd-commonphy | 71 | 76 |
| musb_hdrc | 73 | 83 |
| sprd_usbm | 74 | 82 |
| **phy-sprd-qogirn6pro**（即失败的 hsphy） | **75** | **79** ❌ |
| phy-sprd-usb31-qogirn6pro | 76 | 80 |
| sprd-pamu3 | 77 | 81 |
| sc27xx_typec | 78 | 99 |
| dwc3-sprd | 79 | 85 |
| musb_sprd | 80 | 84 |

**T508N 把 PHY/UDC 栈（76–85 行）放在 charger/ADC 链（90–106 行）之前** —— 与 mu300 经验正好倒置。
由于厂商驱动"缺依赖即**硬错误、永不重探**"，`phy-sprd-qogirn6pro` / `musb_sprd` / `dwc3-sprd`
很可能在 `vbus-supply`(otg-vbus) 与 ADC/fuel-gauge 就绪前 probe 失败并永久死亡 ——
这与 T508N 观察到的"hsphy probe 断链 + REG33 disabled"高度吻合。

**⚠️ 另发现一处一致性风险**：T508N 有**两份互相矛盾**的清单——
`initramfs/init.c` 的 `mods[]`（182–233 行，49 项）**完全没有** `musb_hdrc`/`musb_sprd`/
`phy-sprd-qogirn6pro`/`bq2560x-charger`/`sc27xx_adc`/`sprd_battery_info`/`sc27xx_fuel_gauge`；
而 `modules.load-final`（152 项）有全部。**需先确认哪份实际生效**，否则排序修复会打空靶。

> **可立即行动**（无需硬件）：把生效清单重排为 mu300 的 61–80 顺序，并补齐 charger 链全部模块。

### 3.2 `sprd,ump962x-regulator` provider —— 正对 T508N 的 ring② NULL ⭐⭐

T508N 的 PHY 补丁 `t508n-repo/patches/phy-sprd-qogirn6pro-regmap-steps.c:710`：

```c
regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-regulator");
```

mu300 的 `upstream/port/drivers/regulator/ump9620-regulator.c` **正是这个 compatible**（608–609 行）：
`{ .compatible = "sprd,ump962x-regulator" }`。

更关键的是，**该文件里有 T508N 正缺的 VDDUSB33 LDO 完整定义**：

| 定义 | 行 | 值 |
|---|---|---|
| `UMP9620_LDO_VDDUSB33_PD` | 43 | `BASE + 0x1e8`（power-down 寄存器） |
| `UMP9620_LDO_VDDUSB33_PD_MASK` | 96 | `BIT(0)` |
| `UMP9620_LDO_VDDUSB33_VOL` | 144 | `BASE + 0x1ec` |
| `UMP9620_LDO_VDDUSB33_VOL_MASK` | 192 | `GENMASK(7, 0)` |
| `UMP9620_REGU_LINEAR(LDO_VDDUSB33, ...)` | 449–451 | min 1200000, step 10000 |

这解释了 T508N 的 `REG33: disabled` —— **VDDUSB33 rail 的 regulator 描述符/provider 没起来**。
注意 T508N 的 `modules.load` 已加载 `ump9620-regulator.ko`，但 provider **没有产生平台设备**
（ring② `of_find_device_by_node` 返回 NULL），指向 **MFD 父设备 / bus-match 环节**，见 3.3。

### 3.3 MFD `sprd,ump9620` 的接线方式（provider 为何没有平台设备）

mu300 `upstream/port/install.py` 42–60 行给出了**可用的接线法**，即往 `drivers/mfd/sprd-sc27xx-spi.c`
补 UMP9620 的 match data：

```c
static const struct sprd_pmic_data ump9620_data = {
        .irq_base = 0x80,
        .num_irqs = 11,
        .charger_det = SPRD_SC2730_CHG_DET,
};
```
并同时补 `{ .compatible = "sprd,ump9620", ... }`、`{ .name = "ump9620", ... }`，
以及 `REGULATOR_UMP9620` 的 Kconfig/Makefile 挂钩（53–60 行）。

> 这正是"compatible 存在但无平台设备"的典型修法。**建议对照 T508N 生效树检查
> `sprd-ump96xx-spi.ko` 是否把 `ump962x-regulator` 注册为 mfd child cell**。

### 3.4 T508N 的 IPA 上电序列相对厂商源码**缺了 USB31 PLL 一步**

T508N `t508n-repo/patches/t508n-pdon.c` 与 `t508n-repo/docs/USB-IPA-POWER-ROOT-CAUSE.md` §2
列出的"原厂 power_on 序列"为：清 DSLPEN → 置 AUTOSHUTDOWNEN → poll STATE → 置 ACCESSEN → 使能 ipa 时钟。

**但厂商源码 `sipa_sys_phy_v3.c` 的 `ufs_cfg()`（mu300 树内 `upstream/modules/sprd_modem/sipa_sys/sipa_sys_phy/sipa_sys_phy_v3.c` 96–109 行）多出 4 步：**

```c
regmap_update_bits(dispc1_reg, REG_APB_EB, MASK_USB31PLL_EB, MASK_USB31PLL_EB);          // 0x0 bit4
regmap_update_bits(anlg_reg, 0x28, MASK_DBG_SEL_..._USB31PLLV_PD,  ...);                 // anlg REG_SEL_CFG_0 bit8
regmap_update_bits(anlg_reg, 0x0,  MASK_ANALOG_..._USB31PLLV_PD, 0);                     // anlg USB31PLL_CTRL0 bit30
regmap_update_bits(dispc1_reg, REG_APB_EB, MASK_USB31PLL_EB, 0);
```

mu300 把这段**原样移植**进 `upstream/port/drivers/pmdomain/sprd/ums9620-ipa-sys.c`
（`ipa_sys_power_on()` 98–102 行，注释 `/* take the USB31 PLL out of power down */`）。

**T508N 侧核查结果**：
- `t508n-pdon.c`：`USB31PLL|dispc1|anlg` **命中 0 处**
- `phy-sprd-qogirn6pro-regmap-steps.c`：只处理 **USB20**（`REG_ANLG_PHY_G0L_ANALOG_USB20_REG_SEL_CFG_0 0x20`），
  **未触及 USB31 PLL**
- 唯一相关线索在 `patches/musb_sprd-force-vbus.c`（`usb31pllv_frc_on`，对应 stock DTS 第 3100 行
  `usb31pllv_frc_on = <0x04 0xa58 0x01>`），但那是"force on"旁路，非厂商 `ufs_cfg` 正规序列

> **判定**：对 T508N **当前 MUSB(USB2)+vddusb33 卡点**，这条是**次要**的（P1）；
> 但对 T508N **SS/DWC3（USB3.1）路径是必需的**——USB31 PLL 不出 power-down，SS PHY 无法工作。
> 注：DWC3 在 T508N 是 SS-only（dummyphy，无 HS 兜底），所以将来要走 SS 必然需要它。

---

## 4. 对暂停中的 mainline 6.12 线的价值 ⭐⭐⭐

T508N mainline 卡在"8 核之后 + 外设缺失（显示/PMIC/cpufreq）"。mu300 **已把同 SoC 的
mainline 外设做出来了**，且 `upstream/port/` 是**可直接注入 mainline 树的驱动集**：

| mu300 文件 | 行数 | 对 T508N mainline 的意义 |
|---|---|---|
| `upstream/port/install.py` | 146 | **幂等装配脚本**：拷驱动 + 改 Makefile/Kconfig + 改 MFD/eFuse/DWC3/RTC/efuse |
| `port/drivers/clk/sprd/ums9620-clk.c` | 3875 | **UMS9620 时钟驱动**（T508N 6.12 树只有 `ums512-clk.c`，缺 ums9620） |
| `port/drivers/pmdomain/sprd/ums9620-ipa-sys.c` | 190 | **IPA 电源域**（USB3.1/SS PHY 所在的域） |
| `port/drivers/usb/phy/phy-sprd-ums9620-ssphy.c` | 784 | **USB3.1 SS PHY**（T508N 6.12 树 `drivers/usb/phy/` 无任何 sprd 项） |
| `port/drivers/watchdog/ump9620-pmic-wdt.c` | 155 | PMIC 看门狗接管（LK 上电时 armed 300 s） |
| `port/drivers/thermal/sprd_thermal_r5p0.c` | 634 | 19 路片上温区 + eFuse 校准（T508N 6.12 只有老 `sprd_thermal.c`） |
| `port/drivers/regulator/ump9620-regulator.c` | — | **含 VDDUSB33**（见 3.2） |
| `port/drivers/pci/controller/dwc/pcie-sprd.c` | 804 | PCIe RC（Wi-Fi/BT Marlin3，32 MSI 连续块） |
| `port/drivers/cpufreq/sprd-cpufreq-v2-driver.c` | — | ATF DVFS（经 Unisoc SIP SMC） |
| `port/drivers/leds/leds-sc27xx-bltc.c` | — | UMP9620 RGB LED |
| `upstream/patches/0001-spi-sprd-adi-add-UMS9620-restart.patch` | 110 | **UMS9620 重启**：PSCI SYSTEM_RESET 在此固件**永不返回**，改用 PMIC 软复位（`SYS_OFF_PRIO_HIGH`），并接受厂商 compatible `sprd,qogirn6pro-adi` |
| `upstream/wrap-image.py` | 27 | **LK 固定把内核拷到 `0x80080000`**；mainline(≥5.8) text_offset=0 需 2 MiB 对齐 → 前置 branch stub 让 Image 从 `0x80200000` 起跑 |
| `upstream/dts/ums9620-mu300.dts` | 149 | UMS9620 内存/保留区全表（含 `sipc-mem`/`cp-modem`/`tos-mem`/ramoops），及 `sprd,sc-id = "ums9620 1000 1000"` |
| `upstream/mu300-mainline.config` | 314 | mainline config 基线（SPRD clk/IPA PD/SSPHY/DWC3 gadget/thermal/SIP） |
| `upstream/tools/port54.py` | 58 | **5.4 → mainline 机械改写的自动化**（proc_ops、platform remove 返回 void、const bin_attribute） |
| `upstream/modules/wcn_bsp/kinclude/mu300_compat.h` | — | 厂商 out-of-tree 模块跑在新内核的兼容垫片 |
| `upstream/modules/`（wcn_bsp / sprd_wlan_combo / sprdbt_tty / sprd_modem） | 大量 | Wi-Fi/BT/调制解调栈移植到 mainline 的成品 |

**已踩过且给出现成答案的坑（直接省掉重复试错）：**
- `sdhci-sprd`：UMS9620 是 **r11p3** 控制器，厂商 DLL phase = **0x2**（mainline 0x3 会导致 **HS400ES 写全 CRC 错**，读却正常）
- PCIe：`num-vectors` 必须生效，Marlin3 需要**连续 32 个 MSI**
- `of-reserved-mem-skip.patch`：可跳过 `logobuffer`(9 MiB)/`sysdump-uboot`(16 MiB)，MemTotal +26 MiB
- eFuse 必须**只读**（写会永久烧断）
- **自定义最小 DTB 被拒**：经 LK 的 dtbo merge + fixups 后内核 **hang 在 `setup_machine_fdt`** → 实践上沿用 **vendor_boot 里的原厂 DTB**
  （与 T508N「DTB 必须与原厂字节一致、mainline DTB 合并 dtbo 会 panic」的结论**互相印证**）

---

## 5. 可借鉴的工程方法论（无需硬件也能先落地）

| 方法 | mu300 出处 | 对 T508N 的价值 |
|---|---|---|
| **slot b 一次性试验**：只写 `boot_b` + `misc` 的 32 B `bootloader_control`，`boot_a` 永不碰；init 立刻把 slot a 写回 | `boot/flash-trial.sh`、`FINDINGS.md` §1/§17 | T508N **只有 A 槽可用且直接刷 boot_a**，风险高。mu300 的 `tries_remaining=2` 语义（LK 视 `tries==1 && !successful` 为失败）与 LK 回滚规则是**同款 LK 的确切行为**，值得核对 T508N 能否用 B 槽做一次性试验 |
| **掉电也留存的日志**：把阶段表 + dmesg 写到 **boot_b 内 48 MiB 处（8 MiB）**，从 Android 读回 | `boot/init` 29–57 行 | *"pstore only survives a warm reset. Most failures on this device end in a power cut"* —— T508N 同样靠掉电/软重启回读，此法**比 pstore 更抗掉电** |
| **PMIC reset stub 二分定位**：`stub/pmic-reset.c` 做裸机 PMIC 复位当内核入口，`MU300_PROBE_STAGE=N` 指定在某阶段复位，**靠复位周期时长判断是否到达该阶段** | `upstream/README.md` 32–36 行、`debug/install-probe.py` | T508N **无 UART**，这套"用重启周期当探针"的方法能定位 mainline 早期 hang（如 `setup_machine_fdt`），比屏显进度条更早期 |
| **early-recorder**：initramfs 日志循环在 switch_root 结束、journald 要等本地文件系统 → 前几秒掉电无日志；用 service 持续写 dmesg 5 分钟 | `FINDINGS.md` §20 | T508N 已有 150 s 软重启回读，可补"前几秒盲区" |
| **USB CDC-ACM 控制台与 ECM 并存**，且 bind UDC 后**立刻** `ifconfig usb0 up` | `boot/init` 146–167 行；`FINDINGS.md` §12 | host 侧 ECM 只认第一次 CONNECT 通知，晚起会永久 inactive —— T508N 目标正是虚拟串口，此坑必踩 |
| **Android `modem_control` chroot** 的 5 个必需条件（属性区、by-name、假 cmdline、**必须直接 exec**（`sprd_modem_loader` 校验 `current->comm=="modem_control"`）、liblog sink） | `FINDINGS.md` §5 | 若 T508N 将来遇到 PM 协处理器断电，这是**同 SoC 的完整解法**（T508N 的 `sprd_wdt_fiq` 是另一套机制，见 §7） |

---

## 6. 建议的下一步（按优先级）

**P0（无需硬件即可做，硬件回来立刻验证）**
1. **统一并重排模块加载序**：先确认 `init.c mods[]` 与 `modules.load-final` 哪份生效；
   把 charger/ADC/fuel-gauge 链（`extcon-usb-gpio` → `sc27xx_adc` → `sprd_battery_info` → `sprd_tcpm`
   → `sprd-charger-manager` → `sc27xx_fuel_gauge` → `bq2560x-charger`）**整体移到**
   `phy-sprd-commonphy`/`musb_hdrc`/`musb_sprd`/`phy-sprd-qogirn6pro`/`dwc3-sprd` **之前**，
   对齐 mu300 `module-order.txt` 61–80。
2. **对照 `ump9620-regulator.c` 核查 VDDUSB33 provider**：确认 `sprd-ump96xx-spi.ko` 是否把
   `sprd,ump962x-regulator` 注册为 mfd child cell（对照 `install.py` 42–60 的 `sprd,ump9620` match data），
   解释清 `of_find_device_by_node()` 为何返 NULL。

**P1（USB3.1/SS 路径）**
3. 把厂商 `ufs_cfg()` 的 **USB31 PLL 四步**补进 T508N 的电源域上电路径（T508N 当前完全缺失）。

**P2（恢复 mainline 线时）**
4. 直接在 T508N 的 `linux-6.12.103` 树上跑 `upstream/port/install.py`，逐个消解 6.12 vs 6.18 的 API 差异
   （`port54.py` 已把机械改写自动化）；优先 clk → pmdomain → USB SS PHY → watchdog → thermal。
5. 复核 `wrap-image.py` 的 `0x80080000 → 0x80200000` stub（T508N mainline 已能起 8 核，可能已另行解决）。

---

## 7. 不能照搬的部分 / 风险与差异 ⚠️

| 差异 | 说明 |
|---|---|
| **硬件不同** | F50 是 2 GB / 58 GiB eMMC 热点；T508N 是 **3 GB / 29 GB eMMC 平板**。分区表、内存映射、DTS 全不同 |
| **保留内存布局不同** | T508N 有 `iq-mem@aa000000`(64 MiB)、`audio-mem/audiodsp-mem`、`dptx-hdcp`，且 `tos-mem` 起址为 `0xbd040000`（mu300 为 `0xbd060000`）—— **绝不可互换** |
| **断电根因不同** | mu300 的"290 s 断电"是 **PM 协处理器 CM4 watchdog**；T508N 是 **LK 跳内核前关 vddusb33**（`0x21ae8` → pin-mux==0 → `bl 0x11a43c`）。T508N 另有 `sprd_wdt_fiq`(0x641E0000, LOCK@0x20 key 0xe551) 30 s 喂狗。**不要把 mu300 的 modem_control 方案当成 T508N 断电的解** |
| **内核版本错位** | mu300 = 5.4.**254** / 6.18.52；T508N = 5.4.**210** / 6.12.103。驱动移植需再适配 |
| **TEE 结论勿混用** | mu300 **未**讨论 APB-PROT；T508N 已自行**证伪** TEE 防火墙理论。不要拿 mu300 当 TEE 管辖的证据 |
| **`patch_vendor_boot.py` 已知有误** | T508N 自己记录该脚本对 vendor_boot 头解析有误；mu300 的 `upstream/mkvendorboot.py`（65 行，处理 DT table magic `0xd7b7ab1e`）可作**正确实现的参照** |
| **许可** | mu300 脚本/文档 MIT；但 `port/` 驱动与 `modules/` 源自 Unisoc/ZTE vendor 5.4 树 = **GPL-2.0**。并入内核须遵守 GPL；仓库**不含**固件/私有二进制，勿把 mu300 的 device-specific 产物当通用件 |

---

## 8. 证据索引（本次实际读取）

**mu300 侧**
- `work/mu300-linux/README.md`、`docs/FINDINGS.md`（369 行，§1–§30）
- `upstream/README.md`（74 行）、`upstream/install.py`（146 行，行号引用 42–60 / 53–60）
- `upstream/dts/ums9620-mu300.dts`、`upstream/wrap-image.py`、`upstream/mkvendorboot.py`
- `upstream/patches/0001-spi-sprd-adi-add-UMS9620-restart.patch`（110 行）
- `upstream/port/drivers/pmdomain/sprd/ums9620-ipa-sys.c`（190 行，98–102）
- `upstream/port/drivers/regulator/ump9620-regulator.c`（43/96/144/192/240/449–451/608–609）
- `upstream/modules/sprd_modem/sipa_sys/sipa_sys_phy/sipa_sys_phy_v3.c`（96–109 `ufs_cfg()`）
- `boot/init`（284 行，29–57 持久日志 / 146–167 USB gadget）、`boot/module-order.txt`（86 行）
- `boot/flash-trial.sh`（27 行）、`boot/build-boot-image.py`（174 行）

**T508N 侧**
- `t508n-repo/README.md`、`t508n-repo/docs/USB-IPA-POWER-ROOT-CAUSE.md`（87 行）
- `t508n-repo/patches/t508n-pdon.c`（121 行）、`patches/phy-sprd-qogirn6pro-regmap-steps.c`(710)
- `t508n-repo/initramfs/init.c`（182–233 `mods[]`）、`modules.load-final`（152 行）、`modules.load`（63 行）
- `linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620-ot5.dts`（72 行）、6.12 树 sprd 驱动现状
- 子代理只读汇总：`ARCHIVE-20260823.md` §21–§25、`REPORT-USB-CONSOLIDATED.md` §3.1/§3.7、
  `GUIDE-vddusb33-stock-design.md`、`FINAL-BRINGUP-INDEX.md` §5.3、`decomp-analysis/USB-REMAINING-ANALYSIS.md`

---

## 9. 最终判定

| 问题 | 判定 |
|---|---|
| mu300-linux 对 T508N 有帮助吗？ | **有，且价值高**——同 SoC、同启动链、双内核线全通 |
| 对当前唯一卡点（USB 主机不枚举） | **有直接帮助**：3 个具体缺口（加载序倒置 / VDDUSB33 provider / USB31 PLL）+ 1 套无串口方法论 |
| 对暂停的 mainline 6.12 线 | **有直接帮助**：`upstream/port/` + `install.py` 是成品移植件，含 clk/IPA-PD/SSPHY/wdt/thermal/PCIe |
| 能否照搬 | **不能**：硬件/分区/内存映射/断电根因/内核版本均不同，须逐项甄别（§7） |
| 建议 | 立即做 §6 的 P0 两项（纯静态对照，硬件回来即可验证）；mainline 线恢复时优先跑 `install.py` |

> **备注**：本报告为**源码静态对照**，所有"很可能/疑似"均为待验证假设，**未经设备实测**。
> 硬件回来后建议按 §6 的 P0 → P1 顺序做最小验证。

---

## 10. 2026-09-29 补充：启动链 / 内核的可借鉴清单（V50 会话新增）

> 上面 §1–§9 是 2026-08-27 对当时 mu300-linux 的评估。此后 V50 侧做完了
> "官方 MU3351 树 → 内核 → boot 镜像 → OpenWrt rootfs → 一键安装包"整条链（见
> `v50/src/mu300-linux/docs/FROM-SOURCE-TO-OPENWRT.md`），其中**启动链**这部分对 T508N 有新增价值。
> **路径更新**：那时的 `work/mu300-linux/` 克隆已由 `v50/src/mu300-linux`（git，持续维护）取代；
> 官方 GPL 源码树在 `v50/src/zte-oss/mu3351/bsp/kernel5.4/kernel5.4`。

### 10.1 boot 镜像打包（可直接对照 T508N 的 `sprd_mkbootimg.py` 流程）

`v50/src/mu300-linux/boot/build-boot-image.py` 里有 4 条硬约束，T508N 同样适用：

| 约束 | 原因 |
|---|---|
| **复用原厂 boot 头的 header v4 布局与原 AVB footer**，只换 kernel/ramdisk | 设备的校验链假定这个布局；自己造头会被拒 |
| `signature_size = 0` | 让 boot 镜像自身的签名校验失效（配合已解锁/已绕过的设备） |
| ramdisk **必须 LZ4 legacy 帧**（`02214c18` 魔数，8 MiB 一块） | gzip 的 ramdisk 会让 5.4 内核走 `/dev/ram0` 回退路径并 panic |
| 镜像总长 == 原厂分区镜像长度；并断言数据不越过"持久日志区"起点 | 分区大小固定；越界会踩到预留区 |

### 10.2 槽位 / `misc` 引导控制块（T508N 也是 A/B，写法可抄）

* `misc` 偏移 `0x800` 起 32 字节 = `BCAB` 引导控制块：前 4 字节槽后缀、byte12/14 是 slot a/b 的
  `prio|tries|successful`、末 4 字节 CRC32。
* Unisoc LK 把 **`tries==1 && !successful` 视为"已失败"** → 一次性试验必须写 `tries=2`
  （V50 的取值：slot a `0x9f/0x1e`，slot b 试验 `0x9e/0x2f`）。
* V50 的"一次性试验 + 自动回退"完整写法：`boot/flash-trial.sh`（只写 `boot_b` + `misc`）
  与 `boot/init` 里的 `restore_slot_a`／连续 5 次启动失败回退。T508N 可以用同样的显式 BC 写法
  替代"靠 fastboot 猜槽位"的做法。

### 10.3 无 UART 也能拿日志：两种做法互补

| 做法 | 位置 | 特点 |
|---|---|---|
| V50：把 8 MiB 持久日志写进 **`boot_b` 的 48 MiB 处** | `boot/init` 的 `persist()` | 比 pstore 更耐断电；但需要 boot 分区里有空闲区 |
| T508N：`uboot_log` 分区 + `ramoops@fff80000`(pstore) | `t508/firmware/stock-images/uboot_log.img`、`ums9620-ot5.dts` | LK 与内核两段都有；容量小、掉电即失 |

**建议组合**：LCD 探针回答"执行到哪一步"，持久日志回答"为什么停"。
顺带一个可交叉验证的点：**V50 官方平台 DTSI 里 pstore 也是 `ramoops@fff80000`（256 KiB）**，
与 T508N 的地址一致 —— 可作为"该地址是平台约定、不是机型巧合"的旁证。

### 10.4 DTS reserved-memory 对照（直接关系 T508N 的 NOMAP 修复）

| | T508N（自己改的 6.12 DTS） | V50/MU3351（官方 5.4 平台 DTSI） |
|---|---|---|
| 文件 | `t508/src/linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620-ot5.dts` | `v50/src/zte-oss/mu3351/bsp/kernel5.4/kernel5.4/arch/arm64/boot/dts/sprd/ums9620-mach.dtsi` |
| `sipc-mem@87800000`(8 MiB) | `no-map` | **无 `no-map`** |
| `cp-modem@88000000`(0x1d000000 = 464 MiB) | `no-map` | **无 `no-map`** |
| `iq-mem@aa000000`、`ch-ddr@ae600000` | `no-map` | **无 `no-map`** |
| 该文件里唯一的 `no-map` | — | `rebootescrow@fff70000`（64 KiB） |

**怎么用**：T508N `set_zone_contiguous` 穿洞问题的根源是自己把 modem/sipc 区域标了 `no-map`；
官方平台 DTSI 给出的是"厂商本来就按普通 reserved-memory（仍在线性映射里）处理"的写法。
两条路都可走，但结论不同：

1. **保留 `no-map` + 修 `pfn_valid`/`__pageblock_pfn_to_page`**（T508N 现在这条）——
   更安全（内核不会碰 modem/安全世界内存），也正是 `mm/page_alloc.c` 那处改动的意义；
2. 放宽 `no-map`（向厂商 DTSI 看齐）——**不建议**：等于允许内核把 modem 区域当普通内存使用。

无论选哪条，都应把"该区域在厂商侧是否 no-map"作为**证据**记下来，而不是靠猜。

### 10.5 stock 5.4 实现作为 6.12 复刻的参照（多一棵树）

V50 官方树是 **5.4.254**，比 T508N 手上的 stock **5.4.210** 新一档，但同属紫光 UMS9620 BSP：

```
v50/src/zte-oss/mu3351/bsp/kernel5.4/kernel5.4/
├── arch/arm64/kernel/alternative.c      # apply_alternatives / multi_stop 协议
├── arch/arm64/kernel/cpufeature.c       # cpu_hwcap_keys / LSE 能力
├── arch/arm64/kernel/smp.c              # 二级核上线、smp_cpus_done
├── arch/arm64/kernel/smp_spin_table.c   # 二级核取指等待
└── arch/arm64/include/asm/lse.h         # LSE 早补丁/static_key
```

**用法**：把 stock 5.4.210 与这棵 5.4.254 逐文件对比 ——
**差异 = 上游 5.4.210→254 的演进**，**共同点 = 紫光魔改里稳定的部分**。
T508N 在 6.12 上复刻时应只保留"共同点里语义必需"的那些（FINAL-BRINGUP-INDEX §3 已按此方向做过一轮，
现在多一棵独立树可交叉验证，避免把上游行为误当成 OEM 定制）。

### 10.6 模块顺序：文件驱动 + 单一来源

`boot/module-order.txt`（86 项）是 V50 initramfs 的**唯一**加载顺序来源，`boot/build-boot-image.py`
按它打包、`boot/init` 按它 `insmod`。与本文 §3.1 的"加载序倒置"结论配合：
T508N 的 `initramfs/init.c mods[]`、`modules.load-final` 可以收敛成同样一个文件，
避免"内核树里的顺序"和"initramfs 里的顺序"两处漂移。

### 10.7 工程制度（照搬成本最低）

* `docs/FINDINGS.md`：**每个 workaround 都写成因**（gadget 顺序、PM watchdog、AT 通道陷阱…），
  新人不需要读会话日志；
* `docs/INDEX.md` 末尾的"**已验证 vs 待真机**"两列表：一句话说清哪些结论有证据、哪些只是推断；
* 三处写入 + 逐步校验 + 可回退的纪律（`install.sh` 的顺序：文件系统成功 → `boot_b` 校验 → 才写 `misc`）。

### 10.8 反向：V50 ← T508N

* T508N 的 6.12 修复对 V50 的 `upstream/`（mainline 6.18 移植）直接有用：
  LSE static_key 方案、NOMAP `pfn_valid` 检查、`apply_alternatives` multi_stop 同步协议、
  `NR_CPUS/NUMA/SCHED_SMT` 对齐、ramoops 诊断通道；
* T508N 的 **LCD framebuffer 探针法**（无串口调试）在带屏的 UMS9620 机型上可复用；
* 方法论（一次一个变量、探针坐标即证据）同样值得 V50 的 `upstream/` 线借用。

### 10.9 仍按 §7 甄别（本次补充未推翻的差异）

内核版本错位（5.4.**210** vs 5.4.**254**）、保留内存布局与起址不同（如 `tos-mem` 各机型不一致）、
断电根因不同（LK 关 vddusb33 vs PM 协处理器 watchdog）、TEE 结论不可混用 ——
**以上清单是"对照与借鉴"，不是"照搬"**。
