# T508N：原厂 5.4 vs mainline 6.12 并发基础设施对比报告

目标：解释 sysfs/device model 初始化在多核下的偶发卡死，并给出可操作修复。
符号来源：`work/stock_kallsyms.map`；反汇编 mapping `--adjust-vma=0xffffffc010080000`。

---

## 任务 1：锁实现对比

### 1.1 符号地址

| 符号 | 原厂 5.4 | mainline 6.12 |
|---|---:|---:|
| `queued_spin_lock_slowpath` | `ffffffc0102452a8` | `ffffffc08107a7e0` |
| `queued_write_lock_slowpath` | `ffffffc010248314` | `ffffffc08107ac20` |
| `queued_read_lock_slowpath` | `ffffffc0102484c0` | `ffffffc08107aaf8` |
| `_raw_spin_lock` | `ffffffc0116a8160` | `ffffffc08107a210` |
| `_raw_spin_unlock` | `ffffffc0116a7e08` | `ffffffc081079ed0` |
| `mutex_lock` | `ffffffc0116a336c` | `ffffffc0810752bc` |
| `__mutex_lock_slowpath` | `ffffffc0116a32d4` | `ffffffc0810752a4` |
| `down_write` | `ffffffc0116a3aec` | `ffffffc081076bd8` |
| `up_write` | `ffffffc0102427f4` | `ffffffc0800ea1d8` |

原厂没有独立的 `arch_spin_lock` 全局符号；arm64 的 `arch_spin_lock` 是
`asm-generic/qspinlock` 的 static inline，编译进 `_raw_spin_lock`。

### 1.2 原厂 spinlock 用 LSE 还是 LL/SC？

原厂 `_raw_spin_lock` 反汇编（`ffffffc0116a8160`）：
```asm
ffffffc0116a816c: mrs x8, sp_el0
ffffffc0116a8170: ldr w9,[x8,#24]
ffffffc0116a8174: add w9,w9,#1
ffffffc0116a8178: str w9,[x8,#24]     ; preempt_count++
ffffffc0116a817c: mov x8,x0
ffffffc0116a8180: b   0xffffffc0116a81b4 ; runtime alternative: patch 后走下面
; LSE replacement:
ffffffc0116a8188: mov x1,xzr
ffffffc0116a8190: mov w2,#1
ffffffc0116a8198: casa w9,w2,[x8]     ; LSE CAS
ffffffc0116a819c: mov w0,w9
ffffffc0116a81a4: cbnz x1, slowpath
ffffffc0116a81a8: ret
; LL/SC original 在 0x...81b4 -> 0x...87b4 块
```

**结论：原厂正常启动时 spinlock fast path 使用 LSE `casa`。**
未 patch 时才是 LL/SC。mainline 6.12 的 `_raw_spin_lock` 结构完全一样：
```asm
ffffffc08107a220: b   0xffffffc08107a238
ffffffc08107a224: mov w1,#0
ffffffc08107a228: mov w2,#1
ffffffc08107a22c: casa w1,w2,[x0]   ; LSE
...
ffffffc08107a238: prfm pstl1strm,[x0]
ffffffc08107a244: ldaxr w1,[x0]     ; LL/SC
```

### 1.3 slowpath

两者 `queued_spin_lock_slowpath` 都混合使用：
- LSE：`ldseta`、`casa`、`stadd`；
- LL/SC：`ldxr/stxr`；
- WFE 等待：`sevl/wfe`；
- pending/owner 状态机编码一致（0x100 pending、0xff owner）。

原厂 slowpath 片段：
```asm
ffffffc0102452fc: mov w9,#0x100
ffffffc010245300: ldseta w9,w9,[x8]      ; LSE 尝试拿 pending
ffffffc010245310: ldar w9,[x8]
ffffffc010245328: ldxr w10,[x8]          ; LL/SC 退避
...
ffffffc0102453cc: casa w14,w2,[x8]       ; LSE 尝试拿锁
```

mainline slowpath 片段：
```asm
ffffffc08107a7f4: mov w1,#0x100
ffffffc08107a7f8: ldseta w1,w1,[x0]
ffffffc08107a814: sevl / wfe
ffffffc08107a844: ldaxr w1,[x0]          ; LL/SC
ffffffc08107a8b4: casa w1,w7,[x0]        ; LSE
```

**结论：spinlock 算法/顺序/屏障同一套。唯一现实差异是 LSE patch 是否真的被应用。
这正是之前 LSE early patch 的价值。**

### 1.4 mutex

原厂 `mutex_lock`（`ffffffc0116a336c`）与 mainline `mutex_lock`
（`ffffffc0810752bc`）结构一致：

- fast path：`casa x9,x2,[x0]`（LSE CAS）抢 `owner=NULL`；
- 失败调 `__mutex_lock_slowpath`；
- slowpath 最终进入 mutex waiter 链表，标准 MCS/waiter 状态；
- 无额外屏障，无 WFE 超时机制。

### 1.5 rwsem

原厂 `queued_write_lock_slowpath/queued_read_lock_slowpath` 与 mainline
同名函数也同源：
- writer pending 用 `casa` / `stset`；
- reader count 用 `stadd`；
- writer/reader 等待均 `sevl/wfe` + `ldxr`。

---

## 任务 2：LSE 原子在锁路径的使用细节

mainline 6.12 的 `__alt_instructions` 中 LSE cap `ARM64_HAS_LSE_ATOMICS`
共 **12,074** 条，覆盖：
- `_raw_spin_lock` 地址附近有 cap=37 的 alternative 条目；
- `mutex_lock`、rwsem、atomic/cmpxchg、bitops 同样被覆盖。

原厂 5.4 的 alternative 表中也有一个 57,220 条的超大 cap（几乎可确定是 LSE
atomics 组，stock 5.4 枚举号不同），说明原厂同样依赖运行时 patch。

**结论：**
1. 原厂和 mainline 的锁代码本身都要求 LSE patch 生效；
2. mainline 当前已把 LSE early patch 移到 `smp_prepare_boot_cpu()` 的
   IRQ-off 单核阶段，方向正确；
3. 需要确保 `t508n_apply_lse_early()` 后 `_raw_spin_lock` 的
   `b LL/SC` 分支确实被 patch 成 LSE 路径，并且 I-cache 已失效。
   当前实现已加 `caches_clean_inval_pou(_stext,_etext)`，符合原厂
   `dc cvau + ic ivau` 思路。

---

## 任务 3：多核同步基础设施

原厂 `secondary_start_kernel`（`ffffffc010115d40`）关键顺序：

```text
mrs sp_el0
msr tpidr_el1, per_cpu_offset
atomic_inc(&secondary_data.cpu_count)      ; 用 LSE stadd
set swapper_pg_dir TTBR0
dsb nshst; tlbi vmalle1; dsb nsh; isb
secondary_data.status = CPU_BOOT_SUCCESS
notify_cpu_starting(cpu)                   ; 触发 gic_cpu_init、timer 等
set_cpu_online(cpu)
notify_cpu_online(cpu)
set_cpu_active(cpu, true)
local_irq_enable()                         ; 之后进入 idle
```

mainline `secondary_start_kernel`（`ffffffc080020f58`）顺序语义相同：
`__cpu_setup -> notify_cpu_starting -> set_cpu_online -> local_irq_enable`。

GIC/timer 初始化：
- 原厂 `gic_starting_cpu` / `gic_cpu_init`：`ffffffc0109aaa48` 起；
- mainline `gic_starting_cpu`：`ffffffc080575520` 起；
- 都通过 CPUHP/notifier 在 secondary 启动路径执行，无本质差异。

**结论：GIC/IPI/timer 初始化时机一致，没有“原厂多做了什么”的隐藏同步。**

---

## 任务 4：启动时序

原厂 `kernel_init_freeable`（`ffffffc012148288`）调用顺序：
```text
smp_prepare_cpus
workqueue_init
init_mm_internals
do_pre_smp_initcalls
lockup_detector_init
smp_init
sched_init_smp
page_alloc_init_late
do_basic_setup
```

mainline `kernel_init_freeable`（`ffffffc081c5109c`）顺序完全相同。

原厂 `smp_cpus_done`（`ffffffc01214e4b8`）：
```text
setup_cpu_features()
hyp_mode_check()
stop_machine(__apply_alternatives_multi_stop)   ; 完整系统级 alternatives
mark_linear_text_alias_ro()
```

mainline `smp_cpus_done`（`ffffffc081c558bc`）：
```text
hyp_mode_check()
setup_system_features()   ; 当前 apply_alternatives_all() 被跳过
setup_user_features()
mark_linear_text_alias_ro()
```

**关键差异：原厂在 `smp_cpus_done` 中通过 stop_machine 完整应用系统级
alternatives；当前 mainline 跳过了这一步。这是两者最大的启动时序差异。**

---

## 任务 5：config 并发/调试选项

| 选项 | 原厂 | mainline | 影响 |
|---|---:|---:|---|
| `CONFIG_NR_CPUS` | 32 | 512 | 锁队列/位图大小，不直接影响 8 核 |
| `CONFIG_HZ` | 250 | 250 | 相同 |
| `CONFIG_PREEMPT` | y | y | 相同 |
| `CONFIG_QUEUED_SPINLOCKS` | y | y | 相同 |
| `CONFIG_ARM64_LSE_ATOMICS` | y | y | 相同 |
| `CONFIG_DEBUG_SPINLOCK` | n | n | 相同 |
| `CONFIG_PROVE_LOCKING` | n | n | 相同 |
| `CONFIG_DEBUG_ATOMIC_SLEEP` | n | n | 相同 |
| `CONFIG_RCU_FANOUT` | 64 | default | 只影响 RCU 层次，不解释 sysfs 卡死 |
| `CONFIG_RCU_FANOUT_LEAF` | 16 | default | 同上 |
| `CONFIG_RCU_NOCB_CPU` | y | n | 回调时机不同 |
| `CONFIG_RCU_FAST_NO_HZ` | y | n | idle RCU 行为不同 |
| `CONFIG_SCHED_DEBUG` | y | n | 时序不同 |
| `CONFIG_SCHEDSTATS` | y | n | 时序不同 |
| `CONFIG_LTO/CFI/SHADOW_CALL_STACK` | y | n | 原厂 layout/timing 完全不同 |

**结论：并发强相关选项基本一致。LTO/CFI/SCS 和 RCU 调优会让原厂时序不同，
但不应造成 mainline 死锁。**

---

## 任务 6：原厂 sysfs/device 锁顺序

原厂 `kobject_add_internal`（`ffffffc011682444`）核心顺序：
```text
kobject_get(parent)
kobject_get(kobj->sd)
kernfs 的 rb-tree/目录锁（内联，LSE casa 出现在 0x...6824ec）
sysfs_create_dir_ns(kobj, ...)
  -> kernfs_create_dir_ns
     -> kmem_cache_alloc_trace / rb_insert
再创建 glue_dir/attrs
```

原厂 `device_add`（`ffffffc010b75d04`）顺序：
```text
kobject_get(dev)
device_private_init（必要时）
dev_set_name（如有 init_name）
kobject_get(parent)
get_device_parent()
kobject_add() -> kobject_add_internal()
software_node_notify()
platform_notify / bus_notify
device_create_file / device_add_attrs
bus_add_device()
...
```

mainline 6.12 `kobject_add_internal()` / `device_add()` 源码顺序与上述一致：
- `kobject_add_internal`：parent ref -> sysfs dir -> glue_dir -> attrs；
- `device_add`：parent/get_device_parent -> kobject_add -> notify -> attrs ->
  bus_add_device。

**结论：没有发现锁顺序差异。偶发卡死不是原厂与 mainline 的 sysfs 锁序不同。**

---

## 总结：最可能造成偶发卡死的差异

按可疑度排序：

1. **系统级 alternatives 被跳过（最可疑）**
   - 原厂在 `smp_cpus_done` 完整执行 stop_machine + `__apply_alternatives`；
   - mainline 当前只提前 patch LSE，但 `apply_alternatives_all()` 仍跳过；
   - `ARM64_WORKAROUND_REPEAT_TLBI` 99 条、`ARM64_WORKAROUND_SPECULATIVE_AT`
     24 条等 SCOPE_SYSTEM/LOCAL_CPU errata 未生效；
   - 这些恰好影响多核 TLB/屏障/同步时序。

2. **LSE patch 是否完整覆盖**
   - 锁、mutex、rwsem、atomic、bitops 都依赖 LSE alternative；
   - early patch 必须在 IRQ-off、单核阶段完成，并且 I-cache 必须按 VA 失效；
   - 当前 `t508n_apply_lse_early()` 已满足这两点，需刷机确认进度条不再回退。

3. **RCU/调度 debug 选项不同**
   - 不是死锁根因，但会改变偶发竞态窗口；必要时把 mainline RCU 选项对齐原厂
     做对照实验。

4. **LTO/CFI/SCS**
   - 原厂开了，mainline 没开。这只说明原厂时序不同，不应作为修复目标。

---

## 可操作建议

1. 保持 `t508n_apply_lse_early()` 在 `smp_prepare_boot_cpu()` IRQ-off 调用；
   保持 `caches_clean_inval_pou(_stext,_etext)`。
2. 下一步不要继续只 patch LSE，而是恢复 `smp_cpus_done` 的完整
   `__apply_alternatives(&kernel_alternatives, ...)`，但保留
   `icache_inval_all_pou()` 为原厂式 VA 失效；如果 stop_machine 仍卡，
   把 `__apply_alternatives_multi_stop` 换成原厂 `yield` 轮询（已接近）。
3. 打开 `CONFIG_ARM64_ERRATUM_2441007=y`，让 A55 repeat-TLBI 与 A76
   1286807/4193800 一起生效。
4. 先用 `CONFIG_PROVE_LOCKING=n` 保持现状，不要在这个阶段开 lockdep；
   用最小配置缩短时序窗口。
5. 若完整 alternatives 后仍偶发卡死，优先抓 `device_add` 卡在
   `kobject_add_internal` 的哪个 `casa`/`ldaxr` 循环；这比继续猜 CCI/CMN
   更有效。
