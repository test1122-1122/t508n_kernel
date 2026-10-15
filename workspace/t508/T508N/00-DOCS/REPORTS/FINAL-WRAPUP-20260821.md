# T508N（TCL OT5 / Unisoc T770 / UMS9620）mainline 6.12.103 bring-up 最终总结

> 生成：2026-08-21（新会话收尾）。状态：**用户决定放弃 6.12 适配**；本文档是全部资料/成果/
> 结论的最终汇总，替代 FINAL-BRINGUP-INDEX.md 作为唯一入口（索引仍保留，含更细文档表）。
> 本文档**只总结，不新增分析**。SML 反汇编子会话结论已回填 §7.4（详见 §7.4 引用的报告）。

---

## 1. 项目一句话 + 最终结论

- **设备**：TCL T508N（板级 OT5，5G 老人机，FIH 代工），SoC **Unisoc T770 / UMS9620**
  （QogirN6Pro）：4×Cortex-A55 + 3×A76 + 1×A76（CPU7 独立簇，4+3+1），3GB LPDDR4x，
  32GB eMMC，Android 13（TP1A.220624.014），原厂内核 5.4.210（clang12 + LTO + KCFI + PAuth + SCS）。
- **目标（原）**：把 mainline 6.12.103 内核跑通到 initramfs `/init`（boot 到用户空间）。
- **最终状态（2026-08-21）**：
  1. **6.12 适配放弃**——卡点最终精确定位到"第一个二级核（CPU1）的 `psci_cpu_on` SMC 不返回"
     （探针证据，见 §6），多次修复/单变量实验（LSE static_key、P1、WDT、NOMAP、config 对齐等）
     均未解除，根因未定（最可能指向 EL3 固件 SML 的 CPU_ON 路径，见 §7）。
  2. **原厂式 5.4 路径已证明可行**——`work/linux-5.4-custom`（v5.4.210 + 紫光魔改复刻 +
     原厂 config）**成功 boot 到 initramfs 并 exec /init**（8 核上线，stage 0–9 + y=400 + y=460 全亮）。
     这是"T770 硬件 + 启动链 + 原厂式代码"可行的**阳性对照**，也是未来若需交付可用内核的
     **现成基线**。
  3. 调试全程无 UART/无 USB，靠 **LCD framebuffer 探针（320×480 BGRA @0xb0000000）+ fastboot +
     A/B 双槽安全网 + uboot_log/pstore 无串口通道** 闭环。

---

## 2. 全量成果清单（按类别）

### 2.1 反汇编分析（DISASM，A–N 共 14 路，均在 `work/`）

| 路 | 主题 | 关键结论 |
|---|---|---|
| A | CPU 启动 | KPTI=回调不用 stop_machine 且在 alternatives 前；psci/secondary handshake 标准无特判；buffer_init pre-SMP；PAuth 113 处 |
| B | 调度器 | **scheduler_ipi 是真实函数**（fold+wake_list+nohz kick+ksoftirqd）；stopper 队空 preempt_schedule；sugov 25% |
| C | 中断/GIC/Timer | **Unisoc 死循环门**（SGI 间歇问题暗示）；IPI_CPU_STOP 极重；irq_exit softirq 定制；gic_ipi_send_mask dsb st |
| D | 内存管理 | **pfn_valid 拒绝 NOMAP**；memmap_init 逐页跳 no-map；__rmqueue_cma 独立；KFENCE 回移开启 |
| E | 驱动模型/sysfs | mem_sysfs_mutex 全程持有；kernfs 全局 mutex；driver_init 8 成员链（无 devtmpfs/bdi/auxiliary/node） |
| F | 文件系统/VFS | shmem mode=0777；fs 主体与 5.4.210 逐行一致 |
| G | 时钟 | **原厂时钟驱动是外置模块**（clk-sprd.ko + ums9620-clk.ko），镜像内无 |
| H | PMIC/电源 | PMIC 驱动同样外置 .ko（mfd/regulator/ADI 桥/regmap-irqchip 已拆） |
| I | UFS | SCSI_UFS_SPRD_QOGIRN6PRO 为独立模块，镜像内无 vendor host 驱动 |
| J | 定时器/WDT | timer-sprd 与上游一致；**WDT 寄存器布局：LOCK@0x20 键 0xe551、LOAD 17bit HI@0x4/LO@0x0**（据此修正了内核喂狗代码） |
| K | SLUB | **★修正：原厂 SLUB 全链路用 LSE CASP（9 站点）**——"casp 首大规模使用"假设不成立 |
| L | RCU | 77 个函数；原厂配置 RCU_NOCB/FAST_NO_HZ/NO_HZ_IDLE 已列 |
| M | workqueue | cmwq 与上游一致；**跨簇唤醒通道差异 100% 在调度器**（5.4 wake_list+IPI_RESCHEDULE vs 6.12 CSD+IPI_CALL_FUNC）——P1 依据 |
| N | cpuidle/cpufreq | sprd cpufreq-v2 外置模块；DTS 缺 cpufreq 域/idle-states 挂接（mainline 无 sprd cpufreq 驱动） |

### 2.2 深度分析报告（REPORT，work/ 约 20 份）

`REPORT-T508N-stock-analysis`（A/B/C/D 四节）、`REPORT-stock-sync-mechanism`（stop_machine/
alternatives 协议）、`REPORT-stock-vs-mainline-locks`、`REPORT-stock-vs-mainline-completion`、
`REPORT-mainline-completion-wakeup-analysis`（arm64 无 TIF_POLLING_NRFLAG ⇒ 跨核唤醒 100% 依赖 SGI）、
`REPORT-LSE-atomic-completion-analysis`（nr_todo 必归零，H4 排除）、`REPORT-stock-wakeup-chain-analysis`、
`REPORT-stock-alternatives-waiter-analysis`（NOP-落入式对"半新半旧"敏感）、`REPORT-alt-table-integrity`
（"828 异常"是分析误差）、`REPORT-stock-crosscluster-visibility`、`REPORT-stock-page-alloc-init`、
`REPORT-stock-zone-memory`（NOMAP 过洞根因）、`REPORT-stock-driver-init`、`REPORT-stock-of-core-init`、
`REPORT-stock-memory-dev-init`、`REPORT-stock-wakeup-nohz`（竞态 A 唯一失败点）、
`REPORT-CURRENT-RACE-ANALYSIS`（竞态 A/B 根因排序）、`REPORT-A76-requirements`（stock DTS 三簇对比）、
`REPORT-UNISOC-ONLY-CONFIGS`（C 层 312 项紫光独有）、`REPORT-UNISOC-MODS-ON-612`（魔改移植干净树编译通过）、
`REPORT-54-custom-build`（5.4 复刻 0 error）、`REPORT-54-probe-port`（5.4 探针移植）、
`SUMMARY-ALL-ANALYSIS`（汇总，16 项假设排除）、`UNISOC-DETAILED` / `UNISOC-MODS-AND-CONFIG`（权威速查）。

### 2.3 补丁文档（work/PATCH-*.md，未应用/待验证）

- `PATCH-nohz-kick.md`：kick_ilb→smp_send_reschedule（SGI#0）+ scheduler_ipi 消费 nohz kick。
- `PATCH-mem-sysfs-order.md`：恢复 mem_sysfs_mutex + memory_dev_init 挪位（结构对齐，风险 0）。
- `PATCH-diag-sgi-kfence.md`：SGI 发送/接收计数诊断层 + KFENCE（sample=100）诊断方案。

### 2.4 索引/交接文档

- `FINAL-BRINGUP-INDEX.md`（§3 修复清单 14 项 / §6 文档表 / §7 结论 / §10 未来方向）——详细索引。
- `HANDOFF-3-SESSIONS.md`、`SESSION-CONTEXT-COMPACT.md`（turn 14 继续点，2026-08-16）。
- `AGENTS-GUIDE.md`、`无串口验证指南.md`、`SKILL-T508N-framebuffer-debug.md`、`KERNEL-适配方案.md`、
  `WORK-适配材料清单.md`、`REPORT-UMS9620-mainline适配.md`、`REPORT-T508N-mainline-bringup.md`。
- 本会话新增：`work/SESSION-SPLIT-AND-WORKFLOW.md`（会话拆分扫描 + 工作流）、
  `work/FLASH-20260821-P1.md`（实测记录 §1–§10）。

### 2.5 镜像/内核产物

| 产物 | 说明 |
|---|---|
| `boot_54custom_probe.img` / `boot_54custom.img` / `boot_54custom_builtin.img` | **5.4 复刻（probe 版已实测 boot 到 /init）** |
| `work/stock54-custom_Image`（31,621,632 B） | 5.4 复刻内核（gcc13，0 error） |
| `boot_linux.img`（6.12 最后版本，md5 `5de51f45…`，含 y=175/180/185/190/195 探针 + P1 + WDT 修正） | 6.12 魔改树最后构建 |
| `work/boot_linux_P1-backup.img` | P1 轮镜像备份 |
| `work/mainline_Image`（md5 `5de51f45…`） | 6.12 内核副本 |
| `vendor_boot_linux.img`（Aug 16，**旧 DTB**：单簇 4+4 + cpu-idle-states） | 实际刷入的 vendor_boot |
| `work/vendor_boot_linux_new.img` / `work/ot5-new.dtb`（新 DTB：三簇 4+3+1、无 idle-states、capacity/DPC，**未刷**） | 备用 |
| `work/ot5.dtb` | 旧 DTB 本体 |
| `initramfs.cpio` / `initramfs_root/` | busybox initramfs（挂 proc/sysfs/devtmpfs） |
| `work/noop_dtbo.img` | 空 overlay DTBO（防 merge 破坏，**必刷 dtbo_a**） |

### 2.6 源码/工具/材料

- 源码树：`linux-6.12.103`（魔改树，HEAD `cb8e725f3` + **15 个未提交文件**：13 原改 + 探针 y=190/195）、
  `work/linux-6.12-clean`（干净树 + 魔改移植验证）、`work/linux-5.4-custom`（4 commits：pristine→
  olddefconfig→魔改→探针）、`linux-5.4.210`（上游源码 + tar.xz）、`work/stock-5.4.210`。
- 原始材料：`work/stock_Image_full.bin`（38MB）、`stock_kallsyms.map`（**116,704 符号，唯一正确符号表**）、
  `stock_ums9620.dts`（7731 行）、`kernel.config`（原厂 IKCONFIG）、`dtbo0-6.dtb`、`vendor_boot.dtb`、
  `Image/`（78 分区备份）、`payloads/`（DHTB 剥离固件：sml/spl/trustos/teecfg/uboot）、`alt_table_records.csv`。
- 工具：`tools/`（sprd_mkbootimg / patch_vendor_boot / avbtool / lpunpack / lz4dec / cpio_extract /
  build_*.sh / winfastboot / winadb / dts）；工具链 arm-gnu-toolchain-13.2 + musl 交叉；openssl-local。
- 差异补丁：`work/diff-612-{arch,include,memory,mm,sched,unisoc-mods}.patch`（干净树 vs 魔改树）。

---

## 3. 里程碑时间线

| 阶段 | 时间 | 关键节点 |
|---|---|---|
| 0. 启动链逆向 | 08-15 | 识别 T770/UMS9620/OT5；BootROM→SPL→LK 链；DHTB/紫光 LZ4/vendor_boot-DTB/大端 dtbo/紫光 LP 变体/uboot_log 金矿；78 分区镜像解包 |
| 1. mainline 起步 | 08-15~16 | 工具链 + busybox initramfs + `ums9620-ot5.dts`；boot/vendor_boot 打包与 AVB 自签；**DTBO merge 失败→noop_dtbo.img 修复**；b 槽 vendor_boot 误刷→restore_stock.bat 恢复 |
| 2. 8 核上线 | 08-16 00:11 | 二级核 MMU/取指/8 核 idle/smp_init 完成（进度条第 10 格）；EL2 virtual timer（PPI 12）移植 |
| 3. 卡点漂移期 | 08-17 | 卡点间歇漂移：stop_machine / of_core sysfs / memory_dev_init→device_register；"加探针能过、不加就卡"；恢复 stock_kallsyms（116,704）；14+ REPORT + DISASM A–F |
| 4. 修复实施期 | 08-18 | LSE static_key、NOMAP 检查、原厂式 multi_stop、完整 apply_alternatives_all、config 对齐（DEVTMPFS=n / NR_CPUS=32 / NUMA=n / SMT=n / KVM=n / errata）、DTS 三簇；**SM#2 由间歇卡变稳定**；of_core 948 节点通过 |
| 5. 5.4 复刻验证 | 08-18 | `linux-5.4-custom` 编译成功（0 error）；探针移植；**boot 到 initramfs（exec /init y=460 亮）= 原厂式路径在 T770 可行** |
| 6. 6.12 最终卡点 | 08-18 晚 | "9 格 + y=130 不亮"稳定复现；P1 回灌 + WDT 修正 + 每核探针（23:41 构建） |
| 7. 定位与放弃 | 08-21 | 实测：**9 格 + y=175 CPU1 亮、其余全缺 → 卡在第一个二级核 psci_cpu_on(1) SMC 不返回**；第二轮 S1/S2 探针确认（y=190 不亮）；**用户决定放弃 6.12 适配** |

---

## 4. 最终卡点的精确定位（2026-08-21 实测）

**屏幕读数**（像素级 + 用户确认）：stage 0–8 亮（9 格）、stage 9 暗；仅 y=175 CPU1 格亮；
y=180/185（CPU1 未到 secondary_start_kernel）、y=175 CPU2–8（CPU0 未推进）、y=130/210/220/260/300/
340/380/400/420/440/460 全缺；y=190/195（第二轮新增：SMC 返回后/5s 超时后）**均不亮**。

**结论：`__cpu_up(1)` 内 `boot_secondary(1)`（PSCI CPU_ON，SMC→EL3/SML）**永不返回**；
CPU1 从未启动；CPU0 从未尝试第二个核。画面冻结（非慢启动）。

**已排除**（本轮实证）：
- **ATF/DTB/硬件**：同一 SML + 同一 DTB（Aug 16 旧版），5.4-custom 8 核全上 + exec /init → 问题 100% 在 6.12 内核侧。
- **LSE**：当前树 `system_uses_lse_atomics()` 恒 false（D2 实验强制全 LL/SC）仍卡 → LSE/casp 非根因。
- **P1**（ttwu 恒直接入队 + IPI_RESCHEDULE）：读数与 P1 前完全一致，无效。
- **DTB 差异**：非根因（见上）。

---

## 5. 已确认事实（硬结论，勿重复验证）

1. stock 符号映射：`_text=0xffffffc010080000`，`file_off = vaddr − 0xffffffc010080000`；
   `work/System.map` 不是 stock 符号表，`work/stock_kallsyms.map` 才是。
2. stock 二级核等待 alternatives flag：`ldr + yield` 纯轮询（无 dcache/dsb/wfe/sev）；
   CPU0 普通 `str` 置位 → **T770 PoC 一致性本身可用**。
3. stock 内核/DTS/SML **无 CCI/CMN/DSU/SCU 一致性初始化**；device/sysfs 路径无 T770 特判。
4. stock 普通 cache 维护只用 `ic ivau`；`__apply_alternatives` 尾部有条件 `ic ialluis`。
5. arm64 无 TIF_POLLING_NRFLAG + idle 用 WFI ⇒ **跨核唤醒 100% 依赖 SGI**。
6. 5.4 跨簇唤醒 = `rq->wake_list + IPI_RESCHEDULE`；6.12 = `CSD + IPI_CALL_FUNC`；
   workqueue/RCU 层零机制差异，**通道差异 100% 在调度器**（DISASM-M/L）。
7. Unisoc `ipi_handler case0` 有**死循环门**（sprd-ipi-thread/hang-detect）→ 暗示 T770 SGI 投递间歇问题。
8. 原厂 `pfn_valid` 拒绝 NOMAP（6.12 不查 → 过洞卡点，已由 NOMAP 检查修复）。
9. 原厂 SLUB **全链路用 LSE CASP**（9 站点）——"casp 首大规模使用"假设不成立（DISASM-K 修正）。
10. T770 世代驱动（UMS9620 clk/PMIC/UFS/USB）**全部未上游**，原厂均为 `=m` 外置模块。
11. `CONFIG_DEVTMPFS` 是 6.12 独有执行（原厂 n）；`mem_sysfs_mutex` 原厂全程持有（6.12 已删）。
12. 启动链分区使用：BootROM→boot0/1→spl_a→trustos/sml/teecfg→uboot_a(LK)→misc(槽选择)→
    boot_a(内核+init ramdisk)+vendor_boot_a(DTB)+dtbo_a(合并)→AVB 链（解锁态 SKIP VERIFY）；
    init_boot/dtb/avbmeta_rs/ise/hypervsior 分区全零未用；uboot_log 为 LK 日志通道。

---

## 6. 已排除假设（汇总，含 16 项 + 本轮）

原子计数坏（H4）/ 丢唤醒内存序（H3）/ ic ialluis hang / mainline 独有 errata（4118414/1530923）/
alt 表 828 异常写坏 waiter / 唤醒链缺机制 / PoC-cache 硬件不可靠 / CCI-CMN-DSU 未初始化 /
device-sysfs 有 T770 特判 / 锁实现差异 / shuffle-memblock_discard-deferred /
completion waitqueue→swait 是根因（非根因，无条件摘链是放大因素）/ ttwu_queue_cond 同簇修补无效 /
DAIF-GIC（探针正常）/ devices_init 本身 / NOMAP 分到坏页（已修）/ **DTB 差异（本轮）/ LSE+casp（本轮）/
P1 唤醒通道（本轮）**。

---

## 7. 未解决问题与最终线索

### 7.1 未解决根因
6.12 在第一个二级核 `psci_cpu_on(1)` SMC 不返回，根因未定。候选机制（按证据强度）：
1. **SML（EL3）CPU_ON 处理等待二级核启动握手**（每核状态机 flag `tpidr_el3+0x178`：
   0=关/1=启动中/2=运行；二级核必须先进入 SML 启动路径置 1，再经 TrustOS bootstrap 交接回
   非安全内核；若 mainline 的入口/协议导致二级核从未进入该路径 → flag 停 0 → CPU_ON 永久等待
   → CPU0 SMC 永不返回）。—— **大概率是，但 SML 文件不完整无法 100% 确认（见 §7.4）**。
2. **CPU0 在 SMC 时处于异常状态**（如 tick/中断已死、EL 异常）→ SMC 内部 WFI/等待无法完成。
3. 6.12 二级核入口（`secondary_entry` 物理地址/idmap 覆盖）与 SML 期望不符。

### 7.2 历史未决竞态（6.12 独有）
竞态 A（devtmpfs kthread 跨簇唤醒丢失，DEVTMPFS=n 已消除挂点）、竞态 B（memory_dev_init 间歇卡）——
大概率与 7.1 同源（跨核中断/同步在 T770 的间歇性），已随 6.12 放弃而归档。

### 7.3 5.4 复刻树的待办（若未来以 5.4 为交付方向）
- 稳定性多次 boot 验证（目前只实测 1 次成功）。
- 外设驱动：clk/PMIC/UFS 均为原厂 `.ko`（vendor_ramdisk_out/lib/modules/ 有全套未剥离模块；
  注意 vermagic 与 gcc13 构建不匹配，需处理）。
- 若需要，`boot_54custom_probe.img` 可继续作为"原厂式路径可行"的对照基线。

### 7.4 SML 反汇编结论（已回填，子会话 b2f5a26d）

> 完整报告：`work/REPORT-SML-psci-cpu-on.md`（2026-08-21）。对象：`payloads/sml_a.bin`
> （raw，base 0xbd000000，代码 0xbd0001c8–0xbd0224e8，VBAR_EL3=0xbd00e9c8，版本 `v1.4, qogirn6pro`）。

**判定：SML 的 CPU_ON **大概率**等待二级核启动握手（每核状态机 flag），但固件文件不完整，
无法 100% 确认处理器内部轮询循环。**

已确认事实：
1. **mainline 的 SMC64 CPU_ON（0xc4000003）确实进入 SML 的 PSCI 分发**（ESR EC 0x17 →
   0xbd00d4fc 直进分发，跳过 0xbd00d4e4 的 SMC64 拒绝检查）——**排除"SMC64 被 SML 拒绝"**。
2. **SML 存在每核上电握手状态机**：`tpidr_el3 + 0x178`（376）：0=关/1=启动中/2=运行；
   二级核启动代码（0xbd007e84→0xbd007f4c）置 1；0xbd007fa0 读/校验（0→-4、2→-5、1→继续）。
3. **TrustOS（trustos_a.bin）另有一层 bootstrap completion 握手**（`creating bootstrap completion
   thread for cpu %d` / `secondarybootstrap2` / `sm_secondary_init`）→ 二级核上电路径可能为
   **EL3(SML 置 flag=1) → EL1(TrustOS bootstrap) → 非安全内核 secondary_start_kernel**。

无法确认的原因：SMC 分发用两级表（查表表 0xbd026f18 + 函数指针表 0xbd013528），CPU_ON index=0x44；
**查表表 0xbd026f18 超出 137KB 文件**（SML 区 256KB，`sml_a.bin` 只捕获代码段）→ 无法定位
CPU_ON 处理器并确认其是否 `ldr flag; cmp; b.eq` 轮询。

若判定为"是"（最符合所有观察的机制）：
- 握手条件：每核结构体 `tpidr_el3` + 0x178 状态字段，期望 0→1。
- **mainline 需满足**：二级核上电后必须先进入 SML 启动路径（置 1）再经 TrustOS 交接回内核；
  若 mainline 的 `secondary_entry`/启动协议与原厂不同（如入口地址、跳转链），二级核从未进入
  该路径 → flag 停 0 → CPU_ON 永久自旋 → **CPU0 的 SMC 永不返回**（与实测完全一致）。

遗留验证（若未来重启）：① 取完整 256KB SML 镜像补齐 0xbd0224f0–0xbd040000 解析查表表；
② 对比原厂 5.4 与 mainline 6.12 的 `secondary_start_kernel` 入口地址/启动协议。

---

## 8. 未来方向（若重启，按证据强度排序）

1. **先读 §7.4 SML 结论**：若确认 SML CPU_ON 等待握手 → 唯一出路是让内核满足该握手
   （改 6.12 的 `secondary_entry` 或干脆回到 5.4 路径）；若 SML 是标准实现 → 转 2。
2. **SGI/中断投递诊断**（PATCH-diag-sgi-kfence P1–P3）：发送/接收计数按核对照 + 每核心跳。
3. **5.4-custom 作为交付基线**（已验证能 boot）：继续完善/稳定；6.12 列为长期目标。
4. KFENCE / SOFTLOCKUP=y / slub_debug=U / maxcpus 隔离（诊断手段，均为未应用补丁）。
5. 参考 `FINAL-BRINGUP-INDEX.md` §10（12 项候选）与 `work/PATCH-*.md`。

---

## 9. 红线与教训（沉淀）

- 一次只改一个变量；"加探针能过、不加就卡"= 强时序敏感，探针必须作为观测而非修复。
- **配置/代码改动要验证"真的上了设备"**：本次教训——Aug 18 的 DTS 三簇对齐从未进 DTB
  （`vendor_boot_linux.img` 是 Aug 16 旧版，刷机流程未包含 DTB 重建）。
- 不碰外设（USB/eMMC/pinctrl/PMIC/DRM），本阶段范围收敛是纪律。
- 不加 CCI/CMN/DSU；不恢复无条件 `ic ialluis`。
- 6.12 放弃后，`linux-6.12.103` 的 15 个未提交文件保留原样（追溯用）；如需清理，
  先 `git diff > work/final-612-diff.patch` 归档再决定。

---

## 10. 外部参考资源（2026-08-21 新增，已克隆到 work/）

| 仓库 | 克隆位置 | 内容与相关性 |
|---|---|---|
| [ZH-XiJun/android_device_fih_OT5](https://github.com/ZH-XiJun/android_device_fih_OT5) | `work/fih_OT5_device_tree/`（56MB） | LineageOS 设备树：`TARGET_KERNEL_SOURCE := kernel/fih/OT5`、`TARGET_KERNEL_CONFIG := OT5_defconfig`（**vendor 内核树/平台私有，未公开**）；`cmdline console=ttyS1,115200n8`；prebuilts/ 含**原厂内核 Image（38,308,352 B）+ dtb.img（160,163 B）**；manifest.xml 为 sprd HIDL/AIDL 清单；extract-files.sh + proprietary-files.txt 为 vendor blob 清单 |
| [Xiaopan2009/twrp_device_fih_OT5](https://github.com/Xiaopan2009/twrp_device_fih_OT5) | `work/twrp_OT5_device_tree/`（55MB） | TWRP-12.1 设备树（基于上面 LOS 树生成）：确认 A/B（Virtual）+ 动态分区 + boot v4（boot 64MB / vendor_boot 100MB / super 8.5GB）；recovery.fstab 分区表；**可通过 GitHub Actions 构建 TWRP recovery.img**（外部成果，可用于未来刷机/取证辅助） |

**注意（与我们材料的关系）**：
- 仓库内核 md5 `d9626bc9…` ≠ 我们解包 `work/stock_Image_full.bin`（`295663b6…`）——版本字符串显示
  FIH 版是 **`5.4.210-android12-9-ga1d5c173d86b … #2 SMP PREEMPT Wed Mar 13 10:04:50 CST 2024`**；
  我们的是 **`…ga1d5c173d86b-ab1 … #1 SMP PREEMPT Tue Jan 16 17:43:10 CST 2024`**（与 LK buildid
  2024-01-16 吻合）。**同一 vendor 基线的不同构建**——`stock_kallsyms.map`/全部反汇编地址
  只适用于我们 1 月 16 日版本，不要误用 FIH 版核对符号。
- DTB 同尺寸（160,163 B）不同内容，同理。
- 未发现 vendor 内核源码仓库公开（`kernel/fih/OT5` 为平台私有）；若要完整原厂源码，
  唯一来源是第三方放出的 vendor 源码树（如有）。`OT5_defconfig` 名称可用于检索。

---

## 11. ★★ 重要资源：UMS9620 原厂内核源码（2026-08-21 新增）

| 项 | 值 |
|---|---|
| 仓库 | [enceka/android_kernel_zte_ums9620_mifi_f50](https://github.com/enceka/android_kernel_zte_ums9620_mifi_f50)（ZTE 开源站下载，197.7MB，68,619 文件） |
| 克隆位置 | `work/vendor-kernel-ums9620/` |
| 版本 | **VERSION=5.4.210**（与 T508N 原厂内核 `5.4.210-android12-9` 同版本）——Unisoc/Android common 厂商 fork 完整源码 |
| 设备 | ZTE F50（mifi 路由器）→ **平台代码 100% 通用**（UMS9620/T770），板级/config 与 T508N 不同 |

**已验证命中（我们逆向成果的"真身"）**：
- `kernel/sched/core.c:2686` `scheduler_ipi()` = fold + `sched_ttwu_pending()`(wake_list) + nohz kick + ksoftirqd → **= DISASM-B 结论**
- `arch/arm64/kernel/smp.c` `do_handle_IPI` case IPI_RESCHEDULE：`#ifdef CONFIG_SPRD_HANG_TRIGGER / ipi_is_triggered(cpu) → while(1)` → **= DISASM-C 死循环门**
- `arch/arm64/mm/mmu.c:473-508` `memblock_mark_nomap/clear_nomap` 序列 → **= NOMAP 相关实现**
- `drivers/clk/sprd/ums9620-clk.c` → **= DISASM-G 的 ums9620-clk.ko 源码**
- `drivers/mfd/sprd-ump96xx-spi.c` → **= DISASM-H PMIC**
- `drivers/scsi/ufs/ufs-sprd-*.c` → **= DISASM-I UFS**
- `arch/arm64/boot/dts/sprd/ums9620.dtsi` + `ums9620*.dts` + overlay 系列（staple/harrow/tp 板级）→ **原厂 UMS9620 DTS 源码**

**价值**（按优先级）：
1. **验证全部逆向成果**（scheduler_ipi/死循环门/NOMAP/mem_sysfs_mutex/shmem 0777/sugov 25%/LSE 经典替换/irq_exit 定制等——15 处魔改可直接对源码逐条核对）。
2. **5.4-custom 升级为"真厂"**：用真实 vendor 魔改替换手工复刻（D1/B1/B2/C1/E4/F1），保真度飞跃。
3. **驱动适配的生产资料**：T770 世代"未上游驱动"（clk/PMIC/UFS/USB 等）**全部有源码**——未来移植到 5.4-custom/任何内核不再需要逆向。
4. **可复核 6.12 卡点**：`arch/arm64/kernel/psci.c:39 cpu_psci_cpu_boot` / `smp.c:118 __cpu_up` / `smp.c:209 secondary_start_kernel` 源码在手——可对照 mainline 验证二级核入口/启动协议一致性（若重启 6.12）。

**注意**：① ZTE F50 是不同设备——T508N 板级 DTS/config 仍需以我们反编译的
`work/stock_ums9620.dts` + `work/kernel.config` 为准；② 仓库无 git 历史（源码快照），
base commit 与 T508N 的 `a1d5c173d86b` 是否完全同点未知，但同版本 fork 平台代码重叠度 ≥95%。

---

## 12. UMS9620 第三方参考实现（2026-08-21 新增）

| 仓库 | 克隆位置 | 说明 |
|---|---|---|
| [liuwang97/android_kernel_zte_ums9620_mifi_f50](https://github.com/liuwang97/android_kernel_zte_ums9620_mifi_f50) | `work/kernel-ums9620-tuned/` | ZTE U30 Air 自定义内核：**5.4.254 基 + 14 commits**（KernelSU/MGLRU/DAMON/zstd/IRQ 均衡器/module_overlay/USB tether 修复），"boots on 100% stock vendor modules"（2026-07 提交） |
| Enceka 原厂树 | `work/vendor-kernel-ums9620/` | 5.4.210 基线（ZTE 开源站下载）——两者**基版本不同（210 vs 254）**，diff 之前以 API 补丁（`/tmp/u30air_compare.json` 已存）为准确功能差异 |

**关键发现（226 文件补丁分析）**：
1. **没有任何 SMP/PSCI/二级核启动/同步路径的修复** —— "调通"= 仅用户态友好功能 + 两个硬件修复。再次独立证实：**5.4 系 UMS9620 平台启动路径无需 workaround；6.12 卡死是 6.12 独有**。
2. **USB RNDIS 撕裂修复（3d38d9eb）是唯一"平台级"修复**，描述的症状与我们项目高度同构：
   - 症状："silent boot watchdog reset"、"state that only a reboot could clear"、"RNDIS control path wedged until reboot"；
   - 根因：PAM/SIPA 织物 refcount 泄漏（SPRD RNDIS gadget 两个数据端点都标 0xff → 2 次 usb_phy_init，故障代码用 bool 锁存只做 1 次 suspend → refcount 永不清零 → 下一次枚举即 wedge）+ teardown 不等 disconnect 中断处理完（盲睡 20ms）→ 半释放状态；
   - 修复：`dwc->connected` 轮询（500ms 预算）替代盲睡；会话开始/结束强制 `usb_phy_shutdown`；`pamu3_release_quiesce` 轮询 CTL0.DONE（5ms/udelay）；`pamu3_full_stop` 全清；gadget.c 按端点对计数匹配 suspend。
   - **启发**：T770 平台的**固件侧硬件路径对初始化/撕裂竞态高度敏感**，厂商用"显式 refcount + 轮询确认"处理——与我们"原厂式 yield 轮询"结论同哲学。
3. **module_overlay 技术**（a273dc4e）：内核加载时用内嵌 .ko 替换 vendor_boot 里的签名模块（不改签名分区）——**对"不能碰签名分区"的约束是绝佳方案**；已用于注入修复版 dwc3-sprd/pamu3。
4. IRQ 均衡器（sbalance.c）= Sultan Alsawaf 通用方案，非平台修复。

**若未来做 USB 交付**：上述 dwc3-sprd/pamu3 全链路修复 + module_overlay 注入 = 现成路线。
