# REPORT-USB-MAINLINE-DWC3-ACM-SUCCESS

日期：2026-10-11
状态：**成功**。T508N B 槽 mainline 6.18.55 通过 DWC3 + SSPHY 枚举为 USB ACM 串口（Windows COM18），
可进 mainline shell；p75 已挂载并可持久化。

## 0. 结论先行

```text
路线：DWC3 25100000.dwc3 + phy-sprd-ums9620-ssphy + sprd,usb-dummy-phy
结果：Windows 枚举 USB Composite Device VID_1D6B PID_0107
      USB 串行设备 COM18
      mainline shell 可交互（uname/ls/dmesg/mount 等）
镜像：build/boot-images/boot_mainline_6.18.55_t508n_test28.img
md5：1771fc80b7ee8441bd60dd68a7417282
```

MUSB 路线（`sprd,qogirn6pro-musb` + 自写最小 glue）已在 test14~test16 验证：

```text
UDC 可以注册
configfs gadget 可以绑定
usb0 可以出现
但主机一直停在“未知 USB 设备(设备描述符请求失败)”
```

因此最终改走 **MU300 已验证的 DWC3 + SSPHY 路线**，放弃 MUSB。

## 1. 硬件/DTB 事实

stock vendor DTB（`vendor_boot_b.img` 内 FDT）里：

```dts
usb3@25100000 {
    compatible = "sprd,qogirn6pro-dwc3";
    status = "disabled";              /* board dtbo 打开 */
    usb-phy = <&dummyphy>, <&ssphy>;  /* index0=USB2 dummy, index1=USB3 ssphy */
    power-domains = <&sipa_sys>;
    dwc3@25100000 {
        compatible = "snps,sprd-dwc3";
        usb-phy = <&dummyphy>, <&ssphy>;
        usb-pam = <&pamu3>;
        phy_type = "utmi";
        usb-role-switch;
    };
};

dummyphy {
    compatible = "sprd,usb-dummy-phy";
    status = "okay";
};

ssphy@25310000 {
    compatible = "sprd,qogirn6pro-ssphy";
    status = "disabled";              /* board dtbo 打开 */
    power-domains = <&sipa_sys>;
};
```

关键点：

```text
DWC3 index 0 指向 dummyphy
DWC3 index 1 指向 ssphy
mainline 没有 sprd,usb-dummy-phy 驱动
=> devm_usb_get_phy_by_phandle(..., 0) 永远 -EPROBE_DEFER
=> dwc3_core_init() 返回 -EPROBE_DEFER
=> devices_deferred:
   25100000.dwc3   dwc3: failed to initialize core
```

## 2. 根因链

### 2.1 DWC3 deferred 的真因：缺 dummy USB2 PHY

test21 日志：

```text
/persist/test21-deferred-before.txt:
    25100000.dwc3   dwc3: failed to initialize core
/persist/test21-deferred-after.txt:
    25100000.dwc3   dwc3: failed to initialize core
```

`devices_deferred` 只给摘要，真实原因是 DWC3 的 `usb-phy` index 0 找不到 `sprd,usb-dummy-phy` provider。
vendor 5.4 靠 `phy-sprd-dummy.ko` 注册这个 USB2 PHY。

### 2.2 vddusb33 / DPDM

LK normal 启动会按 USB pin-mux 判据关闭 `vddusb33`，并可能把 DPDM 留给充电检测。
因此 mainline 侧需要在 DWC3 probe 前：

```text
1. UMP9620 vddusb33 LDO 使能（PD=0x21e8 bit0=0，VOL=0x21ec=0xD2=3.3V）
2. CHGR_DET_FGU_CTRL(0x23a0) bit14/bit0=1，DPDM 切到 USB PHY
```

### 2.3 sipa-sys / pamu3

test5 曾出现：

```text
ums9620-ipa-sys-pd ... sync_state() pending due to 25210000.pamu3
```

这个 pending 不是 DWC3 deferred 的直接原因；在 dummy PHY 注册后 DWC3 即可 probe。
MU300 主线同样没有单独 pamu3 驱动，PAMU3 reset/PLL 由 SS PHY 驱动内部处理。

## 3. 最终修复

### 3.1 `sprd_usb_power.ko`

位置：`mainline-ref/t508n-mainline/modbuild/sprd_usb_power/sprd_usb_power.ko`

作用：

```text
找 PMIC regmap（sprd,ump962x-syscon）
UMP9620_LDO_VDDUSB33_PD  = 0x21e8，bit0=0（enable，低有效）
UMP9620_LDO_VDDUSB33_VOL = 0x21ec，写 0xD2（3.3V）
CHGR_DET_FGU_CTRL        = 0x23a0，bit14/bit0=1（DPDM -> USB PHY）
```

实测回读：

```text
SPRD_USB_POWER: PD=00000000 VOL=000000d2 DPDM=00004201
```

### 3.2 `sprd_dummy_phy.ko`

位置：`mainline-ref/t508n-mainline/modbuild/sprd_dummy_phy/sprd_dummy_phy.ko`

作用：

```text
compatible = "sprd,usb-dummy-phy"
注册一个 no-op legacy usb_phy（USB_PHY_TYPE_USB2）
供 DWC3 index 0 使用
```

加载后：

```text
sprd-dummy-phy soc:ipa-apb:dummyphy: SPRD dummy USB2 PHY registered
devices_deferred 清空
/sys/class/udc/25100000.dwc3 出现
```

### 3.3 DWC3 / SS PHY

mainline 树里已具备：

```text
drivers/usb/dwc3/dwc3-of-simple.c:  compatible "sprd,qogirn6pro-dwc3"
drivers/usb/dwc3/core.c:            compatible "snps,sprd-dwc3"
drivers/usb/dwc3/drd.c:             UMS9620 userspace role switch
drivers/usb/phy/phy-sprd-ums9620-ssphy.c
CONFIG_USB_SPRD_UMS9620_SSPHY=y
```

### 3.4 configfs：只保留 ACM

最终放弃 RNDIS，只保留 ACM 串口：

```text
idVendor   = 0x1d6b
idProduct  = 0x0107
manufacturer = TCL
product      = T508N-mainline6-ACM
serialnumber = T508N-ACM
functions/acm.GS0
```

不再创建：

```text
rndis.usb0
os_desc
usb0 / udhcpd / telnet
```

### 3.5 shell / p75 持久化 / USB 不断开

initramfs 里：

```sh
/bin/busybox --install -s /bin
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

mkdir -p /persist/etc /persist/root /persist/mainline
mount --bind /persist/etc  /etc
mount --bind /persist/root /root

echo -1 > /sys/module/usbcore/parameters/autosuspend
echo on > /sys/devices/platform/soc/soc:ipa-apb/25100000.usb3/25100000.dwc3/power/control

while true; do /bin/busybox sleep 3600; done
```

说明：

```text
test26 原来有 sleep 600 + reboot -f，会 10 分钟主动断开 USB
usbcore.autosuspend 默认 2，空闲会 autosuspend
test28 已同时修掉
```

## 4. 实机验证

### 4.1 枚举 / shell

```text
Windows:
    USB Composite Device VID_1D6B PID_0107  OK
    USB 串行设备 COM18                     OK

COM18:
    ~ # uname -a
    Linux (none) 6.18.55 #5 SMP ...

    ~ # ls /sys/class/udc
    25100000.dwc3

    ~ # cat /sys/module/usbcore/parameters/autosuspend
    -1
```

### 4.2 p75 持久化

```text
~ # mount | grep -E 'p75|/etc|/root'
/dev/mmcblk0p75 on /persist type ext4 (rw,relatime)
/dev/mmcblk0p75 on /etc     type ext4 (rw,relatime)
/dev/mmcblk0p75 on /root    type ext4 (rw,relatime)

~ # touch /root/mainline-persist-check.txt
~ # sync
~ # reboot -f
（重启后 COM18 自动回来）
~ # ls -l /root/mainline-persist-check.txt
-rw-r--r-- 1 root root 0 /root/mainline-persist-check.txt
```

### 4.3 eMMC 访问

```text
~ # ls -l /dev/mmcblk0p75
brw------- 259,67 /dev/mmcblk0p75

~ # touch /persist/mainline-test28-persist.txt
~ # sync
```

重启后文件仍存在。

## 5. 镜像与模块路径

镜像：

```text
build/boot-images/boot_mainline_6.18.55_t508n_test28.img
C:\platform-tools\boot_mainline_6.18.55_t508n_test28.img
md5 = 1771fc80b7ee8441bd60dd68a7417282
```

模块：

```text
mainline-ref/t508n-mainline/modbuild/sprd_usb_power/sprd_usb_power.ko
mainline-ref/t508n-mainline/modbuild/sprd_dummy_phy/sprd_dummy_phy.ko
```

内核：

```text
mainline-ref/src/out-6.18.55/Image.lk
mainline-ref/t508n-mainline/out/Image.lk
```

## 6. 恢复 / 双槽

```text
A 槽 = vendor 5.4 OpenWrt，USB RNDIS + ACM（COM13）
B 槽 = mainline 6.18.55，USB ACM（COM18）

回 A：
    fastboot flash misc misc_slot_a.img
    fastboot reboot
或：
    E:\Download\ums9620_tcl_t508n_magic64\07-切换到A槽-openwrt.bat
```

## 7. 待办

```text
1. mmc1 (22210000.sdio) 每约 20 秒刷 SDHCI 错误日志，SD 卡槽未使用，可关掉/修掉
2. mainline 下持久化 /etc 后加入自定义 init / 服务
3. WLAN / BT / SDIO bring-up
4. 将本报告加入 docs/INDEX.md、FINAL-BRINGUP-INDEX.md、mainline NEXT.md
```
