# REPORT-54-custom-build：复刻原厂 5.4 内核构建报告（5.4.210 + 紫光魔改 + 原厂 config）

> 日期：2026-08-18。构建会话产物：`/home/test/t508n/work/linux-5.4-custom/`
> 镜像：`linux-5.4-custom/arch/arm64/boot/Image`（31,621,632 B）
> 构建日志：`/home/test/t508n/work/build-54-custom.log`
> 源码树 git：3 个 commit（v5.4.210 pristine → olddefconfig → T508N mods），每魔改可 `git show` 查看 diff。

---

## 1. 构建概况

| 项 | 值 |
|---|---|
| 源码 | linux-5.4.210（`git archive v5.4.210` 提取，70087 文件，非 vendor 源码） |
| config | `work/kernel.config`（原厂）→ `make ARCH=arm64 olddefconfig` |
| 工具链 | `arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu`（gcc 13.3，**非原厂 clang 12/LTO**） |
| openssl | 本地 `openssl-local`，经 `CPATH`/`LIBRARY_PATH` 环境变量（未用命令行 HOST_EXTRACFLAGS，避开 dtc libfdt_env.h 覆盖坑） |
| 构建命令 | `make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8` |
| 结果 | **成功，0 错误，17 警告**；产物 `arch/arm64/boot/Image`（31,621,632 B）+ `vmlinux`（289,988,720 B） |

---

## 2. 魔改实现清单（全部验证进入二进制）

| # | 魔改 | 文件（5.4.210 位置） | diff 要点 | 二进制验证 |
|---|---|---|---|---|
| D1 | **pfn_valid NOMAP 拒绝**（最优先） | `include/linux/mmzone.h` pfn_valid() | `ret = early_section(ms) \|\| pfn_section_valid(ms, pfn);` 之后加 `if (ret && early_section(ms)) ret = memblock_is_map_memory((phys_addr_t)pfn << PAGE_SHIFT);`——early section（`SECTION_IS_EARLY`=bit3，与 stock 反汇编 "early section bit3" 逐位吻合）走 memblock 二分 + NOMAP(bit2) 拒绝；`memblock_is_map_memory` 为外部函数，前置声明避免 include 循环 | `pfn_valid@0xffffffc0100a6af0` 内 `bl memblock_is_map_memory` ✓ |
| D2 | **memmap_init_zone 逐页跳过 no-map** | 无独立改动 | 5.4 `memmap_init_zone()` 已逐页调 `early_pfn_valid(pfn)`（=pfn_valid，mmzone.h:1403），D1 生效后 no-map 区 struct page 天然不被初始化——与 stock 语义一致（`__next_mem_pfn_range` 不跳 NOMAP、靠 pfn_valid 拒绝） | 随 D1 ✓ |
| B1 | **scheduler_ipi 扩展**（fold+wake_list+自触发 nohz idle balance+唤醒 ksoftirqd） | `kernel/sched/core.c` scheduler_ipi() | 原 5.4 已有 fold + wake_list→sched_ttwu_pending + `got_nohz_idle_kick()`→idle_balance=1+raise_softirq；**补 stock 独有的 ksoftirqd 唤醒**：kick 分支内 `if (rq->curr==rq->idle && rq->nr_running==0 && !need_resched() && housekeeping_cpu(cpu,HK_FLAG_MISC)) wake_up_process(__this_cpu_read(ksoftirqd))`；cpu_isolated 用 housekeeping_cpu(HK_FLAG_MISC) 近似（5.4 无 vendor 的 isolation 全局） | `scheduler_ipi@0xffffffc010135900` 内 `bl try_to_wake_up` + `bl idle_cpu` + `bl raise_softirq_irqoff` ✓ |
| B2 | **cpu_stopper_thread 队空显式 preempt_schedule** | `kernel/stop_machine.c` cpu_stopper_thread() | `if (work){...goto repeat;} preempt_schedule();`——队空收尾显式让出（无 TIF_NEED_RESCHED 时为空操作，安全） | `cpu_stopper_thread@0xffffffc0101d6830` 内 `bl preempt_schedule` ✓ |
| E4 | **mem_sysfs_mutex（memory_dev_init 全程持有）** | `drivers/base/memory.c` | **无需改动**：5.4.210 原生已有 `static DEFINE_MUTEX(mem_sysfs_mutex)`（:29）且 memory_dev_init 全程 `mutex_lock/unlock`（:805-812）——与 DISASM-E "≡5.4.210 逐句对应"一致；**是 6.12 删了它**，复刻 5.4 天然带回 | —（源码级确认） |
| F1 | **shmem mode=0777**（无 S_ISVTX） | `mm/shmem.c` shmem_init_fs_context() | `ctx->mode = 0777 | S_ISVTX` → `ctx->mode = 0777`（0x1ff） | `mov w0,#0x1ff`（=0777 octal，无 0x400 sticky）✓ |
| C1 | **死循环门（可选，降级为计数）** | `arch/arm64/kernel/smp.c` handle_IPI case IPI_RESCHEDULE | stock 在 `ipi_cpu==cpu && state==1` 时 `while(1)`（hang-by-design，vendor 全局不存在于本复刻）；**降级**：`DEFINE_PER_CPU(unsigned long, t508n_ipi_resched_count)` + case 内 `this_cpu_inc()`，spin 骨架注释保留，不自旋 | `t508n_ipi_resched_count` 符号存在 ✓ |
| sugov | **sugov_set_freq_margin 25%（可选）** | `kernel/sched/cpufreq_schedutil.c` get_next_freq() | 新增 `static unsigned int sugov_freq_margin = 25;` + `void sugov_set_freq_margin(unsigned int)`；get_next_freq 对 raw freq 加 margin 并 clamp 到 `policy->cpuinfo.max_freq` | `sugov_set_freq_margin` 符号 ✓ |

魔改总量：6 文件、+81/-4 行（git commit `2ca901fb0`）。

---

## 3. olddefconfig 差异（原厂 config → gcc13 环境）

`kernel.config`（167,734 B）→ `make ARCH=arm64 olddefconfig` 后 861 行 diff。要点：

1. **编译器探测**：`CC_IS_CLANG=y`（clang 12.0.5）、`GCC_VERSION=0` → `CC_IS_GCC=y`、`GCC_VERSION=130300`。
2. **clang 专有项自动关闭**（gcc 下 olddefconfig 依依赖关系剔除）：`LTO_CLANG`、`CFI_CLANG`、`CFI_CLANG_SHADOW`、`SHADOW_CALL_STACK`、`SHADOW_CALL_STACK_VMAP`、`CC_HAVE_SHADOW_CALL_STACK`。
3. **KFENCE 符号丢弃**：config 里 `CONFIG_KFENCE=y`（sample=100）等 6 项——5.4.210 上游无 KFENCE Kconfig（KFENCE 为 5.12+ 特性，stock 回移），olddefconfig 自动丢弃。**未回移 KFENCE**（工程量较大，见 §6 建议）。
4. **Unisoc/SPRD 专有 Kconfig 丢弃**（上游 5.4.210 无这些符号）：`SPRD_CORE_CTL`、`SPRD_CORE_ISOLATION`、`SPRD_ROTATION_TASK`、`ARM_SPRD_CPUIDLE`、`ARM_SPRD_CPUFREQ_V2=m`、`ARM_SPRD_CPUFREQ_PUBLIC=m`、`SPRD_SIP_FW=m`、`CPU_FREQ_TIMES`、`CPU_FREQ_GOV_INTERACTIVE`、`PM_GENERIC_DOMAINS*`、`ARM64_AMU_EXTN`、`COMPAT_VDSO`、`GENERIC_IRQ_CHIP`、`GENERIC_IRQ_IPI` 等。原 config 中 `=m` 的符号在无 MODULES 支持路径下报 2 个 warning（`CHARGER_MANAGER`、`ION_CMA_HEAP` symbol value 'm' invalid）后被修正。
5. **保留一致的关键项**（与原厂反汇编佐证吻合）：`NR_CPUS=32`、`NUMA=n`、`SCHED_SMT=n`、`NO_HZ_IDLE=y`、`PREEMPT=y`、`HZ=250`、`JUMP_LABEL=y`、`DEVTMPFS=n`、`ZONE_DMA32=y`（DMA32/NORMAL/MOVABLE 3 zone）、`KVM=y`、`MODULES=y`、`RANDOMIZE_BASE=y`（KASLR）、`ARM64_ERRATUM_1286807/1165522=y`、`1530923/4118414=n`、`KALLSYMS_ALL=y`、`IKCONFIG=y`、`PSTORE_RAM=y`。
6. `CMDLINE` 保持 `""`（原厂同为空）。

---

## 4. 编译日志摘要

- **结果**：`BUILD EXIT: 0`，0 error。
- **警告 17 条**（均无害）：
  - OpenSSL 3.0 弃用 API 警告 ~8 条（certs/extract-cert.c、sign-file.c 的 `ERR_get_error_line`、`ENGINE_*`）——openssl-local 为 3.x 所致。
  - `prof_cpu_mask` 地址恒非 NULL `[-Waddress]` ×3（5.4 老代码在 gcc13 的已知警告，kernel/profile.c）。
  - `comparison between two arrays [-Warray-compare]` ×2（5.4 vs gcc13 已知）。
  - 其余零星（未显式 `-Werror`，不影响产物）。
- 编译时长约 5 分钟（j8，全量）。

---

## 5. 镜像对比（stock vs custom）

| 项 | stock_Image_full.bin | custom Image | 差异 |
|---|---|---|---|
| 尺寸 | 38,308,352 B | 31,621,632 B | **-6,686,720 B（-17.5%）** |
| 符号总数 | 116,704（stock_kallsyms.map） | 114,414（System.map） | 规模接近（-2%） |
| 关键符号 | 全部存在 | 全部存在（见下） | 一致 |

**关键符号存在性对比**（stock_kallsyms.map vs 本构建 System.map，全部 1:1）：

| 符号 | stock 地址 | custom 地址 |
|---|---|---|
| pfn_valid | 0xffffffc010126b38 | 0xffffffc0100a6af0 |
| sched_ttwu_pending | 0xffffffc0101f3d48 | 0xffffffc010132464 |
| scheduler_ipi | 0xffffffc0101fc658 | 0xffffffc010135900 |
| sugov_set_freq_margin | 0xffffffc01023a5e0 | 0xffffffc01015ed90 |
| cpu_stopper_thread | 0xffffffc01030adc8 | 0xffffffc0101d6830 |
| shmem_init_fs_context | 0xffffffc0104349fc | 0xffffffc0102953f0 |
| memmap_init_zone | 0xffffffc01169d1e0 | 0xffffffc010e7c7b4 |
| preempt_schedule | 0xffffffc01169e87c | 0xffffffc010e7ebf4 |
| memory_dev_init | 0xffffffc01218ec8c | 0xffffffc011793f1c |

**尺寸差异主因**（预期内，非缺陷）：
1. 编译层差异：stock = clang 12 + **LTO + 全内核 KCFI**（3102 个 4B thunk 蹦床 + 调用点范围检查 + SHADOW_CALL_STACK + PAC 113 处 xpaclri）；custom = gcc 13 无 LTO/CFI/SCS/PAC。
2. stock 含 **KFENCE 回移实现**（~0x4bfa2c-0x4c1bd4 + 2MB 池预留 + 分配路径 6 处挂钩），custom 无。
3. 少量 SPRD 驱动/调试符号差异（config 丢弃项）。

---

## 6. 与原厂的差异评估 + 建议

**已对齐（行为等价或已复刻）**：
- pfn_valid NOMAP 拒绝（D1/D2）——本构建在 5.4 层复刻，比 6.12 树在 `__pageblock_pfn_to_page` 打补丁更贴近 stock 原语义（stock 本身就在 pfn_valid 层拒绝）。
- scheduler_ipi 三职责合一（B1）、stopper 队空让出（B2）、shmem mode 0777（F1）、mem_sysfs_mutex（E4，5.4 原生即有）、sugov 25% margin、IPI 死循环门（降级为计数）。
- config 侧：NR_CPUS=32/NUMA=n/SCHED_SMT=n/DEVTMPFS=n/HZ=250/NO_HZ_IDLE=y/errata 对齐/KALLSYMS_ALL=y 等全部复刻。

**已知偏差（建议后续处理）**：
1. **KFENCE 未回移**：stock 实际开启 KFENCE（sample=100），本构建没有。若需行为完全一致，可从 5.12+ 回移 KFENCE（工程量 ~2-3 天）或改用 6.12 树的原生 KFENCE 观测。对"复刻原厂行为"影响：stock 每次分配多一次 kfence 门控读 + 周期守卫页分配，偶发 KFENCE 报告。
2. **KCFI/LTO/SCS/PAC 无法用 gcc13 复刻**：属编译加固层，不改变 MM/调度算法；若要逐字节接近需 clang 12 工具链（环境无）。
3. **C1 死循环门为降级计数**：vendor 的 hang-detect 全局（ipi_cpu/thread_state）无源码依据，未复刻自旋本身；如需"检测到 SGI 丢失即挂起"的调试能力，建议在 6.12 树做 SGI 计数层（sent vs received delta）而非复刻 while(1)。
4. **cpu_isolated 豁免用 housekeeping_cpu(HK_FLAG_MISC) 近似**：默认配置下恒 true，与 stock 默认行为一致；若有 isolcpus 场景需复核。
5. **sugov margin 施加位置为推断**（raw freq 后 clamp）；stock 具体施加点需反汇编 0xffffffc01023a5e0 调用链进一步确认。
6. olddefconfig 丢弃的 SPRD Kconfig 符号（SPRD_CORE_CTL 等）对应源码不在上游 5.4.210 内，本构建无法编译它们——若需这些特性需 vendor 源码。

**产物可用性**：Image 可直接用于 `tools/sprd_mkbootimg.py` 打包（与 6.12 流程相同）。建议下一步：用本镜像 + 原厂 boot 流程做启动对比（观察 no-map 拒绝、IPI 通道、softirq 时序是否与原厂一致），或在 6.12 树对照本报告的 B1 实现补齐 kick_ilb 通道。
