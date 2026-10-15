# T508N bring-up 全部反编译/分析成果汇总（供后续"当前问题分析"子 agent 使用）

> 生成：2026-08-18。本文件由"汇总子会话"依据以下材料整理（只汇总，未新增分析/结论）：
> `work/` 下 14 份 REPORT-*.md、`AGENTS-GUIDE.md`、`work/HANDOFF-3-SESSIONS.md`、
> `work/SESSION-CONTEXT-COMPACT.md`、`SKILL-T508N-framebuffer-debug.md`，
> 以及 `linux-6.12.103` 的 `git status`/`git diff`（HEAD `cb8e725f3`）。
> 下一个子 agent **只读本文件即可开工**；详细证据链可回查对应 REPORT 文件（文中有引用）。
>
> ⚠️ **两个时间差提醒（重要）**：
> 1. 探针坐标已换代：`HANDOFF/AGENTS-GUIDE` 里的 y=120/370/390/410/430/440/450 是**旧构建**坐标；
>    当前代码（git diff，镜像 Aug 18 07:38 构建）的坐标见第 6 节。旧观测（y=450 亮等）是旧探针语义，
>    引用时注意。
> 2. `REPORT-stock-wakeup-nohz.md`（最新，23:23）描述的"ttwu_queue_cond 已被 T508N 修补
>    （删除同簇 nr_running→wakelist 分支）"状态 **与当前 git 工作树不一致**：`kernel/sched/core.c`
>    当前**无任何修改**（git status 16 个文件里没有它），该实验不在当前树中（可能已回退/未落盘）。
>    需要时先核实实际代码再引用。

---

## 1. 已确认事实清单（硬件/启动链）

### 1.1 设备与调试环境
- 设备：TCL T508N（OT5），SoC **Unisoc T770/UMS9620**，CPU = **4×Cortex-A55 + 3×Cortex-A76 + 1×Cortex-A76**（DTS），Android 13，原厂内核 **5.4.210**；目标 mainline **6.12.103**。
- 内存：3GB LPDDR4x，DTS `memory@0x80000000, size 0xc0000000`（2GB–5GB 连续，LK cs0 1.5G + cs1 1.5G）；**stock DTS 是 8GB 占位**（0x2_00000000），LK 裁剪到实际。
- 调试限制：**无 UART、无 USB**；只有 LCD framebuffer 探针 + fastboot。
- 帧缓冲：32bpp BGRA，320×480，stride=1280B，基址 **0xb0000000**，白色=0xFFFFFFFF。
- boot 链：BootROM → SPL → LK（uboot 分区）→ 内核；LK 加载 raw `Image` @0x80080000，dtb @0xbbf00000。
- 刷机：`tools/sprd_mkbootimg.py --kernel work/mainline_Image --ramdisk initramfs.cpio --output boot_linux.img --key tools/ot5_boot_key.pem` → `fastboot flash boot_a boot_linux.img && fastboot reboot`。
- 交叉工具链：`/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin/`。

### 1.2 符号表与反汇编映射（最关键）
- `work/System.map` 是 mainline 旧构建产物，**不是原厂符号表，不可用于 stock 分析**。
- 正确原厂符号表：**`work/stock_kallsyms.map`**（116,704 个符号，已从 `stock_Image_full.bin` 内 kallsyms 恢复；恢复脚本 `work/recover_kallsyms.py`）。
- 原厂映射：`_text/_head = 0xffffffc010080000`，`_stext = 0xffffffc010081000`；**file_off = vaddr − 0xffffffc010080000**。
- 正确 objdump：
  `aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0xffffffc010080000 --start-address=<addr> --stop-address=<addr+n> work/stock_Image_full.bin`
- mainline 6.12 符号表：`linux-6.12.103/System.map`（**仅与同批构建的 vmlinux/Image 配套**，Aug 17 13:22 构建 与 Aug 18 07:38 构建地址可能漂移，引用 mainline 地址务必核对构建时间）。
- 原厂 5.4 上游源码已拉取：`work/stock-5.4.210/`（git.kernel.org v5.4.210）。

### 1.3 硬件/一致性事实
- **DIC=0、IDC=0**：A76 `CTR_EL0=0x8444c004`、A55 `CTR_EL0=0x84448004`（bit29=DIC=0，bit28=IDC=0）→ `icache_inval_all_pou()` **不会提前 return，`ic ialluis` 真实执行**（LSE-atomic 报告 §3.2）。
- **无 CCI/CMN/DSU/SCU 一致性初始化**：stock 内核 kallsyms（`cci_*/ccn_*/cmn_*/dsu_*/scu_*` 均无）、stock DTS/DTBO、SML（BL31，只管电源/DVFS）都没有；**不要加这些节点/驱动/初始化**。
- T770 的 **PoC 一致性本身可用**：原厂用"普通 `str` + 二级核 `yield` 轮询普通 `ldr`"（无 dcache/dsb/wfe/sev）跨 cluster 通信且整套系统跑通。**不要再怀疑 cache 硬件**。
- "普通 store 跨核不可靠"的精确含义：对**无同步**的普通 store → 另一核**单次** load，ARMv8 内存模型不保证任何时点的可见性（传播时延导致"有的核读到、有的读不到"），这是标准架构行为，不是 T770 缺陷。原厂规则 = "用同步换确定性"（原子/锁 / dmb+IPI / 轮询，三选一）（crosscluster 报告 §3.4）。
- **A76/A55 具体 revision 未实测**（`MIDR_EL1` 读值探针尚未刷/未读回）；config 开 `1286807`（A76 r0p0–r3p0）与 `1165522`（A76 r0p0–r2p0）说明按 ≤r3p0/r2p0 覆盖（LSE-atomic 报告 §5.1 建议读 MIDR 定案 4118414 差异是否真实）。

### 1.4 原厂行为事实（多报告坐实）
- 原厂 `__apply_alternatives_multi_stop`（0xffffffc010114c0c）二级核等待 = **`ldr` + `yield` 纯轮询**（0x114c50–0x114c64），无 dcache/dsb/wfe/sev；见到 1 后 `isb`（0x114c68）。CPU0 = `__apply_alternatives()` 返回后**普通 `str all_alternatives_applied=1`**（0x114c9c/14ca0），无显式 dcache clean、无 sev；唯一屏障来自 `__apply_alternatives` 尾部 `dsb ish; [cpu_hwcaps.bit28=DIC ? skip : ic ialluis]; dsb ish; isb`（0x114e94–14eac）。
- 原厂普通 cache 维护只用 **`ic ivau`**（`__flush_icache_range` 0xffffffc010086e40：`dc cvau`→`dsb ish`→`ic ivau`→`dsb ish`→`isb`；`invalidate_icache_range` 0xffffffc010086f20）；全内核唯一 `ic ialluis` 在 `__apply_alternatives` 尾部、受 DIC 门控。
- 原厂 stop_machine 完成计数：LSE **`ldaddal`**（multi_cpu_stop 0x30a564 / cpu_stopper_thread 0x30aefc）+ LL/SC `ldxr/stlxr`+`dmb ish` 回退；与 mainline 同源。
- 原厂 5.4 **completion = 传统 waitqueue**（`complete` 0xffffffc010227a70：`casa` 抢 wait.lock → done++ → 遍历 wait_queue → `blr entry->func`（autoremove/default_wake_function）→ `stlrb` 释放）；**waker 不摘链、waiter 醒后持锁自摘**。mainline 6.12 = **swait**（`swake_up_locked` 队列空静默返回；唤醒第一个 waiter 后**无条件 `list_del_init`——唤醒失败也摘链**，这是放大因素，非根因）。两者都收敛到 `try_to_wake_up`。
- 原厂 `smp_cpus_done`（0xffffffc01214e4b8）= `printk → setup_cpu_features(update/enable caps + static_key_enable) → hyp_mode_check → stop_machine(__apply_alternatives_multi_stop) → mark_linear_text_alias_ro`；**KPTI 在 5.4 走 enable_cpu_capabilities 的 cpu_enable 回调（不用 stop_machine）**；mainline 6.12 的 `kpti_install_ng_mappings` 用 stop_machine → 该窗口 stop_machine 次数 = 原厂 1 次 vs mainline 2 次。
- 原厂 alternatives 规模：**77,662 槽**（`__alt_instructions` 0xffffffc0121cd988→0x122b11f0，0xE3868 B），最大类 cap42（PAN/uaccess，`msr pan,#1` 类）57,220 条 + LSE(cap11) 11,331 条；mainline 6.12 探针构建 30,477 槽。**"原厂规模小所以不卡"不成立——原厂大 2.5 倍照跑**。
- 原厂 driver_init 链（0xffffffc01218e794 反汇编，8 调用）：`devices_init(0x1218dcd8) → buses_init(0x1218ddb0) → classes/firmware/hypervisor(内联) → of_core_init(0x121a00a8) → platform_bus_init(0x1218df80) → cpu_dev_init(0x1218e6c4) → memory_dev_init(0x1218ec8c) → container_dev_init(0x1218e814)`；**无 bdi_init/devtmpfs_init（原厂 `# CONFIG_DEVTMPFS is not set`）/auxiliary_bus_init/node_dev_init**；cpu→memory 顺序（6.12 是 memory→node→cpu）。
- 原厂 `page_alloc_init_late`（0xffffffc01216ab1c）只调 `set_zone_contiguous`（0x4975ec，pageblock=1024 页 4MB，**无 cond_resched**）；`memblock_discard` 两版都因 `CONFIG_ARCH_KEEP_MEMBLOCK=y` 被编译掉；shuffle 两版 boot 时都不跑（原厂 static key 默认关 / mainline 编译掉）。**6.12 把 `buffer_init`/`mem_init_print_info`/`page_alloc_sysctl_init` 从 pre-SMP（start_kernel/arch mem_init）搬进 post-SMP 的 `page_alloc_init_late()`**。
- 原厂 `pfn_valid`（0xffffffc010126b38）：section 有效 &&（early section → **memblock 二分查找 + NOMAP 拒绝**（`bic w0, w9, w8, lsr #2`，bit2=MEMBLOCK_NOMAP）| 非 early → subsection_map 位测试）。mainline 6.12 arm64 用通用 SPARSEMEM `pfn_valid`（mmzone.h:2024，`early_section(ms) || pfn_section_valid(ms,pfn)`），**完全不看 NOMAP**。
- 原厂 DTS reserved-memory **除 rebootescrow 外全部是普通 reserved（无 no-map）**；我们的 `ums9620-ot5.dts` 把 modem/安全区（sipc-mem@0x87800000、cp-modem@0x88000000、iq、tos、sml、audio、audiodsp 等）**全部标了 no-map**，且缺 cma_share/rebootescrow、audio 尺寸 9M vs 3M 等。
- **arm64 无 `TIF_POLLING_NRFLAG`**（5.4 与 6.12 都是），idle 用 `wfi()`（idle.c:30 / 5.4 process.c:71-75）→ 跨核唤醒 **必发 IPI**（`set_nr_if_polling`/`set_nr_and_not_polling` 恒走发 IPI 分支），无 polling 兜底。
- **SGI 传输机制两版逐字节等价**：SGI ID 0–7 → `ICC_SGI1R_EL1` 写入（原厂 `gic_ipi_send_mask` 0xffffffc0109aa6bc 在 0x9aa770 有 `dmb ishst` 后写**内存映射** SGI1R，per-cpu+0xf00；mainline `irq-gic-v3.c:1428` 是 `dsb(ishst)` → sysreg `ICC_SGI1R_EL1` → `isb`）→ irq domain → `ipi_handler(irq−ipi_irq_base)` → `do_handle_IPI` switch。**不存在"5.4 用 SGI#0、6.12 用别的编号"的硬件级新依赖**；差异只在"哪个 IPI 语义承载唤醒工作 + 唤醒队列放哪"。
- 原厂 `scheduler_ipi`（0xffffffc0101fc658）是**真实函数**：fold + 处理 `rq->wake_list`（sched_ttwu_pending）+ **自触发 nohz idle balance kick + 唤醒 ksoftirqd**（Unisoc 定制，上游 5.4 用 got_nohz_idle_kick）；mainline 6.12 `scheduler_ipi` 是 **inline 仅 preempt_fold_need_resched**，wake_list 处理整体迁到 IPI_CALL_FUNC/CSD。

### 1.5 当前 config 关键项（linux-6.12.103/.config，已核实）
- errata：`1286807=y`、`1165522=y`、`1463225=y`、`3194386=y`、`2054223=y`、`3117295=y` 等；**`4118414 is not set`、`1530923 is not set`、`2441007 is not set`**（后两者与 stock 同口径，只开 A76 的 1286807/1165522）。
- 并发/特性：`ARM64_LSE_ATOMICS=y`、`PREEMPT=y`、`HZ=250`、`NR_CPUS=512`（stock 32）、`NUMA=y`（stock 关）、`ZONE_DMA=y`（stock 关）、`DEVTMPFS=y`+`DEVTMPFS_MOUNT=y`（stock 关）、`BLK_DEV_INITRD=y`、`ARCH_KEEP_MEMBLOCK=y`、`ARM64_PAN=y`、`JUMP_LABEL=y`、`MEMORY_HOTPLUG=y`、**`# CONFIG_KVM is not set`**（KVM 已关）、**`# CONFIG_SOFTLOCKUP_DETECTOR is not set`**（stock 开——6.12 卡死时无 softlockup 诊断，诊断盲区）、`# CONFIG_SHUFFLE_PAGE_ALLOCATOR is not set`、`# CONFIG_ARM64_PSEUDO_NMI is not set`。

### 1.6 构建状态（当前）
- HEAD：`cb8e725f3` "T508N mainline 6.12.103 bring-up baseline: framebuffer probes + LSE early patch + stop_machine alternatives instrumentation"。
- 未提交修改 16 个文件（+204/−58），见第 3 节。
- **`work/mainline_Image` 与 `boot_linux.img` = 2026-08-18 07:38 构建；所有已改源文件最晚修改 08-17 23:33 → 当前镜像已包含全部当前探针与代码改动**（此前 GUIDE/HANDOFF 反复提示的"镜像不含最新探针"问题已解决）。

---

## 2. 反汇编成果清单（符号 + 地址 + 结论，按报告来源组织）

> 地址均为原厂 5.4 二进制（`work/stock_Image_full.bin`，stock_kallsyms.map 确认）除非标注 mainline。
> mainline 地址来自 Aug 17 13:22 探针构建（`linux-6.12.103/System.map`），**不同构建可能漂移**。

### 2.1 REPORT-T508N-stock-analysis.md（A/B/C/D 四节：符号表纠正、device path、errata、CCI/CMN、alternatives）
| 符号 | 原厂地址 |
|---|---|
| `device_register` / `device_initialize` / `device_add` | `ffffffc010b75afc` / `b75b2c` / `b75d04` |
| `dev_set_name` / `get_device_parent` | `ffffffc010b77180` / `b771f8` |
| `kobject_add` / `kobject_add_varg` | `ffffffc011683da4` / `68329c` |
| `bus_add_device` | `ffffffc010b7f58c` |
| `memory_dev_init` / `create_memory_block_devices` / `init_memory_block` | `ffffffc01218ec8c` / `bc134c` / `bc1610` |
| `devices_init` / `driver_init` / `of_core_init` | `ffffffc01218dcd8` / `18e794` / `1a00a8` |
| `__flush_icache_range` / `invalidate_icache_range` | `ffffffc010086e40` / `086f20` |
| `__apply_alternatives` | `ffffffc010114cd8`（尾部 0x114e94–14eac 条件 `ic ialluis`，判定位 `cpu_hwcaps` bit28 @ `ffffffc01250c600`） |
| mainline `__alt_instructions` | `ffffffc081ce74dc`..`081d40960`，0x59484 B，**30,475 条** |
- 结论 A：原厂 device model/sysfs 路径（device_register/device_add/kobject_add/memory_dev_init）**无 T770 特判**（逐条扫描无 dsb/dmb/isb/dc/ic/tlbi/errata 痕迹）。
- 结论 B：errata config 差异——mainline 独有开 `1530923`（A55 spec-AT）、`3194386`（A76 SSBS）、`4118414`（A76 TLBI repeat，ALL_VERSIONS）；建议补开 `2441007`（A55 repeat-TLBI，**当前未开**）。
- 结论 C：无 CCI/CMN 节点与符号；SML 不管一致性互联；**不要加**。
- 结论 D（关键，历史）：mainline 曾整体跳过系统级 `apply_alternatives_all()` → 12,074 条 LSE + 99 条 repeat-TLBI + 24 条 spec-AT 未 patch → 全内核退化 LL/SC。**该问题已修复（当前代码恢复完整 apply_alternatives_all + LSE early static_key，见第 3 节）。**

### 2.2 REPORT-stock-vs-mainline-locks.md（锁/原子/多核同步/启动时序）
| 符号 | 原厂 5.4 | mainline 6.12 |
|---|---|---|
| `queued_spin_lock_slowpath` / `queued_write_lock_slowpath` / `queued_read_lock_slowpath` | `ffffffc0102452a8` / `248314` / `2484c0` | `ffffffc08107a7e0` / `07ac20` / `07aaf8` |
| `_raw_spin_lock` / `_raw_spin_unlock` | `ffffffc0116a8160` / `6a7e08` | `ffffffc08107a210` / `079ed0` |
| `mutex_lock` / `__mutex_lock_slowpath` | `ffffffc0116a336c` / `6a32d4` | `ffffffc0810752bc` / `0752a4` |
| `down_write` / `up_write` | `ffffffc0116a3aec` / `0102427f4` | `ffffffc081076bd8` / `0800ea1d8` |
| `secondary_start_kernel` | `ffffffc010115d40` | `ffffffc080020f58` |
| `kernel_init_freeable` | `ffffffc012148288` | `ffffffc081c5109c` |
| `smp_cpus_done` | `ffffffc01214e4b8` | `ffffffc081c558bc` |
- 结论：spinlock/mutex/rwsem 算法、指令、顺序**同源**（都 LSE `casa` fast path + LL/SC 回退 + `sevl/wfe` 等待，pending/owner 编码一致）。唯一现实差异 = LSE patch 是否生效（当前已由 LSE early static_key 解决）。
- `secondary_start_kernel` 顺序两版一致（`msr tpidr_el1 → atomic_inc(cpu_count) → TTBR0/tlbi → status=CPU_BOOT_SUCCESS → notify_cpu_starting → set_cpu_online → notify_cpu_online → set_cpu_active → local_irq_enable`）。
- `kernel_init_freeable` 调用序两版相同；**关键差异：原厂 `smp_cpus_done` 经 stop_machine 完整应用系统级 alternatives，mainline 曾跳过（已修复）**。
- sysfs 锁顺序（`kobject_add_internal` 0xffffffc011682444 / `device_add` 0xffffffc010b75d04）**无差异**。
- config 并发选项：原厂 LTO/CFI/SCS=y、RCU 调优不同、NR_CPUS 32 vs 512——影响时序，不构成死锁。

### 2.3 REPORT-stock-sync-mechanism.md（stop_machine/alternatives 同步协议，核心）
| 符号 | 原厂地址 |
|---|---|
| `__apply_alternatives_multi_stop` / `__apply_alternatives` | `ffffffc010114c0c` / `114cd8` |
| `apply_alternatives_all` / `all_alternatives_applied` | `ffffffc01214e354` / `ffffffc01250ca18` |
| `stop_machine` / `multi_cpu_stop` / `stop_cpus` | `ffffffc01030a2f0` / `30a420` / `30a60c` |
| `__clean_dcache_area_poc` / `__flush_dcache_area` / `__clean_dcache_area_pou` | `ffffffc01008709c` / `086fcc` / `087004` |
| `psci_cpu_on` / `psci_cpu_off` / `psci_cpu_suspend` / `cpu_cluster_pm_enter` / `cpu_cluster_pm_exit` | `ffffffc010f5ca3c` / `f5c990` / `f5c794` / `38013c` / `380218` |
- 核心证据：原厂二级核等待 = `yield` 轮询普通 `ldr`（无 dc/dsb/wfe/sev）；CPU0 普通 `str` 置 flag（无 dcache clean/sev）。**原厂协议比 mainline 当时的实现（每轮 `dcache_inval_poc`）更裸却能跑 → PoC 可用，问题在"核是否进入循环/flag 地址是否一致/派发状态机"**。
- 原厂 stop_machine 完成同步：LSE `ldaddal` + `dmb ishst`；LL/SC `ldxr/stlxr`+`dmb ish`——与 mainline 同源。
- 推荐可移植方案（已在当前代码实施）：二级核 `while (!READ_ONCE(flag)) cpu_relax(); isb();`；CPU0 `dsb(ish); WRITE_ONCE(flag,1)`；不要每轮 dcache_inval_poc；不用 wfe/sev。

### 2.4 REPORT-stock-vs-mainline-completion.md（completion + smp_cpus_done 窗口）
| 符号 | 原厂地址 |
|---|---|
| `complete` / `complete_all` | `ffffffc010227a70` / `227898` |
| `__wake_up_common` / `default_wake_function` / `autoremove_wake_function` | `ffffffc010226000` / `1fa220` / `225640` |
| `try_to_wake_up` / `wake_up_process` / `smp_send_reschedule` | `ffffffc0101f6084` / `1f6060` / `117470` |
| `wait_for_common` / `wait_for_completion` | `ffffffc0116a0610` / `6a0d30` |
| `start_kernel` / `rest_init` / `kernel_init_freeable` / `do_basic_setup` | `ffffffc012148ab8` / `11698490` / `148288` / `148598` |
| `smp_cpus_done` / `setup_cpu_features` / `hyp_mode_check` | `ffffffc01214e4b8` / `14e198` / `14e50c` |
| `update_cpu_capabilities` / `enable_cpu_capabilities` | `ffffffc0101129f4` / `ffffffc01214e0a0` |
| `apply_alternatives_all` / `apply_boot_alternatives` / `mark_linear_text_alias_ro` | `ffffffc01214e354` / `14e388` / `150948` |
| `kpti_install_ng_mappings` / `early_irq_init` / `init_IRQ` / `time_init` / `tick_init` | `ffffffc0101133fc` / `15a940` / `14c810` / `14d5a0` / `15def8` |
| `smp_init` / `sched_init_smp` / `smp_prepare_cpus` | `ffffffc01215e7d8` / `1577fc` / `14e9bc` |
| `cpu_hwcaps` | `ffffffc01250c600`（bit28=DIC） |
- 结论 TL;DR：① completion 原语不是差异点（H3 排除）；② `ic ialluis` 已弱化（早期 LSE patch 执行过且 boot 通过）；③ smp_cpus_done→do_basic_setup 窗口两版同源（tick/IRQ 在 start_kernel 就绪并贯穿，无"patch 后恢复"特殊动作；唯一中断开关是 multi_cpu_stop 的 per-CPU local_irq_disable/restore）；④ **最可疑 = errata 覆盖范围**（4118414/1530923 首启假设——**注意：被 2.11/2.12 修正为两版实际相同**，见下）。
- KPTI：原厂 enable_cpu_capabilities 回调（cpu_install_idmap，不用 stop_machine）先于 alternatives；mainline kpti_install_ng_mappings（stop_machine）后于 alternatives → 窗口内 stop_machine 原厂 1 次 / mainline 2 次。

### 2.5 REPORT-mainline-completion-wakeup-analysis.md（mainline 唤醒链，纯源码）
- 新事实 F1–F7：F1 arm64 无 `TIF_POLLING_NRFLAG`（跨核唤醒必发 IPI）；F2 idle 用 `wfi()`（idle.c:30）；F3 `CONFIG_ARM64_PSEUDO_NMI` 未开（纯 DAIF，无 PMR）；F4 `hard_irq_disable()` arm64 为 no-op（multi_cpu_stop 只有 `local_irq_disable()`=daifset#3 有效）；F5 DIC 是 alternative 站点——早期 LSE patch 时未 patch→`ic ialluis` 执行；晚期 apply_alternatives_all 时已 patch→若 DIC=1 则跳过（**T770 DIC=0，仍执行**）；F6 只开 A76 errata；F7 REPEAT_TLBI/spec-AT patch 点不在 tick/调度/中断投递热路径（tlbflush.h 广播 flush / fault.c + KVM）。
- 完整唤醒链（6.12）：`cpu_stop_signal_done(:80 atomic_dec_and_test(nr_todo) → :81 complete)` → `complete_with_flags(:20 锁, :22 done++, :24 swake_up_locked)` → `swake_up_locked(:29 try_to_wake_up)` → `try_to_wake_up(:4245 TASK_WAKING, :4296 ttwu_queue)` → `ttwu_queue_cond(:3915 !cpus_share_cache→wakelist)` → `__ttwu_queue_wakelist(:3837 ttwu_pending=1, :3838 __smp_call_single_queue)` → `__smp_call_single_queue(smp.c:411 llist_add, :412 send_call_function_single_ipi)` → `arch_send_call_function_single_ipi(smp.c:859 smp_cross_call, IPI_CALL_FUNC)` → `gic_write_sgi1r` → CPU0 `do_handle_IPI(:964 generic_smp_call_function_interrupt)` → `__flush_smp_call_function_queue(:586 csd_do_func(sched_ttwu_pending))` → `sched_ttwu_pending(:3792 ttwu_do_activate)` → `ttwu_do_activate(:3690 activate_task, :3691 resched_curr, :3693 ttwu_do_wakeup)` → 中断返回 `schedule()`。
- **核心结论：arm64 上 `wait_for_completion` 返回 = CPU0 收到 SGI(IPI_CALL_FUNC) 并跑 `sched_ttwu_pending`。x=44 不亮 ⟺ SGI 没到/没处理；与"无心跳(PPI 死)"构成同一中断投递停摆的两个证据。**
- 根因排序：R1=CPU0 收不到 SGI（核心）；R2=晚期 patch 的 I-cache 一致性不彻底（M2：DIC=1 跳 ic→8 核轮无 ic 失效——**T770 DIC=0，此机制不适用**；M1：ic ialluis 8 核广播隐患——已被 2.6 排除）；R3=DAIF/GIC（需探针排除）；R4/R5 排除。
- 探针落点建议表（§3.4，供后续参考）：smp_cross_call 入口 / timer_handler 入口 / do_handle_IPI 入口 / sched_ttwu_pending 入口 / try_to_wake_up 入口 / resched_curr 入口 / DAIF 读值 / CTR_EL0 DIC 读值。

### 2.6 REPORT-LSE-atomic-completion-analysis.md（LSE atomic + thread_ack/nr_todo + alternatives 副作用）
- `atomic_dec_and_test` = LSE **`ldaddal`**（full ordering，acq+rel；`_relaxed`=ldadd、`_acquire`=ldadda、`_release`=ldaddl）；LL/SC = `ldxr + stlxr + dmb ish`（全序等价）；返回 `old-1`，`cbnz` 判零（≡ 旧值==1）。
- stock `multi_cpu_stop`（0xffffffc01030a420）内 thread_ack：LSE 0x30a55c–0x30a584（`ldaddal w8,w9,[x24]` → `add` → `cbnz` → `atomic_set(thread_ack,N)` 普通 str → `dmb ishst` → `WRITE_ONCE(state)`）；LL/SC 0x30a5f0–0x30a604。
- stock `cpu_stopper_thread`（0xffffffc01030adc8）内 nr_todo + complete：0x30aef4–0x30af0c（`ldaddal` → `cbnz` → `bl 0x227a70 complete(&done->completion)`）。
- 漏洞排查结论：初值都是 8、时序有屏障、无并发写、归零判据唯一 → **thread_ack 归零 3 次（PREPARE/DISABLE_IRQ/RUN/EXIT 各 8→0）证明 `ldaddal` 跨 8 核正确 ⇒ nr_todo 必归零 ⇒ complete() 必被调用。H4 正式排除，勿再分析 atomic。**
- DIC 判定链：`icache_inval_all_pou()`（cacheflush.h:133）= `alternative_has_cap_unlikely(ARM64_HAS_CACHE_DIC) ? return : (ic ialluis; dsb ish)`；DIC cap 是 SYSTEM_FEATURE，在 `setup_system_capabilities` 里 update 先于 apply_alternatives_all；DIC 站点本身被同轮 patch。**T770 CTR_EL0 bit29=0 → ic ialluis 真实执行**。
- stock `__apply_alternatives` 尾部（0x114e94–14eac）= `dsb ish; [cpu_hwcaps.bit28 ? skip : ic ialluis]; dsb ish; isb`——与 mainline `dsb(ish); icache_inval_all_pou(); isb()` **逐字等价**（原厂用 tbnz 位图测试，mainline 用 alternative 分支）。
- **`ic ialluis` 排除**：原厂在完全相同语境（smp_cpus_done → stop_machine，7 个二级核关中断 yield spin）执行同一条广播 `ic ialluis` 且跑通 Android；早期单核也执行过没挂。单核 vs 8 核的仅两点差异（广播域大小、对端 spin）被原厂同语境直接证伪。
- REPEAT_TLBI patch 模板（tlbflush.h:176-202）：`__repeat_tlbi_sync` = `__tlbi(vale1is,0)` + `dsb ish`；99 个落点在所有 `flush_tlb_*`/`switch_mm` TLB flush 收尾的 `__tlbi_sync_s1ish`。**不落在 tick 处理内**。
- SPECULATIVE_AT = `ALTERNATIVE("nop","isb",...)`，站点 **100% 在 KVM/hyp**（host 无 `at` 指令，grep 证实）；bring-up 不跑 guest，对 boot/tick 无影响。
- errata 差异表（§5.1，关键）：1286807 两版都=y（A76 r0p0–r3p0）；**4118414 mainline =y（MIDR_ALL_VERSIONS(A76) ⇒ 若 A76>r3p0 则 mainline 打、原厂不打）——但当前 .config 实际 is not set（见 2.11 修正）**；2441007 两版都不开（A55 不打 REPEAT_TLBI）；1165522 两版都=y（A76 r0p0–r2p0）；1530923 mainline =y（A55 spec-AT，KVM-only）——当前 .config 实际 is not set。
- 建议：读 MIDR_EL1 画屏（A76 revision 判定 4118414 差异是否真实）；Flash 1（y=440/450）钉死 complete/waiter/心跳；单变量回退 REPEAT_TLBI（关 4118414+1286807+3194386）；若确诊 H1 则查 ttwu→smp_send_reschedule→GIC SGI 与 multi_cpu_stop 退出的 local_irq_restore。
- 关键符号：`multi_cpu_stop` 0x30a420、`stop_cpus` 0x30a60c、`cpu_stopper_thread` 0x30adc8、`swake_up_locked` stock 0xffffffc010226d90。

### 2.7 REPORT-AGENTS-GUIDE-analysis.md（卡点归因：H1–H4）
- 硬事实链：① thread_ack 8 核归零 3 次 ⇒ LSE `ldaddal` 正确；② 8 核都过 signal_done ⇒ `atomic_dec_and_test(nr_todo)` 8 次 ⇒ 必有一次 true ⇒ **complete() 必然被调用**；③ nr_todo 与 thread_ack 用同一原语。
- 假设排序（当时）：H1=complete 已调但唤醒不生效（最可能，"无心跳"佐证 tick 停摆）；H2=stop_machine 后 tick/IPI 停摆的诱因（候选：ic ialluis 广播副作用 / errata workaround 首启 / 跨核唤醒 IPI 不送达）；H3=swait 空操作/waiter 不在队列（基本排除）；H4=nr_todo 未归零（排除）。
- stock `complete`（0x227a70）反汇编坐实：`casa` 抢 wait.lock → `done++` → 队列空判断 → 遍历 waitqueue → `blr entry->func`（KCFI 校验，桩表 0xffffffc0116ce9a0 → default_wake_function）→ exclusive 成功后停止 → `stlrb` 释放。**stock 5.4 = 传统 waitqueue**。
- 发现代码/文档不一致：当时 `icache_inval_all_pou()` 仍是 mainline 原版 `ic ialluis`，而文档计划是 `dsb(ish)`（当前代码已在 multi_stop 里额外加 `icache_inval_all_pou()+isb()` 双保险，见第 3 节——注意这与"不用无条件 ic ialluis"红线仍存在表述张力，文档待更新）。
- Flash 1/2/3 验证方案（供参考）：y=440 x=8（入 wait）/y=450（complete 被调）/y=440 x=44（waiter 返回）；swait 队列空分支探针；icache_inval_all_pou 改回 dsb(ish)；关 4118414/1286807。

### 2.8 REPORT-stock-wakeup-chain-analysis.md（原厂完整唤醒链，SM#2 卡点压缩）
- **实测探针事实（旧探针构建，Aug 17 下午）**：y=450 亮（complete 已调）；x=296 亮（直接入队）；**x=188 亮（IPI_RESCHEDULE 已到 CPU0 do_handle_IPI）**；x=44/x=152 不亮（无 IPI_CALL_FUNC/wakelist）；y=440 计数不亮（wait 未返回）；timer/PPI 探针亮；**CPU0 DAIF 正常**。
- **卡点已被压缩到：「do_handle_IPI 收到 IPI_RESCHEDULE 之后」**——之后只有 scheduler_ipi（fold）→ 中断返回 → idle 循环 need_resched 检查 → schedule_idle → kernel_init。
- 反汇编（原厂）：
  - `cpu_stopper_thread` 0x30adc8：入口 preempt_disable（0x30ae08）+ `preempt_count_inc` 调 fn（0x30aec8）→ `blr fn`（0x30aedc）→ nr_todo dec_and_test（0x30aefc）→ complete（0x30af08）→ preempt_count_dec → 队空收尾**显式 preempt_schedule()**（0x30b070–0x30b0ac，Unisoc 定制）→ 返回 smpboot_thread_fn。
  - `smpboot_thread_fn` 0x1e3dc8：与 v5.4.210/6.12 逐行一致（无 work 时 `preempt_enable_no_resched(); schedule();`）。
  - `resched_curr` 0x1f3b30：同核分支 `stset`(LSE 置 TIF bit1)+清 [current+28]；跨核分支 `stset` + `smp_cross_call(cpumask_of(cpu), 0)`（IPI_RESCHEDULE=0）。
  - `smp_send_reschedule` 0x117470 → `smp_cross_call` 0x1169b4 → `__ipi_send_mask` 0x278694 → GIC。
  - `ipi_handler` 0x116cac：jump table 分发；IPI_RESCHEDULE → `scheduler_ipi` 0x1fc658（真实函数）。
  - `try_to_wake_up` 0x1f6084：跨核走 ttwu_queue_remote（`rq->wake_list` + IPI_RESCHEDULE）。
- 结论：唤醒链（stopper→complete→ttwu→resched_curr→SGI→do_handle_IPI→scheduler_ipi→idle→schedule_idle）**逐环节等价**，原厂没有 mainline 缺失的"保证 waiter 被调度"机制。
- 真实代码演进差异（均不影响本场景语义）：① scheduler_ipi 函数 vs inline；② wakelist 传输通道 5.4 `rq->wake_list`+IPI_RESCHEDULE ↔ 6.12 CSD+IPI_CALL_FUNC；③ 6.12 `ttwu_queue_cond` 新增"目标 rq 空→wakelist"分支（同簇唤醒到空闲核时 6.12 走 CALL_FUNC、5.4 走直接入队+RESCHEDULE；**SM#2 实测 CPU0 rq 非空未触发**）；④ 原厂 stopper 队空多一次 preempt_schedule；⑤ thread_info 布局定制（preempt_count@+24、折叠标志@+28、state@+40，语义等价）。
- 最可能解释（该报告 R1）：CPU0 中断返回路径/idle 循环/调度代码在 8 核 patch 后取到旧指令（"半新半旧"）→ SGI 能进 do_handle_IPI（探针亮）但返回路径/need_resched 检查异常 → kernel_init 永不被调度。后续报告（2.12）在此基础上把矛头进一步指向跨簇 CSD+IPI_CALL_FUNC 传输本身。
- 5.4 源码对照已拉取：`work/stock-5.4.210/`（kernel/stop_machine.c、smpboot.c、sched/core.c、arch/arm64/kernel/smp.c、thread_info.h、process.c）。

### 2.9 REPORT-stock-alternatives-waiter-analysis.md（alternatives 规模 + waiter 精确序列 + LSE 打补方式）
- waiter 侧精确序列（stock `wait_for_completion` 0x6a0d30 → `wait_for_common` 0x6a0610）：持锁（casa 0x670）→ 查 done → **入队**（list_add_tail，WQ_FLAG_EXCLUSIVE）→ **置 __state**（task+40）→ 解锁 → `schedule_timeout`（0x748）→ 醒来持锁重查 done → done==0 且 timeout 未到则重新置态再睡 → **waiter 自摘链**（0x7fc `bl list_del` 0x952ab0）。先入队后置态、都在锁内 ⇒ 无丢唤醒窗口。
- waker 侧（stock complete 0x227a70）：`done++`（UINT_MAX 除外）→ 队列空直接返回 → 遍历 waitqueue（跳过 WQ_FLAG_WOKEN）→ `blr func`（default_wake_function 0x1fa220 / autoremove）→ exclusive 且唤醒成功才停；**waker 不摘链**。
- mainline `swake_up_locked`（swait.c:21-31）：队列空静默返回；唤醒第一个 waiter；**无条件 `list_del_init`（唤醒失败也摘）**——一旦 try_to_wake_up 因状态异常返回 0，kernel_init 队列项被不可逆摘除，后续 complete 再也找不到它。**与"y=440 永不返回"吻合（放大因素，非根因）**。
- alternatives 规模（二进制解析 struct alt_instr=12B）：原厂 `__alt_instructions` 0xffffffc0121cd988→0x122b11f0，0xE3868，**77,662 槽（~77,116 有效）**：cap42(PAN)=57,220、cap11(LSE)=11,331、cap10=3,822、cap9=1,724、cap1=1,433、cap5=1,027。mainline 探针构建 0xffffffc081ce7514→0x081d409b0，0x5949C，30,477 槽（29,649 有效口径）：cap95(DEVICE_LOAD_ACQUIRE)=13,066、**CB37(LSE asm-goto NOP-落入)=10,712**、37(classic LSE)=880、51(VHE)=2,385、CB1=1,137、39(NESTED_VIRT)=558、98(repeat-TLBI)=97、68(KPTI)=67、99(spec-AT)=24。
- **调度/唤醒核心函数内替代点：原厂 ~320 条 vs mainline ~47 条**（原厂 complete 6、wait_for_common 8、try_to_wake_up 11、__schedule 16、do_idle 28、scheduler_ipi 6、cpu_stopper_thread 8 等，绝大多数 cap42 PAN；mainline __schedule 18、resched_curr 2、try_to_wake_up 1，**mainline 的 complete/swait 内部替代点为 0**）。"mainline 12K 更容易碰到调度/唤醒代码"在数字上不成立。
- **errata 配置修正**：探针构建 .config（Aug 17 12:44）里 `4118414`、`1530923` 均 is not set → 与 stock 同口径；表内 cap98=97/cap99=24 只是静态条目，是否 apply 取决于 A76 修订。
- **机制级新发现（LSE 打补方式）**：原厂 11,331 条 LSE 全部**经典替换**（memcpy 覆盖 orig 站点，orig/alt 两独立副本）；mainline 11,592 条中 **10,712 条是 `alternative_has_cap_likely()` 运行时检查（`ALTERNATIVE_CB("b %l[l_no]",...,alt_cb_patch_nops)`）→ 把分支原地 NOP、执行流落入紧跟的内联 LSE 序列**（同 cache line "NOP+LSE" 一体），880 条经典替换。若 8 核 ic ialluis 广播存在任何不完备，**NOP-落入式对"半新半旧"比覆盖式更敏感** → 首选单变量（已通过改 static_key 弃用此方式，见第 3 节）。
- 综合归因（该报告 §6.3）：complete()（y=450）确实执行，但 swake_up_locked 未对 pid==1 调 ttwu（探针不亮）⟹ SM#2 complete 时 kernel_init 的 swait 队列项不在队列中，或 `wake_up_state()` 因 `__state` 异常返回 0 ⟹ **kernel_init 的 task/栈侧状态在 8 核 patch 风暴中被破坏**（CPU0 半新半旧指令，尤其 __schedule/resched_curr/中断返回路径的 LSE NOP-落入站点）+ swait 无条件摘链放大。
- 符号速查（该报告附录，mainline 地址为 Aug 17 13:22 构建）：`wait_for_completion` mainline 0xffffffc0810737d4；`complete` 0xffffffc0800e3a88；`swake_up_locked` 0xffffffc0800e40ac；`try_to_wake_up` 0xffffffc0800c54cc；`resched_curr` 0xffffffc0800c0c70；`__schedule` 0xffffffc081072008；`do_idle` 0xffffffc0800d9444；`cpu_stopper_thread` 0xffffffc08016cfd8；`multi_cpu_stop` 0xffffffc08016d6e8；`cpu_stop_signal_done` 0xffffffc08016cf4c；`stop_machine` 0xffffffc08016dc80；`__apply_alternatives` 0xffffffc080020830；`__apply_alternatives_multi_stop` 0xffffffc081c55458；`apply_alternatives_all` 0xffffffc081c555b8；`alt_cb_patch_nops` 0xffffffc08106edac。

### 2.10 REPORT-alt-table-integrity.md（alt 表完整性：828 条异常证伪）
- 解析对象：`linux-6.12.103/vmlinux`（Aug 17 13:22 构建，与 Image/System.map 同批，已逐字节比对 Image 的 alt 表一致）；脚本 `work/parse_alt_table.py`，完整数据 `work/alt_table_records.csv`。
- **结论："828 条字段异常"无法复现 = 分析误差，不是真实危险**。正确语义解析 30,477 条：**0 条 orig 越界、0 条 alt 越界、0 条 len 异常、0 条 cap 异常、0 条写窗口跨段、0 条未对齐**。"828" = 30,477 槽 − 29,649"有效"的口径差，其"有效"判定与二进制不符。
- per-cap 实测修正（与 2.9 前报告不符，前报告基于另一构建/解析）：95→13,178；CB37→10,901；37(classic)→1,175；51→2,401；CB1→1,220；39→558；54→142；98→**99**；68→71；99→24；42→54；36→53；45→59；25CB→50。
- origptr 落区：.text 29,991 / .init.text 359 / .rodata.text 123 / .exit.text 4；**全部写入目标都在可执行文本段内**，没有任何一条写 .data/.bss/.percpu/alt 表自身；与 init_task（0xffffffc0826a4640）、runqueues（0xffffffc081e5cc00）、__per_cpu_offset（0xffffffc08269d730）最近距离也在 .text 内部合法 LSE 站点。
- **"lm_alias(origptr) 写坏 waiter 状态"的物证路径不存在**（lm_alias 越界写坏只在 origptr 越界时成立，本表 0 越界）。
- 建议：可加一次性启动期 `is_kernel_text(origptr)` 校验（防未来构建回归）；waiter 卡点调查放弃该路径，回到 2.9 主假设（CPU0 半新半旧 + swait 无条件摘链）。
- 关键地址：`__alt_instructions` 0xffffffc081ce7514 / end 0x081d409b0（.altinstructions 段 fileoff 0x1cf7514，size 0x5949C）；`alt_cb_patch_nops` 0xffffffc08106edac；`kernel_init` 0xffffffc0810701e0。

### 2.11 REPORT-stock-crosscluster-visibility.md（原厂跨 cluster 可见性操作模式）
- 原厂跨核写四种模式（无"裸普通 store 被单次读"）：① 原子 RMW（thread_ack/nr_todo `ldaddal`、TIF 跨核置位 `stset`、spinlock `casa`）；② 持锁临界区（completion done++ 锁内普通 str + `stlrb` 释放、rq->nr_running）；③ **前置 store barrier + 普通 store + 轮询读者**（msdata->state = `atomic_set`(str) + `dmb ishst` + `WRITE_ONCE`，读者 yield 轮询；all_alternatives_applied = 前置 `dsb ish` + 普通 str，读者 yield 轮询 + isb）；④ 原子位 + IPI（resched_curr `stset` → SGI，GIC 侧 `dmb ishst` 兜底）。
- 原厂 SGI 发送路径反汇编：`gic_ipi_send_mask` 0xffffffc0109aa6bc：0x9aa770 `dmb ishst` → 0x9aa788 `str`（内存映射 SGI1R，per-cpu 指针+0xf00）；mainline `irq-gic-v3.c:1428`：`dsb(ishst)` → `gic_write_sgi1r(ICC_SGI1R_EL1)` → `isb()`。两版"发 IPI 前先发布普通 store"语义一致。
- **修正（重要）**：`REPORT-stock-vs-mainline-completion.md §7` 声称 mainline 开 4118414/1530923——**与实测 .config 不符（均 is not set）**；其"mainline 首次在 T770 打 repeat-TLBI/spec-AT"假设不成立，其 §8 建议 1 是空操作。两版在 T770 上实际 errata patch 覆盖相同（仅 1286807/1165522）。
- 探针自省（重要）：`t508n_sm2_done`（alternative.c:237 写，swait.c:30/33/37、core.c:3797/3985 读）是**内核里唯一"裸普通 store + 单次读"跨核模式**，是探针自身问题不是内核逻辑；且 SM#2 的 complete 必由最后一个执行完 fn 的核调用（同核先写后读），**同核自写自读不可能读到 false**——若观测到"waker 读 false"，要么是 fn 之前的另一次唤醒，要么需复核探针写入位置/时序。
- 建议：写侧 `smp_store_release(&t508n_sm2_done,true)`、读侧 `smp_load_acquire` 或至少 READ_ONCE；不要用 dcache clean/inval 处理该 flag。

### 2.12 REPORT-stock-page-alloc-init.md（page_alloc_init_late 区域）
- stock `page_alloc_init_late` 0xffffffc01216ab1c 反汇编：主循环只 `bl set_zone_contiguous`（0x4975ec），**无 memblock_discard 调用**（ARCH_KEEP_MEMBLOCK=y 编译掉）；shuffle 挂在 jump label（0xab38 nop）后默认关。
- stock `buffer_init` 0xffffffc0121717f4：`bl kmem_cache_create_usercopy`(0x44e064) → `bl nr_free_buffer_pages`(0x4959dc) → `bl __cpuhp_setup_state`(0x1939cc)——在 **start_kernel（单核 pre-SMP）** 执行。
- 6.12 结构性差异：`buffer_init`/`mem_init_print_info` 从 pre-SMP 搬进 post-SMP 的 `page_alloc_init_late()`（mm/mm_init.c:2291），另新增 `page_alloc_sysctl_init()`。原厂执行顺序：smp_init → sched_init_smp → page_alloc_init_late → page_ext_init → do_basic_setup。
- config：SHUFFLE mainline 未开（编译掉）/ 原厂开但 static key 关（boot 都不 shuffle）；NUMA=y、ZONE_DMA=y 是 mainline 独有；DEFERRED_STRUCT_PAGE_INIT 两版都未开。
- 判别表（当前探针已实现）：(188,240) 亮 224 不亮=卡 mem_init_print_info；(224,240) 亮 260 不亮=卡 buffer_init（最高嫌疑 post-SMP 首次 cpuhp/slab）；(260,240) 亮 296 不亮=不可能（memblock_discard 编译掉）；(296,240) 亮 (8,230) 不亮=卡 set_zone_contiguous（查 zone 布局）；(8,230) 亮 152 不亮=卡 page_alloc_sysctl_init。
- 修复建议：把 buffer_init 搬回 start_kernel（复刻原厂时序）；page_alloc_sysctl_init 提前；config 对齐（NUMA=n、ZONE_DMA=n）；maxcpus=1 对照。

### 2.13 REPORT-stock-zone-memory.md（set_zone_contiguous / NOMAP / 内存布局，上一轮核心卡点）
- stock `set_zone_contiguous` 0x4975ec 反汇编：pageblock=1024 页（4MB），循环体**无 cond_resched**；`__pageblock_pfn_to_page` 0x497338；`pfn_valid` 0x126b38（early section 分支：memblock 二分查找 + **NOMAP 拒绝** `bic w0, w9, w8, lsr #2`（flags bit2=MEMBLOCK_NOMAP → return 0）；`pfn_to_online_page` 0x4974f8 同样拒绝）。
- mainline 6.12：arm64 不定义 pfn_valid → 通用 SPARSEMEM 版（mmzone.h:2024，只看 SECTION_HAS_MEM_MAP + early_section/pfn_section_valid，**无 NOMAP 检查**）；`pfn_to_online_page` 0xffffffc080247750 同样不看 NOMAP；`__pageblock_pfn_to_page` 0xffffffc08023ccfc（只查 end_pfn，且该函数无原子指令）。
- 内存布局：stock 与 mainline 的 `set_zone_contiguous` **遍历物理范围相同**（stock: DMA32 2–4GB + NORMAL 4–5GB；mainline: ZONE_DMA 2–4GB + DMA32 空 + NORMAL 4–5GB）；**差异在"过洞行为"**——我们的 DTS 把 sipc-mem@0x87800000（2.109GB）/cp-modem@0x88000000（2.125GB）等全部标 no-map，mainline 遍历**穿过 no-map 空洞**（stock 在第一个 no-map pageblock 即 return），卡点推算 = 第一个 no-map 交界（y=230 bar 停 6–7 格，与"疑似 2GB 附近"吻合）。
- vmemmap 完整性：6.12 sparse_init/memmap_init 用无 NOMAP 过滤的 `for_each_mem_pfn_range`，no-map 区 section 有 vmemmap、struct page 已初始化；vmemmap 后备页分配跳过 NOMAP → 不会落在 modem 区。纯 MM 逻辑层面遍历本应跑完；**主嫌疑 = 过洞触碰 modem/安全世界区域边界时同步异常（data abort）或总线级 stall**（无 console，oops 不可见，表现为卡死）。
- 修复建议（**已实施**）：`__pageblock_pfn_to_page` 开头加 `pfn_is_map_memory(start/end)`（= memblock_is_map_memory，排除 NOMAP）→ 复刻原厂"遇洞即停"；配套建议 DTS 去 no-map、config 对齐（NUMA=n、ZONE_DMA=n）。

### 2.14 REPORT-stock-driver-init.md（driver_init 链）
- 6.12 `driver_init()`（drivers/base/init.c）调用序：`bdi_init → devtmpfs_init → devices_init → buses_init → classes_init → firmware_init → hypervisor_init → of_core_init → platform_bus_init → auxiliary_bus_init → memory_dev_init → node_dev_init → cpu_dev_init → container_dev_init`（14 个，CPU0 顺序执行）。
- stock driver_init 反汇编 8 调用链见 §1.4；**原厂无 bdi/devtmpfs/auxiliary/node**；顺序 cpu→memory（6.12 是 memory→node→cpu）。
- 各成员工作量/嫌疑：devices_init（4 kernfs 目录+device_link_wq，**代码无卡点**）；of_core_init（数千节点 sysfs attach，链内最重，**历史卡点#1**）；memory_dev_init（~48 次 device_register，**历史卡点#2**；6.12 删了 5.4 的 mem_sysfs_mutex）；node_dev_init（postcore→链内）；cpu_dev_init（8 次 device_register）。
- 锁/同步：kernfs 每 root rwsem（5.4 全局 kernfs_mutex，本链只有 CPU0 触碰，无差异）；device_add 的 devt=0 → **不触 devtmpfs_create_node**（链内无 kdevtmpfs 交互）；SLUB 6.12 用 `try_cmpxchg128`（LSE `casp`/`caspl` 或 `ldxp/stxp`，5.4 是 cmpxchg_double）；原子选择机制 = `system_uses_lse_atomics()` static_key（**已修复为原厂式，见第 3 节**——jump_label patch 即便某核 I-cache 未同步，执行的也是旧 LL/SC 分支，无指令级混用竞态，这解释了 stop_machine 已通过）。
- 假设排序：H1（实测卡点不是 devices_init 而是链内更重成员，先验证）、H2（8 核并发下低层原语间歇原子/锁活锁）、H3（of_core_init 量级）、H4（memory_dev_init 48 次 device_register）、H5（I-cache/alternatives 残余，低）、H6（devtmpfsd 干扰——链内低、**链后高**：do_initcalls 之后首个带 devt 的 device_add 会走 devtmpfs_create_node + wait_for_completion）、H7（"慢"而非"死"，softlockup 关 = 诊断盲区）。
- 验证方案：一次刷机读全 y=220/240/260/300/340/380（当前探针已实现）；maxcpus=1 隔离；LSE A/B（临时不 enable static_key 强制 LL/SC）；of_core_init 循环内探针；开 SOFTLOCKUP/HARDLOCKUP。
- config 差异备忘：DEVTMPFS stock 关 mainline 开（kdevtmpfsd 常驻）；NR_CPUS 32 vs 512；SOFTLOCKUP stock 开 mainline 关；NUMA 等。

### 2.15 REPORT-stock-wakeup-nohz.md（异簇唤醒/wakelist/NOHZ——最新报告，直接指向竞态 A）
- 决策点对比：5.4 `ttwu_queue`（core.c:2492）+ `ttwu_queue_remote`（:2445）：异簇（不共享 LLC）→ `rq->wake_list` + **IPI_RESCHEDULE**；同簇→直接入队。6.12 `ttwu_queue_cond`（:3884，报告称已被 T508N 修补删掉 `!nr_running→wakelist` 分支，仅剩异簇→wakelist）+ `__ttwu_queue_wakelist`（:3831）：per-CPU `call_single_queue`(CSD) + **IPI_CALL_FUNC**。**跨簇唤醒在修补后仍走 CSD+IPI_CALL_FUNC**（"同簇修补无效"的直接原因：竞态在跨簇路径）。⚠️ 当前树无该修补（见文件头提醒）。
- 传输通道：两版都是标准 llist 模式（"队列空→非空 → 恰发一个 IPI"），逻辑上均无丢失唤醒窗口；**间歇性失败只能来自投递/处理层偶发故障**（SGI#1 偶发丢失 / CSD 队列未被冲刷 / ATF CPU_SUSPEND 返回丢 SGI / 指令流问题）。
- 6.12 的 NOHZ idle balance kick（`kick_ilb`，fair.c:12374）也改成 `smp_call_function_single_async(ilb_cpu, &rq->nohz_csd)`（CSD+IPI_CALL_FUNC，且 CSD_FLAG_LOCK 未清时静默 -EBUSY 丢 kick）；原厂/5.4 用 `smp_send_reschedule`（IPI_RESCHEDULE）。**6.12 把所有跨核异步通知（唤醒+nohz kick）都押注在 CSD+IPI_CALL_FUNC 链路上，原厂全部走 IPI_RESCHEDULE（T770 上被证明可靠）**。
- 原厂 scheduler_ipi 0x1fc658 完整反汇编：fold → wake_list 非空则 `rcu_irq_enter → [idle] tick_irq_enter → irqtime_account_irq → sched_ttwu_pending` → **自触发 idle balance kick**（rq 空闲 && wake_list 空 && !need_resched && !cpu_isolated → `idle_balance=1` + `raise_softirq(SCHED_SOFTIRQ)` + 唤醒本核 ksoftirqd）→ irq_exit。上游 5.4 用 `got_nohz_idle_kick()`；6.12 只有 fold。
- **竞态 A 完整链条**：`devtmpfs_init(CPU0) → kthread_run(devtmpfsd) → kthread_create_on_node: wake_up_process(kthreadd)[同簇直接入队 ✓] → kthreadd: kernel_thread → wake_up_new_task(p)`（两版相同：直接入队 + IPI_RESCHEDULE，选到空闲 A76 核）→ kdevtmpfs 在 A76 跑 `kthread()` 入口：`__set_current_state(UNINTERRUPTIBLE); complete(create->done); schedule_preempt_disabled()` → 睡在 A76 → `kthread_create_on_node` 返回（探针 y=270 x=80 亮 ✓）→ **`kthread_run` 二次 `wake_up_process(kdevtmpfs)`（CPU0/A55 → A76，跨簇现有任务唤醒）**：5.4 `ttwu_queue_remote`（wake_list + IPI_RESCHEDULE，可靠）vs 6.12 `__ttwu_queue_wakelist`（CSD + IPI_CALL_FUNC，★唯一失败点）→ `wait_for_completion(&setup_done)`（y=270 x=116 不亮 = 卡死）。
- **根源判断：跨簇 CSD+IPI_CALL_FUNC 传输/处理在 T770 上间歇性失效是最符合证据的根因**（同簇修补无效 + kthread_run 返回但线程未运行 + 卡点漂移 + 5.4 同硬件可靠）。
- **竞态 B（memory_dev_init device_register 前卡）与 A 大概率同源**：memory_dev_init 自身无跨簇唤醒依赖（devt=0），而是"更早某次跨簇唤醒失败 → 某核/任务进入坏状态（持锁任务未醒/软中断堆积/idle balance 停滞）→ CPU0 在其后的分配/锁/同步点卡住"，卡点随"哪次唤醒丢失"漂移。
- 修复方案：**P1**（首选，一行改动）：`ttwu_queue_cond` 恒 false → 所有唤醒直接入队 + resched_curr + IPI_RESCHEDULE（完全回到 5.4 语义，只用可靠的 SGI#0）。**P2**：恢复 5.4 式 rq->wake_list + IPI_RESCHEDULE 承载 wake_list（改动大）。**P3**：诊断隔离 `nohz=off` / `cpuidle.off=1` / `maxcpus=4`（仅 A55）/ pin kdevtmpfs 到 CPU0。**P4**：A76 侧 do_handle_IPI(IPI_CALL_FUNC) 到达 + sched_ttwu_pending 探针，区分"SGI 未到/CSD 未处理/处理后未调度"。**P5**：config 对齐。
- 其他差异扫描：`wake_up_new_task` 两版无差异（都不走 wakelist）；kthread() 入口无实质差异；GIC/SGI 基础设施同构；softlockup 关闭=卡死无诊断。
- 关键符号：`scheduler_ipi` stock 0xffffffc0101fc658；`ipi_handler` 0x116cac；`gic_handle_irq` 0x081a4c；`gic_smp_init` 0xffffffc012184e90；`set_smp_ipi_range` 0xffffffc01214eaf0；`try_to_wake_up` 0x1f6084；`sched_ttwu_pending` 0x1f3d48；`__cpu_isolated_mask` 0xffffffc012359d60；`ipi_irq_base` 0xffffffc012359ce8；`ksoftirqd` per-cpu 0xffffffc012322080。

---

## 3. 已修复的问题（当前代码改动，git diff HEAD cb8e725f3，16 文件 +204/−58）

> 以下为**当前工作树实际内容**（2026-08-18 07:38 构建的镜像包含全部）。

### 3.1 LSE static_key（替代 6.12 asm-goto NOP-落入）——修了什么
- `arch/arm64/include/asm/lse.h`：`__lse_ll_sc_body` 的 LSE 选择从 `alternative_has_cap_likely(ARM64_HAS_LSE_ATOMICS)`（asm-goto `b l_no` + alt_cb_patch_nops NOP-落入，10,712 条 CB 型）**改为原厂 5.4 式 static_key**：
  ```c
  system_uses_lse_atomics() = static_branch_likely(&arm64_const_caps_ready) &&
                              static_branch_likely(&cpu_hwcap_keys[ARM64_HAS_LSE_ATOMICS]);
  ```
- `arch/arm64/include/asm/cpufeature.h`：extern 声明 `cpu_hwcap_keys[ARM64_NCAPS]`、`arm64_const_caps_ready`。
- `arch/arm64/kernel/cpufeature.c`：`DEFINE_STATIC_KEY_ARRAY_FALSE(cpu_hwcap_keys, ARM64_NCAPS)` + `DEFINE_STATIC_KEY_FALSE(arm64_const_caps_ready)`；`enable_cpu_capabilities()` 里对每个 cap `static_branch_enable(&cpu_hwcap_keys[caps->capability])`。
- `arch/arm64/kernel/alternative.c` `t508n_apply_lse_early()`（在 `smp_prepare_boot_cpu` 后、IRQ 关单核阶段）：不再 `__apply_alternatives` 只 patch LSE，改为 `set_bit(LSE, system_cpucaps)` + `static_branch_enable(&cpu_hwcap_keys[LSE])` + `static_branch_enable(&arm64_const_caps_ready)` + `caches_clean_inval_pou(_stext,_etext)` + `isb()`。
- **修了什么**：弃 6.12 的"分支原地 NOP、执行流落入相邻 LSE 序列"的指令几何（2.9 报告的机制级头号嫌疑）；jump_label 改 b 指令目标由 `kick_all_cpus_sync`（IPI）同步 I-cache，**未同步核执行旧 b（LL/SC）分支，语义正确仅慢，不存在指令级混用竞态**。

### 3.2 NOMAP 检查（__pageblock_pfn_to_page）——修了什么
- `mm/page_alloc.c` `__pageblock_pfn_to_page()` 开头新增（`#include <linux/memblock.h>`）：
  ```c
  if (!memblock_is_map_memory(start_pfn << PAGE_SHIFT) ||
      !memblock_is_map_memory((end_pfn - 1) << PAGE_SHIFT))
      return NULL;
  ```
- **修了什么**：复刻原厂 5.4 `pfn_valid` 的 NOMAP 拒绝语义（memblock_is_map_memory 排除 NOMAP），`set_zone_contiguous` 在第一个 no-map pageblock（sipc-mem/cp-modem ≈2.1GB）即 return，不再穿过 no-map 空洞触碰 modem/安全世界区域（2.13 报告的同步异常/总线 stall 主嫌疑）。

### 3.3 __apply_alternatives_multi_stop 原厂式改造（alternative.c）
- 二级核：`while (!READ_ONCE(all_alternatives_applied)) cpu_relax(); isb();`（纯 yield 轮询，去掉逐轮 `__pi_dcache_inval_poc`）。
- CPU0：`__apply_alternatives(&kernel_alternatives, false, remaining_capabilities)` → `dsb(ish)` → `WRITE_ONCE(all_alternatives_applied, 1)` → **双保险 `icache_inval_all_pou(); isb();`**（注释：ic ialluis 广播可能对执行 patch 的 CPU0 自身失效不完整，补一次）。
- 注意：这里的 `icache_inval_all_pou()` 在 DIC=0 时执行 `ic ialluis` —— 与"不用无条件 ic ialluis"红线存在文档张力（2.7 指出的一致性整改项，代码与文档二选一对齐）。

### 3.4 恢复完整系统级 alternatives
- `apply_alternatives_all()` 不再跳过，恢复 `apply_alternatives_vdso()` + `stop_machine(__apply_alternatives_multi_stop, NULL, cpu_online_mask)`（与原厂 smp_cpus_done 一致）。修：LSE/REPEAT_TLBI/SPECULATIVE_AT 等系统级 patch 重新生效（修复 2.1 结论 D）。

### 3.5 stop_machine 完成路径（kernel/stop_machine.c）
- 新增全局 `volatile bool t508n_sm2_done;`（SM#2 开始置位，用于区分 SM#2 的唤醒 IPI 与早期 IPI；**注意 2.11 的自省：这是内核里唯一裸普通 store 跨核点，建议后续改 release/acquire**）。
- `cpu_stop_signal_done()` 改为返回 `bool`（最后一个把 nr_todo 减到 0 的返回 true）。
- multi_cpu_stop 里 CPU0 state 观测探针（y=150/160，见第 6 节）。

### 3.6 set_zone_contiguous 去掉 cond_resched（mm/mm_init.c）
- 循环体 `cond_resched()` 删除（排除调度/软锁嫌疑，与原厂无 cond_resched 一致），加迭代计数探针（y=230）。

### 3.7 DTS：删除 8 个 CPU 节点的 cpu-idle-states（arch/arm64/boot/dts/sprd/ums9620.dtsi）
- 所有 A55/A76 CPU 节点删掉 `cpu-idle-states = <&LIT_CORE_PD>/<&BIG_CORE_PD>`（idle-states 删除实验；nohz 报告 §3.3：CPUidle 状态两版面对相同 DTB 状态集，pd-big ATF CPU_SUSPEND 偶发丢 SGI 不能排除，需 cpuidle.off 隔离）。

### 3.8 KVM 关闭
- `.config`：`CONFIG_VIRTUALIZATION=y` 但 **`# CONFIG_KVM is not set`**（SPECULATIVE_AT 的 KVM 站点全部不生效，相关 alternative 不会 apply）。

### 3.9 探针系统全面重排（旧 y=120/370/390/410/430/440/450 探针与 t508n_pass/mark_row 调用已删除）
- 新增 y=150/160/180/200/220/230/240/260/270/280/290/300/310/325/340/380 探针，见第 6 节。
- `t508n_fbready()` 删除"新镜像标识"方块；`t508n_probe_timer`（y=60 tick 状态 + y=100 EL 级别）、`t508n_wdt_pet`（y=140 WDT 心跳）保留。

---

## 4. 已排除的假设（结论 + 依据报告）

| # | 假设 | 结论 | 依据 |
|---|---|---|---|
| 1 | 原子计数/`nr_todo` 未归零（H4，atomic 坏） | **排除**：thread_ack 8 核归零 3 次证明 LSE `ldaddal` 正确；nr_todo 同原语同次数必归零，complete() 必被调用；stock 反汇编逐字节同构 | AGENTS-GUIDE-analysis §3.2；LSE-atomic §1/§2/§6.1 |
| 2 | 丢唤醒/内存序（H3，waiter 不在队列） | **排除（软件层）**：completion 同一把 wait.lock 覆盖"入队+置态"与"done+++唤醒"，无经典丢唤醒；单 waiter 场景 waitqueue/swait 行为等价 | AGENTS-GUIDE-analysis §3.2；completion 报告 §1/§2/§6 |
| 3 | `ic ialluis` 在 T770 上 hang / 二次执行致 tick 停摆 | **排除**：原厂在完全相同语境（smp_cpus_done→stop_machine，7 核关中断 spin）执行同一条广播 `ic ialluis` 且跑通 Android；早期单核也执行过没挂；DIC=0 时真实执行；"单核 vs 8 核"两点差异被原厂同语境证伪 | LSE-atomic §3.5/§4.3；AGENTS-GUIDE-analysis §3.2 |
| 4 | mainline 独有启用 4118414/1530923（repeat-TLBI/spec-AT 首启破坏 tick） | **排除（当前构建）**：实际 .config 两者 is not set，与 stock 同口径；且 REPEAT_TLBI patch 点不在 tick 路径、SPECULATIVE_AT 站点全在 KVM（KVM 又已关）。修正了 completion 报告 §7 的假设 | crosscluster §3.3；alternatives-waiter §3.4；mainline-completion-wakeup F7 |
| 5 | alt 表字段异常（828 条）→ lm_alias 写坏 waiter 状态 | **排除**：正确解析 30,477 条全部合法、0 越界；写入目标全在可执行段，物理上不可能写坏 init_task/runqueues/task 栈 | alt-table-integrity 全文 |
| 6 | 唤醒链代码差异（原厂比 mainline 多了保证 waiter 被调度的机制） | **排除**：complete→ttwu→resched_curr→SGI→do_handle_IPI→scheduler_ipi→idle→schedule_idle 逐环节等价；preempt_count 处理两版一致；差异仅是 scheduler_ipi 函数/inline、wakelist 通道、ttwu_queue_cond 空闲分支（本场景未触发） | wakeup-chain §6.1/§6.2 |
| 7 | PoC/cache 硬件不可靠 | **排除**：原厂普通 str+yield 轮询跨 cluster 通信跑通整套系统；原厂无 CCI/CMN/DSU 特判；不要再怀疑 | sync-mechanism §5.3；AGENTS-GUIDE 红线 |
| 8 | CCI/CMN/DSU/SCU 未初始化 | **排除**：原厂内核/DTS/SML 均无；不要补 | T508N-stock-analysis C 节；sync-mechanism 任务 3 |
| 9 | device model/sysfs 有 T770 特判（原厂加了 mainline 没加） | **排除**：原厂 device_register/device_add/kobject_add/memory_dev_init 无任何额外 barrier/cache/errata | T508N-stock-analysis A 节 |
| 10 | 锁实现差异（spinlock/mutex/rwsem） | **排除**：两版同源（LSE casa + LL/SC 回退 + sevl/wfe），唯一差异是 LSE patch 是否生效（已修复） | locks 报告 §1/§2 |
| 11 | shuffle / memblock_discard / deferred page init 卡点 | **排除**：mainline shuffle 编译掉、原厂 static key 关；memblock_discard 两版都因 ARCH_KEEP_MEMBLOCK=y 编译掉；deferred 两版未开 | page-alloc-init §1/§5.2 |
| 12 | completion waitqueue→swait 迁移是根因 | **排除（非根因）**：都收敛到 try_to_wake_up、单 waiter 等价；但 swait 的"无条件 list_del_init（唤醒失败也摘）"是放大因素（可逆性丢失） | completion 报告；alternatives-waiter §2 |
| 13 | ttwu_queue_cond 两种形态（同簇 nr_running→wakelist 分支 vs 直接入队） | **实验无效（同簇修补未解决竞态）**：删掉同簇"目标 rq 空→wakelist"分支后竞态仍在 → 失败点不在同簇而在跨簇路径（CSD+IPI_CALL_FUNC）。⚠️ 该修补不在当前树（见文件头提醒 2） | wakeup-nohz §0.1/§4.1 |
| 14 | DAIF.I 未恢复 / GIC 状态被 stop_machine 破坏（R3） | **探针显示正常（旧观测：CPU0 DAIF 正常、timer PPI 亮），静态分析 save/restore 对称**；但"中断投递停摆"的最终排除仍需在线探针（do_handle_IPI/timer_handler 入口）钉死 | wakeup-chain §0；mainline-completion-wakeup §2.4/R3 |
| 15 | "卡在 devices_init 本身" | **基本排除（代码层）**：devices_init 与 5.4 逐行一致（仅多 device_link_wq），无跨 CPU 锁依赖环/无等待；若实测卡住是链内更重成员（of_core_init/memory_dev_init）或低层原语 | driver-init §0/§1.3 |
| 16 | 分到 NOMAP 坏页（分配器拿 modem 页） | **排除**：free_low_memory_core_early 跳过 NOMAP，NOMAP 页不进 buddy；但 memmap_init 无 NOMAP 过滤 → struct page 被初始化为 present → set_zone_contiguous 会过洞（已由 3.2 修复） | driver-init §4.5 |

---

## 5. 当前未解决问题（竞态）

### 5.1 竞态 A：devtmpfs `wait_for_completion(&setup_done)` 间歇卡（kdevtmpfs 未 complete）
- 现象/探针证据（当前探针语义）：`devtmpfs_init` 的 y=270 **x=80 亮**（kthread_run 返回 = create->done 已完成）但 **x=116 不亮**（wait_for_completion 未返回）；kdevtmpfs 线程侧 y=290 **x=8 不亮**（线程未实际运行/未到 devtmpfsd 入口）。
- 机制链条（nohz 报告 §4.1，最新）：kthread_create_on_node/kthreadd/wake_up_new_task 路径两版一致且可靠（探针证明 create->done 完成）；**唯一失败点 = `kthread_run` 的二次 `wake_up_process(kdevtmpfs)`（CPU0/A55 → 空闲 A76，跨簇现有任务唤醒）**：6.12 走 `__ttwu_queue_wakelist`（CSD + IPI_CALL_FUNC），5.4 走 `ttwu_queue_remote`（rq->wake_list + IPI_RESCHEDULE）。
- 已实测（旧探针构建）：y=450 亮（complete 已调）；**x=188 亮（IPI_RESCHEDULE 已到 CPU0 do_handle_IPI）**；x=44/x=152 不亮（无 IPI_CALL_FUNC/wakelist）；y=440 计数不亮（wait 未返回）；timer/PPI 探针亮、CPU0 DAIF 正常 → 卡点在"do_handle_IPI 收到 IPI 之后"的调度/中断返回路径。

### 5.2 竞态 B：memory_dev_init → device_register 前间歇卡
- 现象：y=340 行在 device_register 前（x=296，memory.c:691）或循环内间歇卡；历史上 of_core_init sysfs attach 也卡过；卡点逐次漂移。
- 现状判断（nohz 报告 §4.3）：memory_dev_init 自身无跨簇唤醒依赖（devt=0 不触 devtmpfs）；**与竞态 A 大概率同源**——更早某次跨簇唤醒失败把某核/任务带进坏状态（持锁任务未醒、软中断堆积、空闲核失联→idle balance/RCU/页分配异常），CPU0 在 memory_dev_init 的重分配阶段撞上该状态；卡点随"哪次唤醒丢失"漂移。

### 5.3 已知观测汇总（读屏）
- y=325（每核 IPI 到达心跳）：**8 格全亮**（A76 也能收到 IPI，投递层未死）。
- y=310（每核 tick 心跳）：**A55 前 4 格亮、A76 后 4 格不亮**（A76 处于 NOHZ 空闲，tick 停）。
- 两种唤醒路径（CSD+CALL_FUNC / 直接入队+RESCHEDULE）下竞态相同。
- "加探针能过、不加就卡"的强时序敏感特征（SKILL 坑#5，stop_machine 时代遗留；当前阶段仍被引用为低层原语间歇活锁的佐证）。

### 5.4 当前最可能根因（最新报告排序，仅汇总）
1. **跨簇 CSD+IPI_CALL_FUNC 传输/处理在 T770 上间歇性失效**（6.12 独有暴露面；原厂同场景用 IPI_RESCHEDULE 可靠）——nohz 报告根因判断。
2. CPU0 在 8 核 patch 风暴（SM#2 ic ialluis 边界）后执行"半新半旧"指令（尤其 __schedule/resched_curr/中断返回路径），叠加 swait 无条件摘链导致 kernel_init 队列项不可逆丢失——alternatives-waiter/alt-table-integrity 主假设（与 1 可叠加：I-cache 不彻底 → 中断路径异常 → IPI 处理失效）。
3. （待在线排除）中断投递层本身（GIC/ATF CPU_SUSPEND 返回丢 SGI / DAIF）——R3。

### 5.5 未决的验证项（报告建议，未执行或未读回）
- 读 `MIDR_EL1` 画屏：确认 A76 revision（判定 4118414 差异是否真实——当前 config 已关，优先级下降）。
- 读 `CTR_EL0` DIC：已静态确认 DIC=0（LSE-atomic §3.2），观测探针未刷/未读回。
- 单变量回退实验：关 repeat-TLBI（1286807）对照；`ttwu_queue_cond` 恒 false（P1，nohz 报告首选，**当前树未实施**）；`nohz=off` / `cpuidle.off=1` / `maxcpus=4` / `maxcpus=1` 隔离；强制 LL/SC（不 enable LSE static_key）A/B。
- 开 `CONFIG_SOFTLOCKUP_DETECTOR`（消除"慢 vs 死"盲区；stock 开、mainline 关）。
- 探针钉死断点（P4）：A76 侧 `do_handle_IPI(IPI_CALL_FUNC)` 到达 / `__flush_smp_call_function_queue` / `sched_ttwu_pending` 执行，区分投递/处理/调度。
- `t508n_sm2_done` 改 release/acquire（crosscluster 建议，当前仍是裸 volatile store）。

### 5.6 当前代码状态（git diff 清单，HEAD cb8e725f3）
- 修改 16 文件：`arch/arm64/boot/dts/sprd/ums9620.dtsi`（删 cpu-idle-states）、`arch/arm64/include/asm/cpufeature.h`、`arch/arm64/include/asm/lse.h`、`arch/arm64/kernel/alternative.c`、`arch/arm64/kernel/cpufeature.c`、`arch/arm64/kernel/smp.c`（IPI 心跳探针）、`arch/arm64/kernel/t508n_dbg.c`、`drivers/base/core.c`（devices_init 探针）、`drivers/base/devtmpfs.c`（setup 探针）、`drivers/base/init.c`（driver_init 探针）、`drivers/base/memory.c`（memory 探针）、`drivers/clocksource/arm_arch_timer.c`（tick 心跳探针）、`init/main.c`（阶段探针）、`kernel/stop_machine.c`（SM#2 探针 + signal_done bool）、`mm/mm_init.c`（page_alloc 探针 + 去 cond_resched）、`mm/page_alloc.c`（NOMAP 检查）。
- 未修改（与报告描述不一致需注意）：`kernel/sched/core.c`（ttwu_queue_cond 修补**不在树中**）、`arch/arm64/include/asm/cacheflush.h`（icache_inval_all_pou 仍是 mainline 原版 ic ialluis，未改成文档计划的 dsb(ish)）。

---

## 6. 探针布局速查（当前固件，2026-08-18 07:38 构建）

> 屏幕 320×480，白块 = 探针点亮。**旧构建的 y=120/370/390/410/430/440/450 探针已全部删除**，以下为当前坐标。
> API：`t508n_paint_bar(x,y,w,h)`；`t508n_stage(n)` 顶部进度条（y=20，10 格，n=0..9）；
> `t508n_mark_row(y,n)`（行末标注，当前代码已不用）；`t508n_pass(y)`（3 大方块，已不用）。

| y | 含义 | 详细坐标 |
|---|---|---|
| 20 | 顶部进度条（t508n_stage 0..9） | x=8+n*30，28 宽 20 高：1=kernel_init 到达、2=kthreadd 完成、3=smp_prepare_cpus、4=workqueue_init、5=init_mm_internals、6=rcu_init_tasks、7=do_pre_smp_initcalls、8=lockup_detector_init、9=smp_init 完成（二级核上线） |
| 60 | tick 状态（t508n_probe_timer） | x=8：长条(300px)=jiffies 推进（tick 正常）；中条(150px)=无 tick |
| 100 | 异常级别（t508n_probe_timer） | x=8/38/68 每根 15 宽：1 根=EL1、2 根=EL2、3 根=EL3 |
| 140 | **WDT 心跳**（t508n_wdt_pet，每 2s） | x=8 宽 20：tick 活着才触发；**不闪 = tick 死** |
| 150 | **SM#2 multi_cpu_stop CPU0 状态观测**（仅 t508n_sm2_done && cpu==0） | x=8+state*36（state 0..7 各一格，看到哪个 state）；x=188 ack_state 后；x=224 rcu_momentary_eqs 后；x=260 循环退出（MULTI_STOP_EXIT） |
| 160 | **SM#2 multi_cpu_stop 退出路径**（t508n_sm2_done 门控） | x=8 循环退出；x=44 local_irq_restore 后 |
| 180 | **SM#2 per-CPU signal_done**（t508n_sm2_done 门控） | x=8+cpu*24，16 宽：8 核各一格，缺格=该核没到 cpu_stop_signal_done |
| 200 | **SM#2 complete**（last && t508n_sm2_done） | x=8 宽 24：最后一个核把 nr_todo 减到 0 并 complete 后点亮 |
| 220 | do_basic_setup 行 + __stop_cpus 完成计数 | x=8 cpuset_init_smp 后；x=44 driver_init 后；x=80 init_irq_proc 后；x=116 do_ctors 后。**另有 `__stop_cpus` 计数 x=44+i*10（stop_machine.c:489，每次 stop 多一格，上限 8）——注意与 x=44 位置重叠** |
| 230 | set_zone_contiguous 迭代计数 | x=44+n*10（n=0..7，每 8 次迭代一格，最多 8 格）；x=8 宽 24 = set_zone_contiguous 全部完成（返回后） |
| 240 | kernel_init_freeable post-SMP 行 | x=8 sched_init_smp 后；x=44 workqueue_init_topology 后；x=80 async_init 后；x=116 padata_init 后；x=152 page_alloc_init_late 后；x=188 mem_init_print_info 后；x=224 buffer_init 后；x=260 memblock_discard 后；x=296 shuffle_free_memory 后 |
| 260 | driver_init 行 | x=8 devtmpfs_init 后；x=44 devices_init 后；x=80 classes_init 后；x=116 hypervisor_init 后；x=152 of_core_init 后 |
| 270 | devtmpfs_init 行 | x=8 vfs_kern_mount 后；x=44 register_filesystem 后；**x=80 kthread_run 后**；**x=116 wait_for_completion(&setup_done) 后（不亮=竞态 A 卡死）**；x=276 (8×8) devtmpfs_init 正常完成 |
| 280 | devices_init 内部（core.c） | x=8 devices_kset 后；x=44 dev_kobj 后；x=80 block 后；x=116 char 后；x=152 device_link_wq 后（devices_init 完成） |
| 290 | devtmpfsd 线程（devtmpfs.c） | **x=8 线程启动（不亮=线程未运行=竞态 A 关键判据）**；x=44 setup（unshare+mount）完成；x=80 complete(&setup_done) 后 |
| 300 | driver_init 后半行 | x=8 auxiliary_bus_init 后；x=44 memory_dev_init 后；x=80 node_dev_init 后；x=116 cpu_dev_init 后；x=152 container_dev_init 后 |
| 310 | **每核 tick 心跳**（arm_arch_timer timer_handler） | x=8+cpu*24，16 宽：8 核各一格；**缺格=该核 tick 死**（观测：A55 前 4 格亮、A76 后 4 格不亮 = A76 NOHZ 空闲） |
| 325 | **每核 IPI 到达心跳**（smp.c do_handle_IPI 入口） | x=8+cpu*24，16 宽：8 核各一格；**缺格=该核从未收到 IPI**（观测：8 格全亮 = A76 收 IPI 正常） |
| 340 | memory_dev_init 行 | x=188 block_sz 计算后；x=224 subsys 注册后进入循环；**x=296 device_register 前（memory.c:691，竞态 B 卡点）**；x=260 memory_dev_init 完成 |
| 380 | memory.c __add_memory_block device_register 返回 | x=8 宽 24：每块 register 返回后点亮（正常应每块都亮） |

### 6.1 关键读屏判读速记
- **y=270 x=80 亮但 x=116 不亮 + y=290 x=8 不亮** → 竞态 A（kdevtmpfs 线程未被跨簇唤醒）。
- **y=325 8 格亮 + y=310 A76 缺格** → IPI 投递层正常，A76 只是 NOHZ 空闲 → 问题在"IPI 处理/调度推进"或"唤醒事件从未发出（跨簇 CSD 路径）"。
- **y=200 亮（SM#2 complete 已调）+ y=150/160/180 全亮** → stop_machine 阶段通过；卡点在后续唤醒。
- **y=230 x=44+n 停 6–7 格** → 曾指示 NOMAP 过洞（已由 3.2 修复，正常应快速返回并亮 (8,230)）。
- 心跳（y=140）持续闪 + 进度条停滞 → 系统还活着但推进停在某 initcall（"慢"而非"死"，softlockup 关无打印）。
