# REPORT-SGI-IPI-COUNTS-20261005 — SGI/IPI 计数诊断层（纯观测）实施记录

> 目标选择：HANDOFF §8 候选 1「SGI/IPI 计数诊断（PATCH-diag-sgi-kfence.md P1–P3，纯观测优先）」。
> 目标内核：`/home/test/t508n/t508/src/linux-6.12.103`（当前 6.12 魔改树，HEAD `cb8e725f3` + 未提交改动）。
> 原则：只加计数/心跳/画屏，不 arm 死循环门，不改调度/中断/内存语义；一次只改这一个诊断变量。
> 生成：2026-10-05。**未刷机、未读屏；所有"观测结论"均为未验证。**

---

## 0. 先记一条与 HANDOFF 时间差有关的现实约束（重要）

HANDOFF/FINAL-BRINGUP-INDEX 的候选清单停留在 2026-08-18；后续报告已经推进到更精确的卡点：

- `reports/FLASH-20260821-P1.md` §6–§7：屏幕像素级读数显示 **stage 0–8 亮、stage 9 不亮，
  仅 y=175 CPU1 格亮，y=180/185/190/195 全缺**。
- `reports/REPORT-SML-psci-cpu-on.md`：反汇编 EL3 SML 后的结论是 **mainline 的 SMC64 CPU_ON
  确实进入 PSCI 分发**，SML 存在每核握手状态机；但 CPU_ON 处理器本体受 SML 文件不完整限制未
  100% 定位。
- `reports/FINAL-WRAPUP-20260821.md` §4：最终卡点定位为
  **`__cpu_up(1)` 的 `boot_secondary(1)`（PSCI CPU_ON SMC）不返回**。

因此本 SGI/IPI 计数层**不能直接观测当前 SMC 挂死**：CPU1 从未执行到 `secondary_start_kernel`，
更不会执行 `do_handle_IPI()`。本记录的定位是：

1. 按 HANDOFF 候选 1 完成一个**可复用、低干扰的发送/接收计数基线**；
2. 在当前卡点下，预期它只会给出"CPU0 进入 SMC 前可能还有少量 IPI；CPU1–7 无接收"的阴性结果；
3. **不能据此否定/确认 SGI 丢失**；真正的下一步仍是 SML 完整镜像 / `secondary_start_kernel`
   入口协议对比（见 §7）。

---

## 1. 改了什么（仅观测层）

| 文件 | 内容 |
|---|---|
| `arch/arm64/include/asm/t508n_dbg.h` | 新增 `T508N_IPI_RESCHEDULE/CALL_FUNC`、`t508n_ipi_sent/rx/heartbeat` 原型 |
| `arch/arm64/kernel/smp.c` | `smp_cross_call()` 发送钩子；`do_handle_IPI()` 的 RESCHEDULE/CALL_FUNC 接收计数 |
| `arch/arm64/kernel/t508n_dbg.c` | 计数数据、全局发送计数、每核接收计数、fb 位图刷新、250 ms 心跳 |
| `kernel/time/timer.c` | `run_local_timers()` 首行调用 `t508n_ipi_heartbeat()`（`CONFIG_ARM64` 下） |

**没有加入**的内容：
- 没有 arm Unisoc 死循环门（`while(1)` / `/proc/t508n_ipi_gate`）；
- 没有 `t508n_ipi_gate_armed/spin`；
- 没有 KFENCE、pstore、CMDLINE 改动；
- 没有改 `ttwu_queue_cond`、LSE、NOMAP、scheduler、stop_machine 等既有变量。

补丁归档：`t508/work/sgi-ipi-counts.diff`。

---

## 2. 探针坐标（已避开既有探针）

### 2.1 全局发送计数行

- **y=140，x=8 起，块间距 6 px、块宽 4 px、块高 6 px**
  - 块 0–7：`IPI_RESCHEDULE` 发送次数 & 0xFF（bit0 最左）
  - 块 8–15（x=8+8*6=56）：`IPI_CALL_FUNC` 发送次数 & 0xFF
- 仅 boot CPU 在心跳中刷新；多核同像素竞争已避免。

### 2.2 每核接收/delta/心跳行

| CPU | y | 说明 |
|---|---|---|
| CPU0 | 170 | x=160 起，10 个位块 |
| CPU1 | 190 | 与既有 `boot_secondary 返回` y=190 探针同 y，但既有探针在 x=8，本诊断从 x=160 起，不重叠 |
| CPU2 | 230 | 避开 of_core y=210 |
| CPU3 | 250 | — |
| CPU4 | 270 | 避开 driver_init y=260（260–269），从 270 开始 |
| CPU5 | 290 | — |
| CPU6 | 310 | 避开 memory/sysfs y=300（300–309），从 310 开始 |
| CPU7 | 330 | 避开 y=340 行；y=340 是既有 memory 探针 |

每核行 10 个位块的位义（从 `x=T508N_IPI_ROW_X` 即 x=160 起，bit0 最左）：

| 块 | x | 含义 |
|---|---|---|
| 0–7 | 160 + i*6 | 本核收到 RESCHEDULE+CALL_FUNC 总数 & 0xFF |
| 8 | 160+8*6=208 | `sent_to[cpu] > rx[cpu]`，未消化 delta > 0 |
| 9 | 160+9*6=214 | 本核心跳：每 250 ms 翻转（仅本核 tick 活着并进入 `run_local_timers()` 才会重绘） |

刷新节流：每核 16 jiffies（HZ=250 ⇒ 250 ms）至多刷一次；全局行也随 boot CPU 心跳刷新。

---

## 3. 判定矩阵（仅在刷机读屏后才成立）

| 屏幕形态 | 判定 | 备注 |
|---|---|---|
| CPUy 行 rx 继续增长、delta=0、心跳翻转 | 投递正常 | — |
| CPUy 行 rx 冻结、delta=1、心跳仍翻转 | 该核活着但发给它的 IPI 未被消化 | 支持"发了没到/到了没处理"；需持续观察 |
| CPUy 行 rx 冻结、心跳熄灭 | 该核没有 tick（死/长关中断） | 仅能说明 tick 停止 |
| CPUy 行整体全黑 | 该核从未进入 `run_local_timers()` | 当前 CPU1–7 的预期形态 |
| 全局发送块增长而所有 CPU 行不增长 | 发送侧在发、接收侧没到 | 需排除 offline/未上线目标 |
| 全局发送块也不增长 | 当前阶段没有 RESCHEDULE/CALL_FUNC 流量 | CPU0 已进 SMC 挂死时的预期 |

**当前卡点的预期读数（未验证）**：CPU0 行在进入 `psci_cpu_on(1)` 前可能还有少量心跳；
一旦 CPU0 卡在 SMC，CPU0 行心跳也冻结；CPU1–7 行不会出现任何接收/心跳。
这与"CPU1 未启动"一致，不能区分"CPU1 没收到 IPI"与"CPU1 从未到 IPI 处理点"。

---

## 4. 构建证据（已在本机完成）

```sh
cd /home/test/t508n/t508/src/linux-6.12.103
export PATH=/home/test/t508n/shared/toolchains/arm-gnu-toolchain-13.2/bin:$PATH
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- -j8 Image
```

- 构建日志：`t508/work/build-sgi-counts.log`
- `arch/arm64/boot/Image` md5：`6ffcd2d602e67f3f6d20d3429d8ac6c2`
- `build/images/mainline_Image` md5：同上
- vmlinux 符号核对（`nm`）：
  - `t508n_ipi_sent`
  - `t508n_ipi_rx`
  - `t508n_ipi_heartbeat`
  - `t508n_ipi_sent_cnt` / `t508n_ipi_sent_to`
  - `t508n_ipi_rx_resched` / `t508n_ipi_rx_call`
- 构建只有 2 个既有 warning（`t508n_paint_grid`、`t508n_apply_lse_early` 缺原型），与本补丁无关。

### 4.1 打包 boot_a 试验镜像

必须使用**最小 busybox initramfs**（`build/initramfs/initramfs-minimal.cpio`），而不是
105 MB Alpine initramfs，也不是 stock/Magisk 的 `boot_ramdisk_cpio.bin`：

```sh
cd /home/test/t508n/t508
python3 tools/sprd_mkbootimg.py \
  --kernel build/images/mainline_Image \
  --ramdisk build/initramfs/initramfs-minimal.cpio \
  --output build/boot-images/boot_linux_sgi.img \
  --key tools/ot5_boot_key.pem
```

- 产物：`t508/build/boot-images/boot_linux.img` / `boot_linux_sgi.img`
- 大小：67,108,864 B（64 MB 分区）
- md5：`97231132ccd1c58188c18458055de677`
- 内嵌 kernel md5：`6ffcd2d602e67f3f6d20d3429d8ac6c2`（与源 Image 一致）
- 内嵌 ramdisk：cpio `1,205,760 B`，md5 `a0dfd66a71355333b36a228baae4306a`，
  解包后与 `build/initramfs/initramfs-minimal.cpio` 完全一致。
- AVB `info_image`：`Image size 67108864`、`Original image size 45656110`、`Hash descriptor` 正常。

---

## 5. 当前无法完成的部分（未验证项，必须显式标注）

| 项 | 状态 |
|---|---|
| `fastboot flash boot_a boot_linux_sgi.img` | **未执行** |
| 当前槽位/设备在线确认 | **未执行**；本机 `fastboot devices` / `adb devices` 均无设备 |
| LCD 拍照/像素判读 | **未执行** |
| pstore / uboot_log 采证 | **未执行** |
| 计数层是否改变时序、是否触发"加探针能过" | **未验证**；这是本诊断的固有风险 |
| SGI 丢失是否成立 | **未验证**；本机无法从当前屏幕读数下结论 |

按项目红线，若后续有设备：

1. 先确认当前槽位与安全网，**只刷 a 槽 boot_a**；
2. 不碰 `vendor_boot_a`、`dtbo_a`、`trustos_a`、`teecfg_a`；
3. 读屏按 §2 坐标；
4. 回官方系统后拉 `uboot_log` 与 `/sys/fs/pstore/console-ramoops-0`；
5. 先跑一次不 arm 门的纯观测，保留 gating A/B 作为下一单变量。

---

## 6. 与 HANDOFF/PATCH 原文的差异

- `PATCH-diag-sgi-kfence.md` 的 P1 含默认关闭的 Unisoc 死循环门与 `/proc/t508n_ipi_gate`；
  本次为满足"纯观测、不改功能"**未移植门**。
- `PATCH-diag-sgi-kfence.md` 的 P3 建议每核行 y=170+i*20；本树既有 y=190/210/260/300/340 等探针，
  逐行核对后改为 y=`170,190,230,250,270,290,310,330` 且 x 从 160 起，避免覆盖既有证据。
- `PATCH-diag-sgi-kfence.md` 的 P3b 直接在 `t508n_dbg.c` 使用 `IPI_CALL_FUNC`；该枚举只在
  `arch/arm64/kernel/smp.c` 内可见，直接编译会失败。本次改为在 `t508n_dbg.h` 定义
  `T508N_IPI_RESCHEDULE/CALL_FUNC` 稳定 ABI 编号，调用点由 smp.c 负责映射。

---

## 7. 基于最新证据的真正下一步（建议，未执行）

当前 6.12 卡点是 CPU1 的 `psci_cpu_on(1)` SMC 不返回，SGI 计数层无法直接解决。优先级建议：

1. **补全 SML 256 KB 镜像**：`REPORT-SML-psci-cpu-on.md` 指出分发查表表
   `0xbd026f18` 超出当前 `payloads/sml_a.bin` 的 137 KB；补齐后定位 index 0x44 的 CPU_ON handler。
2. **对比 stock 5.4 与 mainline 6.12 的 `secondary_start_kernel` 入口协议**：
   CPU_ON 传入的 entry point、EL3→TrustOS→非安全内核交接路径是否一致。
3. 若仍要保留 SGI 计数层：只在刷入当前 `boot_linux.img` 后读取 §3 矩阵，作为"CPU1 未到
   内核"的旁证，而不是根因证据。

---

## 8. 产物落位说明（2026-10-05 补）

- 新诊断镜像同时落了规范名：
  - `t508/build/boot-images/boot_linux.img`（md5 与 `boot_linux_sgi.img` 相同）
  - `t508/build/boot-images/boot_linux_sgi.img`
- 替换前的旧 `boot_linux.img` 已备份为：
  - `t508/work/boot_linux_pre-sgi-backup.img`
- 这样按项目既有命令 `fastboot flash boot_a boot_linux.img` 就会刷到本次 SGI/IPI 诊断内核；
  但**本记录生成时仍未刷机**。

---

## 9. 最新完整 boot 包（2026-10-05 补）

在 §4.1 修正为最小 busybox initramfs 后，重新打包并补齐 vendor_boot：

| 文件 | 内容 | 关键校验 |
|---|---|---|
| `build/boot-images/boot_linux.img` | 最新 6.12.103 诊断内核 + 最小 initramfs | md5 `97231132ccd1c58188c18458055de677` |
| `build/boot-images/vendor_boot_linux.img` | stock `vendor_boot_a` + 最新三簇 DTB `firmware/stock/ot5-new.dtb` | md5 `bfdf92a6356edea4ba6f35dc16759dc0` / sha256 `7c8f112ceaa4582629ca33f47256284aa225ac610b8595c7847c8cabdee2c437` |
| `build/boot-artifacts/noop_dtbo.img` | 空 overlay DTBO | sha256 `b8b1c586e8dc095831cac648bb62a312ff3bfcd3002bf340845c3170ef1acf5d` |

- vendor_boot 写入的 DTB 已回读校验：与 `firmware/stock/ot5-new.dtb` 完全一致，尾部零填充。
- 完整包目录：`build/boot-images/t508n-linux-boot-latest/`
- 压缩包：`build/boot-images/t508n-linux-boot-latest.tar.gz`
  - sha256 `67f88b060d486dbf30984113534a4964c1921d46500c4b59ec351b6a68c22877`
- 包内 `FLASH.txt` 明确要求：b 槽保留官方；先只刷 `boot_a` 单变量；
  `vendor_boot_a` / `dtbo_a` 必须作为后续单独变量，且绝不刷 `trustos_a` / `teecfg_a`。
- 本包**未执行任何刷机**。
