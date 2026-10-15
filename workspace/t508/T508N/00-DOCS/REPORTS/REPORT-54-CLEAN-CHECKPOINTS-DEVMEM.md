# REPORT-54-CLEAN-CHECKPOINTS-DEVMEM — 5.4 精简检查点 + DEVMEM=y

> 目标：
> 1. 屏幕检查点只保留 y=20 十格进度条 + y=400 + y=460；
> 2. 打开 `CONFIG_DEVMEM=y`，让最小 initramfs 的 `/init` 能用 `devmem` 画白条，
>    从而验证 `/init` 用户态是否真正执行。
> 对象：`linux-5.4-custom` 复刻树。

---

## 1. 检查点精简

删除的显示点：

| y | 原含义 | 处理 |
|---|---|---|
| 60 | driver_init 前段 4 格 | 删除 |
| 100 | driver_init 后段 4 格 | 删除 |
| 140 | memory_dev_init 内部 3 格 | 删除 |
| 155 | memory block 循环计数 | 删除 |
| 180 | do_basic_setup 5 格 | 删除 |
| 430 | /init 不存在 / prepare_namespace | 删除 |
| mark_row | 400/460 行号小方块 | 删除调用 |

保留：

| y | 含义 |
|---|---|
| 20 | stage 0–9 十格进度条（`t508n_stage`） |
| 400 | do_initcalls 完成 |
| 460 | run_init_process / exec init 前 |

修改文件：

```text
drivers/base/init.c      (driver_init 探针全部删除)
drivers/base/memory.c    (memory_dev_init 探针/计数删除)
init/main.c              (y=180、y=430、mark_row 删除；保留 y=400/460)
```

内核源码仍保留 `t508n_mark_row()` 实现，但已无调用点。

---

## 2. 打开 DEVMEM

`.config` 变更：

```diff
-# CONFIG_DEVMEM is not set
+CONFIG_DEVMEM=y
+# CONFIG_STRICT_DEVMEM is not set
```

说明：

- 仅 `DEVMEM=y` 时，olddefconfig 会自动带上 `STRICT_DEVMEM=y`；
- arm64 的 `STRICT_DEVMEM=y` 会拒绝 `/dev/mem` 访问 system RAM；
- framebuffer 物理地址 `0xb0000000` 很可能被当作 RAM / reserved RAM 处理，因此显式
  `# CONFIG_STRICT_DEVMEM is not set`，否则用户态白条仍画不出来；
- `LOCALVERSION=` 环境变量已保持为空；`CONFIG_LOCALVERSION=""`。

> 注意：`CONFIG_LOCALVERSION_AUTO=y` 未改动，构建出的 release 仍会带
> `-gc4bd9f7ae561-dirty`；若后续要严格对齐原厂 vermagic / 加载原厂模块，
> 需要另立一项禁用 LOCALVERSION_AUTO。

---

## 3. 构建与打包

构建：

```sh
cd /home/test/t508n/t508/src/linux-5.4-custom
export LOCALVERSION=
export PATH=/home/test/t508n/shared/toolchains/arm-gnu-toolchain-13.2/bin:$PATH
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- olddefconfig prepare
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- -j8 Image
```

产物：

```text
src/linux-5.4-custom/arch/arm64/boot/Image
kernel md5 = 8840d249ee3269d1c9415bae1efc9752
```

按已验证的 V50align 打包：

```sh
python3 tools/build_boot_v50align.py \
  --stock-boot firmware/stock-images/boot_a.img \
  --kernel src/linux-5.4-custom/arch/arm64/boot/Image \
  --ramdisk-cpio build/initramfs/initramfs-minimal.cpio \
  --out build/boot-images/boot_54custom_clean_debug_v50align.img
```

镜像：

```text
build/boot-images/boot_54custom_clean_debug_v50align.img
C:\platform-tools\boot_54custom_clean_debug_v50align.img
size  = 67,108,864 B
md5   = f68536c9bffcdd1a3e35e0cd15be6ef9
sha256= afca39e9c202b41bfbbf0ed1dc5506d368c7daed5cb0b6ddb56b2a5375146f26
kernel= 8840d249ee3269d1c9415bae1efc9752
cpio  = a0dfd66a71355333b36a228baae4306a
```

离线验证：

- header v4、`signature_size=0`；
- LZ4 legacy magic `0x184c2102`，block_len `882722`，解出 cpio md5 一致；
- 原厂 AVB footer 回读，公钥 sha1 `ea410c1b...`；
- 输出总长 64 MiB。

---

## 4. 预期屏幕

精简后应只剩：

```text
y=20   十格进度条 0–9
y=400  一个主方块（do_initcalls 完成）
y=460  一个主方块（exec init 前）
```

若 `/init` 用户态执行成功，应额外出现：

```text
y=10..13 4 条全宽白线（initramfs /init 用 devmem 画的用户态反馈）
```

说明：

```text
y=400 + y=460 = 内核到达 run_init_process 前
y=10..13 全宽白线 = /init 用户态真正执行
```

当前状态：**新镜像已构建并离线验证；尚未刷入、未做真机 LCD 验证。**

---

## 5. 真机刷入记录（2026-10-05 19:4x，fastboot）

设备在 fastboot，`current-slot: a`，使用 Windows fastboot 写入：

```text
Sending 'boot_a' (65536 KB)   OKAY [  1.746s]
Writing 'boot_a'              OKAY [  0.421s]
Finished. Total time: 2.176s
Rebooting                     OKAY [  0.000s]
```

- 只写 `boot_a`；
- 未写 `boot_b`、vendor_boot、dtbo、uboot、trustos、teecfg；
- 写入镜像 md5 `f68536c9bffcdd1a3e35e0cd15be6ef9`。

日志：

```text
t508/work/fastboot_flash_boota_clean_debug.log
```

### 5.1 预期读数

精简检查点后：

```text
y=20   十格进度条（stage 0–9）
y=400  一个主方块（do_initcalls 完成）
y=460  一个主方块（exec /init 前）
```

若 `/init` 用户态执行成功，额外出现：

```text
y=10..13  4 条全宽白线
```

当前状态：**已刷入并 reboot；等待 LCD 读屏。**

---

## 6. 根因修复：内建 initramfs 被符号链接生成成空

### 6.1 现象

精简检查点后首次启动：

```text
y=20 十格亮
y=400 亮
y=460 不亮
无顶部全宽白线
```

### 6.2 根因

`.config` 原为：

```text
CONFIG_INITRAMFS_SOURCE="/home/test/t508n/initramfs_root"
```

该路径是 **符号链接**：

```text
/home/test/t508n/initramfs_root -> t508/firmware/initramfs_root
```

内核的 `usr/gen_initramfs_list.sh` 使用 `find` 且没有 `-H`，不会跟随符号链接，因此生成的内建 initramfs 只有 512 B / 50 B gzip，里面没有 `/init`。
于是内核 `kernel_init_freeable()` 的 `ksys_access("/init")` 失败，`ramdisk_execute_command=NULL`，走不到 `run_init_process()`，y=460 不亮。

### 6.3 修复

改为真实目录：

```diff
-CONFIG_INITRAMFS_SOURCE="/home/test/t508n/initramfs_root"
+CONFIG_INITRAMFS_SOURCE="/home/test/t508n/t508/src/initramfs/out"
```

并保留：

```text
CONFIG_DEVMEM=y
# CONFIG_STRICT_DEVMEM is not set
```

重新生成后内建 initramfs：

```text
usr/initramfs_data.cpio.gz = 723,681 B
包含 init / bin/busybox / bin/devmem
```

外部 ramdisk 同时改成系统 `lz4 -l -12` 标准 legacy 帧。

### 6.4 新镜像

```text
build/boot-images/boot_54custom_clean_debug_builtin_v50align.img
md5    = a181ebefd35bf3c0e24560024c07f1c5
kernel = 11575b7a71f053cf6cbc948b466cc466
cpio   = a0dfd66a71355333b36a228baae4306a
```

### 6.5 真机刷入

```text
fastboot getvar current-slot -> a
Sending 'boot_a' (65536 KB)   OKAY [  1.754s]
Writing 'boot_a'              OKAY [  0.419s]
Rebooting                     OKAY [  0.000s]
```

只写 `boot_a`。

### 6.6 本次预期

```text
y=20   十格
y=400  一个主方块
y=460  一个主方块
```

若 `/init` 用户态执行成功：

```text
y=10..13  4 条全宽白线
```

当前状态：**已刷入并 reboot；等待 LCD 读屏确认 y=460 与顶部白线。**

---

## 7. 第二次修复：内建 initramfs 已生效但 /dev/mem 未生成

### 7.1 第二次观测（用户口述）

```text
没有白线
460 亮
```

说明：

- 内建 initramfs 修复成功：/init 存在，内核到达 `run_init_process()`；
- 但 `/init` 用户态没有画出白条。

### 7.2 最可能原因

最小 initramfs 的 `init` 脚本：

```sh
mount -t devtmpfs devtmpfs /dev 2>/dev/null || mdev -s
```

5.4 config 里 `CONFIG_DEVTMPFS` 未开，因此依赖 `mdev -s` 创建 `/dev/mem`。
若 mdev 没把 `/dev/mem` 建出来，`devmem` 写 `0xb0000000` 必然失败，白条不会出现。

### 7.3 修复：init 内显式 mknod

在 `src/initramfs/out/init` / `src/initramfs/init` 中增加：

```sh
[ -c /dev/console ] || /bin/busybox mknod /dev/console c 5 1
[ -c /dev/mem ]    || /bin/busybox mknod /dev/mem c 1 1
[ -c /dev/kmsg ]   || /bin/busybox mknod /dev/kmsg c 1 11
```

并把 `devmem` 调用改为显式 `/bin/devmem`。

已确认生成的 built-in initramfs 中含：

```text
[ -c /dev/mem ] || /bin/busybox mknod /dev/mem c 1 1
```

### 7.4 新内核与新镜像

```text
kernel Image md5 = 2dcf8225b504eed0a961ee1ac18f02da
built-in initramfs gzip = 723,809 B

镜像：
build/boot-images/boot_54custom_clean_debug_mknod_v50align.img
C:\platform-tools\boot_54custom_clean_debug_mknod_v50align.img

size  = 67,108,864 B
md5   = a798d788cddc9a0aa7a311f47366b58d
sha256= fb58dceab31929d97c2cb75ba8674bf27c7f173254a6db8c4839758337941e53
cpio  = c414731514180a186c5e1db439289780
```

当前状态：**新镜像已构建/离线验证；等待 fastboot/SPD 刷入 boot_a。**

---

## 8. mknod 修复版真机刷入（2026-10-05 20:02，fastboot 自动等待）

```text
[20:02:14] fastboot device: T6PA03CJ3EH02ZW fastboot
[20:02:14] current-slot: 'a'
[20:02:14] flashing boot_a (mknod fix)
Sending 'boot_a' (65536 KB)   OKAY [  1.751s]
Writing 'boot_a'              OKAY [  0.420s]
Finished. Total time: 2.182s
[20:02:16] flash_exit=0
[20:02:16] rebooting
```

- 只写 `boot_a`；
- 未写 b 槽、vendor_boot、dtbo、uboot、trustos、teecfg；
- 刷入镜像 md5 `a798d788cddc9a0aa7a311f47366b58d`；
- 内建 initramfs 含显式 `mknod /dev/mem c 1 1`。

### 本次预期

```text
y=20   十格
y=400  一个主方块
y=460  一个主方块
```

如果 `/init` 执行且 `/dev/mem` 可用：

```text
y=10..13  4 条全宽白线
```

当前状态：**已刷入并 reboot；等待 LCD 读屏。**

---

## 9. bigbar 诊断版真机刷入（2026-10-05 20:05，fastboot 自动等待）

```text
[20:05:25] fastboot device: T6PA03CJ3EH02ZW fastboot
[20:05:25] current-slot: 'a'
[20:05:25] flashing boot_a (bigbar init)
Sending 'boot_a' (65536 KB)   OKAY [  1.765s]
Writing 'boot_a'              OKAY [  0.419s]
Finished. Total time: 2.194s
[20:05:27] flash_exit=0
[20:05:27] rebooting
```

- 只写 `boot_a`；
- 镜像 md5 `866dcc7d0c9af0e50684e6fef70043d6`；
- 内建 initramfs 含最小诊断 init；
- init 不依赖 mdev，直接 `mknod /dev/mem c 1 1` 后在 y=300..339 画 320x40 大白条。

### 本次预期

```text
y=20   十格
y=400  一个主方块
y=460  一个主方块
y=300..339  320x40 大白条（/init 用户态执行证据）
```

当前状态：**已刷入并 reboot；等待 LCD 读屏。**

---

## 10. 参考 V50 init 后的 C init 诊断（2026-10-05 20:1x）

### 10.1 V50 `boot/init` 可借鉴点

- `#!/bin/busybox sh`，不依赖预置 `/bin/sh`；
- 启动时 `busybox --install -s /bin` 自建 applet 链接；
- `echo 7 > /proc/sys/kernel/printk`；
- devtmpfs 失败后 tmpfs + 显式 mknod；
- 每阶段 `echo "<3>..."> /dev/kmsg`，供 pstore 持久化。

V50 没有 framebuffer/`/dev/mem` 用户态画屏方案，因此不能直接解释白条；
但提醒我们脚本执行链上不能依赖过多少预设。

### 10.2 当前 C init 诊断版

为进一步区分“/init 没执行”与“/dev/mem 失败”，`/init` 换成静态 C 程序：

```text
mknod /dev/mem c 1 1
open("/dev/mem", O_RDWR | O_SYNC)
mmap(0xb0000000)
画 y=300..339 320x40 白条
失败：sleep 5 -> reboot(RB_AUTOBOOT)
```

镜像：

```text
build/boot-images/boot_54custom_cinit_bigbar_v50align.img
md5    = e5906fa1023b9987c388ee5d2f9f9777
kernel = f27bec94d106de1a59da91879bcfa01d
cpio   = dbd79ea2bd2094157a95d6d6953799a1
```

### 10.3 真机刷入

```text
fastboot getvar current-slot -> a
Sending 'boot_a' (65536 KB)   OKAY [  1.733s]
Writing 'boot_a'              OKAY [  0.419s]
Rebooting                     OKAY [  0.000s]
```

当前状态：**C init 已刷入 boot_a 并重启；等待“白条 vs 自动重启 vs 无反应”的 LCD 结果。**

---

## 11. C init 真机验证成功（2026-10-05，用户确认）

用户读屏：

> 有白条

结合此前：

```text
y=20   十格
y=400  亮
y=460  亮
y=300..339  320x40 白条
```

可以确认：

```text
1. 内核正常运行到 run_init_process / exec /init 前；
2. initramfs /init 用户态真正执行了；
3. /dev/mem 可以 open/mmap；
4. 用户态写物理 framebuffer 0xb0000000 能显示；
5. C 静态 init 方案有效。
```

同时说明：

- 之前 shell init 的无白条问题，不在“内核 /init exec”本身，而在 shell 脚本执行链
  （`#!/bin/sh` 依赖 / busybox/applet/PATH/mdev 等）；
- V50 采用 `#!/bin/busybox sh` + `busybox --install -s /bin` + 显式 mknod 是有道理的；
- 后续可把 C init 换回 V50 风格 shell init，用同样方式验证。

当前结论：**T508N 5.4 复刻路线已闭环到 initramfs /init 用户态执行；
framebuffer 用户态探针可用。**

仍未解决：mainline 6.12 的 CPU1 PSCI CPU_ON 卡点（与本 5.4 init 验证无关）。

---

## 12. V50 风格 shell init 真机刷入（2026-10-05 20:29，fastboot 自动等待）

```text
[20:29:57] fastboot device: T6PA03CJ3EH02ZW fastboot
[20:29:57] current-slot: 'a'
[20:29:57] flashing boot_a (V50-style init)
Sending 'boot_a' (65536 KB)   OKAY [  1.756s]
Writing 'boot_a'              OKAY [  0.419s]
Finished. Total time: 2.185s
[20:29:59] flash_exit=0
[20:29:59] rebooting
```

镜像：

```text
build/boot-images/boot_54custom_v50init_bigbar_v50align.img
md5    = 945b7014371e170539c3e1bfe656eb23
kernel = 11da77941e9a0e7d67d2aa2d8a9519c8
cpio   = de0035fd06f83b7b6a03a657f9af8978
```

本次 init 特征：

```text
#!/bin/busybox sh
busybox --install -s /bin
devtmpfs 失败 -> tmpfs + mknod /dev/mem
devmem 首写失败 -> 5 秒后 reboot
成功 -> y=300..339 320x40 白条
每阶段写 /dev/kmsg KERN_ERR
```

当前状态：**V50 风格 shell init 已写入 boot_a 并 reboot；等待 LCD 白条结果。**

---

## 13. 最小 V50 风格 shell init 验证成功（2026-10-05，用户确认）

用户读屏：

> 有白条

结合之前的检查点：

```text
y=20   十格
y=400  亮
y=460  亮
y=300..339  320x40 白条
```

结论：

```text
1. 内核 exec /init 正常；
2. binfmt_script 正常；
3. /bin/busybox shell 能执行；
4. /bin/fbdraw 静态 helper 能 mmap /dev/mem 并写 framebuffer；
5. 最小 V50 风格 shell init 路线可用。
```

之前完整 V50 风格 init 失败的可能原因：

```text
- busybox --install -s /bin 的批量 symlink 操作；
- mount devtmpfs/tmpfs/proc/sysfs 组合；
- 逐像素调用 5120 次 busybox devmem（每次一个进程）过慢/卡住；
- 某条 setup 命令在 T508N 5.4 环境下阻塞。
```

当前可用基线：

```text
boot_54custom_minshell_bigbar_v50align.img
md5 = 5b0e06138906e0a1ec7cbd1edb728ff3
```

其 init 只保留：

```sh
#!/bin/busybox sh
busybox mkdir /dev
mknod /dev/mem
busybox devmem 首写
/bin/fbdraw
sleep
```

即：V50 的“直接 busybox + 显式 mknod”最简形态有效。
