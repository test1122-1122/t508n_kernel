# T508N（UMS9620 / T770）USB 启动流程分析

> 范围：仅从**带 USB 功能的分区/阶段**分析 USB 启动流程——BootROM 下载、LK(uboot 分区)、
> vendor_boot(recovery)、boot(Android gadget) 以及 DTS USB 节点。
> 按用户要求：**忽略所有 USB 失败/不可用路线**（mainline 移植未完成、TEE/IPa 域墙、
> SError 红屏、"USB gadget 彻底关闭" 终局、LK 保电补丁 v1–v5 等，见 §6）。
> 依据：工作区全部 .md（`REPORT-T508N-启动流程分析.md`、`UBOOT-LK-ANALYSIS.md`、
> `work/uboot_analysis/REPORT.md`、`UBOOT-TEE-USB-LOAD-POWER.md`、`USB-ENABLE-IN-BOOT-GUIDE.md`、
> `TASK-A-B-USB-DTS-C-离线成果.md`、`work/srag/UnisocBypass-main/docs/*`、
> `t508n-repo/docs/ARCHIVE-20260823.md` 等）+ vendor ramdisk 实况核对。

---

## 1. 结论先行（一句话）

T508N 的 USB 启动不是单一流程，而是**四级 USB 触点串在启动链上**：
芯片级 **BootROM USB 下载（FDL）** → 引导器级 **LK 的 download/fastboot（USB 协议）** →
系统级 **recovery（vendor_boot 内，configfs gadget 同时暴露 adb+fastboot）** →
**Android（stock 内核模块化 USB 栈 → adb）**；
其中 LK 在跳内核前用 USB pin-mux 寄存器决定**关不关 USB 电源域**，是"USB 在哪个阶段活着"的总开关。

---

## 2. 带 USB 功能的分区/阶段清单

| 分区/阶段 | 内容 | USB 功能 |
|---|---|---|
| BootROM（芯片内置，无分区） | mask ROM，0x100000–0x111FFF | **USB download mode**：SPL 失效/按键组合 → HDLC 帧协议收 FDL1/FDL2（见 §3.1） |
| `mmcblk0boot0/1`（= spl_a 冗余副本） | SPL，DHTB 签名 | 本身无 USB 代码；是 USB 下载流程的目标（FDL 写 splloader 的落点）；校验失败 → 回退 BootROM 下载 |
| `spl_a/b`（4MB） | SPL 引导器 | 同上；对 uboot 做 SIMGHDR RSA 校验（fused 设备），决定 uboot 能否被替换 |
| **`uboot_a/b`（3MB，内容=紫光定制 LK sprldk）** | LK 引导器，DHTB 签名，load base 0xb5000000 | **USB 功能最密集**：早期 USB 初始化/charger 检测、`download mode!`(FDL/ResearchDownload 循环)、fastboot(USB 协议)、跳内核前 USB 电源判定（见 §3.2） |
| **`vendor_boot_a/b`（100MB）** | vendor ramdisk + 内核 DTB + **recovery 全套** | **recovery 的 USB**：`system/bin/recovery` + `librecovery_ui*.so` + `recovery.fstab` + `init.recovery.common.rc`（configfs gadget，**adb + fastboot 双协议**，见 §3.3） |
| `boot_a/b`（64MB） | 内核(5.4.210 raw Image) + ramdisk | **Android 的 USB**：stock 内核 USB 栈全为外置模块（dwc3-sprd/musb/phy-sprd 等，在 vendor ramdisk/vendor_dlkm）→ adb（本项目无串口闭环的"回官方拉日志"通道即靠它，Magisk root） |
| `misc/miscdata` | bootctrl(A/B) + 启动标志 | 决定 normal/recovery/charger 等模式选择（模式探测链的一环） |
| `uboot_log`（16MB） | LK 多会话日志 | 记录了 download 会话证据："reply exec act as Romcode"、`do_download`、刷 boot_b 后重启 → **本机镜像本身就是 FDL 协议 dump 的** |
| DTS USB 节点（供内核侧） | `usb3@25100000`(DWC3)、`usb@a00000`(MUSB 仅 peripheral/download)、`hsphy@300000`、`ssphy@25310000`、`pamu3@25210000`、extcon-gpio | 内核侧 USB 的硬件映射：fastboot/download/recovery/adb 的实际控制器与 PHY（见 §3.4） |

---

## 3. USB 启动流程逐级分析

### 3.1 阶段 0 —— 芯片级：BootROM USB download（最早的 USB 启动）

- **触发**：关机态 音量下+插 USB（BootROM 读 AON_APB boot strap，recovery 请求位）；或 eMMC 里没有合法 SPL 时自动回退。
- **协议**（平台家族参考，T618 BootROM 反汇编 `analysis/bootrom/DISASSEMBLY.md`）：
  - 传输 = HDLC 字节流（帧头 0x7E、0x7D 转义、CRC16 校验）；
  - 5 命令表：`connect` / `cmd_start`(任意地址+长度) / `cmd_recv_data`(写内存) / `cmd_exec`(跳到写入地址) / `cmd_end_data` → 本质是**无认证的任意写 + 跳转原语**；
  - `send_status` 状态位经 SRAM 0x5020 回给 host。
- **FDL 链**（`fdl/README.md`、`SIMGHDR_FORMAT.md`）：
  - FDL1 装入 **SRAM 0x5500**（T508N 实测参数 `fdl fdl1-dl.bin 0x65000800`）执行 → 初始化 DDR；
  - FDL2 装入 **DRAM 0x9efffe00**（T508N 实测 `fdl fdl2-dl.bin 0xb4fffe00`）→ 全盘读写协议处理（spd_dump/ResearchDownload）；
  - 签名：FDL1 属 Key A 组、FDL2 属 Key B 组（与 SML/TrustOS/UBoot 同组）；无 efuse 设备 BootROM 只校 DHTB hash → **未签名 FDL 也可用**（锁机可刷）。
- **T508N 实测走通**（`ARCHIVE-20260823.md` §12/§13）：
  ```
  spd_dump.exe --wait 300 exec_addr 0x65012f48 \
    fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 \
    exec r uboot reset        # 读出 3MB LK → uboot_t508n_lk_orig.bin
  # 写回同理：... exec w uboot <img> reset（多次成功）
  ```
  → 这条 USB 通道是**读/写引导器分区（uboot/spl/sml/trustos…）的唯一免解锁途径**，也是本仓库
  对 LK 做 magic64 签名保持补丁的实验通道（补丁实验本身属于被排除路线，但通道本身是可用 USB 启动流程）。

### 3.2 阶段 1 —— 引导器级：LK（uboot 分区，sprdlk）的 USB

**LK 启动早期就初始化 USB**（`UBOOT-TEE-USB-LOAD-POWER.md` §2）：
- 设 `vddusb33` rail = 3.3V（`0x110370`，boot 早期 0x5684 调用）；
- 上电包装 `0x11a3f0` = 使能 rail(`0x110270`) → HS-PHY/控制器位(`0x11b55c`) → DWC3 init(`0x116708`)；
  调用点 4 处：`0xcdd8`(boot 早期 usb init)、`0x18aa8`(驱动 init)、`0x209e8`(sprd_preload 线程)、`0x1165e0`(DWC3 再入)；
- charger 检测（bq2560x）→ 充电/关机充电模式。

**多源模式探测**（`UBOOT-LK-ANALYSIS.md` §2.3，字符串 `get_mode_from_*`）：
`chipram_env → bat_low → miscdata_boot_flag → file_extend → watchdog → alarm → pctool → gpio_extend → smpl → charger → keypad`，
最终归为 NORMAL / RECOVERY / FASTBOOT / DOWNLOAD / CHARGE / ENGTEST…。

**模式分发器 0xda00–0xe0e0**（`UBOOT-LK-ANALYSIS.md` §2.4、`work/uboot_analysis/REPORT.md` §C1）：

```
0xda00  [normal]   -> b 0x21ae8   （公共跳内核）
0xda34  [download] "download mode!" -> b 0x13a08   ★ FDL/下载主循环（do_download），不进内核
0xda70  [recovery] -> b 0x21ae8
0xdb04  [fastboot] -> b 0x21ae8   （fastboot USB 协议循环）
0xdb48  [charger]  -> …
其余 (engtest/autotest/apkmmi/calib…) 均最终 b 0x21ae8
```

- **download mode（USB 下载）**：`0x13a08` 是 FDL/ResearchDownload 循环，`do_download`、`reply exec act as Romcode`
  —— 即 LK 自己也能"扮演 Romcode"继续跑 FDL 下载协议（uboot_log 里那次刷写 boot_b 的会话就是这条路）。
- **fastboot（USB）**：LK 内置 fastboot 协议（`fastboot flash/set_active/reboot`，tools/README 与无串口验证指南全程使用），
  按键进入（关机→音量下+电源），本项目的所有刷机（boot_a/vendor_boot_a/dtbo_a）都走它。

**跳内核前的 USB 电源总开关**（`0x21ae8` 公共函数内，`work/uboot_analysis/REPORT.md` §C2）：
```
0x21b58  bl 0x3174          ; w0 = ([0x642e0034] != 0) ? 1 : 0   ← USB pin-mux 是否激活
0x21b5c  cbz w0, 0x21d18    ; "未用" → 关电分支
0x21d18  bl 0x11a43c        ; vddusb33 整体关断 + "usb_pin_mux config is closed!"
0x21b60  … 跳内核: blr x4 -> 0x80080000（x0=DTB@0xbbf00000）
```
语义：**下载/fastboot 等早期把 USB 拉起来的模式 → pin-mux 非 0 → 保电跳内核；
普通 normal 启动（未用 USB）→ 关掉 vddusb33 再跳内核**（本项目"内核侧没 USB"的直接原因；
LK 补丁 v1–v5 试图强制保电属于被排除路线，见 §6）。

### 3.3 阶段 2 —— 系统级：recovery（vendor_boot 分区内）的 USB

- **进入路径**：LK recovery 模式（misc/按键）→ 公共 `0x21ae8` 跳内核 → 内核挂载 vendor_boot ramdisk
  （`REPORT-T508N-启动流程分析.md` §5：boot ramdisk 2.06MB + vendor ramdisk 23.4MB，recovery 资源全在 vendor ramdisk 里）。
- **vendor ramdisk 内 recovery 全套**（实况核对 `work/vendor_ramdisk_out/`）：
  - `system/bin/recovery` + `system/lib64/librecovery_ui{,_ext}.so` + `res/`（恢复 UI）；
  - `system/etc/recovery.fstab`（+ `recovery.tmpfsdata.fstab`）：**完整分区表**——含 spl/mmcblk0boot0、
    uboot、sml、trustos、teecfg、vbmeta*/dtbo、boot、modem 域(l_modem/l_gdsp/l_agdsp/wcnmodem/gnssmodem/pm_sys)、
    super 逻辑分区(system/vendor/odm/product/vendor_dlkm，erofs+AVB)、userdata(f2fs+inlinecrypt)、cache、prodnv；
  - `init.recovery.common.rc`：**configfs USB gadget 完整装配**（见下）；
  - `lib/modules/modules.load.recovery`：recovery 专用内核模块链，**加载全套 USB 栈**——`dwc3-sprd-core.ko`、
    `dwc3-sprd.ko`、`phy-sprd-{dummy,usb31-qogirn6pro,commonphy}.ko`、`sprd_usbm.ko`、`sprd-bc1p2.ko`、
    `sprd-pamu3.ko`、`ptn38003a-i2c.ko`、`sc27xx_typec.ko`、`sc27xx_pd.ko`、`extcon-usb-gpio.ko`、
    `usb_f_vser.ko`、`sipa_usb.ko`、`ax88179_178a.ko` 等 → **recovery 走 DWC3 + configfs 复合 gadget**。
- **recovery 的 USB 枚举配置**（`init.recovery.common.rc`）：
  - 主 gadget `g1`：**idVendor 0x1782（Unisoc）**、idProduct 0x4d00，serialno=${ro.serialno}，MaxPower 120；
  - **函数集**：`ffs.adb` + `ffs.fastboot`（functionfs，mount 到 /dev/usb-ffs/adb 与 /dev/usb-ffs/fastboot）
    → **一个 recovery 里同时具备 adb 和 fastboot 两个 USB 功能**；另有 mtp/ptp/rndis/vser/accessory/audio_source/midi/gser 全套备用；
  - 组合配置：`sys.usb.config=adb,vser,gser` → PID 0x5d06、config "adb_vser_gser"。
- **TWRP 扩展**（`work/twrp_OT5_device_tree/README.md`、FINAL-WRAPUP §10）：社区已有 TWRP-12.1 设备树
  （A/B Virtual + 动态分区 + boot v4，recovery.fstab 分区表确认），可经 GitHub Actions 构建 `recovery.img`
  → recovery 的 USB（adb sideload / mtp / fastboot）是未来刷机/取证的现成通道。

### 3.4 阶段 3 —— Android：boot 分区的 USB

- stock 5.4.210 内核的 USB 全在外置模块：DWC3 链（`dwc3-sprd-core.ko`+`dwc3-sprd.ko`）、
  HS/SS PHY（`phy-sprd-qogirn6pro.ko` / `phy-sprd-usb31-qogirn6pro.ko`）、**MUSB**（`musb_hdrc.ko`，
  `usb@a00000`，dr-mode=peripheral）+ `musb_sprd` glue → **adb**；
- DTS 侧（`TASK-A-B-USB-DTS-C-离线成果.md` §2.2/§2.3）：`usb3@25100000`(dwc3, `snps,sprd-dwc3`)、
  `usb@a00000`(MUSB, 仅 peripheral/download)、`hsphy@300000`、`ssphy@25310000`、`pamu3@25210000`、
  `extcon-gpio`(vbus-gpio=pmic_eic0) 的完整 overlay 已从原厂 DTBO 提取；
- 本项目实际依赖的正是这条：**官方 b 槽 Android（Magisk root）的 adb** 用来回拉 uboot_log/pstore
  （`无串口验证指南.md`），是整个无串口调试闭环的 USB 腿。

---

## 4. USB 启动流程全景图

```
                     ┌──────────────────────────────────────────────────────┐
  关机 + 音量下 + USB │ BootROM (芯片)                                        │
  或 SPL 校验失败     │  · AON_APB strap 检测 → enter_usb_recovery            │
  ─────────────────► │  · USB 枚举, HDLC 帧协议(0x7E/CRC16)                   │
                     │  · 5 命令表: connect/start/recv_data/exec/end_data     │
                     │    = 无认证任意写 + 跳转原语                           │
                     │  · FDL1 → SRAM(0x65000800) → DDR init                  │
                     │  · FDL2 → DRAM(0xb4fffe00) → 全盘读写                  │
                     │    (spd_dump/ResearchDownload: r/w uboot/spl/… )       │
                     └───────────────┬──────────────────────────────────────┘
                                     ▼
                     ┌──────────────────────────────────────────────────────┐
                     │ SPL (spl_a / mmcblk0boot0/1)  DHTB+SIMGHDR RSA 校验   │
                     │  → 装 SML(0xbd000000)/TrustOS(0xbd040000)/LK           │
                     └───────────────┬──────────────────────────────────────┘
                                     ▼
                     ┌──────────────────────────────────────────────────────┐
                     │ LK = uboot_a/b (sprdlk @0xb5000000)                   │
                     │  · 早期 USB init: vddusb33=3.3V → PHY位 → DWC3 init    │
                     │    (0xcdd8/0x18aa8/0x209e8/0x1165e0) + charger 检测     │
                     │  · 模式探测 get_mode_from_* 链                          │
                     │    ├─ download → 0x13a08 FDL 循环(不进内核, act as     │
                     │    │              Romcode) ───────────────► USB 下载   │
                     │    ├─ fastboot → USB fastboot 协议 ────────► 刷机通道   │
                     │    ├─ recovery → 公共 0x21ae8（USB 已拉起,pin-mux≠0,   │
                     │    │              保电）───────────────────► 见下       │
                     │    └─ normal   → 公共 0x21ae8（pin-mux=0 → 0x21d18     │
                     │                 关 vddusb33 再跳内核）                  │
                     └───────────────┬──────────────────────────────────────┘
                                     ▼ 0x80080000 (内核) + DTB@0xbbf00000
        ┌────────────────────────────┴────────────────────────────┐
        ▼  recovery (vendor_boot ramdisk)                           ▼  Android (boot ramdisk)
   · modules.load.recovery 载全套 USB 模块                            · stock 内核模块化 USB 栈
   · configfs gadget g1:                                             · dwc3-sprd / musb_sprd → adb
     VID 0x1782 / PID 0x4d00 (0x5d06)                                · adb root (Magisk) → 回拉日志
     ffs.adb + ffs.fastboot (+vser/gser/mtp/rndis…)                  (本仓库无串口闭环的 USB 腿)
   · recovery.fstab 全分区表 → 可刷/备份引导链
   · TWRP (twrp_OT5_device_tree) 可构建 recovery.img 扩展
```

---

## 5. 关键结论

1. **三条可用的 USB 启动通道**（互不依赖，任意时刻至少一条可用）：
   ① BootROM 下载模式（FDL，免解锁，可读写引导器分区）；
   ② LK 的 download/fastboot（引导器级 USB，本仓库全部刷机都走 fastboot）；
   ③ recovery / Android 的系统级 gadget（adb + fastboot 双协议）。
2. **recovery 是"分区级 USB 功能"最完整的一层**：它不只是一个恢复 UI，而是带完整 configfs 复合 gadget
   （adb+fastboot+vser+gser+mtp/rndis）的迷你系统，其 `modules.load.recovery` 明确加载 dwc3-sprd 全链 +
   PHY + typec/PD + pamu3 + sipa_usb —— 这是原厂设计里 USB 各功能（下载/刷机/调试串口）的标准汇聚点。
3. **USB 电源语义是"按模式"的**：下载/fastboot/recovery 在 LK 早期就拉起 USB 域并保持；
   normal 启动在跳内核前按 pin-mux 寄存器 `[0x642e0034]` 关断 vddusb33 —— 所以"内核起来后没有 USB"
   不是硬件能力问题，而是 LK 的模式相关关电行为。
4. **下载协议全线贯通**：BootROM 的 HDLC/FDL 与 LK 的 `do_download`（"reply exec act as Romcode"）是同一
   ResearchDownload 家族的实现，本机 78 分区镜像本身就是经该协议 dump 的（uboot_log 会话证据）。

---

## 6. 被排除路线（按用户要求忽略，仅登记不展开）

| 路线 | 状态 |
|---|---|
| mainline 6.12 USB 驱动移植（dwc3-sprd/phy-sprd/musb glue + DTS 补全） | 未完成（WORK-适配材料清单 任务A/B 搁置）；6.12 从未走到 USB——本课题 USB 实测全部在 **5.4 线**（见 §7） |
| 内核侧 USB 上电：TEE/SIP 域墙（REG_FW/TZPC 保护 IPa/模拟域），dwc3/MUSB/PHY 全路径 SError 红屏 | 失败（ARCHIVE §10/§11/§15、USB-ENABLE-IN-BOOT-GUIDE §5） |
| LK 保电补丁 v1–v5（0x21b5c/0x21d18/0x11a3f0 变体 + 关电位 NOP） | 实测仍红，收益递减停止（ARCHIVE §13/§14） |
| "USB gadget 彻底关闭" 终局判定（NS 侧无合法上电路径） | 结论登记（ARCHIVE §11/§15） |
| 无串口验证指南 §4 的 "USB gadget 终极方案"（musb_hdrc 移植） | 未可用（需补驱动） |
| 5.4 复刻树 USB 模块（vermagic 不匹配等） | 未启用（FINAL-WRAPUP §7.3） |

> 上述被排除项不影响 §3 中"原厂设计里 USB 在启动各阶段如何走"的分析——它们只是
> **本项目自建内核侧 USB 的失败尝试**，而原厂 boot/recovery 的 USB 功能（本文主体）一直可用。

---

## 7. 为什么自建内核侧 USB 不可用（根因链，按证据强度）

> **对象澄清（重要）**：本课题的自建内核是 **mainline 5.4 线**（`linux-5.4-custom` / `boot_54vendor`：
> 5.4.210 + 原厂模块链 + 原厂 DTB/uboot + 自绑 configfs gadget），**不是 6.12**——6.12 从未走到 USB
> （其 USB 驱动从未移植，见 §6 首行，属另一条未走通的路）。5.4 线上的实测表明驱动层**全部工作**。
>
> ⚠️ **2026-08-25 终局修正（ARCHIVE §19，本报告 §7 下半部分结论以此为准）**：
> 早期"SSphy 物理层 = APB-PROT 墙、NS 无合法路径"的判定**已被实测推翻**——真因是
> **IPa/USB 域处于 pm_genpd suspended、从未被唤醒**（域未醒 → 访问寄存器 = 总线错误 SError，
> 普通 ARM 行为，无需防火墙解释）。`echo on > …/25100000.usb3/power/control` →
> runtime-PM resume → `sipa_sys_do_power_on_cb_v3` 上电 → **域 active、无 SError**，
> ssphy/pamu3/UDC/gadget/extcon 全绑定成功。唯一剩余 = **dwc3 "not attached"**（USB3 链路训练/
> 数据角色层，`data-role` 未设 + typec 角色）。SML 无 USB SMC 的结论仍成立，但是"另一层墙"，
> 与本问题无关。

### 事实链（5.4 线上已实测/已反汇编确认，ARCHIVE-20260823 §6/§10/§11/§13–15 + §19 修正）

1. **驱动层：全通**。`udc: 25100000.dwc3` 注册 ✓、`25100000.usb3 -> dwc3-sprd` 绑定 ✓、
   `extcon0/1 state=1`（插入检测）✓、`gadget: BOUND 25100000.dwc3`（configfs 绑定）✓ ——
   DWC3/gadget/插入检测全链工作，**不是"缺驱动"**。
2. **LK 时序层**：normal 启动跳内核前按 pin-mux `[0x642e0034]` 关 vddusb33（`0x21d18`）；
   magic64 补丁 v1/v2/v3/v5 曾"仍红屏"——**§19 解释：那些实验从未唤醒 pm_genpd 域**
   （LK 保电 `0x11a3f0` ≠ 内核 `sipa_sys_do_power_on_cb_v3` 域状态机序列），红屏是"域未醒"
   而非防火墙。
3. **SML/TrustOS 面（§19 定性为"另一层墙"，非本问题）**：
   - SML 完整 1MB 反汇编：SMC 分发 owner 0=PSCI / 2=SIP / 4=Trusty，owner 52(SPRDSEC) 无 handler；
     SIP 只有 PERF/DBG/STORAGE/PWR(只读)/DVFS/NPU —— **无 usb/ipa/power-domain/prot 服务**；
   - TrustOS 有 TZPC/REG_FW 防火墙机制（`tzfw_test.c`），但受保护外设命名清单
     （uart/spi/i2c/dma/gpio/efuse/ce/tmr_rtc）**不含 USB**；
   - **与早期"USB 寄存器全落保护段"的推断相反，§19 实测：域唤醒后 ssphy/pamu3 寄存器可访问**。
4. **最终卡点（更新）**：不再是 SError/枚举不可能，而是 **dwc3 "not attached"**——
   USB3 链路训练/数据角色未就绪（`data-role` 属性未设、dtb 槽位无余量、typec 角色初值）。

### 对照：原厂系统为什么 adb 能用（与自建 5.4 的差别 = 授权栈）
"原厂能用"与"NS 侧无合法路径"**不矛盾**：原厂能用 ≠ AP 存在合法上电路径，
而是 **原厂整套软件（内核 + vendor 模块链 + TEE 侧）在出厂时就是配套授权好的**：

1. **TEE = 安全外围总闸（会话授权制）**（ARCHIVE §10）：受保护外设（含 USB/IPa）的放行
   按会话/按启动状态授予；原厂内核启动时走完厂商授权握手，TrustOS 启动时按段配置
   REG_FW/TZPC 防火墙，让**受信的原厂栈**访问 USB/IPa 域 —— stock adb 因此存在。
2. **自建 5.4 只复刻了 NS 侧一半**（`SMC-DISPATCH-USB-REPORT.md` §D 的实测定性链）：
   - NS 侧全通：`udc: 25100000.dwc3` 注册、`dwc3-sprd` 绑定、extcon 检测、gadget BOUND；
   - 但 **ssphy/glue 永远 -517（供应商链接 DORMANT）= 在等授权/上电依赖**；强行放行 probe
     → 写受保护寄存器 → SError；
   - LK 保电 v1–v5（让内核拿到已上电域）→ 仍 SError → **判定为真 REG_FW/TZPC 墙**，
     不是"没上电"（这正是报告 §D 设计的决定性实验，ARCHIVE §13/§14 已执行）。
3. **SMC 面已穷尽，授权不走 SMC**：SML 只暴露 PSCI(CPU) + SIP(时钟/DVFS/NPU/只读PWR) +
   Trusty(gatekeeper/keymaster/storage/rpmb/gsp/vsp/disp…，**无任何 usb/ipa/prot 服务**)；
   owner 52(SPRDSEC) 无 handler；域上电本身是纯寄存器写（内核 `sipa_sys_do_power_on_cb_v3()` /
   LK `0x11a3f0`），无对应 SMC 可调 → **放行不是"某个 SMC 发放"的，而是 TrustOS 启动时
   按段配置的防火墙本身对"谁"开**。
4. **诚实边界（已被 §19 实测闭合）**：§D 原文"防火墙是否恰好把 USB-IPa 寄存器列入保护段 vs 只是
   域没上电仍需实测定性" —— 后续实测（ARCHIVE §19，2026-08-25）给出答案：**不是墙，是 pm_genpd
   域未唤醒**；唤醒后寄存器可访问、无 SError。原厂/自建的差别从"授权栈"修正为
   **"原厂栈正常触发了 genpd 上电，我们之前从没触发"**（USB-IMAGE-SIDE-REPLICA-CHECKLIST.md
   同此结论：固件不区分镜像，差异在镜像的 DTB `power-domains` 绑定与模块链）。

### 结论（已按 ARCHIVE §19 终局修正）
5.4 线上"自建内核侧 USB 不可用"的完整因果（2026-08-25 实测版）：
**LK 普通启动关域（直接原因）＋ 内核侧 pm_genpd 域从未被唤醒（真因）＝ 访问寄存器即总线错误 SError**
—— 不是安全固件防火墙，普通 ARM 行为。
- **解药（已实测）**：`echo on > …/25100000.usb3/power/control` → runtime-PM resume →
  `sipa_sys_do_power_on_cb_v3`（DSLPEN→AUTOSHUTDOWNEN→STATE→ACCESSEN→PLL）→ **域 active、无 SError**，
  ssphy/pamu3/UDC/gadget/extcon 全绑定 ✓。
- **当前唯一剩余**：**dwc3 "not attached"** —— USB3 链路训练/数据角色层（`data-role` 未设、
  dtb 槽位 156777B 无余量、typec 角色初值）。候选：pyfdt 无损插 `data-role` /
  `sprd_tcpm.c` 强制 `typec_set_data_role(DEVICE)` / 物理 UART。
- 早期"APB-PROT 墙 / NS 无合法路径 / 课题终结"表述（ARCHIVE §15）已被 §19 **推翻**；
  SML 无 USB SMC 仍成立，但属于"另一层墙"，与本问题无关。
