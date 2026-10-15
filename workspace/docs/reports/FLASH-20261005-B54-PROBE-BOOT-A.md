# FLASH-20261005-B54-PROBE-BOOT-A — 写入 5.4 复刻 probe 到 boot_a

> 设备：TCL T508N / OT5，序列号 `T6PA03CJ3EH02ZW`
> 目标：按用户选择 B，将 `boot_54custom_probe.img`（5.4.210 复刻 + 最小 initramfs）
>       写入 `boot_a`，作为 mainline 6.12 卡点后的 5.4 阳性对照/稳定性复测。
> 约束：b 槽官方系统保留；不碰 trustos/teecfg；只写 boot_a。

---

## 1. 待刷镜像

```text
C:\platform-tools\boot_54custom_probe.img
```

| 项 | 值 |
|---|---|
| 大小 | 67,108,864 B（64 MB） |
| md5 | `bdbc0a04096058be88fe1b728f51b0f7` |
| kernel | 31,621,632 B（5.4.210 + OEM mods + framebuffer probe） |
| ramdisk | 882,734 B 容器（最小 busybox initramfs） |

---

## 2. 实际刷入：Windows fastboot 自动等待刷写器

```text
[19:07:50] fastboot device: T6PA03CJ3EH02ZW fastboot
[19:07:50] current-slot: 'a'
[19:07:50] flashing boot_a (B: boot_54custom_probe.img)
Sending 'boot_a' (65536 KB)   OKAY [  1.757s]
Writing 'boot_a'              OKAY [  0.413s]
Finished. Total time: 2.180s
[19:07:52] flash_exit=0
[19:07:52] rebooting
```

- 只写 `boot_a`；
- `current-slot: a`；
- 未写 `boot_b`、`vendor_boot_a/b`、`dtbo_a/b`、`uboot_a/b`、
  `trustos_a/b`、`teecfg_a/b`；
- 镜像 md5 `bdbc0a04096058be88fe1b728f51b0f7`。

日志：

```text
/home/test/t508n/t508/work/wait_fastboot_flash_boota_b54.log
```

---

## 3. 5.4 复刻 probe 的 LCD 布局（本次读屏依据）

来自 `REPORT-54-probe-port.md`：

| y | 含义 |
|---|---|
| 20 | 进度条 10 格，x=8+n*30，stage 0–9 |
| 60 | driver_init 前段：devices_init(8) classes_init(44) hypervisor_init(80) of_core_init(116) + mark_row(60,1) |
| 100 | driver_init 后段：platform_bus_init(8) cpu_dev_init(44) memory_dev_init(80) container_dev_init(116) + mark_row(100,2) |
| 140 | memory_dev_init 内部：block_sz 计算后(8) subsys 注册后(44) block 循环完成(80) |
| 155 | memory block 循环计数：x=44+n*10，每 4 block 一格，最多 8 格 |
| 180 | do_basic_setup：cpuset_init_smp(8) driver_init 后(44) init_irq_proc(80) do_ctors(116) usermodehelper_enable(152) |
| 400 | do_initcalls 完成(8) + mark_row(400,6) |
| 460 | exec init 前(8) + mark_row(460,9) |

**正常满格路径（历史已验证）**：

```text
stage 0–9 全亮 + y=400 亮 + y=460 亮 = boot 到 initramfs /init
```

当前状态：**B 镜像已写入 boot_a 并 reboot；LCD 读屏结果待用户反馈。**
