# T508N USB 深挖补遗：SS PHY 完整 init 与 UMS9620 时钟表

## 1. 原厂 `ums9620-clk.ko` 时钟表已解出

模块：`work/vendor_ramdisk_out/lib/modules/ums9620-clk.ko`
配合 `clk-sprd.ko`。以下数组是 `struct clk_hw_onecell_data`，
`num` 在数组首 4 字节，`hws[]` 从数组首 +8 开始。

### 1.1 `ums9620_ipaapb_gate_hws`（DTS phandle 0x4e）

`num=5`，基址 `0x4040`：

| id | 时钟 | gate 寄存器 | enable_mask | sc_offset |
|---|---:|---|---:|---:|
| 0 | `usb_eb`         | 0x04 | BIT(0) | 0x1000 |
| 1 | `usb_suspend_eb` | 0x04 | BIT(1) | 0x1000 |
| 2 | `usb_ref_eb`     | 0x04 | BIT(2) | 0x1000 |
| 3 | `pam_usb_eb`     | 0x04 | BIT(4) | 0x1000 |
| 4 | `pam_wifi_eb`    | 0x04 | BIT(6) | 0x1000 |

对应 stock DWC3 DTS：
```text
clocks = <&ipaapb_gate 0>, <&ipaapb_gate 2>, <&ipaapb_gate 1>, ...
clock-names = "core_clk", "ref_clk", "susp_clk"
=> core_clk = usb_eb, ref_clk = usb_ref_eb, susp_clk = usb_suspend_eb
pamu3: clocks = <&ipaapb_gate 3>; => pam_usb_eb
```

### 1.2 `ums9620_ipadispcglb_gate_hws`（DTS phandle 0x48）

`num=7`，基址 `0x3a78`：

| id | 时钟 | gate 寄存器 | enable_mask | sc_offset |
|---|---:|---|---:|---:|
| 0 | `ipa_dpu1_eb`     | 0x00 | BIT(0)  | 0x1000 |
| 1 | `ipa_dptx_eb`     | 0x00 | BIT(1)  | 0x1000 |
| 2 | `ipa_hdcp_eb`     | 0x00 | BIT(2)  | 0x1000 |
| 3 | `ipa_tca_eb`      | 0x00 | BIT(3)  | 0x1000 |
| 4 | `ipa_usb31pll_eb` | 0x00 | BIT(4)  | 0x1000 |
| 5 | `ipa_ckg_eb`      | 0x00 | BIT(5)  | 0x1000 |
| 6 | `ipa_trng_eb`     | 0x00 | BIT(6)  | 0x1000 |

对应 stock DWC3 DTS 中的
`ipa_dpu1_clk/ipa_dptx_clk/ipa_tca_clk/ipa_usb31pll_clk`
就是本数组 id 0/1/3/4。

### 1.3 `ums9620_ipa_clk_hws`（DTS phandle 0x47）

`num=8`，基址 `0x3e00`：

| id | 时钟 |
|---|---:|
| 0 | `ipa_axi` |
| 1 | `ipa_apb` |
| 2 | `usb_ref` |
| 3 | `ap_ipa_axi` |
| 4 | `ap_ipa_apb` |
| 5 | `ipa_dpu` |
| 6 | `ipa_dpi` |
| 7 | `usb_153m6` |

stock DWC3 的 `ipa_usb_ref = <&ipa_clk 2>` 就是 `usb_ref`。

### 1.4 `ums9620_aon_gate_hws`（DTS phandle 0x10）

`num=112`，基址 `0x5ea0`。USB 相关：

| id | 时钟 | gate 寄存器 | enable_mask | sc_offset |
|---|---:|---|---:|---:|
| 25 | `aon_usb2_top_eb` | 0x04 | BIT(8)  | 0x1000 |
| 26 | `otg_phy_eb`      | 0x04 | BIT(9)  | 0x1000 |

stock MUSB DTS：`clocks = <&aonapb_gate 26>`，即 `otg_phy_eb`。

### 1.5 `ums9620_ipaglb_gate_hws`

`num=4`，基址 `0x3ca8`：

| id | 时钟 |
|---|---:|
| 0 | `ipa_eb` |
| 1 | `tft_eb` |
| 2 | `ipa_access_phycp_en` |
| 3 | `ipa_access_pscp_en` |

---

## 2. `dwc3-sprd.ko` 时钟获取顺序

由 `objdump -dr` 的 ADR 重定位反查 `.rodata`，确认 probe 依次获取：
```text
core_clk
ref_clk
susp_clk
ipa_usb_ref
ipa_usb_ref_source
ipa_usb_ref_default
ipa_dpu1_clk
ipa_dptx_clk
ipa_tca_clk
ipa_usb31pll_clk
```
与 DTS `clock-names` 完全一致（DTS 顺序 `core/ref/susp/ipa_usb_ref/
ipa_usb_ref_source/ipa_dpu1/ipa_dptx/ipa_tca/ipa_usb31pll/
ipa_usb_ref_default`；源码中 `ipa_usb_ref_default` 获取位置稍有不同）。

---

## 3. `sprd_ssphy_init` 完整流程（已校正容器偏移）

私有结构布局：
```text
priv + 0x000: dev/label 等头部
priv + 0x008: struct usb_phy phy   <-- usb_add_phy_dev(priv+8)
priv + 0x180: notifier
priv + 0x198: bc1p2
priv + 0x528: phy_glb_regs 的 ioremap 地址（直接 readl/writel）
priv + 0x536: syscon aon-apb regmap
priv + 0x544: syscon ana-g0l regmap
priv + 0x552: syscon ipa-apb regmap
priv + 0x560: syscon ipadispc1-glb regmap
priv + 0x568: syscon ipa-usb31-dp regmap
priv + 0x576: syscon ipa-usb31-dptx regmap
priv + 0x584: PMIC regmap
priv + 0x592: vdd regulator
```
`init(struct usb_phy *phy)` 中 `x0 = priv+8`，函数做 `x19 = x0-8 = priv`；
`shutdown()` 未减 8，因此 shutdown 的 `[x19,#off]` 等于 `priv+#off+8`。

### init 顺序

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
    dptx   0x010 = hs_host_eye_pattern
}

aon    0xd14 |= BIT(2)
ana    0x004 read; ana 0x004 = (val | 0x5)
aon    0x138 |= BIT(22)               ; 0x400000
aon    0xd14 &= ~BIT(1)
aon    0xd14 |= BIT(2)

; wait ipadispc 0x74 bit2, 500 x 1ms
ipadispc 0x074 read until bit2

ipa_dp 0xc08 read; ipa_dp 0xc08 = val | BIT(0)

; 以下直接读写 phy_glb iomem：
glb +0x2c = (glb+0x10 & 0x3f)
glb +0x10 = (glb+0x10 & ~BIT(4)) | BIT(0) | (priv+640 ? BIT(4):0)
glb +0x20 |= 0x6
glb +0x24 = 0x0003d090
glb +0x08 = 0xffff
glb +0x04 |= 0x3
; 读 aon 0x004，用其 BIT(10) 决定 glb+0x14 插入值 0x17/0x13
glb +0x14 = (glb+0x14 & ~0x1f) | (aon_val & BIT(10) ? 0x17 : 0x13)
glb +0x18 = ((aon_val >> 8) >> 2 插入 bit2..3) ^ BIT(2)
msleep(10)

; wait glb+0x08 bit[1:0] 非 0, 500 x 1ms
glb +0x08 = 0xffff
if (!priv+584) usleep_range(1000,2000)
priv+632 = 1
unlock
```

### shutdown 顺序

```text
dptx   0x01c |= BIT(0)
aon    0x204 &= ~BIT(24)
aon    0x190 |= BIT(4)
aon    0x350 |= 0x3
aon    0x138 &= ~0x1400
aon    0x004 &= ~0x300
aon    0xd14 &= ~BIT(2)
ipa_apb 0x004 read; ipa_apb 0x004 = 0x10; usleep(1,10)
aon    0xd14 |= BIT(1)
ptn38003a_mode_usb32_set(0)
ssphy onoff=false; vdd disable
```

### reset_core 顺序

```text
ana     0x000 |= 0x3
aon     0x010 |= 0x300
ipa_apb 0x004 read; ipa_apb 0x004 = 0x10; usleep(1,10)
aon     0xd14 &= ~BIT(1)
usleep(20000,30000)
ana     0x000 &= ~0x3
aon     0x010 &= ~0x300
ipa_apb 0x004 read; ipa_apb 0x004 = 0x0
```

### usb_config_4_cts 顺序

```text
ipadispc 0x020 = 0x10
ipadispc 0x020 = 0x08
ipadispc 0x008 = 0x21
ipadispc 0x00c read; write(0x00c)
ipadispc 0x008 = 上一步旧值
ipadispc 0x044 = priv+612
ipadispc 0x048 = priv+616
```

## 4. 对移植的直接影响

1. 真实 gate 寄存器极少，完全可以直接写一个新的
   `drivers/clk/sprd/ums9620-clk.c`，先把上表 6 个 USB 相关 gate 和
   `usb_ref` 父时钟注册出来；其他时钟以后补。
2. `ums9620-clk.ko` 里的 `*_hws[]` 已经给出完整 ID 顺序，可按同样方法
   把整个文件重写进 mainline `clk-sprd`。
3. SS PHY 初版 driver 至少要实现 init/shutdown/reset_core 三张表；
   `usb_config_4_cts` 与 USB3 CTS 相关，ADB 可先放后。
4. `sprd_usbm` 依赖先简化为全局 mutex + onoff bit，不实现完整 notifier，
   HS/SS PHY probe 就不会被卡住。
