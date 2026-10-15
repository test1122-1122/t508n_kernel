# FLASH-20261005-SGI-IPI-BOOT-A — 2026-10-05 写入 boot_a 记录

> 设备：TCL T508N / OT5，序列号 `T6PA03CJ3EH02ZW`
> 目标：把 2026-10-05 打包的 6.12.103 SGI/IPI 计数诊断内核写入 `boot_a`
> 约束：b 槽官方系统保留；不碰 `trustos_a` / `teecfg_a`；一次一个变量。

---

## 1. 写入前状态

### 1.1 ADB 在线

`adb devices -l`：

```text
T6PA03CJ3EH02ZW  device product:OT5_CM_FIH model:T508N device:OT5
```

`adb shell getprop ro.boot.slot_suffix`：

```text
_a
```

即当前槽 = **a 槽**，写 `boot_a` 符合“a 槽试验、b 槽保留官方”。

### 1.2 待写镜像

```text
C:\platform-tools\boot_linux_t508n_latest.img
```

| 项 | 值 |
|---|---|
| 大小 | 67,108,864 B（64 MB） |
| md5 | `97231132ccd1c58188c18458055de677` |
| 内嵌 kernel md5 | `6ffcd2d602e67f3f6d20d3429d8ac6c2` |
| 内嵌 initramfs cpio md5 | `a0dfd66a71355333b36a228baae4306a` |

---

## 2. SPD/SP 下载模式探测（已连接，但只读）

Windows 侧 `spd_dump.exe` 曾成功连过一次 SP 下载模式：

```text
Successfully connected to port: 8
CHECK_BAUD bootrom
BSL_REP_VER: "SPRD3"
...
Reading Partition List
Read Part Done: user_partition+0x0, target: 0x8000, read: 0x8000
```

当前设备分区表回读确认：

```text
boot_a        64MB
boot_b        64MB
vendor_boot_a 100MB
vendor_boot_b 100MB
trustos_a     6MB
teecfg_a      1MB
```

只读探测命令末尾带 `reset`，设备随后重启回 Android；后续 `adb shell reboot download`
没有进入 FDL/SP 下载模式，而是正常重启回 Android。因此 SPD 工具这次**未用于写 boot_a**。

---

## 3. 最终实际写入路径：Windows fastboot

因 `reboot download` 未进入 LK FDL 模式，改用项目约束允许的 fastboot 通道：

```text
adb reboot bootloader
fastboot getvar current-slot   -> current-slot: a
fastboot flash boot_a C:\platform-tools\boot_linux_t508n_latest.img
fastboot reboot
```

fastboot 输出：

```text
Sending 'boot_a' (65536 KB)                        OKAY [  1.742s]
Writing 'boot_a'                                   OKAY [  0.415s]
Finished. Total time: 2.167s
Rebooting                                          OKAY [  0.000s]
```

写入分区：**仅 `boot_a`**。  
未写：`boot_b`、`vendor_boot_a/b`、`dtbo_a/b`、`uboot_a/b`、`trustos_a/b`、`teecfg_a/b`。

---

## 4. 写入后设备状态（未获得 ADB/fastboot）

重启后约 8 秒：

```text
adb devices   -> 无设备
fastboot devices -> 无设备
Windows PnP   -> 无 VID_1782 的 ADB/Fastboot/Download 设备
```

这与“mainline 6.12 在 CPU1 `psci_cpu_on(1)` 处挂死、USB 未起来”的预期一致；  
但不能仅凭 USB 消失判断内核是否执行到某处，必须以 LCD 探针为准。

---

## 5. 下一步要读的 LCD 探针坐标

| y | 含义 | 期望/判读 |
|---|---|---|
| 20 | stage 进度条 0–9 | 当前已知稳定卡点下预期 0–8 亮、9 不亮 |
| 60 | tick 状态 | 长条=正常 tick，中条=无 tick |
| 100 | EL1/2/3 | 正常应 EL1 |
| 130 | 8 核 bringup 完成 | 当前已知卡点下预期不亮 |
| 140 | 全局 IPI 发送计数 | 块 0–7 = RESCHEDULE 发送低 8 位；块 8–15 = CALL_FUNC 发送低 8 位；x=8 起 |
| 170 | CPU0 诊断行 | x=160 起：块 0–7 rx 低 8 位；块 8 delta；块 9 心跳 |
| 190 | CPU1 诊断行 | 同上；注意此 y 既有 x=8 的 boot_secondary 探针 |
| 230/250/270/290/310/330 | CPU2–CPU7 诊断行 | 同上，x=160 起 |
| 175/180/185 | 现有每核启动探针 | CPU1 正常启动时会亮 |
| 190/195 | 现有 SMC 返回/超时探针 | SMC 返回后会亮 |
| 210/220/400/420/440/460 | 后续阶段 | 当前已知卡点下预期不亮 |

判读建议：

1. 若仍为“stage 0–8 + 仅 y=175 CPU1”，则与 2026-08-21 结论一致：卡在 `psci_cpu_on(1)`。
2. 观察 y=140 全局发送计数是否变化；若 CPU0 进 SMC 前有 IPI 流量，可能先亮后冻结。
3. 观察 CPU0 行 y=170 的块 9 心跳是否先亮后冻结。
4. 当前卡点下 CPU1–7 行 y=190/230/… 预期全黑。

---

## 6. 结论状态

- `boot_a` 写入：**已执行，fastboot 回报 OKAY**。
- 是否成功启动到新内核：**未验证**，需要 LCD 读屏。
- b 槽官方系统：**未触碰**。
- TEE 分区：**未触碰**。
- SGI/IPI 计数层：**已随内核写入 boot_a**，但尚未读屏/未验证。

---

## 7. 首次 LCD 观测（用户口述，2026-10-05）

用户读屏原话：

> 进入 fastboot 后重启，显示最上面 9 格，中间 1 格。

初步映射：

| 屏幕元素 | 对应探针 | 判读 |
|---|---|---|
| 最上面 9 格 | y=20，stage 0–8 亮、stage 9 暗 | `smp_init()` 未完成，与已知卡点一致 |
| 中间 1 格 | **待确认**：可能是 y=175 CPU1 `__cpu_up(1)` 探针；也可能是 x=160 起的 CPU0/Y=170 诊断心跳块 | 需确认 x 位置 + 是否闪烁 |

与 2026-08-21 已知读数（stage 0–8 + 仅 y=175 CPU1）高度一致，但仍需区分：

- **静态 1 格、靠左 x≈8、y≈175** → 现有 `__cpu_up(1)` 探针；说明 CPU0 卡在 `boot_secondary(1)` 内。
- **闪烁 1 格、靠右 x≈160–220、y≈170** → 本次 SGI/IPI 诊断的 CPU0 心跳块；说明 timer 心跳有在刷新。
- 若两者都存在，应能看到左侧静态块 + 右侧闪烁块，不应只有 1 格。

因此当前结论：

- `boot_a` 已写入成功（fastboot OKAY）。
- 屏幕已进入 mainline 6.12 早期探针画面，不是官方 Android。
- “9 格 + 1 格”确认了 **未进入 8 核 bringup 完成以后**；但单格身份尚未 100% 确认。
- SGI/IPI 计数层是否刷新、CPU0 心跳是否存在：**未验证**。

---

## 8. 首次观测后的修正：心跳首帧门 bug（2026-10-05 19:0x）

首次观测“最上面 9 格 + 中间 1 格”与 2026-08-21 相同，说明本次 SGI/IPI 诊断行
（y=140 全局发送、y=170/190/... 每核行）没有刷新。排查代码发现：

```c
if (!time_after_eq(now, *last + T508N_IPI_HB_JIFFIES))
    return;
```

早期 `jiffies = INITIAL_JIFFIES = (unsigned long)(-300*HZ)`，`*last=0` 时
`now` 与 `16` 的带符号差为负，`time_after_eq()` 为假；要等 jiffies 回绕约 5 分钟后
才会画第一帧。早期 bring-up 卡死时根本到不了回绕点。

修正：

```c
if (!this_cpu_read(t508n_ipi_hb_seen)) {
    this_cpu_write(t508n_ipi_hb_seen, true);  /* 每核首次强制放行 */
} else if (!time_after_eq(now, *last + T508N_IPI_HB_JIFFIES)) {
    return;
}
```

修正后重新构建并打包：

| 项 | 值 |
|---|---|
| kernel Image md5 | `82c43c3f22215508158fffd1c18168ad` |
| boot_linux.img md5 | `634019e8ffdddee33aa13a1692fd87d1` |
| initramfs cpio md5 | `a0dfd66a71355333b36a228baae4306a` |
| 首次刷入的旧镜像备份 | `work/boot_linux_sgi_hbgate-bug.img` |
| 修正版 Windows 路径 | `C:\platform-tools\boot_linux_t508n_latest.img` |

已启动后台 fastboot 自动等待刷写器：

```text
script: work/wait_fastboot_flash_boota.sh
log:    work/wait_fastboot_flash_boota.log
behavior: 检测到 fastboot 设备 -> 读 current-slot -> 仅当 slot=a 时 flash boot_a -> reboot
```

修正版尚未刷入；等待设备进入 fastboot。

---

## 9. 修正版通过 SPD 工具写入 boot_a（2026-10-05 19:0x）

设备进入 SP 下载模式后，最终按用户要求使用 Windows 侧 `spd_dump.exe` 写入修正版：

```text
Successfully connected to port: 8
BSL_REP_VER: "SPRD3"
EXEC FDL2
Reading Partition List
Total number of partitions: 75
Storage is emmc
Device is using slot a
file size : 0x4000000
[========================================] 100.0%
Write Part Done: boot_a, target: 0x4000000, written: 0x4000000
```

关键点：

- 只写 `boot_a`；
- 写入大小 `0x4000000` = 64 MB，与 `boot_a` 分区一致；
- 工具明确报告 `Device is using slot a`；
- 未写 `boot_b`、`vendor_boot_a/b`、`dtbo_a/b`、`uboot_a/b`、`trustos_a/b`、`teecfg_a/b`；
- 写入镜像为修正版：kernel md5 `82c43c3f22215508158fffd1c18168ad`，
  boot image md5 `634019e8ffdddee33aa13a1692fd87d1`；
- 末尾带 `reset`，设备应已重启进入修正版内核。

### 9.1 这次要读的新增探针

| 坐标 | 含义 |
|---|---|
| y=20 | stage 0–9 |
| y=140 | 全局 IPI 发送计数，x=8 起；块 0–7 RESCHEDULE，块 8–15 CALL_FUNC |
| y=170 | CPU0 诊断行，x=160 起；块 9 心跳应约 250 ms 闪烁 |
| y=190 | CPU1 诊断行，x=160 起 |
| y=230/250/270/290/310/330 | CPU2–CPU7 诊断行 |
| y=175/180/185/190/195 | 既有每核启动/SMC 返回/超时探针 |

当前状态：**修正版已通过 SPD 工具写入 boot_a；是否进入新内核/新增行是否显示，待 LCD 读屏验证。**

---

## 10. 修正版 fastboot 重刷成功（2026-10-05 19:05）

用户进入 fastboot 后，后台等待刷写器自动检测到设备并完成重刷：

```text
[2026-10-05T19:05:43+08:00] fastboot device: T6PA03CJ3EH02ZW  fastboot
[2026-10-05T19:05:43+08:00] current-slot: 'a'
[2026-10-05T19:05:43+08:00] flashing boot_a from C:\platform-tools\boot_linux_t508n_latest.img
Sending 'boot_a' (65536 KB)   OKAY [  1.713s]
Writing 'boot_a'              OKAY [  0.413s]
Finished. Total time: 2.136s
[2026-10-05T19:05:45+08:00] flash_exit=0
[2026-10-05T19:05:45+08:00] rebooting
```

- 重刷镜像：固定心跳首帧后的 `boot_linux.img`，md5 `634019e8ffdddee33aa13a1692fd87d1`
- 仅刷 `boot_a`；slot=a；b 槽与 TEE 分区未触碰。
- 至此同一修正版镜像已由两条 Windows 侧通道写入过 `boot_a`：
  - SPD/`spd_dump`：`Write Part Done: boot_a, target: 0x4000000, written: 0x4000000`
  - fastboot：`Sending 'boot_a' (65536 KB) OKAY` / `Writing 'boot_a' OKAY`
- 设备已 `reboot`，等待 LCD 读屏验证新增 SGI/IPI 行。
