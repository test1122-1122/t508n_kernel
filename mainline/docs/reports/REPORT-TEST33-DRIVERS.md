# TEST33 驱动前向移植（离线）

日期：2026-10-15
设备：TCL T508N / UMS9620 / Qogirn6Pro

## 本轮目标

在无设备条件下，把 vendor 5.4 中已有、mainline 缺失的“外围驱动”先移植/编译出来。

## 1. mainline 现有驱动启用

已修改内核 config：

```text
CONFIG_INPUT_SC27XX_VIBRA=y
CONFIG_LEDS_SC27XX_KPLED=y
```

已有：

```text
CONFIG_FUEL_GAUGE_SC27XX=y
CONFIG_RTC_DRV_SC27XX=y
CONFIG_SC27XX_ADC=y
CONFIG_NVMEM_SC27XX_EFUSE=y
CONFIG_SPRD_THERMAL_R5P0=y
CONFIG_ARM_SPRD_CPUFREQ_V2=y
```

新增 compatible：

```text
drivers/input/misc/sc27xx-vibra.c
    { .compatible = "sprd,ump9620-vibrator", .data = &sc2731_data }
```

## 2. 键盘驱动

来源：

```text
vendor-kernel-ums9620/drivers/input/keyboard/sprd_keypad.c
```

移植到：

```text
modbuild/sprd_keypad/sprd_keypad.ko
```

适配：

```text
补 <linux/input.h> / <linux/of.h> / <linux/of_device.h>
platform_driver.remove -> void
```

DT compatible：

```text
sprd,sc9860-keypad
```

对应 T508N DTS：

```text
keypad@1b0000
```

## 3. 快充驱动

来源：

```text
vendor-kernel-ums9620/drivers/power/supply/sc27xx_fast_charger.c
```

移植到：

```text
modbuild/sc27xx_fast_charger/sc27xx_fast_charger.ko
```

适配：

```text
补 <linux/interrupt.h> / <linux/usb/phy.h>
SQC_NONE_TYPE 缺省定义
power_supply_config.of_node -> fwnode
platform_driver.remove -> void
```

DT compatible：

```text
sprd,ump9620-fast-chg
```

注意：

```text
不自动加载，手动：
    /usr/sbin/t508n-load-fchg
```

## 4. 搜索到的其他项目

```text
MotorolaMobilityLLC/sprd-kernel-modules-display        -> 空仓库
MotorolaMobilityLLC/sprd-kernel-modules-common-camera  -> 空仓库
MotorolaMobilityLLC/sprd-kernel-modules-video          -> 空仓库
MotorolaMobilityLLC/sprd-kernel-modules-audio          -> 空仓库
```

音频：

```text
jingpad-bsp/vendor_sprd_modules_audio
MotorolaMobilityLLC/sprd-kernel-modules-audio
```

内容为 Android HAL，不是 kernel codec 驱动。

传感器：

```text
jingpad-bsp/vendor_sprd_modules_sensors
```

内容为 Android sensors HAL，可用来确认芯片型号：

```text
accel: qmax981 / mir3da / lis2dh / bma253 / lis3dh
mag:   akm099xx / VTC
pls:   LTR558ALS / EPL2182 / FOCALTECH
```

## 5. 产物

```text
Image.lk
    md5 = bfbac11d0c381bca3dbc27906b82897c

boot_mainline_6.18.55_t508n_test33.img
    md5 = 4088972b992006e7d47065dfdcb8476c
```

SPD：

```text
E:\Download\ums9620_tcl_t508n_magic64\11-刷B槽-test33.bat
```

## 6. 上机验证

```text
1. 键盘
   insmod sprd_keypad.ko
   dmesg | grep -i keypad
   evtest / input event（如有工具）

2. 振动
   内核已内建 INPUT_SC27XX_VIBRA
   dmesg | grep -i vibra
   /sys/class/input/

3. 键盘灯
   内核已内建 LEDS_SC27XX_KPLED
   ls /sys/class/leds/

4. 电池 / RTC / ADC
   /sys/class/power_supply/
   /dev/rtc*
   /sys/bus/iio/devices/

5. 快充（手动）
   /usr/sbin/t508n-load-fchg
   dmesg | grep -i fchg
```

## 7. 未完成

```text
显示 DPU/DSI/panel/backlight    vendor 5.4 DRM 可移植，工作量大
Sensorhub                       vendor 5.4 有源码，可移植
GNSS                            vendor 5.4 有源码，可移植
Camera/ISP/CSI/CPP              无开源源码
VPU/JPEG/VDSP                   无开源源码
NPU                             无开源源码
Audio codec                     无开源 kernel 源码
```
