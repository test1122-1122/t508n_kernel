# T508N 紫光（Unisoc/SPRD）魔改部位与内核设置汇总

> 生成：2026-08-18。来源：6 路反编译（DISASM-A~F）+ 14 份历史分析报告 + 实机验证。
> 原厂内核 = stock_Image_full.bin（Android 13，5.4.210 基，Unisoc fork）。
> 本文档用于：① 理解原厂与 mainline 的差异；② 指导 bring-up 的后续对齐决策。

---

## 1. 紫光魔改部位清单（相对上游 v5.4.210）

### 1.1 调度器（DISASM-B）

| # | 位置（原厂符号/地址） | 魔改内容 | 相对 5.4.210/6.12 |
|---|---|---|---|
| B1 | `scheduler_ipi`（0xffffffc0101fc658） | **扩展为真实函数**：收到 IPI_RESCHEDULE 后就地处理 `rq->wake_list`（调 `sched_ttwu_pending`）+ 自触发 nohz idle balance（`idle_balance=1` + SCHED_SOFTIRQ + 唤醒 ksoftirqd，带 cpu_isolated 豁免） | 上游 5.4.210/6.12 的 scheduler_ipi 只做 fold；**6.12 是 inline 纯 fold** |
| B2 | `cpu_stopper_thread`（0xffffffc01030adc8） | 队空收尾**显式 `preempt_schedule()`**（`__schedule(true)`）再返回 smpboot | 上游 5.4.210/6.12 直接返回，让 smpboot_thread_fn schedule |
| B3 | `sugov_set_freq_margin` | sugov 频率余量 **25%** | 上游无此函数（Unisoc 定制调频策略） |
| B4 | LSE 原子 | **经典 ALTERNATIVE memcpy 替换**（LL/SC ↔ LSE 双指令单 asm） | mainline 6.12 用 asm-goto `alt_cb_patch_nops`（b→NOP 落入式）——**机制级差异，已修（LSE static_key）** |

### 1.2 中断/GIC（DISASM-C）

| # | 位置 | 魔改内容 | 说明 |
|---|---|---|---|
| C1 | `ipi_handler` case 0（IPI_RESCHEDULE，0xffffffc010116d48） | **Unisoc 死循环门**：`if (ipi_cpu(0x123e91ec)==cpu && state(0x1279e7d8)==1) while(1);`——sprd-ipi-thread/hang-detect 设施（`send_ipi_init` 0x1218b2f8，initcall6） | 若被误置位，某在线核收到 RESCHEDULE IPI 即自旋——**症状与我们的竞态 A/B 同构**；暗示 T770 SGI 投递确实有间歇问题，原厂有检测、mainline 无 |
| C2 | `ipi_handler` case IPI_CPU_STOP（0x116e70） | **极重**：Android vh hook + 自旋锁 + printk + sprd_dump_stack_reg + minidump/sysdump + local_cpu_stop | 6.12 仅 `local_cpu_stop(cpu)`；原厂跑通证明可靠，不建议照搬 |
| C3 | `irq_exit`（0x19d724）softirq 分派 | **定制**：`pending & 0x41` → 立即 `__do_softirq`；其余 → 唤醒 ksoftirqd | 上游无此分支 |
| C4 | `gic_ipi_send_mask` v3（0x9ae9fc） | `dsb st`（比 6.12 的 `dsb ishst` 更强）→ `msr ICC_SGI1R_EL1` → `isb` | 传输机制与 6.12 等价（SGI ID 0-7、ipi_desc + set_smp_ipi_range 方案） |

### 1.3 内存管理（DISASM-D）

| # | 位置 | 魔改内容 | 说明 |
|---|---|---|---|
| D1 | `pfn_valid`（0xffffffc010126b38） | early section 走 **memblock 二分查找 + 拒绝 MEMBLOCK_NOMAP(bit2)** | 上游 5.4.210/6.12 的 pfn_valid 均无 NOMAP 检查——**已修（`__pageblock_pfn_to_page` 加 memblock_is_map_memory）** |
| D2 | `memmap_init_zone` | **逐页 pfn_valid 跳过 no-map 区** | 6.12 `memmap_init_range` 无逐页检查（会初始化 no-map struct page） |
| D3 | `__rmqueue_cma`（0x495458） | 独立函数（上游是内联 fallback），读 zone 字节 @+80 作门控 | — |
| D4 | boot_pageset 初始化 | 带两条显式 `dmb ishst` | boot 单核期，非竞态点 |
| D5 | **KFENCE 回移** | 回移 5.12+ KFENCE 且**实际开启**：`kfence_sample_interval=100`、分配路径 6 处挂钩、fault 路径挂钩、memblock 预留 2MB 池 | mainline 6.12 config `CONFIG_KFENCE` 未开；反证"分配开销/时序"非卡点根因 |

### 1.4 驱动模型/sysfs（DISASM-E）

| # | 位置 | 魔改内容 | 说明 |
|---|---|---|---|
| E1 | **fw_devlink 半成品**：`defer_fw_devlink_count`@0x127a20c0 + `deferred_suppliers` 全局链表@0x123eef08 + `device_link_add_missing_supplier_links`@0xb77690 | vendor 定制设备链接延迟机制 | 5.4.210 与 6.12.103 都没有；memory 块 fwnode=NULL 跳过，of_core_init 平台设备会走 |
| E2 | **全内核 KCFI**（CONFIG_CFI_CLANG=y） | 3102 个 `__typeid__` 4 字节 b 蹦床表；函数指针存蹦床地址 + 调用点范围检查（失败 `brk #0x800`） | 6.12 无 CFI；唯一原厂特有的"指针/指令一致性"暴露面（ESR 判据） |
| E3 | `kobj_type` 布局（56B，default_attrs@+16） | vendor 定制布局 | 上游 5.4 已删该字段 |
| E4 | `mem_sysfs_mutex`（0x123f1070） | memory_dev_init 全程持有 | **6.12 已删**（对齐候选：恢复） |
| E5 | kernfs | 全局 `kernfs_mutex`@0x123b16c0 | 6.12 改 per-root rwsem |

### 1.5 文件系统（DISASM-F）

| # | 位置 | 魔改内容 | 说明 |
|---|---|---|---|
| F1 | `shmem_init_fs_context` | 默认 mode=**0777**（0x3ff） | 上游 5.4.210/6.12 均为 0777|S_ISVTX（0x5ff）——tmpfs 根无 sticky，不影响竞态 |
| F2 | fs/VFS 主体 | **无 T770/Unisoc 特判**（init_mount_tree/do_mount/fs_context/ramfs/shmem/namei/dcache 与 5.4.210 逐行一致） | fs 原语差异排除 |

### 1.6 启动/CPU（DISASM-A + 历史）

| # | 位置 | 魔改内容 | 说明 |
|---|---|---|---|
| A1 | KPTI | **cpu_enable 回调（idmap remap，不用 stop_machine）且在 alternatives 之前** | 6.12 KPTI 显式 stop_machine 且在 alternatives 之后（**smp_cpus_done 窗口 stop_machine 2 次 vs 原厂 1 次**） |
| A2 | `psci_cpu_die` | 传 **0x10000**（上游传 0） | 只影响 hotplug/深睡 |
| A3 | `cpu_disable` | 返回 **tos_resident_on**（上游恒 0） | 同上 |
| A4 | psci suspend | 丢上下文掩码按 **OSI 位**选择 | 同上 |
| A5 | `__cpu_up` | 旧式错误状态 switch（case1 用 cpu_can_disable）；secondary 上线 5s 超时兜底 | 竞态不符合"核未上线" |
| A6 | **PAuth** | **113 处 `xpaclri`（PAuth alternative 站点，5.4.210 上游无此代码 = Unisoc fork）** | 配合 KCFI/SHADOW |
| A7 | thread_info 布局 | preempt_count@+24、折叠标志@+28、state@+40（Unisoc 定制） | 语义等价 |
| A8 | 启动时序 | `buffer_init`/`mem_init_print_info` 在 **start_kernel 单核 pre-SMP** | 6.12 搬进 post-SMP 的 page_alloc_init_late（8 核并发首触 slab/cpuhp） |
| A9 | driver_init 顺序 | 原厂：devices→buses→classes→firmware→hypervisor→of_core→platform→cpu→memory→container（**无 bdi/devtmpfs/auxiliary/node**） | 6.12 新增 bdi/devtmpfs/auxiliary、node 移入、memory 排 cpu 前 |

---

## 2. 原厂内核设置（config 关键项 vs mainline 6.12）

| 配置项 | 原厂 stock | mainline 6.12（原始） | 我们的当前值 | 备注 |
|---|---|---|---|---|
| CONFIG_DEVTMPFS | **n**（0 个 devtmpfs 符号） | y | **n（已对齐）** | ★竞态根因之一（devtmpfs kthread 跨簇交互） |
| CONFIG_NR_CPUS | **32** | 512 | **32（已对齐）** | 512 时 per-cpu/RCU 树按 512 构建 |
| CONFIG_NUMA | **n** | y | **n（已对齐）** | 无 NUMA 硬件却开 → 调度/内存语义错误 |
| CONFIG_SCHED_SMT | **n** | y | **n（已对齐）** | 无 SMT 硬件却开 → SMT 调度域错误 |
| CONFIG_NO_HZ_IDLE | y | y | **n（实验）** | 原厂开着（kick_ilb 走 RESCHEDULE）；我们关掉消除 CSD |
| CONFIG_HZ | 250 | 250 | 250 | 一致 |
| CONFIG_CFI_CLANG | **y**（+SHADOW 旧式） | n | n | 全内核 KCFI（蹦床表） |
| CONFIG_KFENCE | **y**（回移，sample=100） | n | n | 内存分配保护 |
| CONFIG_LTO_CLANG | **y** | n | n | 编译层 |
| CONFIG_ARM64_ERRATUM_1286807 | y | y | y | A76 repeat-TLBI（r0-r3） |
| CONFIG_ARM64_ERRATUM_1165522 | y | y | y | A76 speculative-AT |
| CONFIG_ARM64_ERRATUM_4118414 | **n** | y | **n（已对齐）** | A76 全版本 repeat-TLBI |
| CONFIG_ARM64_ERRATUM_1530923 | **n** | y | **n（已对齐）** | A55 speculative-AT |
| CONFIG_ARM64_ERRATUM_2441007 | n | n | n | A55 repeat-TLBI |
| CONFIG_CPU_IDLE / ARM_CPUIDLE | y / y | y / (无 ARM_CPUIDLE?) | y（idle-states 已删） | 原厂有 LIT/BIG_CORE_PD + cluster-pd |
| CONFIG_KVM | ? | y | **n（已关）** | nvhe 无法引用 kernel static_key |

---

## 3. 我们已做的对齐/修复改动（当前树）

| # | 改动 | 修复的问题 | 状态 |
|---|---|---|---|
| 1 | LSE 改 static_key/jump_label（原厂式，弃 asm-goto NOP-落入） | SM#2 stop_machine 卡点 | ✅ 有效 |
| 2 | `__pageblock_pfn_to_page` 加 memblock_is_map_memory（NOMAP） | set_zone_contiguous 过洞卡 | ✅ 有效 |
| 3 | of_core_init 循环内 phandle_cache 写注释（5.4 式循环外预填充） | 第 64 节点附近卡 | ✅ 有效 |
| 4 | CONFIG_DEVTMPFS=n | **memory_dev_init 间歇竞态（最后的竞态根因）** | ✅ 有效 |
| 5 | CONFIG_NUMA=n + CONFIG_SCHED_SMT=n | 8 核竞态（调度/内存语义） | ✅ 有效 |
| 6 | ttwu_queue_cond 恒 false（P1，全直接入队+RESCHEDULE） | CSD+IPI_CALL_FUNC 跨簇唤醒间歇失效 | 部分有效（配合 4 才消失） |
| 7 | CONFIG_NO_HZ_IDLE=n、NR_CPUS=32、KVM 关、errata 4118414/1530923 关、idle-states 删 | 对齐/诊断 | 保留（诊断/对齐） |

---

## 4. 未验证/待决策项

1. **CONFIG_NO_HZ_IDLE**：原厂开着（y）且 kick_ilb 走 RESCHEDULE——我们关掉是诊断；完整对齐可恢复 NOHZ + kick_ilb 改 smp_send_reschedule（fair.c 约 5 行）
2. **恢复 mem_sysfs_mutex**（E 建议，无害对齐）
3. **memory_dev_init 挪回 cpu_dev_init 之后**（原厂顺序，消除结构差异）
4. **Unisoc 死循环门（C1）**：mainline 无此检测——如需可移植（诊断 T770 SGI 丢失）
5. **KFENCE 回移**：原厂开启——如需内存保护可移植（CONFIG_KFENCE）
6. **KCFI/PAuth**：原厂开启（编译层）——移植成本高，暂不启用
7. **cpufreq**：原厂 sprd,cpufreq-v2 驱动缺失——A76 无 DVFS（短期可接受）
8. **fw_devlink 半成品（E1）**：原厂独有——memory 块 fwnode=NULL 跳过，暂不移植

---

## 5. 关键地址速查（原厂 stock_kallsyms.map）

```
scheduler_ipi        0xffffffc0101fc658    (扩展: fold+wake_list+nohz kick)
cpu_stopper_thread   0xffffffc01030adc8    (队空显式 preempt_schedule)
ipi_handler          0xffffffc010116cac    (case0 死循环门 0x116d48)
pfn_valid            0xffffffc010126b38    (memblock 二分 + NOMAP 拒绝)
__rmqueue_cma        0xffffffc010495458
device_add           0xffffffc010b75d04
memory_dev_init      0xffffffc01218ec8c
init_memory_block    0xffffffc010bc1610
of_core_init         0xffffffc0121a00a8
__of_attach_node_sysfs (drivers/of/kobj.c:115, 5.4/6.12 逐行相同)
mem_sysfs_mutex      0xffffffc0123f1070
kernfs_mutex         0xffffffc0123b16c0
deferred_suppliers   0xffffffc0123eef08
defer_fw_devlink_count 0xffffffc0127a20c0
device_link_add_missing_supplier_links 0xffffffc010b77690
gic_ipi_send_mask v3 0xffffffc0109ae9fc
irq_exit             0xffffffc01019d724
ipi_cpu (死循环门数据) 0xffffffc0123e91ec
ipi state (死循环门数据) 0xffffffc01279e7d8
KCFI 蹦床表区        0xffffffc0116c5000-0xffffffc0116d9000
```

---

## 6. 对 bring-up 的总体结论

1. **原厂 = 5.4.210 上游语义 + 编译层（LTO/KCFI/PAuth）+ 少量 Unisoc 定制**——无 T770 特判的"一致性/同步加固"机制
2. **跨核通知通道是 6.12 独有差异**（CSD+IPI_CALL_FUNC vs 原厂 RESCHEDULE）——P1 已对齐
3. **driver_init 新增成员（devtmpfs 等）是 6.12 独有执行**——DEVTMPFS=n 已对齐（竞态消除）
4. **T770 的 SGI 投递可能有间歇问题**（原厂死循环门暗示）——mainline 无检测，但 RESCHEDULE 通道实测可靠
5. 剩余对齐候选：NOHZ+kick_ilb、mem_sysfs_mutex、driver_init 顺序、KFENCE
