# T508N — TCL T508N（OT5 / Unisoc T770 / UMS9620）Linux 移植

> 把 TCL T508N（板级代号 **OT5**，FIH 代工）从 Android 改造为 **纯 Linux** 的研究项目。
> 核心目标：把 **mainline 6.12.103** 跑到 initramfs `/init` 执行（外设不在本阶段范围）。

> **新会话先读 [docs/HANDOFF.md](docs/HANDOFF.md)**（交接入口：状态、红线、命令、下一步、
> 给新会话的第一条消息）；深挖时再进 [docs/FINAL-BRINGUP-INDEX.md](docs/FINAL-BRINGUP-INDEX.md)（自包含主索引）。

## 1. 设备事实（已核实）

| 项 | 值 |
|---|---|
| 型号 / 板级 | TCL T508N / **OT5**（`ro.cta.hardware.model=T508N`） |
| SoC | Unisoc **T770 / UMS9620**（QogirN6Pro） |
| CPU | **4×Cortex-A55 + 3×Cortex-A76 + 1×Cortex-A76**（CPU7 独立簇，stock DTS 4+3+1） |
| 内存 / 存储 | 3 GB LPDDR4x；eMMC（A/B 双槽 + super 动态分区） |
| 原厂系统 / 内核 | Android 13；**5.4.210**（Unisoc fork，clang 12 / LTO / KCFI） |
| 显示 | 320×480 LCD，framebuffer 基址 `0xb0000000`，32bpp BGRA |
| 调试口 | **无 UART、无可用 USB**；只有 LCD framebuffer 探针 + fastboot + pstore |

**调试方法**：往内核里塞“探针”（往 framebuffer 画进度条/标记），拍照读屏判断执行到哪一步；
配合 A/B 双槽（b 槽保官方系统，a 槽试验）做安全网。详见
[SKILL-T508N-framebuffer-debug.md](docs/SKILL-T508N-framebuffer-debug.md)、
[无串口验证指南.md](docs/无串口验证指南.md)。

## 2. 当前状态

| 路线 | 状态 |
|---|---|
| 原厂式 5.4 复刻（5.4.210 + 紫光魔改 + 原厂 config） | ✅ **能 boot 到 initramfs**（`exec /init` 探针 y=460 亮）——证明原厂式路径在 T770 可行 |
| mainline 6.12.103 | 🚧 8 核 bringup 完成后卡住（进度条稳定 9 格 + y=130 不亮）；**用户已决定暂停继续修改** |
| 存储 / Alpine rootfs | ✅ eMMC 分区化（super=/，userdata=/home），Alpine 3.24.1 aarch64，chroot 启动 |
| USB gadget（虚拟串口） | ❌ 未达成，全部路线已穷尽（IPa 电源域受 TEE/SIP 固件管辖） |

> 权威入口是 [docs/FINAL-BRINGUP-INDEX.md](docs/FINAL-BRINGUP-INDEX.md)（自包含最终综合索引），
> 继续工作前先读它，不要重读 20,340 行的 `session.jsonl`。

## 3. 目录结构

```
t508/
├── README.md           # 本文件
├── docs/               # 资料：报告 / prompt / 会话归档
│   ├── INDEX.md        #   资料总索引（逐份说明）
│   ├── FINAL-BRINGUP-INDEX.md   # ★ 最终综合索引（主入口）
│   ├── AGENTS-GUIDE.md          # 工作区指引（历史脉络）
│   ├── KERNEL-适配方案.md
│   ├── SKILL-T508N-framebuffer-debug.md
│   ├── WORK-适配材料清单.md
│   ├── 无串口验证指南.md
│   ├── reports/        #   50 篇分析/实验报告
│   ├── prompts/        #   33 篇任务书
│   └── session/        #   session.jsonl（原始会话，勿重读）
├── src/                # 源码（详见 src/README.md）
│   ├── linux-6.12.103/         # ★ mainline 主开发树（HEAD cb8e725f3 + 未提交改动）
│   ├── linux-6.12-clean/       # 干净 6.12 基线（无紫光魔改）
│   ├── linux-5.4-custom/       # 5.4.210 + 紫光魔改复刻树（git）
│   ├── linux-5.4.210/          # 纯净 5.4.210
│   ├── stock-5.4.210/          # 原厂内核源码树
│   ├── vendor-kernel-ums9620/  # 原厂 vendor 内核工作树（git，5.8 GB）
│   ├── t508n-repo/             # 归档仓库（git: t508n_kernel）
│   ├── fih_OT5_device_tree/    # LineageOS 设备树（OT5）
│   ├── twrp_OT5_device_tree/   # TWRP 设备树（OT5）
│   ├── initramfs/              # initramfs 源码（init.c 等）
│   ├── port-skeleton/          # USB 驱动骨架
│   ├── lk-upstream-ref/        # LK 引导上游参考
│   ├── busybox-1.36.1/         # busybox 源码
│   └── tarballs/               # 源码压缩包
├── firmware/           # 原厂固件/镜像/证据（详见 firmware/README.md，约 21 GB）
├── build/              # 构建产物（详见 build/README.md）
├── tools/              # 打包/刷写工具（sprd_mkbootimg.py 等）
└── work/               # 分析工作区（详见 work/README.md）
```

## 4. 关键结论（省时间）

* **boot 打包**：`LOCALVERSION=` 必须为空，vermagic 需等于原厂 `5.4.210-android12-9-ga1d5c173d86b-ab1`，
  否则原厂模块无法加载；改 config 后必须 `olddefconfig`。
* **DTBO**：DTBO merge 失败会导致 LK panic 无限重启；用空 overlay（`noop_dtbo.img`）绕过（见 `无串口验证指南.md`）。
* **符号表**：`t508/work` 里的 `stock_kallsyms.map`（116,704 个原厂符号）才是对的；
  旧 `System.map` 不是 stock 的。
* **stock 反汇编映射**：`_text = 0xffffffc010080000`，`file_off = vaddr - 0xffffffc010080000`。
* **一致性**：stock 无 CCI/CMN/DSU/SCU 初始化，不要加；二级核等待 alternatives flag 是纯 `ldr + yield`。
* **TEE 墙**：USB IPa 域 / RPMB / 安全显示 / 密钥烧写 / TEE 分区本体受 SIP 固件管辖；
  日常 Linux 使用零影响，但 USB gadget 撞墙。
* **TEE 分区**：勿刷空 `trustos_a`（触发写保护）；恢复用 `firmware/stock-images/trustos_a.img`。

## 5. 快速上手

```bash
# 交叉工具链
export PATH=/home/test/t508n/shared/toolchains/arm-gnu-toolchain-13.2/bin:$PATH

# 原厂式 5.4 内核构建（配方见 docs/reports/T508N-VENDOR-BUILD-RECIPE.md）
export LOCALVERSION=
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- olddefconfig prepare
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8

# 打包 boot_a（私钥在 tools/ot5_boot_key.pem，勿外传）
python3 tools/sprd_mkbootimg.py --kernel <Image> --ramdisk <initramfs.cpio> \
  --output boot_54vendor.img --key tools/ot5_boot_key.pem

# 刷入（fastboot）
fastboot flash boot_a boot_54vendor.img && fastboot reboot
```

刷写完整流程（含 uboot / SP 下载模式）见技能 `t508n-flashing`（`.dsh/skills/t508n-flashing.md`）。

## 6. 相关文档

* [docs/INDEX.md](docs/INDEX.md) — 全部资料逐份说明
* [docs/FINAL-BRINGUP-INDEX.md](docs/FINAL-BRINGUP-INDEX.md) — ★ 最终综合索引
* [src/README.md](src/README.md) — 源码树清单与 git 状态
* [firmware/README.md](firmware/README.md) — 固件镜像与证据
* [build/README.md](build/README.md) — 构建产物
* [work/README.md](work/README.md) — 分析工作区
* 跨项目：[../docs/cross/REPORT-mu300-linux-对T508N价值评估.md](../docs/cross/REPORT-mu300-linux-对T508N价值评估.md)
