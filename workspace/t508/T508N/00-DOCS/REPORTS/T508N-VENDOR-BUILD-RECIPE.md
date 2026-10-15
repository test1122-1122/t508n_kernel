# T508N 真厂内核构建配方（可复现文档）

> 生成：2026-08-22。**已验收**：boot 到 `exec /init`（8 核上线 + 全部 initcall + 内置 initramfs /init 执行）。
> 本文档 = 从零复现"能跑的真厂内核"的全部步骤。工作区：`/home/test/t508n`。

## 1. 源码与材料

| 项 | 路径 |
|---|---|
| vendor 内核源码树 | `work/vendor-kernel-ums9620/`（Enceka 仓库 = ZTE 开源站 UMS9620 5.4.210，commit `1e79a67`） |
| T508N 原厂 config | `work/kernel.config`（IKCONFIG 提取，167KB） |
| 数据 DTB | `work/stock_ums9620.dts`（原厂 DTB 反编译，7731 行；用 vendor dtc 重编 = **原厂 DTB 字节级** md5 `42d1303c…` / 160,099B） |
| 内置 initramfs | `/home/test/t508n/initramfs_root/`（解包的 busybox rootfs：/init 挂 proc/sys/devtmpfs） |
| 工具链 | `arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin` |
| openssl | `openssl-local/`（CPATH/LIBRARY_PATH） |

## 2. 构建步骤

```bash
K=/home/test/t508n/work/vendor-kernel-ums9620
cd $K

# 1) config：原厂 IKCONFIG 落位 + gcc13 归一
cp /home/test/t508n/work/kernel.config .config
make ARCH=arm64 olddefconfig

# 2) ★ 关键修复 a：关闭 -Werror（gcc13 新告警会致命）
./scripts/config -d CONFIG_WERROR
# 并在 Makefile:480 行把 "KBUILD_CFLAGS := -Wall -Werror -Wundef ..." 
#   改成 "KBUILD_CFLAGS := -Wall -Wundef ..."（厂商写死的裸 -Werror）

# 3) ★ 关键修复 b：内置 initramfs（这是"能跑"的根因修复）
./scripts/config --set-str CONFIG_INITRAMFS_SOURCE /home/test/t508n/initramfs_root
./scripts/config -e CONFIG_CMDLINE_EXTEND -e CONFIG_INITRAMFS_FORCE
# 说明：外部 boot-ramdisk 是紫光 LZ4 容器（magic 0x184C2102），unpack_to_rootfs 只认 cpio
# 必败；5.4-custom 当年靠"内置 + FORCE"成功。FORCE 依赖 CMDLINE_EXTEND/FORCE。
# 注意顺序：olddefconfig 会因依赖未满足丢掉 FORCE → 先设 CMDLINE_EXTEND 再设 FORCE，
# 且不要再跑 olddefconfig（或跑完再补设）。

# 4) 可选：dd.c wait_for_device_probe 40s 超时（原厂 DTB 有驱动 probe 慢/卡风险）
#    drivers/base/dd.c: flush_work(&deferred_probe_work) 后
#    把 wait_event(...) 改为 wait_event_timeout(..., 40*HZ)（probe 卡死时放行 boot）

# 5) 构建
export PATH=/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
cp arch/arm64/boot/Image /home/test/t508n/work/vendor54_Image

# 6) 打包（--ramdisk 参数已无用，FORCE 忽略；保留无害）
cd /home/test/t508n
python3 tools/sprd_mkbootimg.py --kernel work/vendor54_Image \
  --ramdisk initramfs.cpio --output boot_54vendor.img --key tools/ot5_boot_key.pem
```

## 3. 刷机组合（已验证）

```bash
fastboot flash boot_a boot_54vendor.img
fastboot flash vendor_boot_a Image/vendor_boot_a.img   # 原厂（DTB 与原厂一致）
# dtbo_a 保持原厂（与原厂 DTB 合并 OK；千万别用 mainline DTB+原厂 dtbo）
fastboot reboot
```

## 4. 探针系统（t508n_dbg，本机可观测性）

- `arch/arm64/kernel/t508n_dbg.c` + `include/asm/t508n_dbg.h`：
  - **类 log 控制台（当前主用）**：`t508n_log_init()`（清屏）/ `t508n_log_print(s)`：
    VGA 8x16 字体 2x 放大渲染（16x32/字符，**20 列 x 15 行**），白字黑底，自动换行/回滚。
  - 内核侧调用点：start_kernel 后、populate_rootfs、driver_init、memory_dev_init、
    wait_for_device_probe、kernel_init 各步、pid1 状态监控（仅变化时输出）、panic（全屏红 + 文本）。
  - **用户态 log 调用**：syscall **441**（`t508n_log`，regs x0=字符串指针，见 `arch/arm64/kernel/sys.c`，
    `SYSCALL_DEFINE1(t508n_log, const char __user *, msg)`；`__NR_t508n_log=441` 在
    `include/uapi/asm-generic/unistd.h`，表项 `[441] = __arm64_sys_t508n_log` 硬编码在 sys_call_table）。
    绕过 /dev/mem（DEVMEM 直写 fb 会 panic）。user 侧封装：`initramfs_root/init.c` 的 `tlog()`。
- 屏 320×480 BGRA @0xb0000000；旧版进度条 API（paint_bar/mark_row/stage）仍保留但不再使用。
- **读屏约定（新版）**：屏幕显示 15 行文本日志；最后几行 = 用户态 init 的模块加载/检查结果。

## 5. 已知坑

1. **gcc13 + 老内核**：`-Werror=dangling-pointer/array-compare/format` 大面积报错 → 关 WERROR + 去裸 -Werror（Makefile:480）。
2. **olddefconfig 会丢配置**：INITRAMFS_FORCE（依赖链）等符号要最后设、设完别乱跑 olddefconfig。
3. **DTB 必须与原厂一致**：真厂内核用原厂 DTB（vendor_boot_a=原厂镜像），dtbo_a 保持原厂；
   mainline DTB 只配 mainline 驱动节点（会 degrade，且与原厂 dtbo 合并 panic）。
4. **pstore/ramoops 环形区**会被下一次任何内核的注册重写——卡死后的日志只能靠"探针"或
   "超时放行后从 init 侧收集"。
5. **WDT**：sprd_wdt_fiq 寄存器 0x641E0000（LOCK@0x20 键 0xe551，LOAD 17bit HI@0x4/LO@0x0）——
   修正版已入 t508n_dbg.c（30s 喂狗）。
