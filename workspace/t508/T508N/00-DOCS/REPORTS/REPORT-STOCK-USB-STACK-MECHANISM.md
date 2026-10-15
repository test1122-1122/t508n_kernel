# T508N 原厂栈机制全分析 —— 为什么原厂 USB 能用

> 面向"原厂栈的整个机制"的完整回答。证据：原厂内核源码
> `work/vendor-kernel-ums9620/`（sipa_sys_pd.c / sipa_sys_phy_v3.c / dwc3-sprd.c 等）、
> 原厂 DTS `work/stock_ums9620.dts`、LK 反汇编 `work/uboot_analysis/REPORT.md`、
> 固件反汇编 `work/rev/SMC-DISPATCH-USB-REPORT.md` + `sml_full.dis`、
> `t508n-repo/docs/{FACTORY-USB-ENABLE-PATH,USB-IPA-POWER-ROOT-CAUSE,ARCHIVE-20260823}.md`。

---

## 0. 一句话结论

原厂 USB 可用 = **三层栈各司其职，且被签名固件在"启动态"里一次性放行**：

> **LK 引导层保电 → 安全固件层（SML/TrustOS）启动时把 USB/IPa/模拟域寄存器段配置成 NS 可访问 →
> 内核 vendor 栈（sipa-sys genpd → phy → dwc3 → configfs gadget）纯寄存器级直驱。**

全程**没有任何 USB 专用 SMC**——"授权"不是一次运行时调用，而是签名固件启动时的**段级配置状态**。
这正是"原厂能用、我们复刻不了"的全部秘密，也是"没有隐藏 SMC、没有魔法"的证据。

---

## 1. 三层全景

```
┌─ 层1 LK（uboot_a/b, sprdlk @0xb5000000）─────────────────────────────┐
│  early boot: 0xcdd8 → 0x11a3f0 = vddusb33(3.3V) + PHY位 + DWC3 init   │
│  跳内核前: 0x3174 读 [0x642e0034](USB pin-mux)                         │
│    ≠0 (USB在用/下载/充电) → 保电跳内核                                  │
│    =0 (普通启动无USB)   → 0x21d18 关 vddusb33                         │
├─ 层2 安全固件（SML@0xbd000000 EL3 + TrustOS@0xbd040000）───────────────┤
│  SML SMC 面：PSCI(CPU核) / SIP(perf/dbg/storage/只读pwr/dvfs/npu) /    │
│               Trusty(gatekeeper/keymaster/storage/rpmb/gsp/vsp/disp)   │
│               owner52(SPRDSEC) 无 handler —— 无任何 usb/ipa/prot 服务  │
│  TrustOS: tzfw_test.c 按段配置 REG_FW_AON_APB/REG_FW_AP_APB + TZPC     │
│           tzpc_ap/mt；SML trusty_get_reg_base 集中管辖受保护段基址     │
│  ★ 放行 = 启动时把这些段配成 NS 可访问（状态），不是运行时服务（调用）   │
├─ 层3 内核 vendor 栈（5.4.210 + 模块链）────────────────────────────────┤
│  模块链: sipa-sys → sipa-core → sipa-dele → sipa_eth → sipa_usb →     │
│          sprd_usbm → sprd-bc1p2 → phy-sprd-commonphy → phy-sprd-dummy │
│          → ptn38003a-i2c → sprd_tcpm → sc27xx_typec →                 │
│          phy-sprd-usb31-qogirn6pro → dwc3-sprd-core → dwc3-sprd       │
│  USB 三节点 usb3@25100000 / pamu3@25210000 / ssphy@25310000 均挂       │
│  power-domains=<0x44> = sipa-sys 电源域                                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. 层1：LK —— 引导层上电与模式判定（`work/uboot_analysis/REPORT.md`）

- **early boot 上电**：`0xcdd8 → 0x11a3f0`（vddusb33 rail 3.3V → `0x11b55c` PHY/控制器位 →
  `0x116708` DWC3 init）；调用点 0xcdd8(boot早期)/0x18aa8(驱动init)/0x209e8(sprd_preload)/0x1165e0。
- **模式分发**：normal / recovery / fastboot / download / charger（`get_mode_from_*` 链）。
- **跳内核前 USB 电源判定**（公共函数 0x21ae8）：
  ```
  0x21b58  bl 0x3174        ; w0 = ([0x642e0034] != 0)
  0x21b5c  cbz w0, 0x21d18  ; USB 未用 → 关电
  0x21d18  bl 0x11a43c      ; vddusb33 关断 + "usb_pin_mux config is closed!"
  ```
- **原厂语义**：USB 连接（pin-mux 激活）/下载/充电时，LK **保留 USB 域上电**再跳内核；
  普通启动且无 USB 才关。→ 原厂内核拿到的是"已上电"的域（条件1 满足）。

## 3. 层2：安全固件 —— 启动态段级放行（`work/rev/SMC-DISPATCH-USB-REPORT.md`）

**SMC 面已穷尽（SML 完整 1MB 反汇编）**：owner 0=PSCI(仅 CPU 核上/下电)、2=SIP(perf/dbg/storage/
只读 pwr/dvfs/npu)、4=Trusty(服务清单无 usb/ipa/ssphy/dwc3/power-domain/apb-prot 任何一项)、
52=SPRDSEC 无 handler。**不存在可解除 USB/IPa APB-PROT 或完成域上电的 SMC。**

**防火墙机制（TrustOS 内）**：`sprd/firewall/qogirn6pro/src/tzfw_test.c` 按段配置
`REG_FW_AON_APB` / `REG_FW_AP_APB` + `TZPC tzpc_ap/tzpc_mm`；SML 侧 `trusty_get_reg_base`
集中管辖"哪些寄存器区受保护"并只向 Secure World 发放。

**关键理解**：原厂对 USB/IPa 域的"放行"是**启动时把对应段配置为 NS 可访问（或未列入保护段）**——
是**状态**，不是**服务**。所以：
- 运行时没有任何 SMC 可调（放行早已完成，无需调用）；
- 复刻 power_on 寄存器序列无用（墙是段级配置，不是序列）；
- LK 保电补丁无用（墙与电无关）；
- "伪造供应商/移除 power-domains" 无用（墙不认这些）。

## 4. 层3：内核 vendor 栈 —— 纯寄存器级 NS 直驱（源码级）

### 4.1 sipa-sys 电源域（`drivers/net/sprd/sipa_sys/sipa_sys_pd.c` + `sipa_sys_phy_v3.c`）
- `sipa_sys_pd.c` = **generic_pm_domain provider**（compatible `sprd,qogirn6pro-ipa-sys-power-domain`，
  DTS phandle 0x44）；consumer（dwc3/pamu3/ssphy）runtime-PM 请求时 → `sipa_sys_pd_power_on` →
  `sipa_sys_do_power_on_cb_v3()`。
- v3 上电序列（**全是 regmap + clk，无任何 arm_smccc**）：
  ```
  ① clear IPA_SYS_DSLPEN       (ipa-sys-dslpen)
  ② set   IPA_SYS_AUTOSHUTDOWNEN
  ③ poll  IPA_SYS_STATE         (MASK 0x1F00，50us/5s 超时)
  ④ set   IPA_SYS_ACCESSEN      (修正 asic 初值)
  ⑤ clk_prepare_enable(ipa_core_parent / clk_ipa_ckg_eb) + clk_set_parent
  ⑥ ufs_cfg(): USB31PLL_EB(dispc1 REG_APB_EB 0x0) + 模拟 USB31PLL 寄存器
     (anlg syscon: REG_SEL_CFG_0 0x28 / USB31PLL_CTRL0 0x0)
  ```

### 4.2 PHY（`phy-sprd-usb31-qogirn6pro`，`sprd_ssphy_init`）
`ptn38003a_mode_usb32_set(1)`（redriver 切 USB3）→ `regulator_enable(vdd = VDDUSB33, PMIC LDO)` →
`sprd_usbm_ssphy_set_onoff(1)` → 写 aon_apb/ipa_apb EB 组（`0x64900004 |= USB2_TOP_EB|OTG_PHY_EB|ANA_EB`、
`0x25000004 |= USB_EB|USB_REF_EB`…）。

### 4.3 DWC3（`dwc3-sprd-core` + `dwc3-sprd`）
绑 `snps,sprd-dwc3`；reg-names `pamu3_glb_regs`/`dwc3_core_regs`；`usb-phy=<hs ss>`、`usb-pam=<pamu3>`；
glue 用 `devm_usb_get_phy_by_phandle` + `usb_role_switch`（切 `USB_ROLE_DEVICE` 起 gadget）。

### 4.4 功能层
- **configfs gadget g1**：VID `0x1782`(Unisoc) / PID `0x4d00`(组合态 `0x5d06`)；`ffs.adb` + `ffs.fastboot`
  + vser/gser/mtp/rndis 全套（recovery 与 Android 同构，`init.recovery.common.rc`）。
- **typec/PD/BC1P2**：`sc27xx_typec` / `sc27xx_pd` / `sprd_tcpm` / `sprd-bc1p2`；`ptn38003a-i2c` redriver。
- **插入检测**：`extcon-usb-gpio`（vbus-gpio = pmic_eic0，**GPIO 级，无保护**——所以 v15.3 能测到
  `extcon state=1` 而没红屏）。

---

## 5. 数据流：从插线到 adb（原厂）

```
插线 → extcon-usb-gpio 检测 vbus(PMIC EIC, GPIO级, 无保护) → dwc3_sprd 角色切 DEVICE
     → usb3/pamu3/ssphy 的 runtime PM 请求电源域
     → sipa-sys genpd power_on → v3 序列(regmap) → IPa/USB 域上电   ★在签名固件已放行的段上, NS 可写
     → phy init(vdd regulator + EB位 + USB31PLL) → dwc3 core 运行
     → configfs gadget BOUND → 主机枚举 → adb/fastboot/vser…
```

每一笔都是普通世界的寄存器写 + regulator + clk + reset——**没有一步是 SMC**。

---

## 6. 关键洞见（回答"为什么原厂能用、我们不行"）

1. **授权 = 启动态（签名固件段配置），不是运行时调用**。原厂栈跑在"安全固件启动时已放行的段"上，
   所以不需要、也没有 USB SMC；这也是复刻序列/LK 保电/伪造供应商全部无效的原因。
2. **自建内核失败的完整因果**（两层都不满足）：
   - 直接原因：LK 普通启动跳内核前已关 vddusb33（`0x21d18`）——域是断的；
   - 硬墙：签名固件对"我们这套非原厂启动态"（改过的 boot 镜像/内核）没有做段级放行——
     NS 写 USB/IPa/模拟域寄存器即 SError（穷尽实测：复刻 power_on / APB-PROT 记录 /
     移除 power-domains / 全量 SIP+trusty / 伪造供应商 / LK 保电 v1–v5 全红）。
3. **"会话授权制"（ARCHIVE §10）的物理解释**：TZPC/REG_FW 段配置随启动会话而定——TEE 是
   "安全外围总闸"，USB/IPa 属于"出厂时授权给原厂栈、不授权给未知镜像"的那一类。

### 6.5 Magisk 注入为什么"不影响"授权（启动态 ≠ 镜像原厂）

**现象**：本机 Magisk 版官方系统（boot ramdisk 被 magiskinit 替换、vbmeta 第三方重签、
`verifiedbootstate=orange`、unlocked）**adb 实测可用**（本仓库回拉 uboot_log/pstore 全靠它）。
→ 说明"非原厂镜像"与"USB 可用"可以共存，"启动态放行"的门槛不是**镜像是否原封**，
而是**原厂授权链是否完整执行**。

**Magisk 注入的改动面**（`REPORT-T508N-启动流程分析.md` §6/§10）：
- 只改 **boot ramdisk**（`init`→magiskinit、`.backup/init`、`overlay.d/sbin/magisk64.xz`）；
- **不动**：内核 Image、vendor_boot/DTB、vendor 模块链、LK(uboot)、SML/TrustOS（签名固件也改不了）。

**Secure world 视角**：SML/TrustOS 在**内核加载之前**（LK 阶段）就被装载并完成防火墙段配置；
它看到的启动参数（lock=unlocked、vbmeta=orange、boot mode=normal、misc/RPMB 状态）
对 Magisk boot 与出厂解锁机**完全一致** → 段级放行按原厂流程执行。
解锁态下 AVB 对**任何**自定义 boot 一律 SKIP VERIFY——TrustOS 不区分 Magisk boot 和第三方 boot。

**自建 boot 与 Magisk boot 的差别（关键）**：
| 环节 | Magisk boot | 自建 boot |
|---|---|---|
| 内核 Image | 原厂原样 | 被改（core.c "放行" hack / dwc3-only pass） |
| DTB / vendor_boot | 原厂原样 | 改过（曾移除 power-domains=<0x44>） |
| vendor 模块链 | 原厂全套，按原厂顺序 | 曾装全 150 个，但 probe 语义被 hack 改变 |
| 根文件系统 | **完整 vendor Android 用户态**（tee_rpc/rpmbserver/keymint@2.0-unisoc/phasecheckserver/trusty 服务…） | busybox/Alpine initramfs，**无 vendor 用户态** |
| 授权链 | 完整执行 | 被破坏/缺失 |

→ **"非原厂启动态"要精确化**：自建 boot 的问题不是"镜像非原厂"，而是
**破坏了原厂栈的运行前提**（改内核握手语义、改 DTB、缺 vendor 用户态授权流程），
因此拿不到安全世界那次放行；Magisk 把原厂栈原样保留并完整运行，放行照旧。

**诚实边界**：md 未定位"授权流程"具体是哪一步（SML 侧无 USB SMC 已证实；
"运行时经既有 Trusty/vendor 通道的段放行"既未被证实也未被排除）。
最自洽的推论：v15 实验把**内核侧**模块链装全仍卡 SSphy，而 **vendor Android 用户态
（tee_rpc/rpmbserver/keymint 等）从未被任何实验包含**——这恰好是 Magisk boot 有、
自建 boot 没有的部分。下一步可验证：原厂内核原封 + 原厂 DTB + 全模块链 + 完整 vendor
用户态服务，观察 SError 是否消失。

---

## 7. 诚实边界（证实 vs 推断）

| 结论 | 状态 | 证据 |
|---|---|---|
| SML SMC 面无任何 USB/ipa/prot 服务；owner52 无 handler | ✅ 证实 | `sml_a.bin` 完整 1MB 反汇编 + `sprd_sip_svc.c` 源码 |
| TrustOS 存在 REG_FW/TZPC 段防火墙机制 | ✅ 证实 | `trustos_a.bin` 字符串/代码（tzfw_test.c、REG_FW_AON_APB/AP_APB、tzpc_ap/mt） |
| LK 普通启动关 vddusb33、USB 在用保电 | ✅ 证实 | `uboot_analysis/REPORT.md`（0x21ae8/0x3174/0x21d18） |
| vendor 内核 USB 栈 = 纯寄存器级（无 SMC） | ✅ 证实 | `sipa_sys_pd.c`/`sipa_sys_phy_v3.c`/`dwc3-sprd.c` 源码 |
| 我们环境下 NS 写 USB/IPa 寄存器 = SError | ✅ 证实 | 穷尽实测（ARCHIVE §6/§11/§13–15） |
| **原厂启动时安全固件"确实对 USB/IPa 段放行"的具体配置（哪些段、什么条件）** | ⚠️ 推断 | 从"原厂可用"反推；SMC-DISPATCH §D 自己标注了这处张力：*"该防火墙是否恰好把 USB-IPa 寄存器列入保护段仍需实测定性"*——在我们环境里实测 = 受保护 |
| 若要坐实：逆向 `trustos_a.bin` 的 `tzfw_test.c` 段表初始化路径，确认 `sipa-sys(0x44)/ssphy(0x25310000)/dwc3(0x25100000)/anag0(0x53)` 是否被列入/排除出 `REG_FW_AP_APB`/`REG_FW_AON_APB` 段 | 未做 | 只确认墙的存在，不提供绕过 |

---

## 8. 结论

原厂栈的机制**没有魔法**：LK 按模式保电 + 安全固件启动时段的放行 + 内核全寄存器级直驱。
原厂能用的唯一不可复刻项，就是**签名固件在启动时对 USB/IPa 段的那一次放行**——
它随原厂整机（含原厂 boot 状态/会话）出厂绑定，NS 侧无法请求、无法伪造、无法补丁。
这就是"USB gadget 四层验证全关闭、NS 侧无合法路径"的机制级解释。

---

## 9. 行业对比：其他厂家如何控制 USB 相关寄存器

### 9.0 行业基准：安全目标不在"外设控制寄存器"
主流 Android SoC 的安全投入集中在四类，而不是 USB 控制寄存器：
1. **内存域隔离**：TZASC/TZMA/GTZC（内存区段）+ SMMU/IOMMU（DMA 面）；
2. **启动链**：secure boot（efuse/密钥）、BootROM→SPL→TEE 逐级验签；
3. **密钥/存储**：RPMB、Trusted Keys、Keymaster/Keymint TA；
4. **可选外设安全分配**：TZPC/ETZPC/GTZC（把**整个外设**划给 secure 或 NS）。

DWC3/PHY/UTMI 的**控制寄存器**在绝大多数 SoC 上**保持 NS 可写**——这正是
dwc3-qcom / dwc3-mtk / dwc3-exynos / rockchip / allwinner 等 glue 全部能上游化的前提。

### 9.1 ARM 参考形态：TZPC/ETZPC/GTZC —— 显式、可配置、可查
- **STM32MP1 ETZPC**：每个外设（含 USB OTG/dwc2、USBH）可配置为 secure 或 NS 访问（RCC TZEN 位 +
  ETZPC 寄存器），机制**公开、设备树可描述**（`st,stm32-etzpc.yaml`），Linux 侧对 secure 外设
  有感知（驱动禁用/跳过）。USB 默认 NS，[ETZPC 文档](https://wiki.stmicroelectronics.cn/stm32mpu/wiki/ETZPC_internal_peripheral)；
- **STM32U5 GTZC**：官方社区指引"TrustZone 下如何使用 USB 外设"——同样是"把 USB 划给 NS 后正常驱动"
  的**可配置模型**（[ST 社区帖](https://community.st.com/t5/stm32-mcus-security/how-to-use-usb-peripheral-with-trustzone-enabled-on-the/m-p/701456)）。
- **与 Unisoc 的对比**：REG_FW/TZPC 是同族机制的形态，但 Unisoc **没做成可配置/可查/对 Linux 开放**——
  放行逻辑落在签名固件（TrustOS）里、无释放 SMC、NS 侧无法请求。

### 9.2 Qualcomm —— "选择性安全化 + 提供 SCM 出口"（最接近 Unisoc 的厂家）
- 绝大部分 USB 寄存器 NS 可访问（dwc3-qcom + phy-qcom-qmp 全 mainline，骁龙手机/平板广泛可用）；
- 但确有**个别 USB 侧寄存器走 TZ**：`qcom_eud`（Embedded USB Debugger）的 `EUD_MODE_MANAGER2`
  必须经 secure call（SCM）访问——mainline 为此专门加补丁
  [usb: misc: qcom_eud: Access EUD_MODE_MANAGER2 through secure calls](https://lkml.org/lkml/2025/6/28/322)（2025-06）。
- 即 QC 的做法是"寄存器级选择性安全化 + 提供 SCM 调用出口"，不是整段防火墙 + 无出口；
  其余安全化落在 SMMU（DMA）、TZASC（内存）、efuse（启动）。

### 9.3 MediaTek / Samsung Exynos / HiSilicon
同一模式：**控制寄存器 NS + 内存/DMA/启动/密钥安全化**。dwc3-mtk + phy-mtk-tphy、
dwc3-exynos + phy-exynos-usbdrd、hi3660 等全部 mainline 可用；secure world 管 boot/efuse/RPMB，
不管 USB 寄存器。

### 9.4 开放系：Allwinner / Rockchip / Amlogic
连 **BootROM USB 下载模式都开放/文档化**（FEL、RockUSB、Amlogic 下载协议），寄存器全 NS——
与 Unisoc 的下载模式（FDL，同样开放）类似，但内核侧没有防火墙。

### 9.5 同厂家代际对照（工作区证据，最有说服力）
- **Unisoc T618（UMS512）**：DHTB hash-only、无 fused（`UMS9620_PORT.md`），寄存器 NS 可写——
  社区 OC 实验直接用户态/模块写 GPU/PMIC 寄存器成功；自定义 SPL/uboot/内核可刷可 boot，USB 正常；
- **Unisoc T820/UMS9620（本机）**：fused + RSA + REG_FW/TZPC 段防火墙锁 USB/IPa/模拟域 + 无释放 SMC
  → 自建内核无法驱动。
- → "USB 寄存器防火墙"不是 Unisoc 的常态，而是**新一代（qogirn6 系）的封闭性设计选择**。

### 9.6 结论
行业常态是"**控制寄存器 NS 可写 + 内存/DMA/密钥/启动安全化**"；少数例外（QC qcom_eud）
也是"选择性安全化 + 提供 SCM 出口"。Unisoc UMS9620 是**异类**：整段 USB/IPa/模拟域寄存器
TZPC 类保护且不留 NS 出口——从工程上看不是"更安全"的行业标准做法，而是**防第三方刷机/认证
的封闭设计**（T618 时代并没有）。

---

## 10. 复杂度评估：这套"校验"可能真的没那么复杂（张力与更简解释）

> 直说：**"复杂版"（启动态段级放行 + USB 在防火墙内 + 无 SMC = 彻底无解）只证实了一部分，
> 其中"USB 寄存器在墙内"从未被逐字节证明；文档自己前后矛盾过，且存在一个更简单的
> 一致解释（H0：SError 只是"域没上电"），它从未被真正证伪。**

### 10.1 文档自身的张力（已记录，非本报告新发现）
- `SMC-DISPATCH-USB-REPORT.md` §D 原文：*"该防火墙是否恰好把 USB-IPa 寄存器列入保护段
  （vs 只是域没上电导致 SError）仍需实测定性"*；
- `ARCHIVE-20260823.md` line 179：*"普通启动 LK 从未给 USB 域上过电（这就是内核 SError 的
  直接原因，**而非 TEE**）"*——与后来 §14/§15 的"真墙"结论**互相矛盾**（文档自己承认）。

### 10.2 支持"简单版 H0（无防火墙参与，只是没上电）"的证据点
1. **Magisk boot 实证**：原封内核+原封 DTB+原封模块链+完整用户态 = USB 正常——全程不需要任何
   "防火墙放行"，说明**只要原厂栈正常上电，就没有墙**；
2. **v15.3 里 dwc3 core 寄存器（0x25100000）可访问**（UDC 注册成功）——若防火墙真的按段锁
   USB 域，0x25100000 也应 SError；它没有 → **至少 dwc3 core 段不在墙内**；
3. **TrustOS 防火墙的受保护外设命名清单**（`SMC-DISPATCH` §C 枚举的字符串：
   `uart1/spi0..2/i2c0..6/dma/gpio/efuse_sec/ce_sec/tmr_rtc`）**没有 USB/dwc3/ssphy/sipa-sys**；
4. **决定性实验可能从未真正上电**：LK 保电补丁 v1–v5 用的 `0x11a3f0`（= rail + PHY 位 + DWC3
   init）**≠** 内核 `sipa_sys_do_power_on_cb_v3()` 的 IPA_SYS 域状态机序列
   （DSLPEN/AUTOSHUTDOWNEN/**STATE 轮询**/ACCESSEN + ipa_core 时钟 + USB31PLL）——实验可能
   "保了 3.3V rail"但**从未把 IPA_SYS 域从休眠态拉出来**，寄存器仍无时钟/无电 → 访问即 SError，
   被误归因为"防火墙"；
5. `t508n-pdon.c`（复刻 sipa_sys power_on 序列）红屏，是在**被改过的内核 + 改过的 DTB** 环境里
   做的——复刻本身可能不完整（syscon phandle/时钟/顺序/时序）；
6. v15.3 虽然用了原厂 DTB + 全模块链 + 原厂 uboot，但**内核被 hack**（core.c "放行" + dwc3-only
   + ssphy/musb 延期）→ genpd/runtime-PM 上电流程可能根本没执行。

### 10.3 一锤定音的实验（至今没人做过）
在**尽量原封**的环境里，从跑起来的系统手动执行完整 vendor 上电路径，观察是否还 SError：
```
基线 = 原封原厂 boot（或 Magisk 式最小改 initramfs） + 原封 LK/DTB/模块链
1) 先确认 IPA_SYS 域当前状态（读 STATE 寄存器，NS 可读与否本身即信息）；
2) 手动完整执行 sipa_sys_do_power_on_cb_v3 序列（DSLPEN→AUTOSHUTDOWNEN→STATE 轮询
   →ACCESSEN→ipa_core 时钟→USB31PLL/ufs_cfg）；
3) 若成功且 USB 枚举 → H0 成立，墙根本不存在，"自建不可用"只是"从没正确上过电"；
4) 若在确认域已上电（STATE=active）后仍 SError → 才是真墙。
```
> 注：文档的"墙"结论建立在 v1–v5 上，但那组实验没有（也无法）保证 IPA_SYS 域处于 active。
> 在域为 off/sleep 状态下，寄存器访问的 SError 是 ARM 总线行为，**不需要任何防火墙来解释**。

### 10.4 结论（修正复杂度预期）
- **已证实**：LK 普通启动关电；SML 无 USB SMC；TrustOS 有 TZPC/REG_FW 防火墙**机制**（但受保护
  清单无 USB）；我们环境的寄存器访问 SError。
- **未证实（推断）**："USB 寄存器在防火墙内"、"启动态段级放行"、"NS 侧彻底无解"。
- **更可能为真**：SError 主因就是"域从未被正确上电"（LK 关了 + 内核侧上电路径从未被完整执行），
  防火墙（若涉及）顶多是模拟域/anag0 的次级因素——**原厂能用的原因可能简单得多：它正常上电，
  我们从未真正上电**。要否定这个更简单的解释，必须先做 §10.3 的实验。

### 10.5 ★ 终局实证（ARCHIVE-20260823 §19，2026-08-25）—— 简单版被实测证实

> 工作区最新文档已把上述分析**全部坐实**，并给出实测解药：

- **推翻的结论**（§19 原文）：早期"USB 域 = APB-PROT 防火墙，NS 无合法路径"**不成立**！
- **真因链**：IPa/USB 域一直处于 **suspended**（pm_genpd 摘要：`25100000.usb3`/`25220000.sipa` =
  suspended）→ **域未醒 → 访问寄存器 = 总线错误(SError)** —— 普通 ARM 行为，无需防火墙解释。
- **TEE/SMC 面**：确无授权服务（§15 仍成立），但那是"另一层墙"，**非本问题的墙**。
- **实测解药**：
  ```
  echo on > /sys/devices/platform/soc/soc:ipa-apb/25100000.usb3/power/control
  → pm-runtime resume → genpd_power_on
    (sipa_sys_do_power_on_cb_v3: DSLPEN→AUTOSHUTDOWNEN→STATE→ACCESSEN→PLL)
  → 摘要: 25220000.sipa active + 25100000.usb3 active + sipa-sys ON!!
  → 无 SError!!
  ```
- **最终达成（全部实测）**：域 active ✓、ssphy 绑定 `bnd-ssphy: OK` ✓、pamu3 绑定 ✓、
  UDC `25100000.dwc3` ✓、gadget BOUND ✓、extcon state=1 ✓、心跳稳定 ✓；
  **唯一剩余 = dwc3 "not attached"**（USB3 链路训练/数据角色层）。
- **后续可行（未做）**：① 无损 DTB 手术（pyfdt 字节级插 `data-role`，dtb 槽位 156777B 无余量，
  收益递减）；② `sprd_tcpm.c` 在 `typec_register_port` 后 `typec_set_data_role(DEVICE)` 强制
  device 角色；③ 物理 UART。
- **关键值存档**：boot_a=8fea9cb0/kernel d04dc2bc；vendor_boot=原厂（+data-role 尝试未刷）；
  uboot=保电版 9de95ea4；组合=历史最佳。

> **结论更新**：§10.3 的"一锤定音实验"已被后续 session 完成——H0（无防火墙，只是域未上电/未唤醒）
> **成立**。本报告 §7/§9 中基于旧资料（ARCHIVE §15）的"真墙/NS 无合法路径"表述应以此为准修正：
> 墙不在安全固件，而在 **pm_genpd 域状态**；激活路径 = runtime-PM resume → sipa-sys v3 上电序列。
> 剩余问题从"能不能访问"变成"链路/角色能不能枚举"（dwc3 not attached）。
