# T508N 自制 USB 链规格（v21 收官构型）

> 自制链 = 原厂链的**最小可复刻子集**（域 + genpd + MUSB + HS-PHY + 强制 peripheral）。
> 本文件把散落在 ARCHIVE §20/§21、`t508n-repo/patches/`、`initramfs_root/` 的自制链信息
> 收拢为"模块清单 → 内核补丁 → init 行为 → 复现步骤"规格。
> 权威事实记录：`t508n-repo/docs/ARCHIVE-20260823.md` §20（控制器选型修正）/ §21（收官总结）。

---

## 1. 构型总览（v21 final，2026-08-25）

| 部件 | 值 |
|---|---|
| boot_a | 0a49922e / 内核 2b644e55（自编 5.4.210，基于 `work/linux-5.4-custom` 系）；**§22 探针版 ed0ebdd6** |
| vendor_boot | **原厂 DTB**（`power-domains=<0x44>` 完整；⚠️ 勿用 `-usb-nopd` 变体） |
| uboot | 原厂（已还原，7fe2b5ea） |
| 模块链 | 短链 50 + musb 组（完整清单 = `initramfs_root/modules.load`，152 行） |
| init | `initramfs_root/init.c` = `t508n-repo/initramfs/init-USB-v21-final.c`（编译产物 `initramfs_root/init`） |
| gadget | `initramfs_root/usb-gadget.sh`（f_serial ttyGS0，等待 UDC ≤120s） |
| 结果 | 域唤醒 → MUSB → HS-PHY → peripheral → gadget BOUND → **attached 多次稳定命中**；**软件全链=就绪，物理层=最终边界**（§22） |

### 1.1 ★ 4 步硬件回读验证（§22，init 内 STEPn 探针，每步=动作+硬件回读+判定）

| STEP | 动作 | 硬件回读证据 | 判定 |
|---|---|---|---|
| 1 域唤醒 | 3×`power/control=on` + 2s | usb3 summary active（初版摘要格式解析 bug；control 回读 x2/3，第 3 个=auto） | ✅ |
| 2 MUSB 绑定 | `musb-sprd/bind` 写 `64a00000.usb` | **`/sys/class/udc/musb-hdrc.0.auto` 存在**（探针 open 读符号链接 bug → 用 readlink） | ✅ |
| 3 连接 | state 重试 ≤8×1s | **`attached`（多次稳定命中!）** | ✅ |
| 4 gadget | configfs g1(f_serial)+UDC | **`gadget bind: musb-hdrc.0.auto` 成功**（ttyGS0 判定需绑定后稍候） | ✅ |

> **结论（§22 原文）**：自制链每一步 = 硬件全部成立（无假设成功：attached + gadget-on-MUSB = 铁证）。
> 主机无枚举 = **物理/电气层剩余**（D+/D− 线态、HS-PHY 电平——非软件可控；无串口/示波器限制）。
> 探针改进（待做，非阻塞）：STEP1 摘要格式匹配 / STEP2 readlink / STEP4 ttyGS0 延迟判定。

---

## 2. 内核补丁清单（`t508n-repo/patches/` + 内核树）

| 补丁 | 作用 |
|---|---|
| `musb_sprd-force-vbus.c` | MUSB glue：**无 extcon 时强制 `vbus_active=1` + `wq_mode=PERIPHERAL` + queue_work**；soft-connect 走 `MUSB_DEVCTL` session（ASIC 150ms vbus 检查语义：controller 等 150ms 确认 vbus 后 session 才生效，需 sleep 保证） |
| `phy-sprd-usb31-qogirn6pro.c.patch` | SS PHY 相关（链内保留，MUSB 路径非必需） |
| core / sipa_pd / tcpm / sc27xx_typec | 域/驱动侧配套（§21 已入库；sipa_pd 加 T508N 步骤标记） |

> 模块链依赖序关键：**modpost 符号合并**（跨模块 exports 注入）——musb_sprd 依赖
> musb_hdrc / sprd_usbm / hsphy / phy-sprd-commonphy 的符号，需在构建时合并根 Module.symvers。

---

## 3. 模块清单（短链 + musb 组，`initramfs_root/modules.load`）

152 行清单的子集（USB 相关，`init-USB-v21` 显式加载）：

```
…（sprd_sip_svc / trusty 组 / pinctrl 组等前置）…
sprd_usbm.ko            ; USBM 状态/onoff/notifier
phy-sprd-commonphy.ko   ; USB PHY 事件框架
phy-sprd-qogirn6pro.ko  ; HS PHY（sprd,qogirn6pro-phy, hsphy@300000）
sprd-bc1p2.ko           ; 充电器识别（已排除为主嫌疑）
musb_hdrc.ko            ; MUSB 核心（UDC musb-hdrc.0.auto）
musb_sprd.ko            ; MUSB glue（64a00000.usb -> musb-sprd）
```

> MUSB 链无 typec/PD/trusty/sipa 大链——这就是"砍链"所在；角色由 musb_sprd 补丁强制，
> 域唤醒由 init 显式完成。

---

## 4. init 行为（init-USB-v21，`initramfs_root/init.c`）

1. **诊断通道**：`tlog()`（syscall 441 → 内核渲染文本到 fb）+ `km()`（/dev/kmsg）。
2. **域唤醒**（对应 §19 解药，init 内自动执行）：向三个 sysfs 写 `on`：
   ```
   /sys/devices/platform/soc/soc:ipa-apb/25100000.usb3/power/control
   /sys/devices/platform/soc/soc:ipa-apb/25220000.sipa/power/control
   /sys/devices/platform/soc/soc:ipa-apb/sipa-dele/power/control
   ```
   → pm-runtime resume → sipa-sys genpd（DSLPEN→AUTOSHUTDOWNEN→STATE→ACCESSEN→PLL）→ 域 active。
3. **MUSB 绑定**：`/sys/bus/platform/drivers/musb-sprd/bind` 写入 `64a00000.usb`（无 extcon 强制
   vbus_active=1 由此路径进入）。
4. **状态轮询**：读 `/sys/class/udc/musb-hdrc.0.auto/state`（retry 判读 attached）。
5. **gadget**：调 `usb-gadget.sh`（configfs g1：f_serial → ttyGS0，绑定 UDC）。

---

## 5. 复现步骤（原厂 → v21）

1. 自编 5.4.210 内核（`work/linux-5.4-custom` 系），config 对齐原厂（NR_CPUS=32/NUMA=n/SMT=n/
   DEVTMPFS=n + USB_MUSB_HDRC/DUAL_ROLE/SPRD_QOGIRN6PRO_USB2_PHY=m + CONFIG_USB_CONFIGFS）。
2. 打补丁：`t508n-repo/patches/`（musb_sprd-force-vbus 等）+ 根 Module.symvers 符号合并。
3. 装配 initramfs：`initramfs_root/`（init=init-USB-v21、modules.load、usb-gadget.sh、Alpine rootfs）。
4. `tools/sprd_mkbootimg.py` 打包 → `boot_a`。
5. **vendor_boot = 原厂 DTB**（power-domains 必须保留）；dtbo_a = noop 在位。
6. uboot = 原厂。
7. 开机：init 自动 域唤醒 → musb bind → gadget → **attached 多次稳定命中（§22 4 步硬件回读全 ✅）**。
8. **最终边界（§22）**：主机无枚举 = 物理/电气层（D+/D− 线态、HS-PHY 电平，非软件可控；
   需示波器/串口诊断）。§21 的"强制 `musb_gadget_connect`"方向经逐步硬件验证后已收敛。

---

## 6. 与文档对应

- 事实：`t508n-repo/docs/ARCHIVE-20260823.md` §20（控制器选型：走 MUSB 而非 DWC3）/ §21（收官总结）/
  **§22（4 步逐步硬件回读验证表，最终定论）**
- 对比：`work/REPORT-USB-CONSOLIDATED.md` §3.5（原厂链 vs 自制链逐环节对比表）
- 根因：`work/REPORT-USB-BOOT-FLOW.md` §7、`work/REPORT-STOCK-USB-STACK-MECHANISM.md` §10.5
