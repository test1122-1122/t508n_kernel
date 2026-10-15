# REPORT-USB-A — T508N MUSB 虚拟串口路线记录

日期：2026-10-06
状态：暂存 / 未打通 UDC（先转向 DWC3 gadget 路线，见 REPORT-USB-B）

## 1. 目标

在 5.4 vendor 内核 + OpenWrt initramfs 上，用 MUSB 设备模式开 USB ACM 虚拟串口。

目标 UDC：

```text
musb-hdrc.0.auto   (64a00000.usb)
```

## 2. 硬件/DTBO 条件

- T508N `usb@a00000`：`sprd,qogirn6pro-musb`，`usb-phy=<&hsphy>`，`dr-mode="peripheral"`。
- 原厂 OT5 DTBO entry 已给：

```dts
hsphy: vdd-supply = <&vddusb33>; extcon = <&extcon_gpio>; status = "okay";
usb:   extcon = <&extcon_gpio>, <&pmic_typec>; status = "okay";
```

- T508N 无外挂 bq2560x / otg-vbus 节点；MUSB peripheral 由主机供 VBUS，理论上不需要 otg-vbus。
- 原厂 stock T508N kernel config：

```text
CONFIG_USB_MUSB_GADGET=y
# CONFIG_USB_MUSB_DUAL_ROLE is not set
CONFIG_SPRD_PAMU3=m
# CONFIG_USB_CONFIGFS_ACM is not set
CONFIG_USB_CONFIGFS_SERIAL=y
CONFIG_USB_F_VSERIAL=m
```

## 3. 已做的镜像/尝试

### 3.1 image3 — MU300 顺序、stock 内核配置（未对齐）

- vendor 内核：`USB_MUSB_DUAL_ROLE=y`、`SPRD_PAMU3=n`。
- 模块按 MU300 相对顺序加载。
- gadget 绑首个 UDC（按字典序）→ 无 UDC，只有 stage7。

### 3.2 max 链 — 关闭 USB3 + MUSB 优先

- 自定义 DTBO 关闭 `usb3/ssphy/pamu3`。
- `t508n-usb-gadget` 优先 `musb-hdrc.0.auto`。
- 模块仍 MU300 顺序。
- 结果：OpenWrt 正常，stage7 亮，无 UDC。

### 3.3 stockandroid — stock modules.load 顺序 + 强制重绑

- loader 改成 stock `modules.load` 顺序 + 第二遍重试。
- S50 执行原厂 `init.c` 强制序列：

```text
/sys/devices/platform/soc/soc:ipa-apb/*/power/control = on
echo 64a00000.usb   > /sys/bus/platform/drivers/musb-sprd/bind
echo 64300000.hsphy > /sys/bus/platform/drivers/sprd-hsphy/bind
```

- 结果：stage7 仍亮，无 UDC。

### 3.4 stockandroid2 — 对齐原厂 MUSB 配置

- 发现 vendor 树配置与原厂 T508N config 不一致：
  - 原厂 `MUSB_GADGET=y`，我们之前 `MUSB_DUAL_ROLE=y`；
  - 原厂 `SPRD_PAMU3=m`，我们之前 disable。
- vendor 树因为 `phy-sprd-qogirn6pro.ko` 需要 `musb_set_utmi_60m_flag`（由 musb_host.c 导出），用 DUAL_ROLE 才能编译。
- patch `musb_sprd.c` 强制 `pdata.mode = MUSB_PERIPHERAL`。
- 补 `sprd-pamu3.ko`，模块集 62 个。
- 结果：仍无 UDC（fastboot boot / 普通启动都试过）。

## 4. 诊断

- 屏幕标记始终停在 stage7（无 UDC）。
- 已构建 diag1 文字诊断镜像，待读取 PHY/MUSB bind、vddusb33 状态、devices_deferred、模块失败数。
- 已知 T508N LK 普通启动会：

```text
读 [0x642e0034] == 0
→ 关 vddusb33
→ pinmux closed
```

- fastboot/SPD 下载模式走 `0x11a3f0` 上电 USB，但主要配 DWC3/SS 路径；MUSB 是否被上电不明确。

## 5. 结论 / 暂存

MUSB 路线的软件侧已尽量对齐：

```text
内核配置、模块顺序、强制 bind、DTBO vdd/extcon、MUSB_PERIPHERAL
```

仍无 UDC，说明剩余障碍更可能是：

```text
LK 普通启动 USB 域/pinmux 关闭
或 MUSB PHY 模拟域被安全固件挡
```

先转向 DWC3 gadget 路线（B）；MUSB 路线保留以下现场：

- 镜像：`boot_54openwrt_stockandroid2_usb.img` md5 `c006abbce36fad89c81990c4dd8cee95`
- 诊断镜像：`boot_54openwrt_diag1.img` md5 `fdd20417a812bb4ca2b4048d10eee5d4`
- 模块链脚本：`work/openwrt54/rootfs-vendor-usb/sbin/t508n-load-usb-vendor`
- 强制重绑脚本：`work/openwrt54/rootfs-vendor-usb/etc/init.d/t508n-usb-gadget`
- `musb_sprd.c` 强制 peripheral 的 patch 在 `src/vendor-kernel-ums9620/drivers/usb/musb/musb_sprd.c`
