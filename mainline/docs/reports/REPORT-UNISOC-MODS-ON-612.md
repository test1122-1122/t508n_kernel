# REPORT-UNISOC-MODS-ON-612：紫光魔改全量移植到干净 6.12.103 树（编译验证 + scheduler_ipi 安全性分析）

> 移植/验证子会话输出（prompt-unisoc-mods-on-612.md）。
> 干净树：`work/linux-6.12-clean/`（从 `dl/linux-6.12.103.tar.xz` 解包，未含 T508N 专项补丁）。
> 参考：`work/linux-5.4-custom`（5.4 复刻，已验证 boot）、`work/PATCH-nohz-kick.md`、
> `work/UNISOC-DETAILED.md`、`work/DISASM-{B,C}-*.md`。
> 完整 unified diff：`work/diff-612-unisoc-mods.patch`（12 文件 / 406 行）。

---

## 0. 结论（TL;DR）

1. **干净 6.12.103 树已移植全部 8+ 项紫光魔改**（scheduler_ipi 扩展 / kick_ilb→RESCHEDULE /
   stopper preempt_schedule / sugov 25% / memmap 跳 no-map + __pageblock_pfn_to_page NOMAP /
   shmem 0777 / 死循环门→计数 / irq_exit softirq 定制 / mem_sysfs_mutex / KFENCE+NO_HZ_IDLE），
   每个魔改都按 6.12 API 适配并给出 diff（§3）。
2. **编译通过**：`make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8` **成功**
   （BUILD_EXIT=0，Image 45.6 MiB），**0 warning / 0 error**（§4；sugov_set_freq_margin 的
   missing-prototype 通过新增头文件声明消除）。
3. **scheduler_ipi → nohz_csd_func 在 stop_machine 上下文安全**（§5 全论证）：
   - `nohz_csd_func()` **不取任何 spinlock**：atomic_fetch_andnot + 本核 rq 字段 +
     `__raise_softirq_irqoff()`（仅置 per-cpu pending 位）；`preempt_fold_need_resched()` 只动
     本核 preempt_count。二者在任何中断上下文合法。
   - stop_machine 的 `MULTI_STOP_DISABLE_IRQ/RUN/EXIT` 相位所有参与核 `PSTATE.I=1` →
     RESCHEDULE SGI **根本不投递**（GIC 在 CPU 侧掩蔽），scheduler_ipi 不会在 stop_machine
     临界区内运行；SGI 在 stop_machine 结束后才送达。
   - `MULTI_STOP_PREPARE` 相位（IRQ 开、cpu_relax 空转）若收到 kick：本核
     `rq->curr == cpu_stopper_thread` → `idle_cpu()==false` → nohz_csd_func **不 raise**
     SCHED_SOFTIRQ，只释放 kick 所有权（"忙则丢"，等价 5.4 `got_nohz_idle_kick` 的
     atomic_andnot+return false）。
   - 竞态窗口（stopper 已唤醒未运行、CPU 仍 idle）raise 的 SCHED_SOFTIRQ：在 IPI 返回
     irq_exit_rcu 分派；`_nohz_idle_balance()` 用 `rq_lock_irqsave`（raw spinlock，无睡眠），
     且 `if (!idle_cpu(balance_cpu)) continue` 会**跳过 stop_machine 中的核**；multi_cpu_stop
     空转核不持有任何 rq 锁 → 无死锁、无 sleep-in-atomic。C3 移植后 SCHED 走 ksoftirqd，
     与 stop_machine 完全错开。
   - `WARN_ON(!(flags & NOHZ_KICK_MASK))` 不可触发（KICK_MASK 位唯一置位者 kick_ilb、
     唯一清位者本核 nohz_csd_func，atomic_read 守卫保证观察到的位在 fetch_andnot 时仍在）。
4. **与 5.4 复刻对应**（§6）：6.12 版按 API 差异适配：scheduler_ipi 用现成 `nohz_csd_func()`
   替代 5.4 的 `got_nohz_idle_kick()`；memmap 逐页跳过从 5.4 的 `pfn_valid` 侧效应改为
   `memmap_init_range` 显式 `memblock_is_map_memory` 检查；mem_sysfs_mutex 按魔改树 6.12 版移植。
   **C3（irq_exit softirq 分派）为 5.4 复刻未移植项，本移植新增**（§6.3 风险提示）。
5. **建议**（§7）：kick_ilb/scheduler_ipi/stopper/sugov/shmem/C1/mem_sysfs_mutex 建议合入魔改树；
   C3 务必作为**独立单变量**验证；KFENCE=y 可选诊断；LSE 干净树保持 6.12 原版（不移植）。

---

## 1. 任务与方法

- 干净树：`dl/linux-6.12.103.tar.xz` → `work/linux-6.12-clean/`；`.config` 从
  `linux-6.12.103/.config`（T508N 魔改树基线：NR_CPUS=32、NUMA=n、SCHED_SMT=n、DEVTMPFS=n、
  KVM=n、ARCH_KEEP_MEMBLOCK=y）复制 → `olddefconfig`。
- 移植源：5.4 复刻 `work/linux-5.4-custom` commit `2ca901fb0`（scheduler_ipi ksoftirqd kick B1、
  stopper preempt_schedule B2、sugov 25% B3、pfn_valid NOMAP D1/D2、shmem 0777 F1、
  IPI 死循环门→计数 C1）+ `work/PATCH-nohz-kick.md`（kick_ilb→RESCHEDULE 设计 A，6.12 版）
  + 魔改树 `linux-6.12.103` 已适配修复（`__pageblock_pfn_to_page` NOMAP、mem_sysfs_mutex）。
- 编译环境（与 UNISOC-DETAILED §3.1 相同）：
  `PATH=arm-gnu-toolchain-13.2.../bin`，`CPATH/LIBRARY_PATH` 指向 `openssl-local`，
  `make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8`。

---

## 2. 配置变更（.config）

| 项 | 基线 | 移植后 | 说明 |
|---|---|---|---|
| `CONFIG_NO_HZ_IDLE` | n（诊断关闭） | **y** | PATCH-nohz-kick §4.4：恢复 tickless idle，使 kick_ilb 路径生效（select NO_HZ_COMMON，代码 gate 全部命中） |
| `CONFIG_NO_HZ_COMMON` | n | y | 由 NO_HZ_IDLE select |
| `CONFIG_KFENCE` | n | **y**（sample=100） | 6.12 原生，对齐原厂 KFENCE（UNISOC-DETAILED §1.3；可选诊断项） |
| 其余（NR_CPUS/NUMA/SCHED_SMT/DEVTMPFS/KVM/ERRATUM…） | — | 不变 | 沿用魔改树基线 |

执行：`./scripts/config --enable NO_HZ_IDLE --enable KFENCE && make olddefconfig`。
注意 6.12 中 `CONFIG_NO_HZ` 为兼容别名（choice default），NO_HZ_IDLE=y 后不另行置位，
代码侧 gate 用 `CONFIG_NO_HZ_COMMON`/`CONFIG_NO_HZ_IDLE`，语义不受影响。

---

## 3. 每个魔改的 6.12 移植（diff 要点）

完整补丁：`work/diff-612-unisoc-mods.patch`。逐项如下（编号沿用任务清单）：

### 3.1 scheduler_ipi 扩展 + kick_ilb → smp_send_reschedule（任务 1+8，PATCH-nohz-kick 设计 A）

**6.12 适配要点**：6.12 的 `scheduler_ipi` 是 sched.h:1968 的全 arch 共享 inline（仅
`preempt_fold_need_resched()`）；恢复为 core.c 真实函数（5.4 布局），并在 fold 后消费
kick_ilb 经 RESCHEDULE 送达的 nohz kick。

- `kernel/sched/fair.c` `kick_ilb()`（:12374）：
  `smp_call_function_single_async(ilb_cpu, &cpu_rq(ilb_cpu)->nohz_csd)` →
  `smp_send_reschedule(ilb_cpu)`（CSD+IPI_CALL_FUNC/SGI#1 → IPI_RESCHEDULE/SGI#0）。
  **6.12 编译坑**：`smp_send_reschedule` 在 6.12 是宏（`trace_ipi_send_cpu` + `__smp_send_reschedule`），
  `trace_ipi_send_cpu` 声明在 `trace/events/ipi.h`——fair.c 原不含该头（core.c 有），
  **新增 `#include <trace/events/ipi.h>`**（5.4 的 smp_send_reschedule 是纯 extern，无此问题）。
- `include/linux/sched.h`：`static __always_inline`（12 行）→ `extern void scheduler_ipi(void);`
  （!CONFIG_SMP 空 inline 保留）。
- `kernel/sched/core.c`：在 `sched_ttwu_pending()`（:3772，CONFIG_SMP 区）之后新增
  `void scheduler_ipi(void)`：`preempt_fold_need_resched()` +
  `#ifdef CONFIG_NO_HZ_COMMON` 下 `atomic_read(nohz_flags(cpu)) & NOHZ_KICK_MASK` 守卫 →
  `nohz_csd_func(this_rq())`。**接收侧逻辑与旧 CSD 回调逐字节复用同一函数**（core.c:1236，
  唯一差异是传输通道）。`nohz_csd_func` 定义于 :1236，同文件可见；`nohz_flags` 宏在
  sched.h:3204。
- **B1（5.4 复刻的 ksoftirqd 唤醒）处理**：PATCH-nohz-kick §4.5 OPT 语义由 **C3 移植
  （§3.7）自动覆盖**——SCHED_SOFTIRQ 在 irq_exit 分派时（pending&0x41==0）唤醒 ksoftirqd，
  与 stock scheduler_ipi 显式 `wake_up_process(ksoftirqd)` 等价；故 6.12 版 scheduler_ipi
  不再内联 wake（避免双重唤醒，注释说明）。

### 3.2 cpu_stopper_thread 队空显式 preempt_schedule（任务 2 / B2）

- `kernel/stop_machine.c` `cpu_stopper_thread()`（:486）：6.12 版函数体与 5.4 结构一致
  （`repeat:` 循环 + `if (work) {...goto repeat;}`），在函数末尾（队空落出点）追加
  `preempt_schedule();`。位置与 5.4 复刻逐字对应；preempt_count 此时为 0（fn 已
  preempt_count_dec），TIF_NEED_RESCHED 未置则 no-op——安全。

### 3.3 sugov_set_freq_margin 25%（任务 3 / B3）

- `kernel/sched/cpufreq_schedutil.c`：新增 `static unsigned int sugov_freq_margin = 25;` +
  `void sugov_set_freq_margin(unsigned int margin)`；`get_next_freq()` 在
  `sg_policy->cached_raw_freq = freq;` 后、`cpufreq_driver_resolve_freq` 前：
  `if (sugov_freq_margin) freq += freq * sugov_freq_margin / 100; freq = min(freq, policy->cpuinfo.max_freq);`
  （与 5.4 复刻逐行同构）。缓存比较用原始 freq（margin 前），与 5.4 一致。
- **6.12 编译坑**：`sugov_set_freq_margin` 无原型 → `-Wmissing-prototypes`（5.4 也有此问题，
  5.4 构建未报错因警告策略不同）。新增声明于 `include/linux/sched/cpufreq.h`（6.12 有该头，
  由 kernel/sched/sched.h:11 引入，build_utility.c 可见）。
- **另一个 6.12 特定事实**：cpufreq_schedutil.c **不是独立编译单元**，而是被
  `kernel/sched/build_utility.c:68` `#include "cpufreq_schedutil.c"`（`CONFIG_CPU_FREQ_GOV_SCHEDUTIL`）——
  直接 `make .../cpufreq_schedutil.o` 是伪目标（会无头编译报错），必须编 build_utility.o。

### 3.4 memmap_init 跳 no-map（任务 4 / D2）

- 5.4 的做法：改 `pfn_valid()`（mmzone.h）使 no-map pfn 判假 → `memmap_init_zone` 的
  `early_pfn_valid()` 逐页检查自然跳过。
- **6.12 差异**：`memmap_init_range()`（mm/mm_init.c:863，5.4 的 memmap_init_zone 已改名并
  迁移）**不再逐页调 pfn_valid**（只保留 overlap_memmap_init/defer_init）；因此本移植在
  `MEMINIT_EARLY` 分支加显式检查：
  ```c
  if (!memblock_is_map_memory(PFN_PHYS(pfn))) { pfn++; continue; }
  ```
  （mm_init.c 已 `#include <linux/memblock.h>`，无需新增头。）跳过页保持全零、不进 buddy
  （`for_each_free_mem_range` 过滤 NOMAP）、其它路径由 pfn_valid/__pageblock_pfn_to_page
  NOMAP 守卫挡住（见 3.5）。`pfn++` 显式递增再 continue——6.12 的循环体末尾才有 `pfn++`，
  continue 不会跳过递增（与 overlap_memmap_init 靠引用改 pfn 不同，不能漏）。

### 3.5 pfn_valid NOMAP（任务 10，6.12 版 = __pageblock_pfn_to_page 修复）

- **决策**：不移植 5.4 的 mmzone.h `pfn_valid()` 改法（改全局 pfn_valid 语义、需前向声明
  memblock 函数避免头循环，风险面大）；**参照 6.12 魔改树已验证的修复**——
  `mm/page_alloc.c` `__pageblock_pfn_to_page()` 开头：
  ```c
  if (!memblock_is_map_memory(start_pfn << PAGE_SHIFT) ||
      !memblock_is_map_memory((end_pfn - 1) << PAGE_SHIFT))
      return NULL;
  ```
  修复 set_zone_contiguous/pageblock_pfn_to_page 穿过 no-map 空洞的路径；
  新增 `#include <linux/memblock.h>`。语义对齐：所有 no-map pfn 对任何消费者不可用。

### 3.6 shmem mode=0777（任务 5 / F1）

- `mm/shmem.c` `shmem_init_fs_context()`（:5018）：`ctx->mode = 0777 | S_ISVTX;` →
  `ctx->mode = 0777;`（去 S_ISVTX，对齐 stock 0x3ff）。一行，无 6.12 API 差异。

### 3.7 死循环门降级计数（任务 6 / C1）

- `arch/arm64/kernel/smp.c`：enum ipi_msg_type 之后新增
  `DEFINE_PER_CPU(unsigned long, t508n_ipi_resched_count);`；
  `do_handle_IPI()` case IPI_RESCHEDULE 内 `scheduler_ipi();` 前
  `this_cpu_inc(t508n_ipi_resched_count);`。6.12 的 do_handle_IPI 与 5.4 handle_IPI 结构一致，
  直接移植。死循环保持注释（hang-by-design，mainline 不启用）。

### 3.8 irq_exit softirq 分派定制（任务 7 / C3）

- `kernel/softirq.c` 非 RT `invoke_softirq()`（:444，6.12 的 irq_exit_rcu → __irq_exit_rcu 调用它）：
  改为 stock 语义——`pending & 0x41`（HI|TASKLET）→ 硬中断上下文内联 `__do_softirq()`
  （沿用 upstream 的 force_irqthreads/IRQ-stack 分支）；**其余（TIMER/SCHED/RCU…）→ 唤醒
  ksoftirqd**（`__this_cpu_read(ksoftirqd)` 非空即 wake_up_process；ksoftirqd 缺失=early boot
  才内联）。
- 6.12 的 `wakeup_softirqd()` = `wake_up_process(__this_cpu_read(ksoftirqd))`，与 stock
  "state==0 && PF_KTHREAD → wake_up_process" 语义等价（wake_up_process 对运行中任务 no-op）。
- **交互**：本定制使 scheduler_ipi 里 raise 的 SCHED_SOFTIRQ 走 ksoftirqd（对齐 stock
  scheduler_ipi 显式唤醒 ksoftirqd 的 B1 语义），并全局把 TIMER/RCU 等 softirq 延迟到
  ksoftirqd——**这是原厂行为，但 5.4 复刻未移植 C3**（§6.3/§7 风险提示）。

### 3.9 mem_sysfs_mutex（任务 10）

- `drivers/base/memory.c`：`static DEFINE_MUTEX(mem_sysfs_mutex);`（+`#include <linux/mutex.h>`）；
  `create_memory_block_devices()` 循环、`remove_memory_block_devices()` 循环、
  `memory_dev_init()` 注册循环 各包 `mutex_lock/unlock`（对齐 5.4 memory.c:704-749 与魔改树
  6.12 版，去除魔改树的探针代码）。6.12 上游已删该锁（用 device_hotplug_lock），移植恢复
  stock 序列化。

### 3.10 KFENCE / LSE（任务 9/11）

- `CONFIG_KFENCE=y`（6.12 原生，sample=100，§2）。
- **LSE：干净树保持 6.12 原版，不移植**（static_key 修复属魔改树 T508N 专项，非 Unisoc
  原厂语义；任务明确暂不移植）。

---

## 4. 编译结果

- 命令：`export PATH=...arm-gnu-toolchain-13.2.../bin:$PATH; export CPATH=...openssl-local...;
  export LIBRARY_PATH=...; make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8`
  （日志：`work/build-612-clean.log`）。
- 结果：**BUILD 成功（BUILD_EXIT=0）**，产物
  `work/linux-6.12-clean/arch/arm64/boot/Image`（45,619,712 B ≈ 43.5 MiB）。
- 全程 **0 个 warning / 0 个 error**（`sugov_set_freq_margin` 的 missing-prototype 已通过
  `include/linux/sched/cpufreq.h` 声明消除；日志 `grep -ciE "warning:|error:"` 均 0）。
- 修改文件（12）：kernel/sched/{fair,core,cpufreq_schedutil}.c、include/linux/{sched.h,
  sched/cpufreq.h}、kernel/stop_machine.c、kernel/softirq.c、mm/{mm_init,page_alloc,shmem}.c、
  arch/arm64/kernel/smp.c、drivers/base/memory.c、.config。
  （注：kernel/sched/cpufreq_schedutil.c 经 `build_utility.c:68` 的 `#include` 编译，非独立目标；
  直接 `make .../cpufreq_schedutil.o` 是伪目标，会无头编译报错——已按 build_utility.o 验证。）

---

## 5. 上下文安全性分析：scheduler_ipi → nohz 处理在 stop_machine 上下文（核心）

### 5.1 scheduler_ipi 的执行上下文（6.12 arm64）

- 调用链：GIC SGI → `ipi_handler()`（arch/arm64/kernel/smp.c:1006）→ `do_handle_IPI()` case
  IPI_RESCHEDULE → `scheduler_ipi()`。整个 IRQ 被 `__el1_irq`（entry-common.c:624）以
  `irq_enter_rcu() ... irq_exit_rcu()` 包裹：**硬中断上下文、IRQ 掩蔽（PSTATE.I=1）、
  preempt_count 含 HARDIRQ_OFFSET**。IPI 返回时 `irq_exit_rcu()` → `__irq_exit_rcu()` →
  `invoke_softirq()` 分派本 IPI 期间 raise 的 softirq。

### 5.2 nohz_csd_func 做了什么（是否有锁/睡眠风险）

```c
flags = atomic_fetch_andnot(NOHZ_KICK_MASK | NOHZ_NEWILB_KICK, nohz_flags(cpu));
WARN_ON(!(flags & NOHZ_KICK_MASK));
rq->idle_balance = idle_cpu(cpu);
if (rq->idle_balance) {
    rq->nohz_idle_balance = flags;
    __raise_softirq_irqoff(SCHED_SOFTIRQ);
}
```
- **不取任何 spinlock**：atomic_fetch_andnot（LSE/LL-SC 原子，无锁）、本核 rq 字段写、
  `__raise_softirq_irqoff()`（`or_softirq_pending(1UL << nr)`，IRQ 已关时置 per-cpu 位）。
  无睡眠、无 RCU 读锁、无内存分配。
- `preempt_fold_need_resched()` = `if (tif_need_resched()) preempt_count |= PREEMPT_NEED_RESCHED`
  ——只动本核 preempt_count，任何上下文合法。
- 因此 scheduler_ipi 本身（fold + nohz_csd_func）在硬中断上下文**无条件安全**；
  问题只剩"软中断被 raise 后的处理时机/上下文"。

### 5.3 stop_machine 期间（boot：apply_alternatives_all @cpufeature.c:3561、
KPTI @1952、enable_cpu_capabilities @3269 → `stop_machine(..., cpu_online_mask)`）

`multi_cpu_stop()`（stop_machine.c:201）状态机：
`PREPARE`（IRQ 开、`stop_machine_yield()`=cpu_relax 空转）→ `DISABLE_IRQ`
（`local_irq_disable(); hard_irq_disable();` = PSTATE.I 置位）→ `RUN`（active CPU 执行 fn）
→ `EXIT`（local_irq_restore）。

| stop_machine 相位 | 该核 IRQ 状态 | RESCHEDULE SGI 能否到达 | scheduler_ipi 行为 |
|---|---|---|---|
| PREPARE（及排队前） | 开 | 能 | 本核 rq->curr == cpu_stopper_thread → `idle_cpu()==false` → nohz_csd_func 只释放 kick 所有权，**不 raise**（忙则丢，等价 5.4 got_nohz_idle_kick 的 atomic_andnot+return false）；非 kick 的 RESCHEDULE 纯 fold 返回 |
| DISABLE_IRQ / RUN / EXIT | PSTATE.I=1（GIC SGI 在 CPU 侧掩蔽） | **不能**（SGI pending 到 IRQ 恢复） | 不运行——stop_machine 临界区与 scheduler_ipi **完全互斥**，天然无竞态 |
| stop_machine 结束后 | 恢复 | 补投递 | 正常处理（空闲则 kick 生效/忙则丢） |

**核心结论 1（互斥）**：stop_machine 的"全核 IRQ 关断"相位保证 scheduler_ipi 不可能在
stop_machine 的 fn 执行期间运行；PREPARE 相位即使收到 kick，本核处于"忙"（stopper 线程）
状态，nohz_csd_func 的门控 `if (rq->idle_balance)` 直接短路——**不存在"在 stop_machine
上下文里 raise 软中断/取 rq 锁"的路径**。

### 5.4 残余竞态窗口（stopper 已唤醒、CPU 仍 idle）

窗口：stop_machine 的 work 入队唤醒本核 stopper 后、stopper 尚未抢占运行前，CPU 仍
`rq->curr==idle`。此时收到 kick → nohz_csd_func → idle_balance=1 → raise SCHED_SOFTIRQ：
- 无 C3：IPI 返回 irq_exit_rcu 内联跑 `sched_balance_softirq()` → `nohz_idle_balance()` →
  `_nohz_idle_balance()`：`rq_lock_irqsave(rq)`（**raw spinlock + irqsave，无睡眠**）遍历
  `nohz.idle_cpus_mask`，`if (!idle_cpu(balance_cpu)) continue` **跳过所有 stop_machine 中的核**
  （它们 rq->curr==stopper）；multi_cpu_stop 空转核不持有任何 rq 锁 → **无锁序交互/无死锁**。
  唯一代价：ilb 完成稍晚（毫秒级），而 stop_machine PREPARE 无时限 → 无正确性影响。
- 有 C3：SCHED 非 0x41 → 唤醒 ksoftirqd → ksoftirqd 在 stopper（更高优先级）之后运行 →
  与 stop_machine 完全错开。
- 补充：即使 stop_machine 期间 CPU 空闲间隙把 balance 跑完，`sched_balance_domains(CPU_IDLE)`
  也不会迁移 stop 线程（stop 任务 `cpus_allowed` 钉死、stop 类不可迁移）。

### 5.5 WARN_ON(!(flags & NOHZ_KICK_MASK)) 不可触发论证

- `NOHZ_KICK_MASK = NOHZ_BALANCE_KICK|NOHZ_STATS_KICK|NOHZ_NEXT_KICK`；置位者**只有**
  `kick_ilb()`（远端 atomic_fetch_or，且仅在置位后发 IPI）；清位者**只有**本核
  `nohz_csd_func()`（atomic_fetch_andnot）。`nohz_run_idle_balance()` 只清 `NOHZ_NEWILB_KICK`
  （不在 KICK_MASK 内），不影响。
- scheduler_ipi 的 `atomic_read(...) & NOHZ_KICK_MASK` 守卫观察到 kick 后，到
  fetch_andnot 之间本核处于硬中断（不可重入、无第三方清位者）→ 位必然仍在 → WARN_ON 不触发。
  稳妥起见仍建议改 `WARN_ON_ONCE`（§7）。

### 5.6 与原厂/5.4 做法对比（任务要求回答的问题）

- **原厂 scheduler_ipi**（DISASM-B B1）：fold + 就地 sched_ttwu_pending + **自触发 idle
  balance kick**（空闲核收到任意 RESCHEDULE 且无活 → idle_balance=1 + raise_softirq +
  wake ksoftirqd，cpu_isolated 豁免）。即原厂 RESCHEDULE 中断"IPI 即干活"。
- **5.4 上游**：`got_nohz_idle_kick()`（busy→drop）+ idle_balance=1 + raise_softirq_irqoff，
  无 ksoftirqd 唤醒（softirq 由 irq_exit 内联跑）。
- **本移植（6.12）**：PATCH-nohz-kick 设计 A——kick 消费逻辑复用现成 `nohz_csd_func()`
  （与旧 CSD 路径逐字节一致），softirq 在 IPI 返回 irq_exit 分派；配合 C3 后等效"wake
  ksoftirqd"。**不直接调 nohz_csd_func 之外的任何东西**，也没有原厂的"自触发"
  （PATCH-nohz-kick §4.5 OPT 默认关闭——避免空闲核收到任意 RESCHEDULE 都触发 balance）。
  原厂的自触发式 OPT 保留为可选加固，建议独立单变量验证后再启用。
- 结论：**PATCH-nohz-kick 设计 A 在 stop_machine 上下文无风险**，不需要修正版；
  唯一可选修正点是 `WARN_ON_ONCE` 与（可选）OPT 自触发的独立验证（§7）。

---

## 6. 与 5.4 复刻的对应关系

| 魔改 | 5.4 复刻（2ca901fb0 / B1-B3,C1,D1/D2,F1） | 6.12 干净树移植（本报告） | 适配说明 |
|---|---|---|---|
| scheduler_ipi 扩展 | core.c 真实函数：fold + wake_list + got_nohz_idle_kick + ksoftirqd 唤醒（B1） | core.c 真实函数：fold + `nohz_csd_func()`；ksoftirqd 唤醒由 C3 的 irq_exit 分派覆盖 | 6.12 无 got_nohz_idle_kick / 无 wake_list 字段（wake 归 CSD）；复用现成 nohz_csd_func |
| kick_ilb | smp_send_reschedule（原厂/5.4 通道） | smp_send_reschedule + fair.c 补 trace/events/ipi.h | 6.12 smp_send_reschedule 是 trace 宏；PATCH-nohz-kick 设计 A |
| stopper preempt_schedule | cpu_stopper_thread 队空 preempt_schedule（B2） | 同位置同语义 | 6.12 函数体结构相同 |
| sugov 25% | sugov_freq_margin=25 + get_next_freq 叠加（B3） | 同构 + 头文件声明 | 6.12 map_util_freq 已有上游 1.25 余量（叠加后 ~56%）；与 5.4 复刻一致 |
| pfn_valid NOMAP | mmzone.h pfn_valid 拒绝 NOMAP（D1/D2 侧效应） | `__pageblock_pfn_to_page` 显式 NOMAP 检查（魔改树 6.12 版） | 6.12 memmap_init_range 不调 pfn_valid，故 D2 需显式逐页检查（3.4）；pfn_valid 全局语义不动 |
| memmap 跳 no-map | memmap_init_zone 靠 early_pfn_valid 跳过 | memmap_init_range MEMINIT_EARLY 显式 `memblock_is_map_memory` 跳过 | 5.4 侧效应 → 6.12 显式化 |
| shmem 0777 | ctx->mode=0777（F1） | 同 | 无差异 |
| 死循环门 | handle_IPI 计数降级（C1） | do_handle_IPI 计数降级 | 6.12 入口函数名不同（do_handle_IPI） |
| irq_exit 定制 | **5.4 复刻未移植 C3** | invoke_softirq 0x41 分派（C3，本移植新增） | 6.12 非 RT invoke_softirq 等价 5.4 irq_exit 分派点 |
| mem_sysfs_mutex | —（5.4 上游本就有） | 恢复 stock 序列化（魔改树 6.12 版去探针） | 6.12 上游已删该锁 |
| LSE | — | **不移植**（保持 6.12 原版） | 任务 11 |
| KFENCE | — | CONFIG_KFENCE=y | 任务 9（可选） |

**干净树与魔改树的差异（有意保留）**：干净树**不含** P1（ttwu_queue_cond 恒 false）、LSE
static_key、探针/进度条、DEVTMPFS/NUMA 之外的其它 T508N 专项——即本树 = 6.12.103 原版 +
纯紫光魔改，用于评估"魔改本身是否成立"。

---

## 7. 建议（哪些移植到魔改树、哪些修正）

1. **kick_ilb→RESCHEDULE + scheduler_ipi 扩展（3.1）**：**强烈建议合入魔改树**（PATCH-nohz-kick
   已验证设计；消除调度侧最后一块 CSD+IPI_CALL_FUNC 暴露面，全部收敛 SGI#0）。
2. **B2 stopper / B3 sugov / F1 shmem / C1 计数（3.2/3.3/3.6/3.7）**：低风险、语义等价，建议合入。
3. **D2 memmap 显式跳过 + __pageblock_pfn_to_page NOMAP（3.4/3.5）**：魔改树已有后者；
   前者（memmap_init_range 逐页跳过）为本移植新增，建议合入以完整对齐原厂 memmap 行为。
4. **C3 irq_exit 分派（3.8）**：⚠️ **建议作为独立单变量验证后再并入**——它全局改变 softirq
   时序（TIMER/SCHED/RCU 延迟到 ksoftirqd），5.4 复刻未移植（5.4 复刻的 ksoftirqd 唤醒在
   scheduler_ipi 内、irq_exit 仍上游语义）。若 T770 上验证"空闲核 SCHED_SOFTIRQ 延迟处理"
   无 wakeup/rebalance 回归，则保留；否则去掉 C3 并改在 scheduler_ipi 内显式 wake ksoftirqd
   （= 5.4 复刻 B1 方式，也等效原厂）。
5. **mem_sysfs_mutex（3.9）**：魔改树已有（带探针）；语义一致，建议保留（探针另行剥离）。
6. **KFENCE=y**：可选诊断，建议开启（配合 sample=100 对齐原厂）。
7. **scheduler_ipi 加固**：把 `nohz_csd_func()` 的 `WARN_ON` 改 `WARN_ON_ONCE`（一行；
   若实机触发说明存在未预期的清位者，抓现场优先）。
8. **验证顺序（PATCH-nohz-kick §8）**：① 3 文件补丁 + NO_HZ_IDLE 仍 n（等价现状，回归 0）→
   ② NO_HZ_IDLE=y（本树配置）→ ③ C3 独立开关 → ④ 死循环门计数 + SGI 收发计数对照
   （smp_cross_call vs do_handle_IPI，delta>0="发了没到"）→ ⑤ 长跑确认无 WARN/softlockup。

---

## 8. 变更文件清单

| 文件 | 改动 |
|---|---|
| `kernel/sched/fair.c` | kick_ilb 通道 RESCHEDULE；+`#include <trace/events/ipi.h>` |
| `kernel/sched/core.c` | 新增 `scheduler_ipi()` 真实函数（fold + nohz_csd_func 守卫） |
| `include/linux/sched.h` | scheduler_ipi inline → extern（!SMP stub 保留） |
| `kernel/sched/cpufreq_schedutil.c` | sugov_freq_margin=25 + sugov_set_freq_margin + get_next_freq 叠加 |
| `include/linux/sched/cpufreq.h` | sugov_set_freq_margin 原型 |
| `kernel/stop_machine.c` | cpu_stopper_thread 队空 preempt_schedule |
| `kernel/softirq.c` | invoke_softirq 0x41 分派（C3） |
| `mm/mm_init.c` | memmap_init_range MEMINIT_EARLY 跳 no-map |
| `mm/page_alloc.c` | __pageblock_pfn_to_page NOMAP 守卫；+memblock.h |
| `mm/shmem.c` | tmpfs 默认 mode 0777（去 S_ISVTX） |
| `arch/arm64/kernel/smp.c` | per-CPU t508n_ipi_resched_count 计数（C1） |
| `drivers/base/memory.c` | mem_sysfs_mutex 恢复（3 处循环包锁） |
| `.config` | CONFIG_NO_HZ_IDLE=y、CONFIG_KFENCE=y |

（补丁全文：`work/diff-612-unisoc-mods.patch`；构建日志：`work/build-612-clean.log`）
