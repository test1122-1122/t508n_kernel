# REPORT-V50ALIGN-BOOT-20261005 — T508N boot 镜像 V50/mu300-linux 经验对齐版

> 目标：按 `docs/cross/REPORT-mu300-linux-对T508N价值评估.md` §10.1 的 4 条 boot 镜像硬约束，
> 制作一版“经验对齐”的 5.4 复刻 probe 镜像，用于与现有 `boot_54custom_probe.img` 做单变量对照。
> 只做镜像，不刷机；刷入仍是后续单独步骤。

---

## 1. 对照基线

| 项 | 现有 B（`boot_54custom_probe.img`） | V50align（本版） |
|---|---|---|
| kernel | 5.4 复刻 probe，md5 `7645bd208c758523abdfe312e5e1b57c` | 同一个 kernel，md5 相同 |
| initramfs cpio | 最小 busybox，md5 `a0dfd66a71355333b36a228baae4306a` | 同一个 cpio，md5 相同 |
| 总大小 | 67,108,864 B | 67,108,864 B |
| boot image md5 | `bdbc0a04096058be88fe1b728f51b0f7` | `ec500f4dca30ea088cdf97025a4a8e62` |
| sha256 | — | `de2ab49bc7107013768b12e8cc91b7d59ae5fde476c6f6fd316298e1dbe77c23` |

---

## 2. V50 §10.1 四条硬约束落实情况

| V50 约束 | V50align 实现 | 证据 |
|---|---|---|
| 复用原厂 boot header v4 布局 + 原 AVB footer | 以 `firmware/stock-images/boot_a.img` 为 base，原 header page + 原 vbmeta blob + 原 AVB footer | `avbtool info_image` 公钥 sha1 `ea410c1b46cdb2e40e526880ff383f083bd615d5`（原厂），vbmeta size 2304 |
| `signature_size = 0` | 显式 `struct.pack_into('<I', hdr, 1580, 0)` | 回读 offset 1580 = 0 |
| ramdisk 必须 LZ4 legacy（`0x184C2102`，8 MiB 块） | `lz4_legacy()`：magic + u32 block_len + raw LZ4 block；本 cpio 1 块 | 回读 magic `0x184c2102`，block_len `882722`，解出 cpio md5 相同 |
| 总长 == 原厂分区镜像长度；不越持久日志/预留区 | 输出 67,108,864 B；`original_image_size` 32,514,048 < 48 MiB guard | `avbtool info_image` Image size 67108864；manifest `original_image_size` |

> 注意：本版**故意保留原厂 AVB footer 的旧 hash descriptor**（与 V50 相同思路）。
> 设备必须处于已解锁/已绕过 boot 校验状态；锁定且强校验 AVB 的设备不能使用。
> 本机历史刷机均为解锁态，当前 B 镜像也使用自签 footer 能启动；V50align 是否被 T508N LK
> 接受，需要一次单独上机验证。

---

## 3. 产物

```text
t508/build/boot-images/boot_54custom_probe_v50align.img
t508/build/boot-images/boot_54custom_probe_v50align.json
C:\platform-tools\boot_54custom_probe_v50align.img
```

构建脚本：

```text
t508/tools/build_boot_v50align.py
```

构建输入：

```text
--stock-boot   firmware/stock-images/boot_a.img
--kernel       work/boot_54custom_probe_kernel.bin      (从已刷 B 镜像提取，保证 kernel 不变)
--ramdisk-cpio work/boot_54custom_probe_ramdisk.cpio    (与 B 相同 cpio)
```

---

## 4. 验证状态

已完成（离线）：

- header v4、`kernel_size`/`ramdisk_size` 正确；
- `signature_size=0`；
- LZ4 legacy magic + 单块长度正确，解出 cpio 与输入 md5 一致；
- 输出总长等于原厂 `boot_a.img` 67,108,864 B；
- 原厂 AVB footer 回读：公钥 sha1 与原厂一致，vbmeta 2304 B；
- `original_image_size` 32,514,048 B，未越过 48 MiB guard。

未完成（真机）：

- 未刷入；
- 未验证 T508N LK 是否接受“原厂旧 hash descriptor + 新 kernel/ramdisk”的组合；
- 未验证 `boot_54custom_probe_v50align.img` 是否能到 initramfs `/init`。

---

## 5. 建议验证顺序（后续单变量）

1. 先确认当前 `boot_54custom_probe.img` 的 LCD 基线是否正常（stage 0–9 + y=400 + y=460）；
2. 再单独刷入 `boot_54custom_probe_v50align.img` 到 `boot_a`；
3. 读屏同样看 5.4 probe 布局；
4. 若启动正常 → 证明 V50 boot 打包经验可用于 T508N；
5. 若卡在 header/AVB 解析或 ramdisk 解压，则回退到现有 `boot_54custom_probe.img`，并把差异缩小到
   “LZ4 帧格式”或“原厂 footer”单独一项，继续单变量。

---

## 6. 首次真机刷入记录（2026-10-05 19:13，fastboot）

```text
[19:13:06] fastboot device: T6PA03CJ3EH02ZW fastboot
[19:13:06] current-slot: 'a'
[19:13:06] flashing boot_a (V50align)
Sending 'boot_a' (65536 KB)   OKAY [  1.731s]
Writing 'boot_a'              OKAY [  0.413s]
Finished. Total time: 2.155s
[19:13:08] flash_exit=0
[19:13:08] rebooting
```

- 只写 `boot_a`；
- `current-slot: a`；
- 未写 b 槽、vendor_boot、dtbo、uboot、trustos、teecfg；
- SPD 等待进程随后已主动停止，避免误触发。

当前状态：**V50align 已刷入 boot_a 并 reboot；LCD 结果未读，未验证启动是否成功。**

---

## 7. 首次 LCD 观测（用户口述，2026-10-05）

用户读屏原话：

> 10 4 4 3 5 最下面两行1 1

初步映射：

| 读数 | 探针 | 判定 |
|---|---|---|
| `10` | y=20 stage 0–9 | **10/10 全亮**，smp_init/do_basic_setup 均完成 |
| `4` | y=60 driver_init 前段 | 4/4 亮 |
| `4` | y=100 driver_init 后段 | 4/4 亮 |
| `3` | y=140 memory_dev_init 内部 | 3/3 亮 |
| `5` | y=155 或 y=180 之一 | 待用户确认是 memory block 计数还是 do_basic_setup 5 格 |
| `1 1` | y=400 / y=460 | 最下面两行各 1 个主标记亮 = do_initcalls 完成 + exec init 前 |

若 `5` 是 y=180、且 y=400/y=460 确实为主标记，则这版 **V50align 已走到 5.4 probe 的正常满格路径**：

```text
stage 0–9 全亮 + y=400 亮 + y=460 亮
= boot 到 initramfs /init
```

即：T508N LK 接受了“原厂 AVB footer + 新 kernel/ramdisk + 真实 LZ4 legacy 帧”的组合，
V50 §10.1 的 boot 打包经验至少在首次启动上未被拒绝。

仍需确认的一点：

- `4 4 3 5` 是 `y=60/y=100/y=140/y=155`，还是 `y=60/y=100/y=140/y=180`？
- y=155 memory block 计数与 y=180 do_basic_setup 是两行不同探针，避免误读。

未验证项：

- 当前只是屏幕探针到 `/init`，没有 UART/ADB 日志；
- 未验证 initramfs 内 `INIT OK` 后的持续运行状态；
- 未验证长时间稳定性与重启行为。

---

## 8. 真机验证结论（2026-10-05，用户确认）

用户确认：

> y=400 / y=460 稳定常亮

结合 stage 0–9 全亮，可判定：

```text
V50align 镜像在 T508N 上进入 5.4 probe 的正常满格路径：
  stage 0–9 全亮
  y=400 亮（do_initcalls 完成）
  y=460 亮（run_init_process 前）
```

由此确认：

1. T508N LK 接受 **原厂 boot header v4 + 原厂 AVB footer** 的布局；
2. T508N LK/内核接受 **真实 LZ4 legacy 帧**（magic `0x184C2102` + block_len + raw LZ4 block）；
3. `signature_size=0` 与“总长 == 原厂 64 MiB”没有破坏启动；
4. V50 §10.1 的四条 boot 打包硬约束，**首次真机启动路径验证通过**。

### 8.1 严格保留的未验证项

`y=460` 由 `run_init_process()` 在 `kernel_execve(init_filename, ...)` **之前**绘制，因此它证明：

```text
内核走到“即将 exec /init”
```

但**不直接证明** `/init` 已经成功执行。最小 initramfs 的 `init` 脚本会：

- 关 WDT；
- 在屏幕最上方 `y=10..13` 画 4 条全宽白线；
- 向 `/dev/kmsg` 打印 `INIT OK`。

所以更严格确认 `/init` 已经跑起来，需要用户补看：

```text
屏幕最上方、stage 进度条上方，是否有一条/一组全宽白色横条？
```

若该全宽白条存在，则可升级为：

```text
initramfs /init 用户态已执行
```

当前结论：**V50 经验对齐版 boot 打包路径真机验证通过；/init 用户态执行待“全宽白条”确认。**

---

## 9. “无全宽白条”的解释与结论边界

用户补充：

> 没有全宽白条

这是因为 5.4 复刻内核配置里：

```text
# CONFIG_DEVMEM is not set
```

而最小 initramfs 的 `/init` 画白条依赖：

```sh
devmem 0xb0000000+... 32 0xffffffff
```

Busybox `devmem` 需要 `/dev/mem`；`CONFIG_DEVMEM=n` 时 `/dev/mem` 不存在，这些写操作必然失败。
因此：

```text
没有全宽白条 ≠ /init 没有执行
```

它只说明：

- `devmem` 用户态画屏路径不可用；
- `/init` 的屏幕反馈设计在当前 5.4 config 下本来就失效。

5.4 config 里同时有：

```text
CONFIG_PSTORE_CONSOLE=y
CONFIG_PSTORE_PMSG=y
CONFIG_PSTORE_RAM=y
```

如果 `/init` 确实执行并成功写 `/dev/kmsg`，理论上 "T508N init started" / "INIT OK"
可能落在 pstore console；但要读 pstore 需要回到官方 Android 或 USB 通道，当前不可用。

### 9.1 最终边界

已真机验证：

```text
V50align 镜像启动链路：stage 0–9 + y=400 + y=460 稳定常亮
= 内核到达 run_init_process / kernel_execve 前
= V50 §10.1 四条 boot 打包硬约束被 LK/内核接受
```

仍未直接验证：

```text
initramfs /init 用户态是否成功执行
```

当前最小 initramfs 的屏幕反馈本身不可用（`CONFIG_DEVMEM=n`），所以不能靠白条判定。
若要闭合 `/init` 验证，应作为下一条单变量：

1. 给 5.4 复刻打开 `CONFIG_DEVMEM=y`（须核对 LOCALVERSION 与原厂模块兼容性）；或
2. 改用能通过 `/dev/fb0`/其他不依赖 `/dev/mem` 的方式画屏；或
3. 回官方系统读 pstore console，检查 `T508N init started` / `INIT OK`。
