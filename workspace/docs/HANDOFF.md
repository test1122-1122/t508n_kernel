# T508N 交接文档（HANDOFF）

> **新会话从这里开始。** 本文是自包含入口：先读 §0-§2 就知道"现在在哪、下一步做什么"，
> 细节按 §8 文档地图深入，**不要**依赖旧对话上下文，**不要**重读 20,340 行的 `session.jsonl`。
> 生成：2026-09-29（工作区重组同日）。自包含主索引仍是
> [FINAL-BRINGUP-INDEX.md](FINAL-BRINGUP-INDEX.md)（2026-08-18 生成，**自包含、权威**）。
> ⚠️ FINAL-BRINGUP-INDEX 的附录用的是重组前的扁平路径（`work/…`、`linux-6.12.103/…`）——
> 兼容软链仍可用，但真实位置是 `t508/src/…`、`t508/work/…`、`t508/firmware/…`、`t508/build/…`，
> 对照见 [../../docs/PATH-MAPPING.md](../../docs/PATH-MAPPING.md)。

## 2026-10-11 更新：mainline 6.18.55 USB DWC3 + ACM 成功

> 本节比下面的 2026-10-09 状态更新；新会话先读本节。

* **成果**：
  * B 槽 mainline 6.18.55 通过 **DWC3 + phy-sprd-ums9620-ssphy + sprd_dummy_phy + sprd_usb_power**
    枚举为 USB ACM 串口，Windows 侧为 **COM18**，可进 mainline shell。
  * 最终镜像：`build/boot-images/boot_mainline_6.18.55_t508n_test28.img`
    md5 `1771fc80b7ee8441bd60dd68a7417282`。
  * 关键修复：
    * `sprd_usb_power.ko`：UMP9620 `vddusb33` LDO + DPDM 切到 USB PHY。
    * `sprd_dummy_phy.ko`：补上 stock DTB 的 `sprd,usb-dummy-phy`，否则 DWC3 index0
      永远 `-EPROBE_DEFER`。
    * configfs 只保留 `acm.GS0`；**RNDIS 按用户要求放弃**。
    * `/bin/busybox --install -s /bin`，mainline shell applet 齐全。
    * p75 (`/dev/mmcblk0p75`) 挂到 `/persist`，并 bind `/etc`、`/root`；重启后文件保留。
    * 去掉 `sleep 600; reboot -f`，`usbcore.autosuspend=-1`，修 USB 过一会自己断开。
  * MUSB 路线验证到 UDC/gadget/usb0 后，主机仍停在
    “未知 USB 设备(设备描述符请求失败)”，已放弃（vendor MUSB 核/DMA EP0 行为不在 mainline）。
* **文档**：
  * `reports/REPORT-USB-MAINLINE-DWC3-ACM-SUCCESS.md`
  * `../mainline-ref/t508n-mainline/USB-DWC3-ACM.md`
  * `../mainline-ref/t508n-mainline/NEXT.md`（顶部 2026-10-11 状态）
* **立即下一步**：
  * 关掉 `mmc1` (22210000.sdio) 每约 20 秒一次的 SDHCI 错误刷屏；
  * mainline 下基于 p75 加自定义 init / 持久化服务；
  * WLAN / BT / SDIO bring-up。

## 2026-10-09 更新：U-Boot magic64 + OpenWrt A / mainline B

> 本节比下面 §0-§10 的 2026-09-29 状态更新，新会话先读本节，再按需回溯旧内容。

* **新增成果**：
  * T508N 专属 U-Boot magic64 解锁补丁已完成并**实机验证**：`androidboot.flash.locked=0`、
    `androidboot.vbmeta.device_state=unlocked`、`verifiedbootstate=orange`。
  * A 槽当前运行 vendor 5.4 OpenWrt（OpenWrt 25.12.5，`/dev/mmcblk0p75` bind 到 `/etc /usr /www /lib /root`）。
  * B 槽已写入 mainline 6.18.55 test5 镜像（`ANDROID!` 校验通过）。
  * 重新打包 `misc_slot_a.img` / `misc_slot_b.img`，可不依赖 fastboot 切换 A/B。
  * 查清 `fastboot` 插入电脑回退 A 的原因：`misc` `bootloader_control` 中 B 槽为
    `unbootable / tries_remaining: 0`，fastboot/download 模式失败后 reset 回 A。
* **文档**：
  * `reports/REPORT-T508N-启动链与校验链.md`
  * `reports/REPORT-T508N-UBOOT-magic64.md`
  * `reports/REPORT-T508N-BOOT-SLOT-与救砖.md`
  * `../mainline-ref/t508n-mainline/BOOT-CHAIN.md`
  * `../mainline-ref/t508n-mainline/UBOOT-MAGIC64.md`
  * `../mainline-ref/t508n-mainline/SLOT-CONTROL.md`
  * `../mainline-ref/t508n-mainline/RECOVERY.md`
* **文件夹整理（2026-10-09，只读盘点已完成）**：
  * 全部指定路径已盘清：登记 8061 个文件 / 126.3 GiB，SHA256 **8029 成功 / 0 失败**。
  * 发现 1695 个内容完全相同组（2995 个冗余副本，理论可回收 58.02 GiB）与 284 组同名冲突；
    其中 `boota/bootb`、`uboota/b`、`spd_dump.exe`（5 版）、`misc` 家族、`vendor_dlkm_b.img` 命名错误最关键。
  * 产物：`INVENTORY-T508N.md/.csv`、`TREE-T508N.txt`、`CHECKSUMS-T508N.sha256`、
    `DUPLICATES-T508N.md`、`CONFLICTS-T508N.md`、`ORGANIZE-PLAN-T508N.md`、
    `reports/REPORT-T508N-FOLDER-ORGANIZATION.md`、`reports/REPORT-T508N-ARTIFACT-MANIFEST.md`。
  * **未删除 / 未移动 / 未覆盖任何文件**。
* **归档已执行（B2-精简归档，2026-10-09）**：`/home/test/t508n/t508/T508N/`，**253 个关键文件 / 1.48 GiB**，逐文件 SHA256 校验 253/253 通过；`MANIFEST.sha256`（261 条）与 `ORIGIN-MAP.csv`（来源映射）齐备。
  * 归档内容：stock 启动链、真机原始备份、magic64、misc 槽位、mainline test5、当前 A 槽 OpenWrt、p75 rootfs、文档与工具链。
  * 只登记未复制：>256 MiB 大文件（`05-MANIFESTS/BIGFILES-MANIFEST.md`）、OpenWrt 历史变体（`OPENWRT-VARIANTS-MANIFEST.md`）、58 GiB 重复副本。
  * Windows 侧仅更新 `E:\Download\ums9620_tcl_t508n_magic64\README-中文.txt`（先备份、后写入、保持 CRLF），其余目录零改动。
* **Windows 一键包**：`E:\Download\ums9620_tcl_t508n_magic64\`（含 U-Boot、misc 槽位镜像、SPD 脚本）。
* **立即下一步**：BROM 运行 `06-切换到B槽-mainline.bat`，验证 mainline 6.18.55 test5；
  失败则 `07-切换到A槽-openwrt.bat` 回 A。
* **注意**：以下 §0-§10 的"无 USB / a 槽试验、b 槽保官方"结论是 2026-09-29 旧状态；
  现在 U-Boot 已解锁，A/B 均可自定义，但 flash/恢复仍按 `RECOVERY.md` 的红线执行。

---

---

## 0. 30 秒摘要

* **设备**：TCL T508N（板级 OT5，FIH 代工），SoC **Unisoc T770 / UMS9620**（QogirN6Pro），
  CPU **4×A55 + 3×A76 + 1×A76**（CPU7 独立簇），3 GB LPDDR4x，Android 13，原厂内核 **5.4.210**。
* **目标**：把 **mainline 6.12.103** 跑到 **initramfs `/init` 执行**；**外设（USB/eMMC/显示/PMIC）不在本阶段范围**。
* **调试条件**：**无 UART、无可用 USB** —— 只有 **LCD framebuffer 探针**（320×480，32bpp BGRA，基址 `0xb0000000`）
  + **fastboot** + **pstore**，外加 A/B 双槽（a 槽试验、b 槽保官方）做安全网。
* **当前状态**：
  * 原厂式 **5.4 复刻** ✅ 已能 boot 到 initramfs（`exec /init` 探针 y=460 亮）→ 证明原厂式路径在 T770 可行；
  * **6.12 魔改树** 🚧 卡在 **8 核 bringup 之后**（进度条稳定 9 格 + y=130 不亮）；
    **用户已决定暂停继续修改**（2026-08-18 晚）；
  * 存储/Alpine ✅（eMMC 分区化，Alpine 3.24.1 aarch64 chroot）；
  * USB gadget ❌ 全部路线穷尽（IPa 电源域受 TEE/SIP 固件管辖）。
* **下一步**：从 §8 选一个，**优先做纯观测的两项**（SGI/IPI 计数诊断、KFENCE），
  纪律是"**一次只改一个变量**"。

## 1. 红线（任何会话都不要越过）

| 红线 | 原因 |
|---|---|
| 别指望串口/日志口 | **无 UART、无可用 USB**；只能用 LCD 探针、fastboot、`uboot_log` 分区、pstore |
| 保持 A/B 安全网：a 槽试验，b 槽保官方系统 | 刷错能靠 b 槽救回；刷前确认当前槽 |
| **绝不刷空 TEE 分区**（`trustos_a`、`teecfg_a`） | 触发写保护；恢复用 `firmware/stock-images/trustos_a.img` / `teecfg_a.img` |
| 5.4 路线：`LOCALVERSION=` 必须为空 | 否则 vermagic 不等于原厂 `5.4.210-android12-9-ga1d5c173d86b-ab1`，原厂模块无法加载 |
| 改 config 后必须 `olddefconfig && make prepare` 再 build | 否则改动静默不生效（老坑） |
| DTBO merge 失败 = LK panic 无限重启 | 用空 overlay `noop_dtbo.img` 绕过（`无串口验证指南.md`） |
| 私钥 `t508/tools/ot5_boot_key.pem` 勿外传 | 签名 AVB 用；`.dsh` 之外不要复制 |
| **一次一个变量** | 这个项目的历史教训：多变量同时改 → 卡点漂移、无法归因 |
| 不要重读 `t508/docs/session/session.jsonl` | 20,340 行；用 FINAL-BRINGUP-INDEX + SUMMARY-ALL-ANALYSIS 代替 |
| 不要照抄 V50 项目的镜像/分区操作 | 同 SoC 不同产品；跨机型刷引导链 = 变砖 |

## 2. 当前状态（逐项）

| 项 | 位置 / 状态 |
|---|---|
| **6.12.103 魔改主树** | `t508/src/linux-6.12.103`，HEAD `cb8e725f3` + **13 个未提交文件**：`arch/arm64/{boot/dts/sprd/ums9620.dtsi, include/asm/{cpufeature.h,lse.h}, kernel/{alternative.c,cpufeature.c,smp.c,t508n_dbg.c}}`、`drivers/of/base.c`、`init/main.c`、`kernel/{sched/core.c,smp.c,stop_machine.c}`、`mm/page_alloc.c` 🚧 |
| **5.4 复刻树** | `t508/src/linux-5.4-custom`，HEAD `c4bd9f7ae`（framebuffer 探针移植）+ 2 个未提交 ✅ |
| 干净 6.12 基线 | `t508/src/linux-6.12-clean` |
| 原厂源码树 | `t508/src/stock-5.4.210`、`t508/src/vendor-kernel-ums9620`（git，5.8 GB） |
| 构建产物 | `t508/build/images/{mainline_Image, stock54-custom_Image}`、`t508/build/boot-images/*.img`、`t508/build/initramfs/initramfs.cpio`、`t508/build/boot-artifacts/`（dtbo、vendor_boot 改版） |
| 固件/证据 | `t508/firmware/stock-images/`（78 分区镜像）、`firmware/stock/`（`stock_kallsyms.map`、`stock_Image_full.bin`、原厂 uboot）、`firmware/device-dumps/`、`firmware/payloads/` |
| 刷写工具 | `t508/tools/`（`sprd_mkbootimg.py`、`build_noop_dtbo.py`、`patch_vendor_boot.py`、`avbtool.py`…）、SP 下载模式 `t508/work/srag/t508n-v6/`（spd_dump + fdl1/fdl2） |
| 技能 | `.dsh/skills/t508n-flashing.md`（**已按新路径更新**）：fastboot 常规分区 + SP 下载模式读写 uboot + 关键坑 |
| 调试技能/指南 | `docs/SKILL-T508N-framebuffer-debug.md`（探针）、`docs/无串口验证指南.md` |
| 存储/Alpine | eMMC 分区化（super=/、userdata=/home），`build/packages/alpine-minirootfs-3.24.1-aarch64.tar.gz` ✅ |
| USB gadget | ❌ 不可达（TEE 墙） |

## 3. 目录地图（2026-09-29 重组后）

```
t508/
├── README.md                       项目总览（设备事实、当前状态、快速上手）
├── docs/
│   ├── HANDOFF.md                  ← 本文件
│   ├── FINAL-BRINGUP-INDEX.md      ★ 自包含最终综合索引（主入口，先读）
│   ├── INDEX.md                    资料逐份说明（reports/prompts/session）
│   ├── AGENTS-GUIDE.md             历史工作区指引
│   ├── KERNEL-适配方案.md、WORK-适配材料清单.md
│   ├── SKILL-T508N-framebuffer-debug.md、无串口验证指南.md
│   ├── reports/                    ★ 53 篇分析/实验报告（含 SUMMARY-ALL-ANALYSIS、UNISOC-DETAILED、PATCH-*、HANDOFF-3-SESSIONS）
│   ├── prompts/                    36 篇任务书
│   └── session/session.jsonl       原始会话（20,340 行，勿重读）
├── src/                            13 棵源码树（linux-6.12.103 / linux-5.4-custom / stock-5.4.210 / initramfs / tools…）
├── firmware/                       stock-images / stock / device-dumps / payloads / footers（约 21 GB）
├── build/                          images / boot-images / boot-artifacts / initramfs / packages
├── tools/                          打包、刷写、DTBO、AVB 工具
└── work/                           disasm（DISASM-A..N + dump）、srag（spd_dump 工具链）、日志、截图、证据
```

## 4. 关键事实速查

| 项 | 值 |
|---|---|
| SoC / 板级 | Unisoc **T770 / UMS9620**（QogirN6Pro）/ **OT5**（FIH 代工） |
| CPU | **4×Cortex-A55 + 3×Cortex-A76 + 1×Cortex-A76**（CPU7 独立簇；stock DTS 4+3+1） |
| 内存 / 存储 | 3 GB LPDDR4x；eMMC（A/B 双槽 + `super` 动态分区） |
| 显示 / 探针 | 320×480 LCD，**framebuffer 基址 `0xb0000000`**，32bpp BGRA（探针画在这） |
| 原厂内核 | 5.4.210（Unisoc fork，clang 12 / LTO / KCFI）；vermagic **`5.4.210-android12-9-ga1d5c173d86b-ab1`** |
| stock 反汇编映射 | `_text = 0xffffffc010080000`，`file_off = vaddr - 0xffffffc010080000` |
| stock 符号表 | `t508/firmware/stock/stock_kallsyms.map`（**116,704 个原厂符号，唯一正确**；旧 `System.map` 不是 stock 的） |
| stock uboot | `t508/firmware/stock/uboot_t508n_lk_orig.bin`，md5 `7fe2b5eaeb68b9b7792ef806ce553e61` |
| 一致性要点 | stock **无** CCI/CMN/DSU/SCU 初始化（不要加）；二级核等 alternatives flag 是纯 `ldr + yield` |
| TEE 墙 | USB IPa 域 / RPMB / 安全显示 / 密钥烧写受 SIP 固件管辖 —— 日常使用零影响，USB gadget 撞墙 |
| 无串口验证 | `uboot_log` 分区（`adb shell su -c "dd if=/dev/block/by-name/uboot_log …"`）+ pstore `/sys/fs/pstore/console-ramoops-0` |
| ⚠️ 时间差提醒 | 旧探针坐标（y=120/370/390/410/430/440/450）与旧观测属旧构建语义，**已失效**；当前探针布局见 FINAL-BRINGUP-INDEX §5.2 |

## 5. 常用命令

```sh
# —— 工具链
export PATH=/home/test/t508n/shared/toolchains/arm-gnu-toolchain-13.2/bin:$PATH

# —— 5.4 复刻（配方见 docs/reports/T508N-VENDOR-BUILD-RECIPE.md）
cd /home/test/t508n/t508/src/linux-5.4-custom
export LOCALVERSION=                      # 必须为空，见红线
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- olddefconfig prepare
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8

# —— 6.12 魔改树（当前卡点树；构建脚本在 t508/tools/）
cd /home/test/t508n/t508/src/linux-6.12.103
# 参考 t508/tools/build_kernel.sh / build_boot_linux.sh / build_initramfs.sh 的用法

# —— 打包 boot_a（私钥勿外传）
python3 /home/test/t508n/t508/tools/sprd_mkbootimg.py --kernel <Image> --ramdisk <initramfs.cpio> \
  --output boot_54vendor.img --key /home/test/t508n/t508/tools/ot5_boot_key.pem

# —— 刷入（fastboot 常规分区）
/mnt/c/platform-tools/fastboot.exe flash boot_a boot_54vendor.img
/mnt/c/platform-tools/fastboot.exe flash boot_b boot_54vendor.img      # 视需要
/mnt/c/platform-tools/fastboot.exe reboot

# —— DTBO 安全网（merge 失败会 panic 无限重启）
python3 /home/test/t508n/t508/tools/build_noop_dtbo.py                 # 生成 noop_dtbo.img

# —— SP 下载模式（读写 uboot/LK；技能 t508n-flashing 有完整流程）
cd /home/test/t508n/t508/work/srag/t508n-v6
./spd_dump.exe --wait 300 exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 \
               fdl fdl2-dl.bin 0xb4fffe00 exec r uboot reset               # 读（备份）
# 写回：把 r uboot 换成 w uboot <镜像>；偶发 usb_send failed 直接重试

# —— 采证
sh /home/test/t508n/t508/tools/collect_evidence.sh
```

> 完整刷写流程（含 vendor_boot/dtbo/vddusb33 与坑）以技能 `.dsh/skills/t508n-flashing.md` 为准。

## 6. 已修复清单（别再重做）

FINAL-BRINGUP-INDEX §3 列了 13 项，摘要如下（细节与证据回查那里）：

1. 恢复完整 `apply_alternatives_all()`（系统级 alternatives 曾整体被跳过）；
2. **LSE 补丁改回原厂 5.4 式 static_key/jump_label**（6.12 的 asm-goto NOP 落入是头号嫌疑）→ SM#2 稳定；
3. `set_zone_contiguous` 加 NOMAP 检查（否则穿过 sipc-mem/modem 空洞）；
4. `__apply_alternatives_multi_stop` 同步协议改原厂式（yield 轮询 + 双保险 I-cache 失效）；
5. `CONFIG_DEVTMPFS=n`（对齐原厂，消除 devtmpfsd 跨簇挂点）；
6. `NR_CPUS=32`、`NUMA=n`、`SCHED_SMT=n`（对齐原厂语义）；
7. `CONFIG_KVM=n`（nvhe 镜像引不到 static_key）；
8. errata 4118414/1530923 关（与原厂同口径）；
9. DTS 恢复 stock 三簇拓扑 + capacity/DPC（CPU7 独立簇）；
10. `of_core_init` phandle_cache 写法改 5.4 式 → 948 节点通过；
11. memory_dev_init 顺序补丁（**当前树已回退/未保留**）；
12. `page_alloc_init_late` 段已通过（NOMAP 修复后）；
13. LSE early patch 时机/范围（并入 #2）。

## 7. 跨项目借鉴（T508N ← V50）★

同一工作区的 **V50/F50 项目**（同 SoC：UMS9620 / QogirN6Pro）已经把"官方 5.4 源码 → boot 镜像 → 可用系统"
整条链跑通，其中**启动链**部分对 T508N 有直接可借鉴项。完整清单（含文件路径与证据）见
[../../docs/cross/REPORT-mu300-linux-对T508N价值评估.md](../../docs/cross/REPORT-mu300-linux-对T508N价值评估.md)
**§10（2026-09-29 补）**；摘要：

| 可借鉴 | V50 侧证据（`v50/src/mu300-linux/`，官方树在 `v50/src/zte-oss/mu3351/`） | 对 T508N 的用处 |
|---|---|---|
| boot 镜像 4 条硬约束：复用原厂 header v4 + 原 AVB footer、`signature_size=0`、**LZ4 legacy** ramdisk、总长==原厂且不越预留区 | `boot/build-boot-image.py` | 对照 `t508/tools/sprd_mkbootimg.py` 的打包流程；尤其 ramdisk 必须 LZ4（gzip 会让 5.4 走 `/dev/ram0` 并 panic） |
| `misc` 32 字节 `BCAB` 槽位语义：prio/tries/CRC32；LK 把 `tries==1 && !successful` 当失败 → 一次性试验写 `tries=2` | `boot/init`、`boot/flash-trial.sh` | T508N 也是 A/B：可照这个写法做"显式 BC + 一次性试验 + 自动回退"，不再靠 fastboot 猜槽位 |
| 无 UART 日志两法互补：V50 把 8 MiB 持久日志写进 `boot_b` 的 48 MiB 处；T508N 有 `uboot_log` + `ramoops@fff80000` | `boot/init` 的 `persist()` | 探针回答"执行到哪一步"，持久日志回答"为什么停"；两边 pstore 同为 `0xfff80000` 可作平台约定旁证 |
| DTS `no-map` 对照：T508N 自己把 `sipc-mem@87800000`/`cp-modem@88000000`/`iq-mem`/`ch-ddr` 标 `no-map`，**官方平台 DTSI 里这些区域没有 `no-map`**（只有 `rebootescrow@fff70000` 是） | `arch/arm64/boot/dts/sprd/ums9620-mach.dtsi` | 给 `set_zone_contiguous` / `pfn_valid` 那处修改提供"厂商本来怎么声明"的证据；结论仍是保留 `no-map` + 修 `pfn_valid`（更安全） |
| stock 5.4 **双树对比法**：5.4.210（T508N stock）vs 5.4.254（V50 官方）逐文件 diff —— 差异=上游演进，共同点=紫光魔改稳定部分 | `arch/arm64/kernel/{alternative.c,cpufeature.c,smp.c,smp_spin_table.c}`、`include/asm/lse.h` | 校验 6.12 复刻是否只保留了"语义必需"的魔改，避免把上游行为误当 OEM 定制 |
| 模块顺序**单文件化**：`boot/module-order.txt`（86 项）是 initramfs 的唯一加载顺序来源 | `boot/module-order.txt`、`boot/build-boot-image.py` | 收敛 T508N 的 `initramfs/init.c mods[]` 与 `modules.load-final`，消除两处顺序漂移（配合交叉报告 §3.1 的"加载序倒置"结论） |
| 工程制度：`FINDINGS.md`（每个 workaround 记成因）、`docs/INDEX.md` 的"已验证 vs 待真机"表、三处写入+逐步校验纪律 | `v50/src/mu300-linux/docs/` | 直接搬格式，降低交接成本 |

**反向也有价值**：T508N 的 6.12 修复（LSE static_key、NOMAP `pfn_valid`、alternatives multi_stop 协议、
`NR_CPUS/NUMA/SCHED_SMT` 对齐）对 V50 的 `upstream/` mainline 6.18 移植有用；LCD 探针法适用于带屏机型。

⚠️ 仍然**不能照搬**：内核版本错位（5.4.**210** vs 5.4.**254**）、保留内存起址与布局不同、
断电根因不同（LK 关 vddusb33 vs PM 协处理器 watchdog）、TEE 结论不可互用 —— 见该报告 §7。

## 8. 下一步候选（FINAL-BRINGUP-INDEX §10 的优先级排序）

> 用户已暂停修改；恢复时**先纯观测、后改动**，一次一个变量。

1. **SGI/IPI 计数诊断**（`reports/PATCH-diag-sgi-kfence.md` P1–P3）：发送侧 vs 接收侧按核对照；
   delta>0 且持续 = "发了没到"的直接证据；每核心跳区分"核死 vs 收不到 IPI"。**先于一切改动做**。
2. **KFENCE**（同文件 K1–K3）：`CONFIG_KFENCE=y`（sample=100，对齐原厂）+ pstore 通道 +
   fault 画屏挂钩；`KFENCE_STATIC_KEYS` 必须 **n**。查"内存被踩"（当前头号嫌疑）。
3. **死循环门 A/B**（同文件层 2）：原厂门等价移植，把疑似核"可控弄死"复现竞态。
4. **5.4 复刻稳定性多次验证**：`boot_54custom_probe.img` 反复 boot，排除"碰巧能过"，作为长期对照基线。
5. **kick_ilb 单变量重试**（`reports/PATCH-nohz-kick.md`）：先"3 文件补丁 + NO_HZ_IDLE 仍 =n"，再打开。
6. **P1 实验严格性复核**（`REPORT-CURRENT-RACE-ANALYSIS.md` §6.8）。
7. **SLUB/LSE A/B**：`slub_debug=U` 一轮；`t508n_apply_lse_early` 不 enable static_key 一轮。
8. **诊断盲区**：`CONFIG_SOFTLOCKUP_DETECTOR=y`（stock 开、mainline 关）+ `DEBUG_LIST/OBJECTS/MUTEXES/SPINLOCK` 一轮。
9. **二分隔离**：`maxcpus=4` / `nohz=off` / `cpuidle.off=1` / `maxcpus=1` 对照。
10. **DTS 完整对齐**（`REPORT-A76-requirements.md` D4）：尚缺 cpufreq 域/idle-states 挂接（短期可接受）。
11. **杂项收尾**：`t508n_sm2_done` 改 release/acquire；读 MIDR_EL1 画屏定 A76 revision；
    `icache_inval_all_pou` 双保险 vs "不用无条件 `ic ialluis`" 红线的文档/代码对齐。
12. **跨簇唤醒机制侧**（若确诊 CSD 失效）：P1（`ttwu_queue_cond` 恒 false）或恢复 5.4 式 wake_list 承载。

## 9. 文档地图（按阅读顺序）

1. 本文件（HANDOFF）
2. [FINAL-BRINGUP-INDEX.md](FINAL-BRINGUP-INDEX.md) ★ —— **自包含主入口**：时间线、已修 13 项、
   5.4 成功记录、当前状态、文档索引表、关键结论、紫光魔改/config 速查、材料工具速查、未来方向
3. [reports/SUMMARY-ALL-ANALYSIS.md](reports/SUMMARY-ALL-ANALYSIS.md) —— 全部分析的汇总（FINAL 的上游）
4. [reports/UNISOC-DETAILED.md](reports/UNISOC-DETAILED.md) —— 紫光魔改 / config / 编译的权威参考
5. 诊断与补丁：`reports/PATCH-diag-sgi-kfence.md`、`PATCH-nohz-kick.md`、`PATCH-mem-sysfs-order.md`、
   `REPORT-CURRENT-RACE-ANALYSIS.md`、`REPORT-LSE-atomic-completion-analysis.md`
6. 其他 53 篇报告 + 36 篇 prompt：`reports/`、`prompts/`（逐份说明见 [INDEX.md](INDEX.md)）
7. 调试方法：[SKILL-T508N-framebuffer-debug.md](SKILL-T508N-framebuffer-debug.md)、[无串口验证指南.md](无串口验证指南.md)
8. 材质清单：[KERNEL-适配方案.md](KERNEL-适配方案.md)、[WORK-适配材料清单.md](WORK-适配材料清单.md)
9. 刷写：技能 `.dsh/skills/t508n-flashing.md`
10. 历史交接：`reports/HANDOFF-3-SESSIONS.md`（2026-08-17 的三会话计划，**已被本文件取代**）、
    `reports/FINAL-WRAPUP-20260821.md`、`reports/ARCHIVE-20260823.md`
11. ★ **跨项目借鉴**：[../../docs/cross/REPORT-mu300-linux-对T508N价值评估.md](../../docs/cross/REPORT-mu300-linux-对T508N价值评估.md)
    （§10 是 2026-09-29 补的启动链清单）、[../../v50/docs/HANDOFF.md](../../v50/docs/HANDOFF.md)
11. 工作区规范：[../../docs/WORKSPACE.md](../../docs/WORKSPACE.md)、路径对照 [../../docs/PATH-MAPPING.md](../../docs/PATH-MAPPING.md)

## 10. 给新会话的第一条消息（可直接粘贴）

```
项目：TCL T508N（OT5 / Unisoc T770 / UMS9620）mainline 6.12 bring-up。
请先读 t508/docs/HANDOFF.md 与 t508/docs/FINAL-BRINGUP-INDEX.md；不要读 session.jsonl。
本次目标：<从 HANDOFF §8 选一个，例如"先做 SGI/IPI 计数诊断，纯观测，不改功能">
约束：无 UART/无 USB，只能 LCD 探针(fb 0xb0000000)+fastboot+pstore；a 槽试验、b 槽保官方；
      绝不刷空 trustos/teecfg；5.4 路线 LOCALVERSION 必须为空；一次只改一个变量，
      每步给出"探针坐标/观测结果"作为证据，并标注哪些结论未验证。
```
