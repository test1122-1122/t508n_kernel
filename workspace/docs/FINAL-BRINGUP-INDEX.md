# T508N（Unisoc T770 / UMS9620）mainline 6.12.103 bring-up 最终综合索引

> ⚠️ **2026-10-09 更新**：U-Boot magic64 已实机验证，A 槽运行 vendor 5.4 OpenWrt，
> B 槽已写入 mainline 6.18.55 test5；fastboot 回退 A 的原因已定位为 `misc` B 槽
> unbootable。最新状态先看 [HANDOFF.md](HANDOFF.md) 顶部更新、
> [reports/REPORT-T508N-启动链与校验链.md](reports/REPORT-T508N-启动链与校验链.md)、
> [reports/REPORT-T508N-UBOOT-magic64.md](reports/REPORT-T508N-UBOOT-magic64.md)、
> [reports/REPORT-T508N-BOOT-SLOT-与救砖.md](reports/REPORT-T508N-BOOT-SLOT-与救砖.md)。
> ⚠️ **2026-10-11 更新**：mainline **6.18.55 DWC3 + SSPHY + USB ACM 已打通**，B 槽 test28 在 Windows
> 上枚举为 COM18，可进 mainline shell；p75 (`/dev/mmcblk0p75`) 已挂载到 `/persist` 并 bind 到 `/etc`、`/root`。
> MUSB 路线在 EP0 描述符阶段失败，已放弃。详见
> [reports/REPORT-USB-MAINLINE-DWC3-ACM-SUCCESS.md](reports/REPORT-USB-MAINLINE-DWC3-ACM-SUCCESS.md)
> 与 `mainline-ref/t508n-mainline/USB-DWC3-ACM.md`。
>
> 本文件以下的 2026-08-18 状态作为历史 bring-up 记录保留。

> 生成：2026-08-18（整理子会话）。用途：**未来会话/交接/继续调试的唯一入口文档**——
> 自包含；需要证据链时按 §6 索引表回查原文，不要重读 `session.jsonl`（20,340 行）。
> 依据：主目录 + `work/` 全部 .md 成果（先读 `work/SUMMARY-ALL-ANALYSIS.md`），
> 以及 `linux-6.12.103` 当前 git 工作树实况（已核实）。本文件**只整理，不新增分析**。
>
> ⚠️ 时间差提醒（SUMMARY 文件头，仍有效）：旧探针坐标（y=120/370/390/410/430/440/450）与
> 旧观测（y=450 亮等）属于旧构建语义；当前探针布局见 §5.2。

---

## 1. 项目一句话 + 目标

- **设备**：TCL T508N（板级 OT5，5G 老人机，FIH 代工），SoC **Unisoc T770 / UMS9620**（QogirN6Pro），
  CPU = **4×Cortex-A55 + 3×Cortex-A76 + 1×Cortex-A76**（CPU7 独立簇，stock DTS 4+3+1），3GB LPDDR4x，
  Android 13，原厂内核 **5.4.210**（Unisoc fork，clang 12/LTO/KCFI）。
- **目标**：把 **mainline 6.12.103** 内核跑通到 **initramfs `/init` 执行**（boot 到用户空间）。
  外设（USB/eMMC/显示/PMIC 等）一律不在本阶段范围。
- **调试限制**：**无 UART、无 USB**；只有 **LCD framebuffer 探针（320×480，32bpp BGRA，
  基址 0xb0000000）+ fastboot**；靠 A/B 双槽做安全网（b 槽保官方系统，a 槽试验）。
- **当前结论**：原厂式 5.4 路径（复刻树）已证明在 T770 上**能 boot 到 initramfs（exec /init y=460 亮）**；
  6.12 魔改树仍卡在 **8 核 bringup 之后**（进度条稳定 9 格 + y=130 不亮）。用户已决定**暂停继续修改**。

---

## 2. 里程碑时间线

| 阶段 | 时间 | 关键节点 | 卡点→修复→进展 |
|---|---|---|---|
| 0. 启动链逆向 | 前期 | 识别 T770/UMS9620/OT5；BootROM→SPL→LK→内核启动链；紫光定制（DHTB 签名、LZ4 ramdisk 容器、vendor_boot 带 DTB、dtbo 大端、super 紫光 LP 变体、uboot_log 日志金矿、Magisk/解锁态） | 产出 `REPORT-T508N-启动流程分析.md` |
| 1. mainline 起步 | 中前期 | 三条路线方案（A1 stock+initramfs / A2 原厂源码 / B mainline 移植）；工具链+busybox initramfs+`ums9620-ot5.dts`；boot/vendor_boot 打包与 AVB 自签 | **DTBO merge 失败致 LK panic 无限重启 → `noop_dtbo.img`（空 overlay）修复**（无串口验证指南） |
| 2. 8 核上线 | 中前期 | 二级核 MMU 上线、8 核全部进 idle、smp_init 完成（进度条第 10 格）；EL2 virtual timer（hyp-virt PPI 12，Marc Zyngier 系列 03/17+16/17）移植 | workqueue_init 卡死（实为 dtbo 问题）；二级核取指失败（idmap 排查）→ 最终 8 核上线 |
| 3. 卡点漂移期 | 中期 | 卡点间歇漂移：stop_machine / of_core_init sysfs / memory_dev_init→device_register；"加探针能过、不加就卡" | 恢复 stock_kallsyms（116,704 符号）；14+ 份 REPORT + 6 路 DISASM（A–F）深挖 |
| 4. 修复实施期 | 后期 | LSE static_key、NOMAP 检查、原厂式 multi_stop、恢复完整 apply_alternatives_all、config 对齐（DEVTMPFS/NR_CPUS/NUMA/SMT/KVM/errata）、DTS 三簇拓扑 | **SM#2（apply_alternatives_all 的 stop_machine）由间歇卡变稳定**；of_core 948 节点通过；set_zone_contiguous 过洞修复 |
| 5. 5.4 复刻验证期 | 2026-08-18 | `linux-5.4-custom`（5.4.210 + 紫光魔改 + 原厂 config）编译成功；探针移植；`boot_54custom*.img` 刷机 | **复刻 5.4 boot 到 initramfs（exec /init y=460 亮）→ 证明原厂式路径在 T770 可行**（§4） |
| 6. 当前收尾 | 2026-08-18 晚 | 6.12 卡点：**进度条稳定 9 格 + y=130 不亮 = 8 核 bringup 后卡**；用户决定暂停修改 | 本索引生成（`FINAL-BRINGUP-INDEX.md`） |

---

## 3. 已修复问题清单（修复 + 效果）

> 全部为 `linux-6.12.103` 当前工作树（HEAD `cb8e725f3` + 11 个未提交文件，见 §5.1）实际内容；
> 部分曾应用后回退的项标注【已回退】。

| # | 问题（根因/背景） | 修复 | 效果 |
|---|---|---|---|
| 1 | **系统级 alternatives 曾被整体跳过**：12,074 条 LSE + 99 条 repeat-TLBI + 24 条 spec-AT 未 patch，全内核退化 LL/SC（REPORT-T508N-stock-analysis 结论 D） | 恢复完整 `apply_alternatives_all()`（`apply_alternatives_vdso()` + `stop_machine(__apply_alternatives_multi_stop, ..., cpu_online_mask)`，与原厂 smp_cpus_done 一致） | 系统级 patch 重新生效 |
| 2 | **LSE 打补方式 = 6.12 asm-goto NOP-落入**（`alt_cb_patch_nops`，10,712 条 CB 型：分支原地 NOP、执行流落入相邻 LSE 序列，"半新半旧"敏感，机制级头号嫌疑） | 改**原厂 5.4 式 static_key/jump_label**（`lse.h` 用 `system_uses_lse_atomics()`；`cpufeature.c` 定义 `cpu_hwcap_keys[ARM64_NCAPS]`/`arm64_const_caps_ready`；`t508n_apply_lse_early()` 改为 set_bit + `static_branch_enable`×2 + `caches_clean_inval_pou(_stext,_etext)` + isb） | jump_label 改 b 指令目标由 `kick_all_cpus_sync`（IPI）同步；未同步核执行旧 b（LL/SC）分支，**语义正确仅慢，无指令级混用竞态** → **SM#2 stop_machine 卡点稳定** |
| 3 | **set_zone_contiguous 穿过 no-map 空洞**：DTS 把 sipc-mem@0x87800000 / cp-modem@0x88000000（≈2.1GB）等标 no-map，6.12 通用 SPARSEMEM `pfn_valid` 不看 NOMAP → 遍历触碰 modem/安全世界区域（同步异常/总线 stall 主嫌疑，y=230 bar 停 6–7 格） | `mm/page_alloc.c` `__pageblock_pfn_to_page()` 开头加 `memblock_is_map_memory(start/end)` NOMAP 检查（复刻原厂 `pfn_valid` 的 NOMAP 拒绝语义） | 遇洞即停，不再过洞；set_zone_contiguous 正常快速返回 |
| 4 | **`__apply_alternatives_multi_stop` 同步协议**：mainline 曾每轮 `dcache_inval_poc`，与"加探针能过、不加就卡"相关 | 改原厂式：二级核 `while (!READ_ONCE(all_alternatives_applied)) cpu_relax(); isb();`（纯 yield 轮询）；CPU0 `dsb(ish); WRITE_ONCE(flag,1)` + **双保险 `icache_inval_all_pou(); isb();`** | 协议与原厂一致；SM#2 通过（注意：`icache_inval_all_pou()` 在 DIC=0 时执行 `ic ialluis`，与"不用无条件 ic ialluis"红线存在文档张力，见 §5.3） |
| 5 | **DEVTMPFS 是 6.12 独有执行**（原厂 `# CONFIG_DEVTMPFS is not set`）：devtmpfsd kthread 跨簇交互是竞态 A 的挂点（`kthread_run` 二次跨簇 `wake_up_process` 走 CSD+IPI_CALL_FUNC） | `CONFIG_DEVTMPFS=n`（对齐原厂） | devtmpfs 挂点消除（底层竞态仍在，漂移到别处） |
| 6 | **NR_CPUS=512 / NUMA=y / SCHED_SMT=y**（无对应硬件却开，per-cpu/RCU 树按 512 构建、调度/内存语义错误） | 对齐原厂：**NR_CPUS=32、NUMA=n、SCHED_SMT=n** | 8 核调度/内存语义修复（部分） |
| 7 | **KVM=y 时 nvhe 独立镜像无法引用 kernel static_key** → 链接报 `undefined reference to __kvm_nvhe_cpu_hwcap_keys` | `# CONFIG_KVM is not set` | 编译通过；SPECULATIVE_AT 的 KVM 站点全部不生效 |
| 8 | **errata 4118414/1530923 mainline 默认 y**（A76 全版本 repeat-TLBI / A55 spec-AT），与 stock 不同口径 | 对齐原厂：两者 `is not set`（仅保留 1286807/1165522=y） | 与 stock 同口径；修正了 completion 报告 §7 的"mainline 首启"假设 |
| 9 | **DTS cpu-idle-states**（pd-big ATF CPU_SUSPEND 偶发丢 SGI 不能排除） | 删除 8 个 CPU 节点的 `cpu-idle-states` → 后又改为**恢复 stock 三簇拓扑（4+3+1，CPU7 独立簇）+ `capacity-dmips-mhz` + `dynamic-power-coefficient`**（当前树，对齐 stock A55=0x1a1/0x9b、A76=0x3fe/0x2d1、CPU7=0x400/0x380） | 调度域/容量语义对齐原厂（A76-requirements 报告 D4 式实验） |
| 10 | **of_core_init 第 64 节点附近卡**（历史卡点 #1） | 循环内 `phandle_cache` 写入注释（5.4 式 = 循环外预填充） | of_core 948 节点通过（y=210 探针） |
| 11 | **memory_dev_init 前 device_register 间歇卡**（竞态 B；6.12 删了 5.4 的 mem_sysfs_mutex、顺序 memory→node→cpu 与 stock cpu→memory 相反） | `PATCH-mem-sysfs-order.md` 给出补丁（恢复 mem_sysfs_mutex 全程持有 + memory_dev_init 挪回 cpu_dev_init 之后）；UNISOC-DETAILED 曾记录"已应用"，**但当前树 drivers/base/{memory,init}.c 无修改 = 已回退/未保留**【已回退】 | 结构对齐项（无害），与根因正交；回退后不影响当前卡点判定 |
| 12 | **buffer_init / mem_init_print_info 从 pre-SMP 搬进 post-SMP 的 page_alloc_init_late()**（8 核并发首触 slab/cpuhp，原厂在 start_kernel 单核执行） | 记录差异；判别表（y=240 段探针）显示该段已通过；`set_zone_contiguous` 去 cond_resched 曾应用【已回退】 | page_alloc_init_late 段已通过（NOMAP 修复后） |
| 13 | **LSE early patch 时机/范围问题**（旧 `__apply_alternatives` 只 patch LSE、未失效 I-cache 会混用） | 改为 static_key 方案（#2）+ 显式 VA 失效 | boot 通过；不再退化 LL/SC |
| 14 | **DTBO merge 破坏 mainline DTB**（stock overlay 合并失败 → FDT_ERR_BADMAGIC → LK panic → b 槽无限重启） | `work/noop_dtbo.img`（board_id/sc-id 匹配的空 overlay） | 合并必然成功；A/B 安全网恢复 |

---

## 4. 复刻原厂 5.4 的成功（关键里程碑）

**结论：原厂式路径在 T770 上可行 —— 同硬件上 5.4 系内核能完整 boot 到 initramfs。**

- **复刻树**：`work/linux-5.4-custom/`（`git archive v5.4.210` 提取，非 vendor 源码；3 个 commit：
  v5.4.210 pristine → olddefconfig → T508N mods `2ca901fb0` → 探针移植 `c4bd9f7ae`）。
- **编译**：`make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8` **成功（0 error）**；
  产物 31,621,632 B（stock 38.3MB，−17.5% 因 gcc13 无 LTO/KCFI/PAuth + 无 KFENCE 回移）；
  日志 `work/build-54-custom.log`；详细报告 `work/REPORT-54-custom-build.md`。
- **魔改实现清单**（全部验证进入二进制，+81/−4）：
  - D1 `pfn_valid` NOMAP 拒绝（mmzone.h，early section 走 memblock 二分 + NOMAP bit2 拒绝，与 stock 逐位吻合）；
  - B1 `scheduler_ipi` 扩展（fold + wake_list + nohz kick + 唤醒 ksoftirqd）；
  - B2 `cpu_stopper_thread` 队空显式 `preempt_schedule()`；
  - E4 `mem_sysfs_mutex`（5.4 上游原生即有，无需改动）；
  - F1 shmem mode=0777（去 S_ISVTX）；
  - C1 IPI 死循环门**降级为计数**（不自旋）；
  - sugov 25% margin（可选）。
  - olddefconfig 差异：clang→gcc（LTO/CFI/SCS 自动关）、KFENCE 丢弃（5.4 无 Kconfig）、
    SPRD 专有 Kconfig 丢弃；保留 NR_CPUS=32/NUMA=n/SMT=n/DEVTMPFS=n/HZ=250/NO_HZ_IDLE=y 等。
- **探针移植**：`work/REPORT-54-probe-port.md`（commit `c4bd9f7ae`）——t508n_dbg 移植（5.4 用
  `__flush_dcache_area` 替代 6.12 的 `dcache_clean_inval_poc`）；y=20 进度条（stage 0–9）+
  y=60/100/140/155/180/400/460 阶段行；打包产出 **`boot_54custom_probe.img`**。
- **boot 实测**（任务书确认）：复刻 5.4 内核 boot 到 initramfs，**exec /init（y=460）亮** ——
  即"进度条 0–9 全亮 + y=400 格亮 + y=460 格亮"的正常满格路径（REPORT-54-probe-port §6 预期）。
- **意义**：① 证明 T770 硬件/启动链对"原厂式代码 + 原厂 config"完全可行；
  ② 6.12 的卡点不是硬件能力问题，而是 **6.12 与 5.4/原厂在跨核通知通道与 driver_init/config 上的差异**（见 §7）；
  ③ 为"6.12 单变量回退到 5.4 语义"（P1/kick_ilb/DEVTMPFS）提供对照基线。
- 产物：`boot_54custom.img`（无探针）、`boot_54custom_probe.img`（带探针）、`boot_54custom_builtin.img`、
  `work/stock54-custom_Image`。

---

## 5. 当前状态（2026-08-18 晚，6.12 魔改树）

### 5.1 代码树与改动（git 实况，HEAD `cb8e725f3` + 11 个未提交文件，+130/−48）

| 文件 | 内容 |
|---|---|
| `arch/arm64/include/asm/lse.h`、`include/asm/cpufeature.h`、`kernel/cpufeature.c` | LSE static_key（§3#2） |
| `arch/arm64/kernel/alternative.c` | 原厂式 multi_stop + `t508n_sm2_done=true` 置位 + CPU0 双保险 `icache_inval_all_pou();isb()` + LSE early static_key |
| `kernel/stop_machine.c` | 全局 `volatile bool t508n_sm2_done;` + cpu_stopper_thread 内 per-CPU 探针（fn 返回后 y=400、signal_done 后 y=420） |
| `kernel/smp.c` | smp_init 内 y=130 探针（bringup_nonboot_cpus 返回后） |
| `init/main.c` | 进度条 stage 0–9 + do_basic_setup（y=220）+ do_initcalls 完成（y=400）+ wait_for_initramfs（y=420）+ console_on_rootfs（y=440）+ exec init 前（y=460） |
| `arch/arm64/kernel/t508n_dbg.c` | 探针 API（paint_bar/stage/mark_row/pass/probe_timer/wdt_pet/fbready） |
| `drivers/of/base.c` | of_core_init 探针（y=210）+ phandle_cache 循环内写注释（§3#10） |
| `mm/page_alloc.c` | `__pageblock_pfn_to_page` NOMAP 检查（§3#3） |
| `arch/arm64/boot/dts/sprd/ums9620.dtsi` | **三簇拓扑 4+3+1（CPU7 独立簇）+ capacity-dmips-mhz + dynamic-power-coefficient**（§3#9；cpu-idle-states 已删） |

### 5.2 当前探针布局（boot_linux.img = 2026-08-18 22:34 构建，含全部改动）

| y | 含义 | 坐标 |
|---|---|---|
| 20 | 进度条 stage 0..9 | x=8+n*30（9 = smp_init 完成/二级核上线） |
| 60 | tick 状态（jiffies 推进） | 长条=正常、中条=无 tick |
| 100 | 异常级别 EL1/EL2/EL3 | x=8/38/68 |
| **130** | **8 核 bringup 完成**（kernel/smp.c，bringup_nonboot_cpus 返回后、smp_cpus_done/SM#2 之前） | x=8 宽 24（**当前不亮 = 卡点**） |
| 210 | of_core_init | x=276 进入定位点；x=8 kset 后；x=44+n*10 节点计数（每 64 节点一格）；x=296 循环完成 |
| 220 | do_basic_setup | x=8 cpuset_init_smp；x=44 driver_init；x=80 init_irq_proc；x=116 do_ctors |
| 400 | do_initcalls 完成（init/main.c） | 同时被 stop_machine.c 用作每核 fn 返回格（x=8+cpu*24）——注意坐标重叠 |
| 420 | wait_for_initramfs 后 | 同时被 stop_machine.c 用作每核 signal_done 格——坐标重叠 |
| 440 | console_on_rootfs 后 | x=8 |
| 460 | **exec init 前**（run_init_process） | x=8，行号 9 |

### 5.3 当前卡点（用户最新观测，任务书原话）

> **SM#2 稳定；进度条稳定停在 9 格（stage 0–8 亮、stage 9 不亮）；y=130 不亮 → 卡在 8 核 bringup 阶段**
> （= "8 核 bringup 后卡"：`smp_init() → bringup_nonboot_cpus()` 未完成返回，故 y=130 不亮、
> stage 9 不亮；SM#2（apply_alternatives_all 的 stop_machine）在更早构建中曾是间歇卡点，
> 经 LSE static_key 修复后已稳定——注意 y=130 在 SM#2 之前，当前观测下 SM#2 尚未执行）。

**尝试过且回退的方案（当前树不含）**：
- `ttwu_queue_cond` 恒 false（P1：所有唤醒直接入队 + IPI_RESCHEDULE，弃 CSD+IPI_CALL_FUNC）——
  曾"部分有效（配合 DEVTMPFS=n 竞态消失）"，后竞态漂移再现，**已回退**（kernel/sched/core.c 无修改）；
- nohz 报告描述的"删除同簇 `nr_running→wakelist` 分支"修补——不在当前树（SUMMARY 文件头提醒 2）；
- `icache_inval_all_pou()` 改 `dsb(ish)`（文档计划）——未实施（当前仍原版 `ic ialluis`；multi_stop CPU0 反而加了双保险 `icache_inval_all_pou()`，与红线存在文档张力）；
- `set_zone_contiguous` 去 cond_resched——已回退（NOMAP 修复后不再需要）；
- mem_sysfs_mutex 恢复 + memory_dev_init 挪位——补丁已写好（PATCH-mem-sysfs-order.md），**未应用/已回退**；
- 旧探针体系（y=120/370/390/410/430/440/450 与 y=150–380 大规模布局）——已全部删除，精简为 §5.2 布局；
- 大规模 config 实验（NO_HZ_IDLE=y + kick_ilb 补丁、KFENCE=y）——补丁文档已备（PATCH-nohz-kick.md / PATCH-diag-sgi-kfence.md），**未应用**；NO_HZ_IDLE=n 保留（诊断）；
- C3 irq_exit softirq 定制——建议独立单变量验证后并入，**未并入**。

**保留的对齐项（当前 config）**：PREEMPT=y、HZ=250、NR_CPUS=32、NUMA=n、SCHED_SMT=n、DEVTMPFS=n、
NO_HZ_IDLE=n（HZ_PERIODIC=y）、KVM=n、JUMP_LABEL=y、ARM64_LSE_ATOMICS=y、ARCH_KEEP_MEMBLOCK=y、
MEMORY_HOTPLUG=y、BLK_DEV_INITRD=y、errata 1286807/1165522=y 且 4118414/1530923/2441007=n、
**KFENCE=n、SOFTLOCKUP_DETECTOR=n（卡死无诊断，盲区）**。

---

## 6. 文档索引表（路径 → 内容概要 → 关键结论）

### 6.1 主目录 /home/test/t508n/

| 文档 | 内容概要 | 关键结论 |
|---|---|---|
| **FINAL-BRINGUP-INDEX.md**（本文件） | 最终综合索引 | 未来会话唯一入口 |
| `AGENTS-GUIDE.md` | 工作区入口/成果地图（2026-08-17） | 先读 HANDOFF/SESSION/REPORT-stock-sync；红线：不加 CCI/CMN/DSU、不用无条件 ic ialluis、不碰外设（部分内容已被 SUMMARY 时间差提醒覆盖） |
| `KERNEL-适配方案.md` | 三条适配路线（A1 stock+initramfs / A2 原厂源码 / B mainline 移植）+ 已交付清单 | boot 链逆向成果如何服务适配；mainline DTB 只能配 mainline 内核 |
| `REPORT-T508N-启动流程分析.md` | 启动链完整逆向（镜像分区/DHTB/LK 时序/boot 格式/super 变体/AOSP 差异） | 设备= T770/UMS9620/OT5/3GB/Android13/5.4.210；紫光定制全景 |
| `REPORT-UMS9620-mainline适配.md` | UMS9620 mainline 支持时间线 + EL2 virtual timer 系列（2026-05） | hyp-virt=PPI 12；6.12.103 缺该支持，已移植（PPI 12 + arch_timer 03/17 改动） |
| `REPORT-T508N-mainline-bringup.md` | bring-up 进度汇总（smp_init 之后） | 早期卡点 memory_dev_init→device_register；早期 workaround（跳过 icache/跳过 alternatives）——**已被后续修复取代** |
| `SKILL-T508N-framebuffer-debug.md` | framebuffer 探针 API/刷机读屏约定/已知坑 | 32bpp BGRA 320×480 stride1280 base 0xb0000000；坑：idmap 漂移、ic ialluis hang、stop_machine 竞态、探针别太多 |
| `WORK-适配材料清单.md` | 材料清单 + 三线任务书（USB/DTS/根因） | stock_kallsyms.map 是正确符号表；USB 移植任务（已归档）；LSE patch 待办 |
| `无串口验证指南.md` | 无串口验证闭环：uboot_log 通道 + pstore/ramoops 通道 + A/B 安全网 | 第二轮修复：noop_dtbo.img 核心；b 槽 vendor_boot 误刷恢复（restore_stock.bat） |
| `prompt-*.md`（30+ 份，主目录+work） | 各轮任务书（标题即内容） | 与对应 REPORT 一一对应（见 6.2 报告侧标注） |

### 6.2 work/ 目录 —— 汇总与交接

| 文档 | 内容概要 | 关键结论 |
|---|---|---|
| **`SUMMARY-ALL-ANALYSIS.md`**（★先读） | 全部反编译/分析成果汇总（2026-08-18）：已确认事实/反汇编成果/已修复/16 项假设排除/未解决竞态/探针布局 | **本索引 §7/§8 的内容源**；两个时间差提醒（探针换代、ttwu 修补不在树）仍有效 |
| `HANDOFF-3-SESSIONS.md` | 内核跑通专项交接（验收标准/Phase 0–4/决策树/红线） | 验收：8 核上线 + alternatives 完整 + driver_init 不卡 + exec /init |
| `SESSION-CONTEXT-COMPACT.md` | 会话压缩上下文（turn 14 继续点） | 背景/已确认事实/已交付文件/下一步候选 |
| `UNISOC-DETAILED.md`（★紫光详记） | 紫光魔改部位 + 原厂 config + 编译环境/坑 权威参考 | §8 速查的内容源；当前卡点（写于更早时点）已被任务书更新 |
| `UNISOC-MODS-AND-CONFIG.md` | 紫光魔改/config 简版汇总 + 已做对齐清单 + 待决策项 | 魔改 B1-B4/C1-C4/D1-D5/E1-E5/F1-F2/A1-A9；关键地址速查 |
| `REPORT-54-custom-build.md` | 复刻 5.4 构建报告（魔改清单/olddefconfig 差异/镜像对比/偏差评估） | **5.4 复刻成功**：0 error；D1 在 pfn_valid 层复刻 NOMAP 更贴近 stock |
| `REPORT-54-probe-port.md` | 5.4 探针移植报告（API 适配/布局/编译/打包） | `boot_54custom_probe.img`；正常满格路径 = stage 0-9 + y=400 + y=460 |
| `REPORT-UNISOC-MODS-ON-612.md` | 紫光魔改全量移植到干净 6.12 树（编译验证 + scheduler_ipi 在 stop_machine 上下文安全性论证） | 8+ 项魔改移植编译通过（0 warning）；scheduler_ipi→nohz_csd_func 在 stop_machine 下无死锁；建议合入顺序；补丁 `diff-612-unisoc-mods.patch` |
| `REPORT-UNISOC-ONLY-CONFIGS.md` | 原厂 config 与标准 5.4/6.12 的差集分析（A=394/B=551/C=312） | C 层 312 项紫光独有（驱动 138/设施 101/内核特性 40/编译层 33）；**T770 世代驱动（UMS9620 clk/PMIC/USB/UFS）全部未上游**，boot 必需项只能逆向或拿源码 |

### 6.3 work/ —— 深度分析 REPORT（按主题）

| 文档 | 内容概要 | 关键结论 |
|---|---|---|
| `REPORT-T508N-stock-analysis.md` | stock A/B/C/D 四节（符号表纠正/device path/errata/CCI-CMN/alternatives） | System.map 不是 stock 符号表；device/sysfs 无 T770 特判；无 CCI/CMN；alternatives 曾整体跳过（已修复） |
| `REPORT-stock-sync-mechanism.md` | 原厂 stop_machine/alternatives 同步协议 | 二级核 = `ldr+yield` 纯轮询（无 dcache/dsb/wfe/sev）；CPU0 普通 str 置 flag；PoC 一致性可用 |
| `REPORT-stock-vs-mainline-locks.md` | 锁/原子/多核同步/启动时序对比 | spinlock/mutex/rwsem 两版同源；唯一现实差异 = LSE patch 是否生效（已修复） |
| `REPORT-stock-vs-mainline-completion.md` | completion + smp_cpus_done 窗口对比 | completion 不是差异点；ic ialluis 弱化；errata 覆盖最可疑（**其 §7 的 4118414/1530923 假设被 crosscluster 报告 §3.3 修正**） |
| `REPORT-mainline-completion-wakeup-analysis.md` | mainline 6.12 唤醒链（纯源码，F1–F7） | **arm64 无 TIF_POLLING_NRFLAG + idle 用 WFI ⇒ 跨核唤醒 100% 依赖 SGI**；x=44 不亮 ⟺ SGI 没到/没处理 |
| `REPORT-LSE-atomic-completion-analysis.md` | LSE `ldaddal` 语义 + thread_ack/nr_todo + alternatives 副作用 | thread_ack 8 核归零 3 次 ⇒ nr_todo 必归零 ⇒ complete 必被调（H4 排除）；ic ialluis 排除（原厂同语境跑通） |
| `REPORT-stock-wakeup-chain-analysis.md` | 原厂完整唤醒链反汇编 vs 6.12 | 唤醒链逐环节等价；旧探针 x=188 亮 = IPI_RESCHEDULE 已到 do_handle_IPI；卡点压缩到"收到 IPI 之后" |
| `REPORT-stock-alternatives-waiter-analysis.md` | alternatives 规模（原厂 77,662 槽 vs mainline 30,477）+ waiter 精确序列 + LSE 打补方式 | mainline 12K 更容易碰到调度代码不成立；**NOP-落入式对"半新半旧"敏感（已通过 static_key 弃用）**；swait 无条件摘链是放大因素 |
| `REPORT-alt-table-integrity.md` | alt 表完整性核查（"828 条异常"真伪） | **828 是分析误差**；30,477 条 0 越界全合法；lm_alias 写坏物证路径不存在 |
| `REPORT-stock-crosscluster-visibility.md` | 原厂跨簇可见性操作四种模式 + SGI 路径 | 无"裸普通 store 被单次读"；SGI 传输两版等价；**修正 completion 报告 §7**；t508n_sm2_done 是唯一裸 store 跨核点（建议 release/acquire） |
| `REPORT-stock-page-alloc-init.md` | page_alloc_init_late 区域 | 原厂此区域几乎无事可做（memblock_discard/shuffle 都编译掉/默认关）；6.12 搬入 buffer_init/mem_init_print_info（post-SMP 首触 slab/cpuhp） |
| `REPORT-stock-zone-memory.md` | set_zone_contiguous / NOMAP / 内存布局 | **原厂 pfn_valid 拒绝 NOMAP，6.12 不查 NOMAP → 过洞卡点**（已修复 §3#3）；vmemmap 完整性 OK |
| `REPORT-stock-driver-init.md` | driver_init 链 14 成员分析 | 原厂 8 成员（无 bdi/devtmpfs/auxiliary/node），顺序 cpu→memory；H1–H7 假设排序；DEVTMPFS 链后嫌疑高 |
| `REPORT-stock-of-core-init.md` | of_core_init / __of_attach_node_sysfs 对比 | 历史卡点 #1；phandle_cache 差异（5.4 循环外预填充）——已注释实验 |
| `REPORT-stock-memory-dev-init.md` | memory_dev_init / device_register 对比 | 历史卡点 #2；6.12 删 mem_sysfs_mutex；device_add 内无 completion/跨核等待 ⇒ 卡死 = 锁自旋/分配 |
| `REPORT-stock-wakeup-nohz.md`（★最新分析） | 异簇唤醒/wakelist/NOHZ 全链 + 竞态根源 | **竞态 A 唯一失败点 = kthread_run 二次跨簇 wake_up_process（6.12 CSD+IPI_CALL_FUNC vs 5.4 wake_list+IPI_RESCHEDULE）**；根源判断 = 跨簇 CSD+IPI_CALL_FUNC 在 T770 间歇失效；修复 P1–P5 |
| `REPORT-CURRENT-RACE-ANALYSIS.md`（★最新一轮） | 竞态 A/B 根源分析（P1 与原版竞态相同约束下） | 失败点压缩到"任务已入队 → 目标核侧"；最可疑 = ①SLUB/内存损坏（casp 首大规模使用）②A76 核被楔住 ③GIC/SGI 丢失；D0–D4 验证方案 + §6.8 复核要求 |
| `REPORT-A76-requirements.md` | stock vs mainline DTS 全量对比（A76 专属） | stock = 三簇 4+3+1 + capacity/DPC/cpufreq 域（我们全缺）——**当前树已按此补三簇+capacity/DPC** |
| `REPORT-AGENTS-GUIDE-analysis.md` | 卡点归因 H1–H4 分析 | 硬事实链：complete 必然被调；H4 排除；swait 队列/__state 怀疑 |
| `TASK-A-B-USB-DTS-C-离线成果.md` | USB 驱动/DTS/LSE 第一轮离线成果 | USB 移植材料（已归档，本阶段不启用） |
| `TASK-A-deepdive-SSPHY-clock.md` | SS PHY 寄存器序列 + UMS9620 时钟表 | USB 深挖（已归档） |

### 6.4 work/ —— 反汇编详报（DISASM，6 路）

| 文档 | 内容概要 | 关键结论 |
|---|---|---|
| `DISASM-A-cpu-boot.md` | 启动/CPU 管理子系统 | KPTI cpu_enable 回调（不用 stop_machine）且在 alternatives 前；psci 定制；PAuth 113 处 xpaclri；thread_info 布局定制；启动时序差异（buffer_init pre-SMP） |
| `DISASM-B-sched-调度器.md` | 调度器 | scheduler_ipi 真实函数（fold+wake_list+nohz kick+ksoftirqd）；stopper 队空 preempt_schedule；sugov 25%；LSE 经典替换 |
| `DISASM-C-irq-中断GIC/Timer.md`（目录内） | 中断/GIC/Timer | **ipi_handler case0 死循环门（Unisoc 独有，暗示 SGI 间歇问题）**；IPI_CPU_STOP 极重处理；irq_exit softirq 定制（pending&0x41）；gic_ipi_send_mask dsb st |
| `DISASM-D-mm-内存管理.md` | 内存管理 | pfn_valid NOMAP 拒绝（memblock 二分）；memmap_init_zone 逐页跳 no-map；__rmqueue_cma 独立；boot_pageset dmb；**KFENCE 回移并开启** |
| `DISASM-E-devm-驱动模型sysfs.md` | 驱动模型/sysfs | fw_devlink 半成品；全内核 KCFI（蹦床表）；mem_sysfs_mutex 全程持有；kernfs 全局 mutex；driver_init 8 成员链 |
| `DISASM-F-fs-文件系统VFS.md` | 文件系统/VFS | shmem mode=0777；fs/VFS 无 T770 特判（与 5.4.210 逐行一致） |

### 6.5 work/ —— 补丁文档（未应用/待验证的完整补丁）

| 文档 | 内容概要 | 关键结论 |
|---|---|---|
| `PATCH-nohz-kick.md` | kick_ilb→smp_send_reschedule（SGI#0）+ scheduler_ipi 恢复真实函数消费 nohz kick（设计 A，3 文件+config） | 让调度侧跨核通知 100% 收敛 SGI#0；等价性/不变量论证；OPT 自触发可选；风险低、回退易 |
| `PATCH-mem-sysfs-order.md` | 恢复 mem_sysfs_mutex（memory_dev_init 全程持有）+ memory_dev_init 挪回 cpu_dev_init 之后 | 结构对齐（无害），预计不消除竞态；git apply --check 通过；依赖/死锁评估 = 风险 0 |
| `PATCH-diag-sgi-kfence.md` | ①SGI 诊断（死循环门等价移植 + 发送/接收计数层 + 每核心跳）②KFENCE（config + fault/report 画屏挂钩） | 计数层判定"发了没到"；门用于 A/B 复现；KFENCE 报告走 pstore console；KFENCE_STATIC_KEYS 必须 n |

### 6.6 work/ —— 材料与脚本（非 .md）

| 路径 | 内容 |
|---|---|
| `stock_kallsyms.map` | 原厂 116,704 符号表（从 stock_Image_full.bin 恢复）——**stock 分析唯一正确符号表** |
| `System.map` | mainline 旧构建产物（非 stock 符号表，勿用于 stock 分析） |
| `stock_Image_full.bin` / `stock_Image.bin` | 原厂 38MB 完整 Image / 8MB boot Image |
| `kernel.config` | 原厂 5.4 config（IKCONFIG 提取） |
| `stock_ums9620.dts` | 原厂 DTB 反编译 DTS（7731 行） |
| `dtbo0~6.dtb`、`vendor_boot.dtb`、`ot5.dtb` | 原厂 DTBO / vendor DTB |
| `linux-5.4.210/`、`linux-5.4.210.tar.xz` | v5.4.210 上游源码（git archive） |
| `stock-5.4.210/` | 上游源码对照（git.kernel.org v5.4.210） |
| `linux-5.4-custom/` | **复刻树**（3+1 commits，含探针） |
| `linux-6.12-clean/` | 干净 6.12.103 树（紫光魔改全量移植验证用） |
| `diff-612-unisoc-mods.patch`、`diff-612-{arch,include,memory,mm,sched}.patch` | 干净树 vs 魔改差异补丁 |
| `alt_table_records.csv`、`parse_alt_table.py` | alt 表解析数据/脚本 |
| `recover_kallsyms.py` | kallsyms 恢复脚本（已完成使命） |
| `strip_probes.py`、`clean_screen2.py`、`disasm_helpers.sh` | 工具脚本 |
| `E-devm-*.dump`、`E-devm-symbols.txt` | device model 反汇编 dump |
| `build-54-custom.log`、`build-612-clean.log` | 构建日志 |
| `patch-0001-lse-early-correct.patch` | LSE early patch 修正补丁（追溯用） |

---

## 7. 关键结论汇总（从 SUMMARY-ALL-ANALYSIS.md 提取）

### 7.1 已确认事实（硬件/启动链）
- **调试环境**：无 UART/USB；fb 探针 32bpp BGRA 320×480 stride1280 base 0xb0000000；LK 加载 raw Image @0x80080000、dtb @0xbbf00000；刷机 `tools/sprd_mkbootimg.py` + fastboot。
- **符号映射**：原厂 `_text/_head=0xffffffc010080000`，file_off = vaddr − 0xffffffc010080000；objdump `--adjust-vma`；mainline 地址随构建漂移（核对构建时间）。
- **一致性**：DIC=0/IDC=0（`ic ialluis` 真实执行）；无 CCI/CMN/DSU/SCU 初始化（stock 内核/DTS/SML 均无，不要加）；**T770 PoC 一致性可用**（原厂普通 str + yield 轮询跨簇通信跑通整套系统）；"普通 store 跨核不可靠" = 标准架构行为，原厂规则 = "用同步换确定性"。
- **原厂行为**：alternatives 77,662 槽（mainline 30,477）——"原厂规模小"不成立；secondary 等待 = 纯 ldr+yield 轮询；completion = 传统 waitqueue（mainline = swait，waker 不摘链 vs 无条件摘链）；arm64 无 TIF_POLLING_NRFLAG ⇒ 跨核唤醒必发 IPI；SGI 传输两版逐字节等价（无"SGI 编号新依赖"）；原厂 scheduler_ipi 是真实函数（fold + wake_list + nohz kick + ksoftirqd）。
- **config 关键项**（当前）：PREEMPT=y、HZ=250、NR_CPUS=32、LSE=y、NUMA=n、DEVTMPFS=n、KVM=n、JUMP_LABEL=y、ARCH_KEEP_MEMBLOCK=y、errata 1286807/1165522=y（4118414/1530923/2441007=n）、SOFTLOCKUP=n、SHUFFLE=n、PSEUDO_NMI=n。
- **内存**：3GB（2–5GB 连续，LK cs0 1.5G+cs1 1.5G）；原厂 reserved-memory 除 rebootescrow 外全部非 no-map（我们全标 no-map——已由 NOMAP 检查兜底）。

### 7.2 已排除假设（16 项，详见 SUMMARY §4）
原子计数坏（H4）/ 丢唤醒内存序（H3）/ ic ialluis hang / mainline 独有 errata（4118414/1530923）/ alt 表 828 异常写坏 waiter / 唤醒链缺机制 / PoC-cache 硬件不可靠 / CCI-CMN-DSU 未初始化 / device-sysfs 有 T770 特判 / 锁实现差异 / shuffle-memblock_discard-deferred / completion waitqueue→swait 是根因（非根因，但无条件摘链是放大因素）/ ttwu_queue_cond 同簇修补（实验无效，失败点在跨簇）/ DAIF-GIC（探针显示正常，最终排除需在线钉死）/ devices_init 本身 / NOMAP 分到坏页（但 memmap_init 会过洞，已修）。

### 7.3 未解决问题（竞态 A/B）
- **竞态 A**：devtmpfs `wait_for_completion(&setup_done)` 间歇卡（y=270 x=80 亮、x=116 不亮；kdevtmpfs 线程未被跨簇唤醒）——当前 DEVTMPFS=n 已消除此挂点，但底层竞态漂移。
- **竞态 B**：memory_dev_init → device_register 前间歇卡（y=296 亮/卡），与 A 大概率同源（更早跨簇唤醒失败 → 某核/任务坏状态 → CPU0 在锁/分配点卡），卡点随"哪次唤醒丢失"漂移。
- **当前卡点（最新）**：8 核 bringup 后卡（§5.3）。
- **最可能根因排序**：① 跨簇 CSD+IPI_CALL_FUNC 传输/处理在 T770 间歇失效（6.12 独有暴露面；原厂用 IPI_RESCHEDULE 可靠）→ 最新一轮更新为 ①SLUB/内存损坏（casp 首大规模使用）②某 A76 核被楔住（y=310 无法区分 NOHZ 与楔死）③GIC/SGI 对 A76 的 IPI 丢失；② CPU0 在 8 核 patch 风暴后执行"半新半旧"指令（叠加 swait 无条件摘链）——经 static_key 修复弱化；③（待在线排除）中断投递层本身。
- **观测事实**：y=325（每核 IPI 心跳）8 格全亮（A76 收 IPI 正常）；y=310（每核 tick）A76 后 4 格不亮（NOHZ 空闲）；两种唤醒路径竞态相同；"加探针能过、不加就卡"强时序敏感。

---

## 8. 紫光魔改 / config 速查（详记见 `work/UNISOC-DETAILED.md`）

- **调度器**：scheduler_ipi 真实函数（IPI 即干活）；stopper 队空 preempt_schedule；sugov 25%；LSE 经典替换（我们已用 static_key 对齐）。
- **中断/GIC**：**ipi_handler case0 死循环门**（`ipi_cpu==cpu && state==1 → while(1)`，sprd-ipi-thread/hang-detect，initcall6 `send_ipi_init`）——**暗示 T770 SGI 投递确有间歇问题（原厂有检测、mainline 无）**；IPI_CPU_STOP 极重（minidump/sysdump）；irq_exit softirq 定制（pending&0x41 内联、其余唤醒 ksoftirqd）。
- **内存**：pfn_valid NOMAP 拒绝（已对齐）；memmap_init 逐页跳 no-map（6.12 需显式，见 UNISOC-MODS-ON-612 §3.4）；**KFENCE 回移并开启（sample=100）**——6.12 原生 CONFIG_KFENCE 可用作诊断。
- **驱动模型**：fw_devlink 半成品；**全内核 KCFI**（3102 蹦床，brk #0x800 失败）；mem_sysfs_mutex 全程持有（6.12 已删）；kernfs 全局 mutex。
- **fs**：shmem mode=0777；fs 主体无特判。
- **启动/CPU**：KPTI = cpu_enable 回调（不用 stop_machine）且先于 alternatives（mainline 窗口 stop_machine 2 次 vs 原厂 1 次）；PAuth 113 处 xpaclri；buffer_init/mem_init_print_info 在 pre-SMP；driver_init 顺序 cpu→memory 无 devtmpfs/bdi/auxiliary/node。
- **config 对齐现状**：DEVTMPFS=n、NR_CPUS=32、NUMA=n、SCHED_SMT=n、NO_HZ_IDLE=n（实验）、KVM=n、4118414/1530923=n、2441007=n、KFENCE=n（待开）、JUMP_LABEL=y。
- **编译环境/坑**（UNISOC-DETAILED §3）：ARM 13.2 工具链 + openssl-local（CPATH/LIBRARY_PATH，**勿用命令行 HOST_EXTRACFLAGS**，会覆盖 dtc 的 libfdt_env.h）；KVM 关（nvhe 无法引用 static_key）；NR_CPUS 改动需全量重编；.idmap.text >4KB 链接报错。

---

## 9. 材料 / 工具速查

### 9.1 构建与刷机命令（标准流程）
```bash
# 编译 6.12 魔改树
export PATH=/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
cd /home/test/t508n/linux-6.12.103 && make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
cp arch/arm64/boot/Image /home/test/t508n/work/mainline_Image
# 打包 + 刷机
cd /home/test/t508n
python3 tools/sprd_mkbootimg.py --kernel work/mainline_Image --ramdisk initramfs.cpio \
  --output boot_linux.img --key tools/ot5_boot_key.pem
# fastboot flash boot_a boot_linux.img && fastboot reboot   （设备需解锁；b 槽保官方系统）
# 回官方系统: fastboot set_active b && fastboot reboot
```

### 9.2 关键产物

| 产物 | 路径 | 说明 |
|---|---|---|
| 6.12 当前镜像 | `/home/test/t508n/boot_linux.img`（2026-08-18 22:34 构建，含全部当前改动与探针） | 主试验镜像 |
| 6.12 内核 | `work/mainline_Image`、`mainline_Image`（Aug 18 22:34） | 与 boot_linux.img 同批 |
| 5.4 复刻镜像 | `boot_54custom.img` / `boot_54custom_probe.img` / `boot_54custom_builtin.img` | 复刻 5.4（probe 版带探针；**boot 到 initramfs 验证用**） |
| 复刻 5.4 内核 | `work/stock54-custom_Image` | 31,621,632 B |
| 干净 6.12 树产物 | `work/linux-6.12-clean/arch/arm64/boot/Image`（45.6MB，紫光魔改全量移植） | 评估"魔改本身"用 |
| initramfs | `initramfs.cpio`、`initramfs_root/`（busybox，/init 挂 proc/sys/devtmpfs 进 shell） | 静态 busybox |
| noop DTBO | `work/noop_dtbo.img` | 防 DTBO merge 破坏（修复项） |
| vendor_boot | `vendor_boot_linux.img` | mainline OT5 DTB（只配 mainline 内核！） |
| 符号表 | `work/stock_kallsyms.map`（原厂）、`linux-6.12.103/System.map`（mainline，随构建漂移） | 反汇编依据 |
| 原厂 DTS/config | `work/stock_ums9620.dts`、`work/kernel.config` | 对照材料 |
| 原厂二进制 | `work/stock_Image_full.bin`（38MB）、`stock_Image.bin`（8MB） | objdump 对象 |
| 固件 | `payloads/`（sml/spl/uboot/trustos/teecfg 载荷） | 安全链材料 |

### 9.3 工具
- **交叉工具链**：`/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin/`
  （gcc 13.3；系统 `/usr/bin/aarch64-linux-gnu-*` 也可 objdump）。
- **打包/解包**：`tools/sprd_mkbootimg.py`（boot.img）、`tools/sprd_ramdisk.py`（紫光 LZ4 ramdisk）、
  `tools/avbtool.py`、`tools/lz4dec.py`、`tools/cpio_extract*.py`、`tools/lpunpack.py`、
  `tools/build_boot_linux.sh` / `build_initramfs.sh` / `build_kernel.sh` / `build_noop_dtbo.py` /
  `patch_vendor_boot.py` / `collect_evidence.sh`。
- **刷机/ADB**：`tools/winfastboot/`、`tools/winadb/`（WSL interop）；`restore_stock.bat`（b 槽恢复）。
- **反汇编**：`aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000
  --start-address=<addr> --stop-address=<addr+n> work/stock_Image_full.bin`。
- **无串口验证**：uboot_log 分区（`adb shell su -c "dd if=/dev/block/by-name/uboot_log ..."`）+ pstore
  （`/sys/fs/pstore/console-ramoops-0`，DTS 已含 ramoops@fff80000，配置见 PATCH-diag-sgi-kfence §2.4）。
- **目录参考**：`Image/`（78 分区镜像备份）、`initramfs_root/`（解包 initramfs）、`busybox-1.36.1/`、
  `aarch64-linux-musl-cross/`（musl 工具链）、`openssl-local/`（构建用 openssl）。

---

## 10. 未来方向建议（未验证项，全部来自既有报告，未新增）

> 用户已决定暂停修改；以下为将来继续调试的候选，按证据强度排序，**一次一个变量**。

1. **SGI/IPI 计数诊断（PATCH-diag-sgi-kfence.md P1–P3，纯观测优先）**：发送侧计数（smp_cross_call）vs
   接收侧计数（do_handle_IPI）按核对照，未消化 delta>0 且持续 = "发了没到"直接证据；每核心跳区分
   "核死（心跳冻结）vs 收不到 IPI（心跳亮 rx 冻结）"。**先于一切改动做**。
2. **KFENCE（PATCH-diag-sgi-kfence.md K1–K3）**：`CONFIG_KFENCE=y`（sample=100，对齐原厂）+ pstore
   console 通道；fault/report 画屏挂钩（先画后打印）；**KFENCE_STATIC_KEYS 必须 n**。诊断"内存被踩"
   （SLUB/链表损坏）——最新一轮的头号嫌疑（casp 首大规模使用）。
3. **死循环门 A/B（PATCH-diag-sgi-kfence.md 层 2）**：原厂门等价移植（boot 参数/proc arm），把疑似核
   "可控弄死"复现竞态——门触发与否本身是一次丢包试验；注意门不是丢包检测，需与计数层联合。
4. **5.4 复刻稳定性多次验证**：`boot_54custom_probe.img` 多次 boot 确认稳定（排除"碰巧能过"）；
   可作为"原厂式路径在 T770 可行"的长期对照基线；可在此树上继续验证 kick_ilb/softirq 等单变量。
5. **kick_ilb 单变量重试（PATCH-nohz-kick.md）**：先"3 文件补丁 + NO_HZ_IDLE 仍=n"（回归 0），
   再 NO_HZ_IDLE=y；把调度侧跨核通知 100% 收敛 SGI#0；WARN_ON 可改 WARN_ON_ONCE。
6. **P1 实验严格性复核（CURRENT-RACE-ANALYSIS §6.8）**：确认"P1 vs 原版竞态相同"是否同探针布局
   下严格对照；复核前 CSD 丢失假设不能彻底排除。
7. **SLUB/LSE A/B**：`slub_debug=U`（或 FZPU）一轮；`t508n_apply_lse_early` 不 enable static_key
   （强制全 LL/SC）一轮——若竞态消失 ⇒ casp/LSE 路径坐实。
8. **诊断盲区消除**：`CONFIG_SOFTLOCKUP_DETECTOR=y`（stock 开、mainline 关）+ 观察 y=60/y=140 WDT
   心跳区分"慢 vs 死"；`CONFIG_DEBUG_LIST/DEBUG_OBJECTS/DEBUG_MUTEXES/DEBUG_SPINLOCK` 一轮。
9. **二分隔离**：`maxcpus=4`（纯 A55）/ `nohz=off` / `cpuidle.off=1` / kdevtmpfs pin CPU0 /
   `maxcpus=1` 对照——确认"含 A76 才现、纯 A55 消失"的边界。
10. **DTS 完整对齐（A76-requirements D4）**：当前已补三簇 + capacity/DPC；尚缺 cpufreq 域/
    idle-states 挂接/#cooling-cells（mainline 无 sprd cpufreq 驱动，A76 无 DVFS——短期可接受）。
11. **杂项收尾**：`t508n_sm2_done` 改 release/acquire（crosscluster 建议）；读 MIDR_EL1 画屏定 A76
    revision（4118414 差异，当前已关，优先级下降）；`icache_inval_all_pou` 双保险 vs "不用无条件
    ic ialluis"红线的文档/代码二选一对齐。
12. **跨簇唤醒机制侧（若确诊 CSD 失效）**：P1（ttwu_queue_cond 恒 false）作为修复；或恢复 5.4 式
    `rq->wake_list + IPI_RESCHEDULE` 承载 wake_list（P2，改动大）。

---

## 附：最常用路径速查

```
/home/test/t508n/
├── FINAL-BRINGUP-INDEX.md        ← 本文件（入口）
├── linux-6.12.103/               ← 6.12 魔改树（HEAD cb8e725f3 + 11 未提交文件）
├── boot_linux.img / mainline_Image   ← 当前 6.12 镜像/内核（Aug 18 22:34）
├── boot_54custom*.img / work/stock54-custom_Image   ← 复刻 5.4 镜像/内核
├── initramfs.cpio / initramfs_root/  ← busybox initramfs
├── tools/                        ← 打包/刷机工具（sprd_mkbootimg.py 等）
├── work/SUMMARY-ALL-ANALYSIS.md  ← 分析汇总（先读）
├── work/UNISOC-DETAILED.md       ← 紫光魔改/config/编译权威参考
├── work/stock_kallsyms.map       ← 原厂符号表（唯一正确）
├── work/stock_Image_full.bin     ← 原厂二进制（反汇编对象）
├── work/linux-5.4-custom/        ← 复刻树（boot 成功）
├── work/linux-6.12-clean/        ← 干净 6.12 树（魔改移植验证）
└── work/PATCH-*.md               ← 未应用的完整补丁（nohz-kick / mem-sysfs-order / diag-sgi-kfence）
```
