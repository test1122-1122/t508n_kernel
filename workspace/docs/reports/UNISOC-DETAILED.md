# T508N 紫光魔改 / 内核设置 / 编译环境 详细记录

> 更新：2026-08-18。本文档为 bring-up 调试的**权威参考**：已知紫光（Unisoc/SPRD）魔改部位、
> 原厂内核设置、以及**影响内核编译/构建的一切因素**。
> 姊妹文档：`work/UNISOC-MODS-AND-CONFIG.md`（简版）、`work/SUMMARY-ALL-ANALYSIS.md`（分析汇总）、
> `work/DISASM-A~F-*.md`（6 路反编译详报）。

---

## 1. 紫光魔改部位（原厂 5.4.210 fork，相对上游）

### 1.1 调度器

| 符号/地址 | 魔改 | 影响/启示 |
|---|---|---|
| `scheduler_ipi` 0xffffffc0101fc658 | 真实函数：fold + **就地处理 rq->wake_list**（sched_ttwu_pending）+ **自触发 nohz idle balance**（idle_balance=1 + SCHED_SOFTIRQ + 唤醒 ksoftirqd，cpu_isolated 豁免） | 原厂 RESCHEDULE 通道"IPI 即干活"；6.12 只 fold（wake_list 归 CALL_FUNC） |
| `cpu_stopper_thread` 0xffffffc01030adc8 | 队空收尾**显式 preempt_schedule()** | 语义等价（都会让出 CPU） |
| `sugov_set_freq_margin` | sugov 频率余量 25% | 调频策略定制 |
| LSE 原子 | **经典 ALTERNATIVE memcpy**（LL/SC↔LSE 双指令单 asm） | 6.12 用 asm-goto `alt_cb_patch_nops`（b→NOP 落入）——**已修（LSE static_key）** |

### 1.2 中断/GIC

| 符号/地址 | 魔改 | 影响/启示 |
|---|---|---|
| `ipi_handler` case0 0xffffffc010116d48 | **Unisoc 死循环门**：`ipi_cpu(0x123e91ec)==cpu && state(0x1279e7d8)==1 → while(1)`（sprd-ipi-thread/hang-detect，`send_ipi_init` 0x1218b2f8） | **暗示 T770 SGI 投递确有间歇问题**（原厂有检测，mainline 无）；症状与我们的间歇竞态同构 |
| `ipi_handler` IPI_CPU_STOP 0x116e70 | 极重：vh hook + 锁 + printk + dump_stack + minidump/sysdump + local_cpu_stop | 6.12 仅 local_cpu_stop |
| `irq_exit` 0x19d724 | softirq 分派定制：`pending & 0x41` → 立即 __do_softirq；其余 → 唤醒 ksoftirqd | softirq 时序差异 |
| `gic_ipi_send_mask` 0x9ae9fc | `dsb st`（6.12 用 dsb ishst）→ ICC_SGI1R → isb | SGI 传输与 6.12 等价 |

### 1.3 内存管理

| 符号/地址 | 魔改 | 影响/启示 |
|---|---|---|
| `pfn_valid` 0xffffffc010126b38 | early section **memblock 二分 + 拒绝 MEMBLOCK_NOMAP(bit2)** | **已修**（`__pageblock_pfn_to_page` memblock_is_map_memory） |
| `memmap_init_zone` | 逐页 pfn_valid 跳过 no-map | 6.12 会初始化 no-map struct page（语义差） |
| `__rmqueue_cma` 0x495458 | 独立函数（zone 字节 @+80 门控） | — |
| boot_pageset init | 两条显式 dmb ishst | 单核期，非竞态点 |
| **KFENCE 回移** | 5.12+ 特性回移且**开启**（sample=100、2MB 池、fault 挂钩） | mainline 6.12 原生支持（CONFIG_KFENCE=y）——诊断"内存被踩"的现成工具 |

### 1.4 驱动模型/sysfs

| 符号/地址 | 魔改 | 影响/启示 |
|---|---|---|
| **fw_devlink 半成品**：`defer_fw_devlink_count` 0x127a20c0、`deferred_suppliers` 0x123eef08、`device_link_add_missing_supplier_links` 0xb77690 | vendor 设备链接延迟 | 5.4.210/6.12 均无；memory fwnode=NULL 跳过 |
| **全内核 KCFI**（CONFIG_CFI_CLANG=y） | 3102 个 `__typeid__` 4B b 蹦床表 + 调用点范围检查（失败 brk #0x800） | 6.12 无；唯一原厂"指针/指令一致性"暴露面（ESR 判据） |
| `kobj_type` 布局（56B，default_attrs@+16） | vendor 布局 | — |
| `mem_sysfs_mutex` 0x123f1070 | memory_dev_init **全程持有**整个注册循环 | **6.12 已删——已恢复**（对齐补丁） |
| kernfs | 全局 `kernfs_mutex` 0x123b16c0 | 6.12 per-root rwsem |

### 1.5 文件系统

| 符号/地址 | 魔改 | 影响/启示 |
|---|---|---|
| `shmem_init_fs_context` | 默认 mode=**0777**（无 S_ISVTX） | 上游 0x5ff；不影响竞态 |
| fs/VFS 主体 | **无 T770 特判**（与 5.4.210 逐行一致） | fs 原语差异排除 |

### 1.6 启动/CPU

| 项 | 魔改 | 影响/启示 |
|---|---|---|
| KPTI | **cpu_enable 回调（idmap remap，不用 stop_machine）且 alternatives 之前** | 6.12：KPTI 显式 stop_machine 且 alternatives 之后（**smp_cpus_done 窗口 stop_machine 2 次 vs 原厂 1 次**） |
| `psci_cpu_die` | 传 0x10000（上游 0） | hotplug/深睡 |
| `cpu_disable` | 返回 tos_resident_on | 同上 |
| psci suspend | 丢上下文掩码按 OSI 位 | 同上 |
| PAuth | **113 处 `xpaclri`（alternative 站点，上游无 = Unisoc fork）** | 配合 KCFI/SHADOW |
| thread_info 布局 | preempt_count@+24、折叠@+28、state@+40 | 语义等价 |
| `buffer_init`/`mem_init_print_info` | **start_kernel 单核 pre-SMP** | 6.12 搬进 post-SMP page_alloc_init_late（8 核并发首触 slab/cpuhp） |
| driver_init 顺序 | devices→buses→classes→firmware→hypervisor→of_core→platform→**cpu→memory**→container（**无 bdi/devtmpfs/auxiliary/node**） | 6.12：auxiliary→**memory→node→cpu**→container + devtmpfs/bdi/auxiliary——**已对齐**（memory 挪回 cpu 后） |

---

## 2. 原厂内核设置（config）

| 配置项 | 原厂 | 6.12 原始 | 我们当前 | 备注 |
|---|---|---|---|---|
| CONFIG_DEVTMPFS | **n** | y | **n** | ★ devtmpfs kthread 跨簇交互是竞态挂点之一 |
| CONFIG_NR_CPUS | **32** | 512 | **32** | 512 时 per-cpu/RCU 树按 512 构建 |
| CONFIG_NUMA | **n** | y | **n** | 无 NUMA 硬件却开 → 调度/内存语义错误 |
| CONFIG_SCHED_SMT | **n** | y | **n** | 无 SMT 却开 → SMT 调度域错误 |
| CONFIG_NO_HZ_IDLE | y | y | **n（诊断）** | 原厂 kick 走 RESCHEDULE；我们关掉消除 CSD（待恢复+kick_ilb 补丁） |
| CONFIG_HZ | 250 | 250 | 250 | 一致 |
| CONFIG_CFI_CLANG | **y**（+SHADOW） | n | n | 全内核 KCFI |
| CONFIG_KFENCE | **y**（回移，sample=100） | n | n | 内存保护（6.12 原生可开） |
| CONFIG_LTO_CLANG | **y** | n | n | 编译层 |
| CONFIG_ARM64_ERRATUM_1286807 | y | y | y | A76 repeat-TLBI（r0-r3） |
| CONFIG_ARM64_ERRATUM_1165522 | y | y | y | A76 speculative-AT |
| CONFIG_ARM64_ERRATUM_4118414 | **n** | y | **n** | 对齐 |
| CONFIG_ARM64_ERRATUM_1530923 | **n** | y | **n** | 对齐 |
| CONFIG_CPU_IDLE / ARM_CPUIDLE | y / y | y | y（idle-states 已删） | 原厂有 LIT/BIG_CORE_PD + cluster-pd |
| CONFIG_KVM | ? | y | **n** | nvhe 无法引用 kernel static_key |
| CONFIG_JUMP_LABEL | y | y | y | static_key 基础设施（LSE 修复依赖） |

---

## 3. 影响内核编译/构建的其他因素

### 3.1 构建环境（本工作区）

| 项 | 值/路径 | 说明 |
|---|---|---|
| 交叉工具链 | `/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin` | 需 export PATH；系统 `/usr/bin/aarch64-linux-gnu-*` 也可用于 objdump 反汇编 |
| openssl 头/库 | 本地解包 `/home/test/t508n/openssl-local/usr/include`（+ `x86_64-linux-gnu/opensslconf.h`）、`usr/lib/x86_64-linux-gnu` | 无 libssl-dev（无 root 装不了） |
| 构建变量 | `CPATH`/`LIBRARY_PATH` 指向 openssl-local | **不能用命令行 HOST_EXTRACFLAGS**（会覆盖 dtc 的 += 导致 libfdt_env.h 找不到） |
| 构建命令 | `make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8` | 带上述 export |
| 打包 | `python3 tools/sprd_mkbootimg.py --kernel work/mainline_Image --ramdisk initramfs.cpio --output boot_linux.img --key tools/ot5_boot_key.pem` | LK 格式 boot 镜像 |
| 刷机 | `/mnt/c/platform-tools/fastboot.exe flash boot_a boot_linux.img`（WSL interop 传 Linux 路径自动转 \\wsl$） | 设备需先进 fastboot |

### 3.2 已知编译坑

1. **HOST_EXTRACFLAGS 覆盖问题**：命令行传 `HOST_EXTRACFLAGS=...` 会**覆盖**（而非追加）Makefile 里
   `scripts/dtc/Makefile` 的 `HOST_EXTRACFLAGS += -I $(src)/libfdt` → dtc 编译报
   `libfdt_env.h: No such file or directory`。**必须用环境变量 CPATH/LIBRARY_PATH**。
2. **opensslconf.h 在 multiarch 子目录**：`usr/include/x86_64-linux-gnu/openssl/opensslconf.h`——
   CPATH 需含该目录。
3. **libcrypto 链接**：本地 `libcrypto.a` 缺 zstd 依赖 → 需用系统 `libcrypto.so.3`（复制到
   openssl-local 并建 `libcrypto.so` 符号链接）。
4. **`scripts/dtc` 误删恢复**：源码树非 git 仓库时用
   `tar -xJf dl/linux-6.12.103.tar.xz -C . 'linux-6.12.103/scripts/dtc'`（通配符需带子目录）。
5. **nvhe/KVM 符号**：kernel 侧新增 static_key（`cpu_hwcap_keys`）后，KVM nvhe 独立镜像
   无法引用 → 链接报 `undefined reference to __kvm_nvhe_cpu_hwcap_keys` → **CONFIG_KVM=n**。
6. **`cpu` 未声明**：清理探针脚本误删 `int cpu = smp_processor_id()` 会编译错。
7. **CONFIG_NR_CPUS 联动**：512→32 需 `make` 全量重编（cpumask/per-cpu 数组尺寸变化）。
8. **.idmap.text >4KB**：探针过多时链接报 "ID map text too big"——探针勿堆在 head.S 早期路径。
9. **DTS 编译**：`cpu-idle-states` 引用删除后 idle-states 节点变孤儿（无害）；cpu-map 三簇
   4+3+1 引用 CPU0-7 必须存在。

### 3.3 我们的魔改对编译的影响（依赖关系）

| 魔改 | 编译依赖 | 影响 |
|---|---|---|
| LSE static_key | CONFIG_JUMP_LABEL=y；cpufeature.c 定义 `cpu_hwcap_keys[ARM64_NCAPS]`/`arm64_const_caps_ready`；lse.h 引用 | KVM 必须关（见 3.2.5） |
| NOMAP 检查 | CONFIG_ARCH_KEEP_MEMBLOCK=y（memblock_is_map_memory 可用） | — |
| phandle_cache 注释 | 无（static 数组仍定义） | — |
| ttwu_queue_cond 恒 false | 无 | — |
| 探针系统 | t508n_dbg.c/h；各文件 extern 声明 | 删探针时注意 extern/变量残留 |

### 3.4 原厂编译特征（反汇编确认，非移植必需）

- **LTO_CLANG + KCFI + SHADOW_CALL_STACK + PAuth**（xpaclri 113 处）——编译层组合
- 函数指针指向 KCFI 蹦床表（0x116c5000-0x116d9000），反汇编需沿 `b` 再跳一层
- `__cpu_up` 旧式错误 switch；psci 定制参数（见 1.6）

---

## 4. 当前代码状态（2026-08-18）

已应用的对齐/修复（git 工作树）：
1. LSE static_key（原厂式，弃 asm-goto NOP-落入）→ SM#2 曾修复（**但竞态漂移——SM#2 偶发又卡**）
2. `__pageblock_pfn_to_page` NOMAP 检查 → set_zone_contiguous 修复 ✅
3. of_core_init 循环内 phandle_cache 写注释 → of_core 948 节点通过 ✅
4. CONFIG_DEVTMPFS=n → devtmpfs 挂点消除 ✅（但底层竞态仍在，漂移到 SM#2/memory）
5. CONFIG_NUMA=n + SCHED_SMT=n → 8 核语义修复（部分）
6. ttwu_queue_cond 恒 false（P1）→ CSD 唤醒路径消除（部分）
7. CONFIG_NO_HZ_IDLE=n、NR_CPUS=32、KVM=n、errata 4118414/1530923=n、idle-states 删
8. **mem_sysfs_mutex 恢复 + memory_dev_init 挪回 cpu_dev_init 之后**（本次新应用）

**当前卡点**：SM#2（apply_alternatives_all）偶发卡（进度条 9 格、y=180/200 全不亮=fn 级未完成/未执行）——
竞态漂移的本质：**T770 多核同步的间歇性问题**（SGI 投递/跨核唤醒/原子/一致性，待死循环门+KFENCE+SGI 计数层定位）。

---

## 5. 待办/下一步

1. 加回 SM#2 fn 级探针（y=370 进入 / y=390 完成）定位"9 格"卡点（fn 未完成 vs 未执行）
2. 刷验证① kick_ilb 补丁（NO_HZ_IDLE 先 n 后 y）
3. 刷验证③ SGI 计数层（纯观测：smp_cross_call 发送 vs do_handle_IPI 接收，delta>0 = "发了没到"）
4. 开启 CONFIG_KFENCE=y（原生）诊断内存踩踏
5. pstore ramoops 报告通道（CONFIG_PSTORE_RAM+CONSOLE）
