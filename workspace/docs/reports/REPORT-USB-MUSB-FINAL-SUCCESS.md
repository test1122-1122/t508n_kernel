# REPORT-USB-MUSB-FINAL — T508N MUSB 虚拟串口打通

日期：2026-10-06
状态：成功，Windows 识别为 COM3

## 最终成功镜像

```text
build/boot-images/boot_54openwrt_musb_final6.img
boot md5   = d07b3e805a6694431e7fb7a480a151f4
kernel md5 = 04d9e1fa4887d4590b8307e92fb129c5
```

vendor_boot_a / dtbo_a 使用原厂 stock。

## 启动链路

```text
62 个 vendor 模块按 MU300 相对顺序加载
→ sprd-hsphy 绑定，PHY 上电
→ 去掉 MUSB 对 DWC3 probe finish 的依赖
→ MUSB musb-hdrc.1.auto UDC 出现
→ configfs t508n + acm.GS0 绑定
→ usb_data_enabled 0→1
→ t508n_connect → usb_phy_init()
→ sprd_hsphy_init 强制 AON-SYS USB20 mux = 0（MUSB）
→ 主机枚举 CDC ACM，Windows COM3
```

## 三个关键修复

1. MUSB 不再等 DWC3：
   `musb_sprd.c` 删除 `dwc3_sprd_driver_probe_finish()` gate。

2. 不走 Android charger-only：
   `boot_charging = 0`，并强制 work 的 `vbus_active=1 / usb_data_enabled=1 / current_state=1`。

3. USB20 mux 切到 AON-SYS：
   `phy-sprd-qogirn6pro.c` 的 `sprd_hsphy_init()` 无条件写
   `REG_AON_APB_AON_SOC_USB_CTRL, MASK_AON_APB_USB20_CTRL_MUX_REG = 0`，
   包括 already-inited 快速路径，由 S50 的 `t508n_connect` 在 configfs 绑定后触发。

## 使用

Windows COM3，115200 8N1，进入 OpenWrt shell。

## 待办

- 清理 S99 重复起 shell 的逻辑，保留 S50 唯一 shell。
- 验证 OpenWrt 完整性、模块、UDC 状态。
- 下一步：eMMC 持久化 / 网络（RNDIS/ECM 或 Wi-Fi）。


---

## 2026-10-06 添加：eMMC 持久化成功

### 结论

USB ACM COM3 + eMMC p75 持久化均已打通。

### 做法

- 保留 initramfs 启动路径（USB/MUSB/mux 修复不变）。
- 在 `/init` 中：

```text
mount /dev/mmcblk0p75 → /persist
bind mount:
  /persist/etc  → /etc
  /persist/root → /root
  /persist/usr  → /usr
  /persist/www  → /www
  /persist/lib  → /lib
exec /sbin/init
```

- 不再 switch_root，不再复制根到 tmpfs；procd 直接跑在 initramfs 根上，p75 提供持久目录。
- 自动恢复 watchdog：

```text
若 90s 内 /dev/ttyGS0 未出现
→ /sbin/t508n-reboot bootloader
→ 自动进 fastboot
```

- 自动 COM3 → fastboot 已验证：

```text
/sbin/t508n-reboot bootloader
→ fastboot devices 在 5~40s 内出现
```

### 持久化验证

```text
touch /etc/persist_test
reboot

重启后：
cat /proc/uptime      = 28.94
ls /etc/persist_test  = 存在（时间戳保留）
mount | grep " /etc " = /dev/mmcblk0p75 ext4 rw
```

### 当前镜像

```text
build/boot-images/boot_54openwrt_bindpersist2.img
boot md5   = 822dfb81efc1709592c552f6773195ef
kernel md5 = 48686a97165a58c8229d75e43befd6a4
```

### 待清理

- S99 里重复起 shell 的逻辑应移除，只保留 S50 的 configfs + ttyGS0 shell。
- p75 的 `/etc` bind 挂载已持久；p46 (Android super, 5.5G) 可后续格式化为额外数据区。
