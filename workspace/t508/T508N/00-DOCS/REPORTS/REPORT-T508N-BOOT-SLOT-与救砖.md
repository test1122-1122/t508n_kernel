# T508N 启动槽位、fastboot 回退与救砖报告

> 设备：TCL T508N / OT5，Unisoc UMS9620（T770）
> 日期：2026-10-09
> 结论先行：T508N 的 A/B 选择由 `misc` 0x800 的 `bootloader_control` 决定，
> 不是由 `boot_a/boot_b` 决定。B 槽被标记为 unbootable 时，fastboot/download
> 模式会 reset 并回退 A 槽。已按 F50 `set_active` 语义重打包
> `misc_slot_a.img` / `misc_slot_b.img`，可不依赖 fastboot 切换槽位。

---

## 1. 真机 U-Boot 日志

```text
ANDROID: bootable slot 0 priority: 15, tries_remaining: 7,
         verity_corrupted: 0, successful_boot: 1
ANDROID: unbootable slot 1 tries_remaining: 0, verity_corrupted: 0
ANDROID: Booting slot_a
get boot mode in boot func array[0]
enter boot mode g_mode_str[3]:DOWNLOAD_MODE
...
SECUREBOOT_ENABLE!
get partition slot: uboot_b
download slot: uboot_b
SECUREBOOT_ENABLE!
get partition slot: uboot_a
download slot: uboot_a
write default USBMUX config flag success
before reboot ,send ack(1)
[reset_cpu]: rebooting
fastboot type boot count 6
```

解释：

```text
A 槽：可启动，priority=15，tries=7，successful_boot=1
B 槽：unbootable，tries=0
fastboot/download 模式失败 -> reset -> 回退 A
```

---

## 2. `misc` bootloader_control 结构

```text
分区：misc
偏移：0x800
结构：bootloader_control
```

当前 A 槽元数据：

```text
0x800: 5f 61 00 00 42 43 41 42 01 02 00 00 ff 00 0f 00
0x810: 00 00 00 00 00 00 00 00 00 00 00 00 96 b1 8d 84
```

解析：

| 字段 | 值 |
|---|---|
| slot_suffix | `_a` |
| magic | `BCAB` |
| A slot_info | priority 15, tries 7, successful 1 |
| B slot_info | priority 15, tries 0, successful 0 |
| CRC32 | `0x848db196`（小端 `96 b1 8d 84`） |

CRC 已用 `crc32(meta[:0x1c])` 验证一致。

---

## 3. F50 `set_active` 参考实现

源码位置：

```text
t508/work/srag-refs/zte-f50-toolkit/mac-f50/w_spd_dump/common.c
```

核心逻辑：

```c
uint32_t tmp[8] = { 0x5F, 0x42414342, 0x201, 0, 0, 0, 0, 0 };
bootloader_control *abc = (bootloader_control *)tmp;
int slot = *arg - 'a';

abc->slot_info[slot].priority = 15;
abc->slot_info[slot].tries_remaining = 6;
abc->slot_info[slot].successful_boot = 0;
abc->slot_info[1 - slot].priority = 14;
abc->slot_info[1 - slot].tries_remaining = 1;
abc->slot_suffix[1] = *arg;
abc->crc32_le = crc32(0, abc, 0x1C);

w_mem_to_part_offset(io, "misc", 0x800, abc, 0x20, 0x1000);
```

`slot_metadata` 2 字节：

```text
byte0: priority:4 | tries_remaining:3 | successful_boot:1
byte1: verity_corrupted:1 | reserved:7
```

选择顺序：

```text
priority -> successful_boot -> tries_remaining
```

---

## 4. 重打包的 misc 槽位镜像

| 镜像 | slot | A 元数据 | B 元数据 | CRC32 |
|---|---|---|---|---|
| `misc_slot_a.img` | `_a` | priority 15, tries 6 | priority 14, tries 1 | `0xc5eabfe6` |
| `misc_slot_b.img` | `_b` | priority 14, tries 1 | priority 15, tries 6 | `0x7010e29e` |

`misc_slot_b.img` 0x800：

```text
5f 62 00 00 42 43 41 42 01 02 00 00 1e 00 6f 00
00 00 00 00 00 00 00 00 00 00 00 00 9e e2 10 70
```

Windows 一键包：

```text
E:\Download\ums9620_tcl_t508n_magic64\
```

脚本：

```text
06-切换到B槽-mainline.bat
07-切换到A槽-openwrt.bat
```

两者都通过 SPD 写 `misc`，不依赖 fastboot。

---

## 5. 救砖与恢复

### 5.1 BROM 永久可用

BootROM 是掩膜 ROM，BROM/SPD 始终可用。主要工具与镜像：

| 用途 | 路径 |
|---|---|
| 完整原厂 Android dump | `E:\rootphone\t508n\savebrock\flash_all_spd_dump\` |
| 原厂 Android 镜像 | `E:\rootphone\t508n\savebrock\Image\` |
| 原解锁工具 | `E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2\` |
| 原机 SPL/U-Boot/boot 备份 | `E:\rootphone\t508n\2unlock\` |
| magic64/槽位一键包 | `E:\Download\ums9620_tcl_t508n_magic64\` |

### 5.2 恢复 Android 启动链

```text
write_part boot_a boota.img
write_part boot_b bootb.img
write_part dtbo_a dtbo_a.img
write_part dtbo_b dtbo_b.img
write_part vbmeta_a vbmeta_a.img
write_part vbmeta_b vbmeta_b.img
write_part misc misc.img
reset
```

若仍 `no valid os found`，用完整 stock dump 写：

```text
uboot_a/b, spl_a/b, vendor_boot_a/b, init_boot_a/b,
dtb_a/b, dtbo_a/b, 全部 vbmeta_*
```

### 5.3 恢复 U-Boot

```text
write_part uboot_a uboot_a_stock.img
write_part uboot_b uboot_b_stock.img
reset
```

或：

```text
E:\Download\ums9620_tcl_t508n_magic64\05-恢复原厂uboot.bat
```

### 5.4 切换槽位

```text
06-切换到B槽-mainline.bat  -> misc_slot_b.img
07-切换到A槽-openwrt.bat   -> misc_slot_a.img
```

---

## 6. 当前已知良好状态（2026-10-09）

```text
uboot_a / uboot_b = magic64 v1
boot_a            = OpenWrt 5.4 auto15
boot_b            = mainline 6.18.55 test5（已写入，ANDROID! 校验通过）
userdata p75      = OpenWrt ext4 rootfs
misc              = A 槽元数据；BCB 区可能有 boot-fastboot 字符串
```

实机验证：

```text
Linux 5.4.210-android12-9-ga1d5c173d86b-ab1+
OpenWrt 25.12.5 r33051-f5dae5ece4
/dev/mmcblk0p75 -> /persist
/etc /usr /www /lib /root bind 到 p75
uhttpd / dnsmasq / hostapd / ubus 正常
t508n-gnssd 运行，/dev/data0_gnss 存在
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
```

---

## 7. 下一步

```text
1. BROM 运行 06-切换到B槽-mainline.bat
2. 观察 mainline 6.18.55 test5 启动
3. 失败则运行 07 切回 A 槽 OpenWrt
4. 成功后继续 mainline USB / WLAN / BT / GNSS 适配
```
