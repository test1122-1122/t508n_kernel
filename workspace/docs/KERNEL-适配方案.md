# T508N (T770/UMS9620) Linux 内核适配方案

> 基于第一阶段启动链逆向成果（REPORT-T508N-启动流程分析.md），
> 本方案给出把 Linux 内核跑上 T508N 的三条路线与实施步骤。

## 0. 启动链成果如何服务于内核适配

| 逆向成果 | 对适配的作用 |
|---|---|
| LK 只加载 **raw ARM64 Image**（拒绝 gzip 内核） | 内核必须输出 `Image`（mainline 默认即 raw，带 MZ/PE 头 ✓） |
| boot.img 布局：内核 @0x1000 页对齐 + 紫光 LZ4 ramdisk 容器 | 已有 `sprd_mkbootimg.py`/`sprd_ramdisk.py` 一键复刻（已回环验证） |
| LK 注入完整 cmdline（`init=/init root=/dev/ram0 console=ttyS1…`） | 内核/init 无需自带 cmdline；initramfs 必须有 `/init` |
| DTB 在 vendor_boot（sc-id `ums9620 1000 1000`）+ DTBO board_id 0x2700 | mainline DTB 需保留匹配或放入 boot.img fix dtb 段 |
| 保留内存图（TOS@bd040000/SML@bd000000/modem@88000000/AGDSP@afa00000…） | 已写入 `tools/dts/ums9620-ot5.dts`，防内核踩安全世界 |
| vbmeta 链 + 解锁态 `SKIP VERIFY` | bring-up 期直接解锁刷机；上锁方案见 tools/README.md |
| LK 有 fastboot 模式 + FDL/ResearchDownload | 刷机通道明确 |
| 串口 uart1 115200n8 + uboot_log 分区 | 启动日志调试通道明确 |
| 内核配置（IKCONFIG）已提取 `work/kernel.config` | 可作为 BSP 源码编译的 defconfig 起点 |

## 路线 A1（最快见成效，1-2 天）：stock 内核 + 自定义 initramfs

**不改内核二进制**，直接复用本机 5.4.210 内核（所有驱动齐全：LCD/触摸/无线/modem），
只替换根文件系统，让设备跑自定义 Linux 用户空间（busybox/postmarketOS 风格）。

1. 制作 initramfs：静态 busybox（aarch64）+ `/init` 脚本（挂载 proc/sys/dev、
   提示 shell；进阶：挂载 USB/emmc 上的 rootfs）。
2. `python3 tools/sprd_mkbootimg.py --kernel stock_Image --ramdisk my.cpio --output boot_linux.img`
   （stock_Image 可直接从 boot_a.img 0x1000 处截取，或后续从 BSP 源码重编）。
3. `fastboot flash boot_a boot_linux.img && fastboot reboot`（设备需解锁）。
4. 串口登录 shell → 得到"能用的 Linux"。
**优点**：显示/输入/网络全部工作；**缺点**：仍是 5.4 内核，非 mainline。

## 路线 A2（BSP 源码自主构建）：拿到紫光内核源码重编

- 源码获取渠道（按优先级）：
  1. TCL/FIH 官方 opensource 发布（TCLOpenSource GitHub 目前只有 TV 平台，需在
     opensource.tcl.com 按型号 T508N/OT5 检索或联系 TCL 合规邮箱）；
  2. 社区镜像（GitHub 搜 `ums9620 kernel` / `qogirn6pro`，同类 T760/T770 机型
     ——如 TCL 40 NxtPaper 等——的 kernel 仓库可用）；
  3. 紫光/Unisoc 官方 BSP（需厂商授权）。
- 构建：`work/kernel.config`（已从本机提取）作为 defconfig；交叉编译
  aarch64；产物 `Image` 交给 `sprd_mkbootimg.py` 打包。
- 之后可自由升级内核版本/裁剪/加 mainline 补丁。

## 路线 B（长期目标）：mainline Linux 移植

**现状核实（2026-08）**：mainline（torvalds/master）已合入 `ums9620.dtsi` +
`ums9620-2h10.dts`（2023-12 LKML 补丁，作者 Unisoc，[LKML 讨论](https://lkml.org/lkml/2023/12/18/101)），
但只有 SoC 骨架：GICv3、ARMv8 timer、PSCI(SMC)、8×CPU、**uart0/1**（`sprd,sc9836-uart` 驱动兼容）、固定时钟。

**阶段 1（串口引导，最优先）**：
1. 交叉编译 mainline（aarch64，`defconfig` + 打开 `SPRD_UART`/`SERIAL_8250`…），
   设备树用 `tools/dts/ums9620-ot5.dts`（3GB 内存 + 保留区 + serial1:115200n8）。
2. 打包 boot.img（工具已就绪）→ 解锁刷入 → 串口观察。
3. 预期结果：内核解压 → 打印版本 → 卡在 rootfs（initramfs 有 busybox 即可进 shell）。
   风险点：mainline GIC/timer 参数与紫光固件一致性（SML 提供 PSCI——stock 同用 smc ✓）。

**阶段 2（逐驱动点亮，对照 stock DTS/内核源码）**：
优先级：pinctrl（qogirn6pro，vendor_dlkm 里有 `pinctrl-sprd-qogirn6pro.ko`，
mainline 需新 compatible）→ 时钟（ums9620 clk）→ eMMC（22200000.sdio，
mainline 有 sdhci-sprd?）→ 显示（st7365p MIPI DSI，480x320）→ USB → 无线/modem（最末）。
可用 `work/stock_ums9620.dts`（stock DTB 反编译，6000+ 行）逐节点对照移植。

**阶段 3**：postmarketOS / Debian 完整系统。

## 立即可以做的事（本仓库已交付）

- [x] 紫光 ramdisk 容器编解码器（回环验证通过，压缩率 0.47–0.60）
- [x] 紫光 boot.img 打包器（布局与 stock 逐字节一致，已用 stock 内核重建验证）
- [x] AVB footer 自签（avbtool add_hash_footer 实测通过，产出 64MB 分区镜像）
- [x] mainline 起步板级 DTS（ums9620-ot5.dts，含安全保留区）
- [x] 交叉工具链（ARM 13.2 + musl）与 mainline 6.12.103 源码已下载
- [x] **mainline 内核已编译成功**（46MB raw Image，SERIAL_SPRD/earlycon 开启，KASLR 关闭）
- [x] 静态 busybox initramfs 已构建（1.24MB cpio，/init 挂载 proc/sys/devtmpfs 后进 shell）
- [x] **boot_linux.img 已产出并通过格式验证**（布局与 stock 一致，ramdisk 回环解包一致）
- [x] vendor_boot_linux.img 已产出（mainline OT5 DTB 已替换，前缀表保留）
- [ ] 需要用户侧：设备解锁后 fastboot 刷入 boot_linux.img + vendor_boot_linux.img，串口观察
- [ ] 需要用户侧：设备解锁状态确认、串口/uboot_log 回读环境、BSP 源码线索
      （若手上有 TCL/紫光源码包，可提供路径由我直接构建）

## 风险与备注

1. mainline 起步若 PSCI 不兼容（SML 实现细节），CPU 热插拔/挂起会异常，但单核引导通常可行。
2. 保留区不完整会导致随机崩溃——已按 stock DTB 全量抄录。
3. 刷机前请备份原分区（本次镜像目录即完整备份）。
4. 上锁态设备务必先解锁，否则自定义 boot 会被拒（`ERROR_PUBLIC_KEY_REJECTED`）。
