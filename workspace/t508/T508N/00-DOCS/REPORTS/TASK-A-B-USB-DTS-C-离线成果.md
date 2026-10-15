# T508N 离线任务 A/B/C 成果（2025-08-16）

范围：任务 A USB 驱动移植、任务 B DTS 补全、任务 C LSE early patch 排查。
所有结论基于原厂材料反汇编，不刷机。

---

## 0. 关键新材料：原厂模块未剥离，可直接 nm/objdump

`work/fs/vendor_dlkm_a/lib/modules/` 与 `work/vendor_ramdisk_out/lib/modules/`
里有未剥离 ELF `.ko`。USB/时钟/pinctrl 全部是模块，不必只从 Image 反推。

### USB 模块
| 模块 | 兼容串 | 依赖 |
|---|---|---|
| `dwc3-sprd.ko` | `sprd,qogirn6pro-dwc3` | sc27xx_typec, ptn38003a-i2c, sprd_usbm, phy-sprd-commonphy |
| `dwc3-sprd-core.ko` | `snps,sprd-dwc3` / `synopsys,sprd-dwc3` | 无 |
| `phy-sprd-usb31-qogirn6pro.ko` | `sprd,qogirn6pro-ssphy` | sprd-bc1p2, sprd_usbm, ptn38003a-i2c, phy-sprd-commonphy |
| `phy-sprd-qogirn6pro.ko` | `sprd,qogirn6pro-phy` (HS PHY) | sprd-bc1p2, sprd_usbm, phy-sprd-commonphy |
| `phy-sprd-dummy.ko` | `sprd,usb-dummy-phy` | 无 |
| `phy-sprd-commonphy.ko` | USBPHY_EVENT 框架 | 无 |
| `sprd_usbm.ko` | USBM 状态/onoff/mutex/notifier | 无 |
| `ptn38003a-i2c.ko` | `sprd,ptn38003a_redriver` | 无 |
| `sc27xx_typec.ko` | `sprd,ump96xx-typec` | sprd_tcpm, phy-sprd-commonphy |
| `sc27xx_pd.ko` | PD | sprd_tcpm |

### 时钟/pinctrl 模块（vendor ramdisk）
- `clk-sprd.ko`：SPRD 通用 clk 框架，导出 `sprd_clk_probe/sprd_clk_regmap_init/ops`。
- `ums9620-clk.ko`：UMS9620 全部 PLL/gate/mux 数据，别名覆盖
  `sprd,ums9620-ipaapb-gate`、`sprd,ums9620-ipa-clk`、
  `sprd,ums9620-ipaglb-gate`、`sprd,ums9620-ipadispc-gate` 等。
  内部可见 `ums9620_ipaapb_gate_hws[]`、`usb_eb/usb_ref_eb/usb_suspend_eb/
  pam_usb_eb/ipa_usb31pll_eb/ipa_tca_eb/ipa_dptx_eb/otg_ref...`。
- `pinctrl-sprd.ko`、`pinctrl-sprd-qogirn6pro.ko`、`gpio-sprd.ko`、`gpio-eic-sprd.ko`。
- `i2c-sprd.ko/i2c-sprd-hw.ko`、`sdhci-sprd.ko` 等。

> 结论：USB 移植不需要“盲反汇编”，应直接以这些 `.ko` 的符号/反汇编为蓝本。

---

## 1. 任务 A：USB 驱动移植分析

### 1.1 DWC3 glue：`dwc3-sprd.ko`

符号：
```text
dwc3_sprd_probe                0x2d0
dwc3_sprd_otg_start_peripheral 0x26c4
dwc3_sprd_otg_start_host       0x28dc
dwc3_core_prepare              0x26b4
limit_dwc3_max_speed           0x2b04
dwc3_sprd_vbus_notifier        0x20e8
dwc3_sprd_id_notifier          0x2298
dwc3_sprd_chg_detect_work      0x1f9c
dwc3_sprd_hotplug_sm_work      0x1970
```

`dwc3_sprd_probe` 流程：
1. `devm_kmalloc` priv 0x420。
2. `dma_set_mask(64)` / `dma_set_coherent_mask(64)`。
3. `of_get_next_available_child()` 取 `dwc3@25100000` 子节点。
4. 创建两个 workqueue：`alloc_workqueue(..., WQ_UNBOUND|WQ_FREEZABLE?, 1)`。
5. 依次 `devm_clk_get`：
   `core_clk`、`ref_clk`、`susp_clk`、`ipa_usb_ref`，
   然后继续取 `ipa_usb_ref_source/ipa_dpu1_clk/ipa_dptx_clk/
   ipa_tca_clk/ipa_usb31pll_clk/ipa_usb_ref_default`。
   每路 `clk_prepare/clk_enable`；失败则回滚。
6. `devm_usb_get_phy_by_phandle(dev, "usb-phy", 0)` 取第一个 PHY。
7. 后续取 vbus regulator、extcon（`extcon_get_edev_by_phandle`）、
   `usb-role-switch`、注册 USB/USB HOST notifier，最后创建 platform child。

关键行为：
- `androidboot.mode=charger` 时 `boot_charging=1`，跳过 hotplug_sm_work；
- `dwc3_sprd_otg_start_peripheral`：
  1. 回调子 DWC3 `gadget->ops->udc_start`（通过 CFI 函数指针）；
  2. `msleep(20)`；
  3. `usb_gadget_set_state(gadget, USB_STATE_NOTATTACHED)`；
  4. 写 DWC3 GCTL/GUSB2PHYCFG 位，使能 peripheral；
  5. `usb_role_switch_set_role(role_sw, USB_ROLE_DEVICE)`；
  6. `pm_runtime_idle`。
- `dwc3_sprd_otg_start_host` 对称，切 host 并 `ptn38003a_mode_usb32_set(1)`。

`dwc3_sprd_core` 是 `snps,sprd-dwc3` 子节点驱动，本质是 vendor DWC3 core。
mainline 已有完整 `drivers/usb/dwc3/core.c`；**bring-up 阶段可先用 mainline
`snps,dwc3` + `dwc3-of-simple`，把 vendor core 替换掉。**

### 1.2 HS PHY：`phy-sprd-qogirn6pro.ko`

`compatible = "sprd,qogirn6pro-phy"`。

`probe`：
- priv 0x248；
- 找 PMIC `sprd,ump962x-syscon` 并 `dev_get_regmap`，用于 DPDM 切换；
- 读 `sprd,vdd-voltage`；
- `devm_regulator_get("vdd")` + `regulator_set_voltage`；
- 读 `sprd,hsphy-device-eye-pattern`、`sprd,hsphy-host-eye-pattern`；
- `syscon_regmap_lookup_by_phandle("sprd,syscon-enable")`；
- `syscon_regmap_lookup_by_phandle("sprd,syscon-anag0")`；
- `register_sprd_usbphy_notifier`；
- `usb_add_bc1p2_init`。

`init` 主要 regmap 序列：
```text
regmap(enable)  0x004 &= ~0x300,  |= 0x300
regmap(enable)  0x138 &= ~0x1400, |= 0x1400
regmap(enable)  0x350 &= ~0x3,    |= 0
regmap(anag0)   0x01c &= ~0x1,    |= 0
regmap(enable)  0x190 &= ~0x10,   |= 0
regmap(enable)  0x204 &= ~0x1000000, |= 0x1000000
regmap(anag0)   0x004 &= ~0x10000,   |= 0x10000
regmap(anag0)   0x010 = hsphy_host_eye_pattern(device)
if (!ssphy_on)
    regmap(enable) 0x010 |= 0x300; usleep_range(20000,30000); 0x010 &= ~0x300;
```
并调用 `sprd_usbm_hsphy_set_onoff(1)`。

`shutdown` 反向清上述位，`sprd_usbm_hsphy_set_onoff(0)`。

### 1.3 SS PHY：`phy-sprd-usb31-qogirn6pro.ko`

`compatible = "sprd,qogirn6pro-ssphy"`。

`probe`：
- priv 0x288；
- PMIC regmap：`sprd,ump962x-syscon`；
- `platform_get_resource_byname("phy_glb_regs")` + `devm_ioremap_nocache`，
  这就是 `reg = <0x25310000 0x10000>`；
- 依次 syscon phandle：
  `sprd,syscon-aon-apb`、
  `sprd,syscon-ana-g0l`、
  `sprd,syscon-ipa-apb`、
  `sprd,syscon-ipa-dispc1-glb-apb`、
  `sprd,syscon-ipa-usb31-dp`、
  `sprd,syscon-ipa-usb31-dptx`；
- 读 `sprd,vdd-voltage`，可选 `vdd` regulator；
- 读 `sprd,ssphy-ctl5-eye-pattern` / `sprd,ssphy-ctl6-eye-pattern`
  （默认 0x1e40a000 / 0x5d000083）；
- 读 `sprd,hsphy-device-eye-pattern` / `sprd,hsphy-host-eye-pattern`；
- `register_sprd_usbphy_notifier` + BC1P2。

`init` 核心序列（校正后的容器关系：`usb_phy` 位于 priv+8，
`init` 内 `x19=priv`；`shutdown` 内 `x19=priv+8`）：
```text
ptn38003a_mode_usb32_set(1)
vdd enable
sprd_usbm_ssphy_set_onoff(1)
if (usbm_event_active && !hsphy_on) {
    aon    0x004 |= 0x1300
    aon    0x138 |= 0x1400
    aon    0x350 &= ~0x3
    dptx   0x01c |= BIT(0)
    aon    0x190 |= BIT(4)
    aon    0x204 |= BIT(24)
    dptx   0x004 |= BIT(16)
    dptx   0x010 = hsphy_host_eye_pattern
}
aon    0xd14 |= BIT(2)
ana    0x004 read; ana 0x004 = val | 0x5
aon    0x138 |= BIT(22)
aon    0xd14 &= ~BIT(1)
aon    0xd14 |= BIT(2)
wait ipadispc 0x074 bit2 (500ms)
ipa_dp 0xc08 read; ipa_dp 0xc08 = val | BIT(0)
glb +0x2c / +0x10 / +0x20 / +0x24 / +0x08 / +0x04 / +0x14 直接写
...
sprd_usbm_mutex_unlock
```

完整尾段与 shutdown/reset_core/usb_config_4_cts 见
`TASK-A-deepdive-SSPHY-clock.md`。

### 1.4 移植建议（杠杆最高）

1. **先复刻最小 USB2 链路**：
   - mainline `phy-sprd` 目录无驱动，新增：
     `phy-sprd-commonphy.c`（事件/notifier 简化版）、
     `phy-sprd-usb31-qogirn6pro.c`、`phy-sprd-qogirn6pro.c`、
     `phy-sprd-dummy.c`。
   - 依赖 `sprd_usbm` 可用一个简单平台驱动模拟
     （hsphy/ssphy onoff 与 notifier，初版可直接 true）。
   - BC1P2/typec/PD/PTN38003A 可以先不做，ADB 固定 peripheral 模式不依赖
     charger detection。
2. **DWC3 核心不要从原厂搬**，mainline `dwc3` 比 5.4 vendor core 好；
   glue 可用 `dwc3-of-simple` 或写一个精简 `dwc3-sprd` glue。
   需要保留：`core_clk/ref_clk/susp_clk` enable，`usb-role-switch` 设为 device。
3. **时钟不能绕开**：至少需要 `ipa_usb_ref`、`susp_clk`、`core_clk`。
   mainline `drivers/clk/sprd` 缺 `ums9620-clk` 数据。
   最可行的两条路：
   a. 从 `ums9620-clk.ko` 反汇编/重写 `ums9620_ipaapb_gate_hws[]`
      时钟表，接入 mainline `clk-sprd`；
   b. bring-up 阶段先提供 fixed-clock 占位，确认 DWC3+PHY probe 能走，
      再补真实时钟。
4. 原厂模块 license=GPL v2，未剥离且带符号，反汇编对照移植合法且快。

---

## 2. 任务 B：DTS 补全

### 2.1 mainline 现状

`arch/arm64/boot/dts/sprd/ums9620.dtsi` 只有 CPU/GIC/uart/固定时钟，
**缺 USB、syscon、clock-controller、pinctrl、aon bus、IPA bus**。
`ums9620-ot5.dts` 只启 uart1 + reserved-memory。

### 2.2 原厂 USB 节点（vendor_boot.dtb 基底）

```dts
/* IPA 总线内 */
ipaapb_gate: ipaapb-gate {
    compatible = "sprd,ums9620-ipaapb-gate";
    sprd,syscon = <&ipa_apb_syscon>;
    clocks = <&ext_26m>;
    #clock-cells = <1>;
    #reset-cells = <1>;
};

usb3: usb3@25100000 {
    compatible = "sprd,qogirn6pro-dwc3";
    reg = <0x0 0x25100000 0x0 0x100000>;
    #address-cells = <2>; #size-cells = <2>;
    ranges;
    clocks = <&ipaapb_gate 0>, <&ipaapb_gate 2>, <&ipaapb_gate 1>,
             <&ipa_clk 2>, <&ext_26m 2>, <&ipadispc_gate 0>,
             <&ipadispc_gate 1>, <&ipadispc_gate 3>, <&ipadispc_gate 4>,
             <&ext_32k>;
    clock-names = "core_clk", "ref_clk", "susp_clk",
                  "ipa_usb_ref", "ipa_usb_ref_source",
                  "ipa_dpu1_clk", "ipa_dptx_clk", "ipa_tca_clk",
                  "ipa_usb31pll_clk", "ipa_usb_ref_default";
    usb-phy = <&dummyphy>, <&ssphy>;
    power-domains = <&ipa_pd>;

    dwc3@25100000 {
        compatible = "snps,sprd-dwc3";  /* mainline 可改 snps,dwc3 */
        reg = <0x0 0x25100000 0x0 0x100000>;
        interrupts = <GIC_SPI 142 IRQ_TYPE_LEVEL_HIGH>;
        interrupt-names = "irq";
        usb-phy = <&dummyphy>, <&ssphy>;
        usb-pam = <&pamu3>;
        phy_type = "utmi";
        usb-role-switch;
        linux,sysdev_is_parent;
        snps,dis_u3_susphy_quirk;
        snps,dis_u2_susphy_quirk;
        snps,usb2-gadget-lpm-disable;
    };
};

pamu3: pamu3@25210000 {
    compatible = "sprd,qogirn6pro-pamu3";
    reg = <0x0 0x25210000 0x0 0x10000>,
          <0x0 0x25100000 0x0 0x100000>;
    reg-names = "pamu3_glb_regs", "dwc3_core_regs";
    clocks = <&ipaapb_gate 3>;
    clock-names = "pamu3_clk";
};

ssphy: ssphy@25310000 {
    compatible = "sprd,qogirn6pro-ssphy";
    reg = <0x0 0x25310000 0x0 0x10000>;
    reg-names = "phy_glb_regs";
    #phy-cells = <0>;
    sprd,syscon-aon-apb = <&aon_apb_syscon>;
    sprd,syscon-ana-g0l = <&ana_g0l_syscon>;
    sprd,syscon-ipa-apb = <&ipa_apb_syscon>;
    sprd,syscon-ipa-dispc1-glb-apb = <&ipadispc_glb_syscon>;
    sprd,syscon-ipa-usb31-dp = <&ipa_usb31_dp_syscon>;
    sprd,syscon-ipa-usb31-dptx = <&ipa_usb31_dptx_syscon>;
    sprd,vdd-voltage = <3300000>;
};

dummyphy: dummyphy {
    compatible = "sprd,usb-dummy-phy";
    #phy-cells = <0>;
};

/* AON 总线内 */
hsphy: hsphy@300000 {
    compatible = "sprd,qogirn6pro-phy";
    reg = <0x300000 0x10000>;
    sprd,syscon-enable = <&aon_apb_syscon>;
    sprd,syscon-anag0 = <&ana_g0l_syscon>;
    sprd,vdd-voltage = <3300000>;
    #phy-cells = <0>;
};

usb: usb@a00000 {  /* MUSB, 仅 peripheral/download */
    compatible = "sprd,qogirn6pro-musb";
    reg = <0xa00000 0x2000>;
    interrupts = <GIC_SPI 8 IRQ_TYPE_LEVEL_HIGH>;
    interrupt-names = "mc";
    clocks = <&aon_gate 0x1a>;
    clock-names = "core_clk";
    usb31pllv_frc_on = <&pmu_apb_syscon 0xa58 1>;
    usb-phy = <&hsphy>;
    phy-names = "usb";
    dr-mode = "peripheral";
    multipoint = "true";
    wakeup-source;
};
```

对应 syscon：
```dts
ipa_apb_syscon:    syscon@25000000 compatible="sprd,ums9620-glbregs","syscon"
ipadispc_glb:      syscon@31800000 compatible="sprd,ums9620-glbregs","syscon"
ipa_usb31_dp:      syscon@31810000 compatible="sprd,ums9520-glbregs","syscon"
ipa_usb31_dptx:    syscon@31890000 compatible="sprd,ums9520-glbregs","syscon"
ana_g0l_syscon:    syscon@64300000 compatible="sprd,ums9620-glbregs","syscon"
aon_apb_syscon:    syscon@64900000 compatible="sprd,ums9620-glbregs","syscon"
```

### 2.3 DTBO0（board_id=0x2700，T508N 候选）USB overlay 内容

- `hsphy`: `vdd-supply = <&vddusb33>; extcon = <&extcon_gpio>;`
  `sprd,hsphy-device-eye-pattern = <0x67bd1c0>;`
  `sprd,hsphy-host-eye-pattern = <0x67bd1c0>; status="okay";`
- `usb@a00000`: `extcon = <&extcon_gpio>; status="okay";`
- `ssphy`: `vdd-supply = <&vddusb33>; extcon = <&extcon_gpio>;`
  HS eye 同上；`sprd,ssphy-ctl5-eye-pattern = <0x1e40a000>;`
  `sprd,ssphy-ctl6-eye-pattern = <0x5d000083>; status="okay";`
- `usb3`: `extcon = <&extcon_gpio>; status="okay";`
- `extcon-gpio`: `compatible="linux,extcon-usb-gpio"; vbus-gpio=<&pmic_eic 0 0>;`
- `virtual_typec`: `compatible="sprd,virtual-typec"; extcon=<&extcon_gpio>;`

> T508N board_id 0x2700 对应 dtbo0/dtbo1/dtbo6。三者 USB overlay 基本相同；
> 初版可直接合并 dtbo0 的 USB fragment。

### 2.4 建议落地顺序

1. 先把 syscon + fixed-clock 占位 + DWC3/SSPHY/HSHPY 节点写进
   `ums9620.dtsi`，但 `status="disabled"`；
2. `ums9620-ot5.dts` 里按 dtbo0 内容开启并补 eye-pattern；
3. 端口 `phy-sprd-*` 最小驱动后使能节点；
4. 再补真实 `ums9620-clk` 数据替换 fixed-clock。

---

## 3. 任务 C：LSE early patch 回退/卡死分析

### 3.1 当前实现

- `init/main.c kernel_init_freeable()` 调用 `t508n_apply_lse_early()`；
- 位置在 `smp_prepare_cpus()` 之前，但**IRQ 已经打开、kthreadd/workqueue
  已存在**；
- `t508n_apply_lse_early()` 直接调用 `__apply_alternatives()`，没有
  stop_machine、没有关中断；
- `icache_inval_all_pou()` 当前是 `dsb(ish)` 空操作，没有失效 I-cache。

### 3.2 回退到 9 格的可能原因

1. **中断/调度竞态**：`__apply_alternatives` 会逐个改写 12,074 个原子操作
   patch 点（4 条指令一个点），中断处理函数可能正好执行到半 patch 序列。
2. **I-cache 陈旧**：boot CPU 已经执行过这些函数，patch 后没有
   `ic ivau/ialluis`，指令缓存可能继续跑旧 LL/SC；行为介于“patch 了一半”。
3. **提前 set `system_cpucaps`** 影响后续 `setup_system_capabilities()` 的
   正常检测/日志路径（影响较小，但仍是状态污染）。
4. 先 patch LSE 后，后续某些还没 patch 的同步路径（repeat-TLBI/
   speculative-AT）仍在跑，时序变化让卡点前移，未必是 LSE 本身出错。

### 3.3 修正方向

已生成补丁：`work/patch-0001-lse-early-correct.patch`。

要点：
```text
- 把调用从 kernel_init_freeable() 移到 smp_prepare_boot_cpu() 内
  setup_boot_cpu_features() 之后；
  此时 early_boot_irqs_disabled=true，IRQ 关闭，只有 boot CPU。
- 保留 icache_inval_all_pou()=dsb(ish)；
  但 LSE early patch 后显式 __flush_icache_range(_stext,_etext)+isb；
  等价原厂 dc cvau + ic ivau，不碰 ialluis。
- 不建议在 kernel_init_freeable 阶段只加 local_irq_disable() 硬补，
  因为 kthreadd 已经存在且随时可能被调度；移到更早上下文最干净。
```

### 3.4 后续建议

- 先只验证“early LSE patch 移到 IRQ-off 后，smp_init 后的 9/10 格是否稳定”。
- 稳定后，再恢复完整 `apply_alternatives_all()`，保留 icache no-op；
  若 stop_machine 仍卡，说明还有 repeat-TLBI/speculative-AT 需要提前 patch。
- 不建议长期跳过 system-wide alternatives。

---

## 4. 交付文件

| 文件 | 内容 |
|---|---|
| `work/TASK-A-B-USB-DTS-C-离线成果.md` | 本报告 |
| `work/patch-0001-lse-early-correct.patch` | C 任务修正补丁 |
| `work/stock_kallsyms.map` | 原厂符号表 |
| `work/REPORT-T508N-stock-analysis.md` | 前期根因报告 |
