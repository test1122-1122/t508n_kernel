# TEST32 离线修复汇总（无设备）

日期：2026-10-15
设备：TCL T508N / UMS9620 / Qogirn6Pro（B 槽 mainline 6.18.55）

## 目标

在没有设备的情况下，先完成以下可离线修复/准备工作：

1. eMMC/SD 控制器
2. GNSS 固件加载
3. WiFi regulatory / 5GHz
4. 蓝牙 SDIO tty
5. 看门狗、modem、GPU 的模块与加载脚本

## 1. eMMC / SD

文件：

```text
drivers/mmc/host/sdhci-sprd.c
```

已确认：

```text
SDHCI_SPRD_DLL_PHASE_INTERNAL = 0x2   /* MU300 r11p3，之前已应用 */
```

本次修改：

```text
只 probe:
    non-removable eMMC (mmc0)
    sprd,name = "sdio_wifi" (mmc2)

不再 probe:
    sprd,name = "sdio_sd" (mmc1)

目的：消除未使用 SD 卡槽的 SDHCI 错误刷屏
```

## 2. GNSS

文件：

```text
modbuild/wcn_bsp/Makefile
modbuild/wcn_bsp/platform/wcn_boot.c
```

修改：

```text
ccflags-y += -DFIRMWARE_PARTITION_DEBUG_EN
```

效果：

```text
gnss_load_firmware_data() 不再直接 return NULL
改为 filp_open(GNSS_FIRMWARE_PATH) 读取 gnssmodem.bin
路径来自 DTB:
    /vendor/firmware/gnssmodem.bin
initramfs 同时提供:
    /dev/block/by-name/gnssmodem_b -> /dev/loop1
    /vendor/firmware/gnssmodem.bin
```

同时保留之前的修复：

```text
GNSS 下载失败时 complete(&marlin_dev->gnss_download_done)
不再阻塞 WiFi 上电
```

## 3. WiFi regulatory / 5GHz

文件：

```text
modbuild/sprd_wlan_combo/common/cfg80211.c
modbuild/sprd_wlan_combo/sc2355/init.c
```

修改：

```text
1. sprd_init_fw() 不再强制 regulatory_hint(wiphy, "00")
   让 cfg80211 拥有 regulatory domain

2. 去掉 REGULATORY_DISABLE_BEACON_HINTS
   允许 beacon country IE 更新域

3. 保留 sc2355_reg_notify
   继续把 cfg80211 的 rules 下发给 firmware
```

参考项目：

```text
research/uwe5622-5ghz-fix
```

## 4. 蓝牙 SDIO tty

新增：

```text
modbuild/sprdbt_tty/sprdbt_tty.ko
```

来源：

```text
research/bsp_module_wcn_jing/bluetooth/driver/tty-sdio
```

已完成 6.18 API 适配：

```text
tty_operations 签名
tty_alloc_driver / tty_driver_kref_put
platform remove 返回值
proc_ops
ktime_get_real_ts64 / rtc_time64_to_tm
wakeup_source_register / unregister
```

新增：

```text
无 DT 时自注册 platform_device("mtty")
无 sprd,name 时默认 name = "ttyBT"
```

依赖：

```text
wcn_bsp.ko 的导出符号
```

## 5. 模块与脚本

test32 initramfs 新增：

```text
sprdbt_tty.ko
sprd_pmic_wdt.ko
mali_kbase.ko
modules/modem/*.ko
```

新增用户脚本：

```text
/usr/sbin/t508n-load-modem   依次加载 modem / SIPC / SIPA 模块
/usr/sbin/t508n-load-wdt     加载 sprd_pmic_wdt.ko
/usr/sbin/t508n-load-mali    加载 mali_kbase.ko
```

自动加载：

```text
sprd_pmic_wdt.ko  仅保留文件，不自动加载（依赖 sbuf，需先跑 t508n-load-modem）
mali_kbase.ko     仅保留文件，不自动加载
sprdbt_tty.ko     自动加载，timeout 20s
```

## 6. 产物

```text
Image.lk
    md5 = 48fa3257e0cf97eba63623f1fa8859cb

boot_mainline_6.18.55_t508n_test32.img
    md5 = f762aaa469cedb2f89def5a501d1d74d

SPD:
    E:\Download\ums9620_tcl_t508n_magic64\10-刷B槽-test32.bat
```

## 7. 上机后验证清单

```text
1. dmesg | grep -iE "mmc1|sdio_sd"
   期望不再刷 SD 卡槽错误

2. dmesg | grep -i gnss
   期望看到 gnss_load_firmware_data / open image file

3. iw reg get
   期望不再固定 country 00: DFS-UNSET

4. insmod sprdbt_tty.ko
   ls /dev/ttyBT*
   cat /sys/class/rfkill/*

5. /usr/sbin/t508n-load-modem
   dmesg | grep -i "Modem Alive"
   /usr/sbin/t508n-load-wdt
   /usr/sbin/t508n-load-mali
```
