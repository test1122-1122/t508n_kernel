# REPORT-54-OPENWRT-T508N — T508N 5.4 OpenWrt 适配（第一版）

> 日期：2026-10-06。按用户要求暂停 6.12 mainline 工作，转 5.4 OpenWrt。
> 设备：TCL T508N (UMS9620/Qogirn6Pro)，3GB RAM / 29GB eMMC，无 UART，
> 调试只能靠 LCD framebuffer / pstore / fastboot。

## 1. 现状与目标

- 5.4 自定义内核已能 boot 到内置 initramfs / 用户态（白条 + /dev/mem 可用）。
- 设备 eMMC 已全盘 Linux 化：`super` mmcblk0p46 (5.5GB ext4, /)、
  `userdata` mmcblk0p75 (24GB ext4, /home)。
- 本次目标：在 **5.4 内核** 上 bring-up **OpenWrt 25.12.5 aarch64** 用户态，
  验证 procd/rcS/LuCI 栈能启动；持久化 eMMC rootfs 下一步。

## 2. OpenWrt rootfs 来源

官方 armsr/armv8：

```text
https://downloads.openwrt.org/releases/25.12.5/targets/armsr/armv8/
openwrt-25.12.5-armsr-armv8-rootfs.tar.gz
size ~4.5 MB, uncompress ~14 MB
DISTRIB_TARGET='armsr/armv8'
DISTRIB_ARCH='aarch64_generic'
```

本地：`work/openwrt54/rootfs-t508/`

## 3. T508N overlay

1. 删除 6.12.94 内核模块（5.4 内核不匹配）：`lib/modules/6.12.94/`。
2. 删除 procd-ujail/procd-seccomp（5.4 无 CAP_PERFMON，V50 已验证 jail 会 crash-loop）。
3. 加入静态 aarch64 helper `/sbin/t508n-progress`：
   - 写 `0xb0000000` framebuffer；stage=1 → y=340，stage=2 → y=380。
4. `/init` 替换为 T508N wrapper：mount proc/sys/devtmpfs/tmpfs，画 stage1，
   `exec /init.openwrt`；原 OpenWrt init 存为 `/init.openwrt`。
5. 增加 `/etc/init.d/t508n-progress` + `rc.d/S20t508n-progress`，procd/rcS S20 时画 stage2。
6. 加最小 `etc/config/system`：hostname `T508N-OpenWrt`、Asia/Shanghai。

## 4. 5.4 内核配置

```text
CONFIG_INITRAMFS_SOURCE="/home/test/t508n/t508/work/openwrt54/rootfs-t508"
CONFIG_INITRAMFS_FORCE=y
CONFIG_CMDLINE_FORCE=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_INITRAMFS_COMPRESSION=".gz"
```

`CMDLINE_FORCE` 是为让 `INITRAMFS_FORCE` 可选（arm64 5.4 Kconfig 无 CMDLINE_EXTEND）。

## 5. 构建产物

```text
kernel: src/linux-5.4-custom/arch/arm64/boot/Image
        size 35,949,056 B, md5 2fb3b32cf544a8825c1744f3c347173f
        built-in initramfs gzip 4,346,591 B, 1029 entries

boot:   build/boot-images/boot_54openwrt_t508.img
        64 MiB V50-align, md5 b81d8be2d41d99c347bf6529f75e76e6
        kernel_size 35,949,056; external placeholder ramdisk 819,571 B
        LZ4 legacy magic 0x184c2102; signature_size=0

vendor_boot_a: firmware/stock-images/vendor_boot_a.img
        md5 68d8bec5fc00b8624b002af97c478390（原厂 stock DTB）
```

## 6. 启动阶段读屏

| 标记 | 位置 | 含义 |
|---|---|---|
| 无标记 | 旧内容/黑屏 | 内核未到 built-in initramfs wrapper |
| y=340 白条 | `/sbin/t508n-progress 1` | 5.4 内核 + OpenWrt initramfs + wrapper 执行 |
| y=380 白条 | `S20t508n-progress` | OpenWrt procd/rcS 已到 S20 服务阶段 |

## 7. 刷机

单变量 a 槽：

```text
fastboot flash vendor_boot_a vendor_boot_a_stock.img
fastboot flash boot_a         boot_54openwrt_t508.img
fastboot reboot
```

b 槽、dtbo、trustos、teecfg、uboot 不动。

等待器：`work/wait_fastboot_flash_openwrt54.sh`、`work/wait_spd_flash_openwrt54.sh`。

## 8. 下一步

1. 验证 y=380 后接入网络/USB gadget（T508N USB2 受 IPA/TEE 限制，需按 mu300 经验）。
2. 把 rootfs 从 initramfs 改为 eMMC 持久化：`super` ext4 + `sdhci-sprd.ko` +
   overlayfs；参考 `firmware/initramfs_root/init.c` 的 `rootfs_boot()`/`run_tar()`。
3. 添加 T508N 专属 network/UCI defaults 与 LCD 日志脚本。

---

## 9. Debug3（2026-10-06 15:17 照片）— OpenWrt 基本启动成功

### 9.1 Debug2 结果

照片（debug2）读数：

- y=300 全宽 → stage1（T508N wrapper）
- y=340 左半宽 → stage2（OpenWrt 原 init 进入）
- y=380 右半宽 → stage3（/new_root tmpfs 挂载）
- 无 y=420 全宽 → 卡在 `cp -pr $DIRS $NEW_ROOT`
- 下方小方块是内核 init/main.c 的 y=400 / y=460 探针

### 9.2 Debug2 卡点根因

`CONFIG_DEVTMPFS_MOUNT=y` 使 `/dev` 在 init 前已是 devtmpfs。
OpenWrt 原 `/init`：

```sh
DIRS=$(echo *)
cp -pr $DIRS $NEW_ROOT
```

busybox `cp -pr` 复制 `/dev` 时，对 `/dev/mem` 等字符设备按普通文件读取 → 永久卡住。

### 9.3 Debug3 修复

1. 内核关闭自动 devtmpfs 挂载：

```text
CONFIG_DEVTMPFS=y
# CONFIG_DEVTMPFS_MOUNT is not set
```

2. `/sbin/t508n-progress` 改为按需 `mknod /dev/mem`，画完后立即 `unlink`
   （仅删除自己创建的节点）。这样 OpenWrt init 执行 `cp -pr /dev` 时 `/dev` 为空。
3. procd/OpenWrt 启动后自己挂 devtmpfs。

镜像：

```text
build/boot-images/boot_54openwrt_t508_debug3.img
boot md5   = 02efdba77cba43ccb7a7b3135a0cb069
kernel md5 = 7b9cda72fcce571abbed37fbb35f50eb
```

### 9.4 Debug3 照片读数（成功）

照片 `IMG_20261006_151740.jpg` 中检测到：

- 顶部 kernel stage 进度条（0~9）
- 两条**全宽**白条：stage1、stage4
- 两条**左半宽**白条：stage2、stage5
- 两条**右半宽**白条：stage3、stage6

即所有六个 debug 标记全部出现：

```text
stage1 wrapper
stage2 OpenWrt 原 init
stage3 /new_root tmpfs
stage4 cp 完成 / switch_root 前
stage5 procd preinit
stage6 OpenWrt rcS S20
```

**结论：5.4 内核 + OpenWrt 25.12.5 aarch64 已成功启动到 rcS S20 服务阶段。**

### 9.5 下一步

1. 增加 S99done/rc.local 标记，确认完整 boot 完成。
2. eMMC 持久化：
   - 用 `super` mmcblk0p46 ext4 做 rootfs/overlay；
   - 用 `userdata` mmcblk0p75 做 /home/overlay；
   - 需要加载/编译 `sdhci-sprd.ko` 及依赖。
3. 网络/管理入口：
   - T508N USB2 gadget（MUSB）受 IPA/TEE 限制，需要专项；
   - 或先给 OpenWrt 加本地 framebuffer 日志/状态脚本。
4. 最终做一个“开机直接进 OpenWrt”的 boot_a 镜像，保留 b 槽安全网。
