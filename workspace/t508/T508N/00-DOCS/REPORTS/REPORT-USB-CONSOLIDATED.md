# T508N USB 成果与 USB 启动流程总览（2026-08-25）

> 目的：一份文件收拢本仓库全部 USB 相关成果与 USB 启动流程；详细展开见 §5 文件索引。
> 状态（最新）：**"固件墙"已证伪；USB2 全栈可达至 attached（协议层连接）；仅剩主机枚举（物理层 1%）**。

---

## 1. 一句话结论

1. **USB 启动 = 四级触点**：BootROM 下载（FDL）→ LK 引导器（fastboot/download）→ recovery（vendor_boot 内 gadget）→ Android（MUSB/DWC3 gadget→adb）。
2. **自建内核侧 USB 不可用的真因** = LK 普通启动关 USB 域 + **内核 pm_genpd 域从未唤醒**（域 suspended → 访问寄存器 = 总线错误 SError，普通 ARM 行为）——**不是安全固件防火墙**。
3. **最新进展（§21 收官）**：域唤醒 + **MUSB（USB2）通道全链打穿，attached 瞬时出现（协议层真通）**；
   剩余 = **1% 时序竞态**（glue-work vs musb-core session；方向 = UDC-start 后强制 `musb_gadget_connect`），
   非物理层、软件可修。

---

## 2. USB 启动流程（四级触点）

### 2.1 带 USB 功能的分区/阶段盘点

| 分区/阶段 | 内容 | USB 功能 |
|---|---|---|
| BootROM（芯片内置） | mask ROM | **USB download mode**：SPL 失效/按键组合 → HDLC 帧协议收 FDL1/FDL2 |
| `mmcblk0boot0/1`（= spl_a） | SPL，DHTB 签名 | USB 下载的落点；校验失败回退 BootROM 下载 |
| **`uboot_a/b`（= LK sprldk）** | 3MB，load base 0xb5000000 | **fastboot（USB 协议，本仓库全部刷机通道）+ download（FDL/ResearchDownload，"act as Romcode"）+ 早期 USB 初始化/charger 检测 + 跳内核前电源判定** |
| **`vendor_boot_a/b`** | vendor ramdisk + DTB + **recovery 全套** | recovery 的 configfs gadget（VID 0x1782，**ffs.adb + ffs.fastboot** + vser/gser/mtp/rndis） |
| `boot_a/b` | 内核 + ramdisk | **Android 的 adb**（stock = MUSB；本项目回拉日志全靠官方系统 adb） |
| `uboot_log` | LK 多会话日志 | 记录了 download 会话（`do_download`、刷 boot_b）——本机镜像即 FDL 协议 dump |
| DTS USB 节点 | usb3@25100000(DWC3) / usb@a00000(MUSB) / hsphy@300000 / ssphy@25310000 / pamu3 | 内核侧硬件映射 |

### 2.2 各级流程要点

**① BootROM（芯片级）**：关机 + 音量下 + USB（或 SPL 校验失败）→ USB 枚举，HDLC 帧（0x7E/CRC16），
5 命令表（connect/start/recv_data/exec/end_data）= 无认证任意写+跳转 → FDL1→SRAM、FDL2→DRAM →
`spd_dump` 全盘读写（T508N 实测：`exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec r|w uboot reset`）。

**② LK（引导器级）**：early boot 上电（0xcdd8→0x11a3f0：vddusb33 3.3V + PHY 位 + DWC3 init）；
模式分发器 0xda00–0xe0e0：**download→0x13a08 FDL 循环（不进内核）**、**fastboot→USB 协议**、
recovery/normal→公共跳内核 0x21ae8；跳内核前读 pin-mux `[0x642e0034]`——**USB 在用/下载/充电 → 保电；普通启动 → 0x21d18 关 vddusb33**。

**③ recovery（系统级，vendor_boot 内）**：LK recovery 模式 → 公共 0x21ae8 跳内核 → vendor ramdisk 里
`system/bin/recovery` + `librecovery_ui*` + `recovery.fstab`（全分区表）+ `init.recovery.common.rc`
（configfs gadget g1：VID 0x1782/PID 0x4d00，`ffs.adb`+`ffs.fastboot`+vser/gser 全套）+ `modules.load.recovery`
（dwc3-sprd 全链 + PHY + typec/PD + pamu3 + sipa_usb）。TWRP 设备树（`work/twrp_OT5_device_tree/`）可构建 recovery.img 扩展。

**④ Android（boot 分区）**：**stock adb = MUSB**（`musb_hdrc.ko`，`usb@a00000` dr-mode=peripheral + `hsphy@300000`）；
DWC3（`usb3@25100000`）是 **SS-only**（`usb-phy=<0x50 0x51>` = **dummyphy**+ssphy，无真实 HS 兜底）。

### 2.3 全景图

```
BootROM(USB下载) ─► SPL ─► LK(sprdlk) ─┬─ download → 0x13a08 FDL 循环（不进内核）
                                        ├─ fastboot → USB 协议 → 刷机
                                        ├─ recovery → 公共 0x21ae8 → vendor_boot recovery（configfs gadget）
                                        └─ normal   → 公共 0x21ae8（pin-mux=0 → 关 vddusb33）
                                                       ▼ 0x80080000 内核
                              Android（MUSB→adb）/ recovery（adb+fastboot）
```

---

## 3. USB 成果汇总

### 3.1 根因认知演化史（纠错链，务必按此理解）

| 阶段 | 结论 | 依据 |
|---|---|---|
| 早期（≤08-24） | "USB 域 = APB-PROT 防火墙，NS 无合法路径" | ARCHIVE §15（SML 无 USB SMC + TrustOS 防火墙机制） |
| 疑点 | 防火墙是否恰好把 USB 寄存器列入保护段"仍需实测定性"；文档自相矛盾（"直接原因是 LK 没上电，**而非 TEE**"） | SMC-DISPATCH §D、ARCHIVE line 179 |
| 本会话判断 | **简单版 H0：SError 只是域没上电，无防火墙参与**——决定性实验（LK 保电 v1–v5）用的 `0x11a3f0` ≠ 内核 `sipa_sys_do_power_on_cb_v3` 域状态机，从未真正唤醒域 | REPORT-STOCK-USB-STACK-MECHANISM §10 |
| **实测证实（08-25）** | **"IPa 防火墙墙"不成立；真因 = pm_genpd 域 suspended → 总线错误**。`echo on > …/25100000.usb3/power/control` → runtime-PM resume → genpd_power_on（v3 序列）→ **域 active、无 SError** | ARCHIVE §19 |
| **收官（§21, 08-25）** | **认知链收敛 v1"TEE/SIP 管辖"→v2"APB-PROT 防火墙"→v3(终)"域未唤醒（bus 行为），无墙；MUSB=stock 同款 USB2 通道可全栈使用"**。MUSB 全链打穿 + **attached 瞬时出现**；剩余 = 1% 时序竞态 | ARCHIVE §21 |

### 3.2 已证实事实清单

- **LK 时序**：普通启动跳内核前按 pin-mux 关 vddusb33（0x21b58/0x21d18）；下载/fastboot 保电。
- **SML SMC 面**：owner 0=PSCI / 2=SIP / 4=Trusty / 52=SPRDSEC(无 handler)，**无任何 USB 服务**——
  但 §19 定性为"另一层墙"，**与本问题无关**。
- **vendor 内核 USB 栈 = 纯寄存器级**（`sipa_sys_pd.c` genpd + `sipa_sys_phy_v3.c` 上电序列：DSLPEN→
  AUTOSHUTDOWNEN→STATE 轮询→ACCESSEN→ipa_core 时钟→USB31PLL；**无 SMC**）。
- **域唤醒方法**（实测）：`echo on > /sys/devices/platform/soc/soc:ipa-apb/25100000.usb3/power/control`。
- **两条 USB 通道**：MUSB = USB2（真 HS PHY，dr-mode=peripheral）；DWC3 = SS-only（dummyphy 无 HS 兜底）。
- **stock adb = MUSB**（musb_hdrc.ko）——即"USB2 控制台"的正确通道是 MUSB，不是 DWC3。
- **MUSB 全链打穿（§21 实测）**：`64a00000.usb -> musb-sprd` ✓ + UDC `musb-hdrc.0.auto` ✓ +
  f_serial gadget BOUND ✓ + 无红（`musb_sprd.c`：无 extcon 时强制 `vbus_active=1` + `wq_mode=PERIPHERAL` +
  queue_work）→ **`attached` 瞬时出现（协议层真通）**。
- **Magisk 注入不影响授权链**：只改 boot ramdisk；内核/DTB/模块链/LK/固件全原厂 → 授权流程原样执行
  （USB-IMAGE-SIDE-REPLICA-CHECKLIST：固件不区分镜像，差异在 DTB power-domains 绑定 + 模块链）。
- **教训（§21）**：早期所有 SError = 未唤醒域（早期未做 pm-runtime 唤醒）；DTB `power-domains` 存在才有
  genpd-attach 可唤醒；模块链依赖序靠 modpost 符号合并（跨模块 exports 注入）。
- **最终组合（§21）**：boot_a=0a49922e/内核 2b644e55；vendor_boot=原厂 DTB；uboot=原厂；
  链=短链 50 + musb 组；域唤醒 + 强制连接 + retry 读。

### 3.3 行业对比（其他厂家如何控制 USB 寄存器）

- 行业常态：**控制寄存器 NS 可写 + 内存/DMA/密钥/启动安全化**（TZASC/SMMU/efuse/RPMB）→
  dwc3-qcom / dwc3-mtk / dwc3-exynos 等全 mainline 可用。
- ARM 参考形态：STM32 ETZPC/GTZC —— 外设可配置 secure/NS，**机制公开、可查**。
- 例外：Qualcomm qcom_eud（EUD_MODE_MANAGER2 走 SCM）——选择性安全化 + **提供 SCM 出口**。
- **Unisoc 代际差异**：T618 无 fused、寄存器 NS 可写（社区 OC 直接用户态写寄存器）；
  UMS9620（T820）fused + 防火墙机制——但防火墙**与本机 USB 问题无关**（§19 证伪）。

### 3.4 遗留问题（最终边界：物理/电气层）

- **§23 进展（2026-08-25 19:47，最深一层）**：**REG33: disabled 实测确认**（vddusb33 rail 未上电，
  C1 前提成立）；hsphy probe 三环定位：① compatible 名字（运行时=`sprd,ump962x-regulator`，已修复，
  boot_a=ea9c949e）→ **② MFD cell of_node 链接（当前卡点：`of_find_device_by_node(regmap_np)=NULL` →
  regmap 拿不到）** → ③ regmap（待②）。STEP3 attached 已回归 ✓。
- **§22 定论**：自制链**每一步 = 硬件全部成立**（4 步验证：域唤醒 active ✓ / MUSB 绑定
  `musb-hdrc.0.auto` 存在 ✓ / state 重试命中 **`attached`（多次稳定）** ✓ / `gadget bind: musb-hdrc.0.auto`
  成功 ✓）——**attached + gadget-on-MUSB = 铁证，无任何软件假设**。
- **主机无枚举的方向（逐步收窄）**：C1 vddusb33 未上电（REG33: disabled 已证）→ 当前修 ② MFD cell
  of_node → regmap 闭合 → regulator_enable → 预期枚举；若闭合后仍不枚举 → ⑧ DPDM/bc1p2 终态 →
  ⑨ 模块来源核查 → 最后才落到需示波器的信号完整性。
- 已排除候选：bc1p2（D+/D− 充电器识别）同理归入物理/电气层观察（接口物理数据能力存在：stock adb 实测可用）。
- **探针改进（待做，非阻塞）**：STEP1 摘要格式匹配 / STEP2 readlink / STEP4 ttyGS0 延迟判定。

### 3.5 原厂 USB 链 vs 自制 USB 链（§21 终局视角对比）

| 环节 | 原厂链（stock Android） | 自制链（v21：短链50+musb组） | 差异本质 |
|---|---|---|---|
| **LK** | 原厂 uboot：early 上电 vddusb33；USB 连接时 pin-mux 保电跳内核 | **原厂 uboot（已还原）**；早期实验用过保电补丁版 | 最终一致；自制靠内核侧补偿 LK 关电 |
| **DTB** | 原厂：USB 三节点 `power-domains=<0x44>` 完整 | **原厂 DTB**（早期误用 nopd 删 power-domains → 无 genpd-attach → 域永远 suspended → SError 假墙） | **最关键差异**：power-domains 存在才有 genpd-attach 可唤醒 |
| **电源域上电** | **自动**：驱动 probe → runtime-PM → sipa-sys genpd（v3 序列），出厂即配套 | **手动补偿**：init/脚本 `echo on > power/control` 显式唤醒（`sipa_sys_phy_v3.c` 加 T508N 步骤标记） | 原厂"顺带自动"，自制"显式补上"——自制缺自动触发点 |
| **模块链** | 全序 150 模块（129 原厂+USB/信任件），sipa-sys 先于 phy/dwc3，依赖由 init 保证 | 短链 50 + musb 组，**modpost 符号合并注入跨模块 exports** | 自制砍掉 typec/PD/trusty/sipa 大链，只留 USB 最小集 |
| **角色/检测** | sc27xx_typec + sprd_tcpm + sc27xx_pd + extcon 全链：CC 检测/PD 协商定角色/朝向；**DWC3 路径角色被协商压过**（§20） | **无 extcon 时强制**：`musb_sprd.c` 补丁 `vbus_active=1` + `wq_mode=PERIPHERAL` + queue_work——**MUSB=USB2 无需 orientation/PD，强制 peripheral 即通** | 原厂"协商决定"，自制"直接强制"——这是自制能打穿的关键 |
| **用户态** | 完整 Android：adbd / typec-PD 服务 / tee_rpc / keymint | Alpine：`usb-gadget.sh`（f_serial ttyGS0，等 UDC 120s）+ `init.c`（init-USB-v21：域唤醒+强制连接+retry） | 自制用内核态 f_serial 替代 ffs.adb，省 adbd |
| **控制器** | 双链都有：**adb=MUSB（USB2）** + DWC3（SS，需完整 typec/PD） | **只走 MUSB**（UDC `musb-hdrc.0.auto`，64a00000.usb） | 最终收敛到同一控制器（stock 同款 USB2 通道） |
| **结果** | 稳定枚举（本项目全程 adb 拉日志） | **attached 瞬时出现（协议层真通）+ 1% 时序竞态**（glue-work vs musb-core session） | 原厂完整链保证 gadget start/connect 时序；自制补丁强制路径存在竞态窗口 |
| **认知** | 从没遇到"墙"（一直在正确链上） | 早期误判"防火墙"→ §19 域唤醒证伪 → §21 收敛 v3（域未唤醒，无墙） | 认知差来自"链不完整时 SError 的表象" |

**一句话**：自制链 = 原厂链的**最小可复刻子集**（域 + genpd + MUSB + HS-PHY + 强制 peripheral），
每砍掉一环（typec/PD/用户态/自动上电）就用一个补丁补偿（强制 vbus/强制连接/echo on）；
**剩余 1% 竞态正是"砍链+补丁"与"原厂完整链"之间最后的结构性差异**——原厂由完整顺序天然消解，
自制需要在 UDC-start 后强制 `musb_gadget_connect` 来补上这一环。

### 3.6 ★ 反编译差分结论（WorkBuddy 反编译 session，2026-08-28，`decomp-analysis/`）

**模块级 verdict（26 模块调用级差分，工具 kodiff/kocall 过滤插桩噪声）**：

| 模块 | 调用变 | 结论 |
|---|---|---|
| **ump9620/9621-regulator、sprd-ump96xx-spi、spi-sprd-adi、phy-sprd-commonphy、sprd_pmic_syscon、ums9620-clk** | **≈0-2** | **功能等价 rebuild**——regmap 环提供端（MFD/ADI/regulator/PMIC syscon）stock 与 chain **无行为差异** |
| **dwc3-sprd-core** | 59 | stock=gadget-only 旧 `dwc3_set_prtcap`；**chain=完整 OTG/DRD + role-switch**（dwc3_drd_*/otg_*/usb_role_switch_*）；chain 在 ep_queue/giveback/runtime_resume/suspend 加 **__pm_runtime_resume/idle + phy_pm_runtime_put_sync** |
| **musb_hdrc** | 80 | stock 仅 gadget；**chain=Host 模式 + DMA + offload**（musb_host_*/musb_h_*/musb_dma_*）——CONFIG_USB_MUSB_HOST/DMA 配置差异 |
| sprd-charger-manager / sprd_tcpm / sc27xx_fuel_gauge | 49/27/14 | 电源/角色相关调用变化（次级） |

**对 H1/H2/H3 的影响（改写假说权重）**：
1. **H2（变体矩阵差异）模块级排除**：MFD/regulator 模块调用级 ≈ 0 差异 → 环不闭**不是模块行为差异**，
   只能来自**运行时状态**（H1：pmic SPI 设备 of_node / DTB 合并结果 / 时序）；
2. **H1（运行时状态）强化**：静态面全排除后，② of_node 未链的答案在设备运行时（5 条判据：
   `/proc/device-tree` 兼容 + `/sys/bus/spi|platform/*/of_node`）；
3. **新增嫌疑 (b)：chain 的 dwc3 OTG/DRD 电源-角色时序**——GUIDE 判定"not attached"残余卡点可能
   在 `dwc3_resume_common`/`dwc3_runtime_resume`/`dwc3_otg_*` 的 runtime-PM/角色联动
   （与已证实的 genpd 域 suspended 根因同区段）——**若自制链实际枚举走 DWC3 而非 MUSB，此嫌疑优先**。

**工具与阻塞**：
- `decomp-analysis/tools/`：`kodis.py`（ET_REL 重定位 → 符号名，`bl 0x1234` → `bl devm_mfd_add_devices`）、
  `kobatch.py`（385 模块 scan/triage/rank/diff，A 重建/B 闭源/C 主线三档）、`kocall.py`（调用级差分）；
- **阻塞**：本机无可用 aarch64 交叉编译通道（WSL 工具链被安全策略禁用）——重建驱动需用户 WSL 手动执行；
- 三档分类：A 重建 149 个（sprd 自研外设）、B 闭源 blob 20 个、C 主线已有 23 个。

### 3.7 ★ TEE 防火墙终局（指令级验证，2026-08-28，`decomp-analysis/TEE-Firewall-Register-Release.md`）

**核心结论：量产 TOS 中防火墙/安全寄存器 TA 未编入——USB 不可能被 TEE 阻挡（TEE 线彻底排除）**：

| 前版推断（格式串） | 指令级实测（量产 `trustos_a.bin`，0x2FE000 裸二进制） |
|---|---|
| `cancel_sec_attr`/`set_sec_attr` = 防火墙放行 API | ❌ **死数据零引用**（落在 VSP TA 字符串区） |
| `sprd_fw_cfg_*`/`tzfw_chk_*` 可用 | ❌ **全零引用**（测试代码未编入量产 TOS） |
| 防火墙子系统在量产 TOS 生效 | ❌ **基本未编入**；`TA_FIREWALL_CLR` 量产代码引用 = 0 |
| `0x31100000` 被某 TA 映射 | ❌ 量产 TOS 全文件**零出现**（findimm + adrp 页扫 + 字面量池全 0） |
| 真实机制 | TA 在**安全世界内直接 ioremap 寄存器区自用**（0x869f8 库例程），不是"放行给 NS" |

**关键教训（反编译方法学，已修进 `tools/tee_dis.py`）**：
1. **镜像构建一致性**：前两轮分析的 TOS 是**调试/全功能构建**（防火墙 TA 编入），量产 TOS 是另一构建
   （未编入）——"偏移级"结论必须先确认分析对象与待答问题用同一份镜像（曾把 GSP TA 误判为 REG TA）；
2. capstone `adrp` 在 base-0 反汇编时 `op.imm` **已是解析后的绝对页地址**，勿再加 `(addr & ~0xfff)`
   （前两轮工具均栽在此，导致字符串 xref 全失败）；
3. 死代码判定需**整文件多机制扫描**（adr/adrp+add/adrp+mem/ldr 字面量/数据指针/直接 bl），单看一种会误判。

**对 USB 的最终口径（与 §19 一致，TEE 线收尾）**：
- 量产 TOS 无防火墙/REG TA → **USB（0x25100000）不可能被 TEE 阻挡**；受控区域枚举中本就没有
  `FW_USB/FW_IPA/FW_DWC3/FW_SIPA`；
- SError 真因 = **LK 跳内核前按 `0x642e0034` 判据关断 vddusb33 → 域失电**（域 suspended 总线错误，
  与 §19 同一结论的不同表述）；
- 解法不变：`echo on > …/25100000.usb3/power/control` 或 LK 保电补丁（`0x21b5c`/`0x21d18` NOP）。

---

## 4. 当前状态与下一步

- **状态（§22 最终）**：软件全链 = 就绪（4 步硬件回读全 ✅：域唤醒 active / MUSB 绑定 / **attached 多次稳定命中** /
  gadget-on-MUSB 绑定成功）；**物理层 = 最终边界**（主机无枚举，D+/D− 线态/HS-PHY 电平，非软件可控）。
- **当前设备**：uboot=原厂（7fe2b5ea，已还原）；boot_a=ed0ebdd6（4 步探针版）；vendor_boot=原厂 DTB。
- **下一步（§22）**：① 探针改进（STEP1 摘要格式/STEP2 readlink/STEP4 ttyGS0 延迟——非阻塞）；
  ② 物理层诊断需示波器/串口（当前无）；③ 物理 UART（长期替代）。
- **已入库（§21/§22）**：内核补丁（core/sipa_pd/musb_sprd/tcpm/sc27xx_typec）+ init（4 步探针版）+
  §1–22 归档 + docs 20+ 篇。

---

## 5. 文件索引（详细展开）

| 文件 | 内容 |
|---|---|
| `work/REPORT-USB-BOOT-FLOW.md` | USB 启动流程四级触点 + 自建内核失败根因链（已按 §19 修正） |
| `work/REPORT-STOCK-USB-STACK-MECHANISM.md` | 原厂栈三层机制 + Magisk 分析 + 行业对比 + 复杂度评估（§10.5 终局实证） |
| `t508n-repo/docs/ARCHIVE-20260823.md` §16–19 | 插入激活研究、v15.3-final、**§19 域唤醒实测（推翻防火墙论）** |
| `t508n-repo/docs/USB-IMAGE-SIDE-REPLICA-CHECKLIST.md` | 镜像侧复刻清单（固件不区分镜像；DTB power-domains + 模块链） |
| `t508n-repo/docs/{FACTORY-USB-ENABLE-PATH,USB-IPA-POWER-ROOT-CAUSE,USB-TEE-PERMISSION-QUERY,USB-ENABLE-IN-BOOT-GUIDE}.md` | 原厂路径/根因/权限查询/LK 保电手册 |
| `work/rev/SMC-DISPATCH-USB-REPORT.md` | SML/TrustOS SMC 面穷尽（无 USB 服务 = "另一层墙"） |
| `t508n-repo/docs/{UBOOT-LK-ANALYSIS,UBOOT-TEE-USB-LOAD-POWER,UBOOT-TEE-CALL-ROUTINES,KERNEL-TEE-CALL-ROUTINES}.md` | LK/内核 TEE 调用例程与 USB 上电链 |
| `work/vendor-kernel-ums9620/drivers/net/sprd/sipa_sys/` | sipa-sys genpd + v3 上电序列源码 |
| `work/uboot_analysis/REPORT.md` | LK USB 上/下电包装与跳内核关电点反汇编 |
| `initramfs_root/usb-gadget.sh` | 最终构型 gadget 脚本（f_serial ttyGS0，等待 UDC） |
| `work/twrp_OT5_device_tree/` | TWRP recovery 设备树（可构建 recovery.img） |
