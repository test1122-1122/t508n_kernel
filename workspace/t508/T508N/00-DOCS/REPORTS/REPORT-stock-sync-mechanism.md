# T508N：原厂 stop_machine / alternatives 同步机制分析报告

目标：回答“CPU0 置位 all_alternatives_applied 后，部分二级核为何读不到”，
并从原厂实现找到可移植方案。

---

## 任务 1：原厂 `__apply_alternatives_multi_stop` 同步机制（核心）

### 符号地址

| 符号 | 原厂地址 |
|---|---:|
| `__apply_alternatives_multi_stop` | `ffffffc010114c0c` |
| `__apply_alternatives` | `ffffffc010114cd8` |
| `apply_alternatives_all` | `ffffffc01214e354` |
| `all_alternatives_applied` | `ffffffc01250ca18` |
| `stop_machine` | `ffffffc01030a2f0` |

### 原厂二级核等待循环

```asm
; cpu = smp_processor_id()
ffffffc010114c40: mrs x8, tpidr_el1
ffffffc010114c48: ldr w8,[x8,x9]        ; cpu id
ffffffc010114c4c: cbz w8, 0x...14c70    ; cpu0 走 patch 路径

; 二级核路径:
ffffffc010114c50: adrp x8, 0xffffffc01250c000
ffffffc010114c54: ldr w9,[x8,#2584]     ; all_alternatives_applied
ffffffc010114c58: cbnz w9, 0x...14c68
ffffffc010114c5c: yield                  ; 注意：不是 wfe，也没有 dc/dsb
ffffffc010114c60: ldr w9,[x8,#2584]
ffffffc010114c64: cbz w9, 0x...14c5c
ffffffc010114c68: isb
```

**结论：原厂二级核循环是纯 `yield` 轮询普通 `ldr`。**
- 没有 `dcache_inval_poc`；
- 没有 `dmb/dsb`；
- 没有 `wfe/sev`；
- 没有超时上限。

### 原厂 CPU0 置位路径

```asm
; CPU0:
ffffffc010114c8c: add x0, sp, #0
ffffffc010114c90: add x2, sp, #16
ffffffc010114c94: mov w1, wzr
ffffffc010114c98: bl  0xffffffc010114cd8  ; __apply_alternatives()
ffffffc010114c9c: mov w8, #1
ffffffc010114ca0: str w8,[x19,#2584]     ; 普通 str，无显式 dcache clean
```

CPU0 唯一屏障来自 `__apply_alternatives` 尾部：
```asm
ffffffc010114e94: dsb ish
; runtime 判断是否 ic ialluis
ffffffc010114ea4: ic ialluis  (条件执行)
ffffffc010114ea8: dsb ish
ffffffc010114eac: isb
```
之后就是普通 `str all_alternatives_applied=1`，**没有 `dcache_clean_poc`、
没有 `sev`**。

### mainline 当前实现

mainline `__apply_alternatives_multi_stop`（`ffffffc081c55458`）当前已经被改造成：

```asm
; 二级核:
ffffffc081c55498: ldr w0,[x20,#16]!   ; flag
ffffffc081c5549c: cbnz w0, done
ffffffc081c554a0: yield
ffffffc081c554a4: mov x0,x20          ; flag addr
ffffffc081c554a8: mov x1,x21          ; flag addr+4
ffffffc081c554ac: bl  __pi_dcache_inval_poc
ffffffc081c554b0: ldr w0,[x19,#16]
ffffffc081c554b4: cbz w0, loop
...
ffffffc081c554cc: isb

; CPU0:
ffffffc081c5555c: str w2,[x0,#16]!    ; flag=1
ffffffc081c55560: bl  __pi_dcache_clean_poc
ffffffc081c55564: dsb ishst
```

### 差异结论

| 机制 | 原厂 | mainline 当前 |
|---|---|---|
| 二级核等待 | `yield` + 普通 `ldr` | `yield` + 每轮 `dcache_inval_poc` |
| CPU0 置位后 | 无显式 dcache clean | `dcache_clean_poc` + `dsb(ishst)` |
| 唤醒 | 无 sev/IPI | 无 sev/IPI |
| I-cache | 条件 `ic ialluis` | VA 失效方案 |

**原厂方案比我们更“裸”，却能工作。这证明 T770 的 PoC 一致性本身可用，
不需要逐轮 invalidate。** 如果 mainline 加了 clean/inval 仍只有一半 CPU 亮，
问题更可能不在 flag 的 cache 可见性，而在这些 CPU 是否真的进入了该循环、
或循环读取的地址/变量不一致。

---

## 任务 2：原厂 stop_machine 完成同步

### 原厂 `multi_cpu_stop` / `stop_cpus`

原厂 `multi_cpu_stop`（`ffffffc01030a420`）的完成计数：

```asm
; 完成时:
ffffffc01030a564: ldaddal w8, w9, [x24]  ; LSE 原子完成计数
ffffffc01030a57c: dmb ishst

; 非 LSE 路径:
ffffffc01030a5f4: ldxr w8,[x24]
ffffffc01030a5fc: stlxr w9,w8,[x24]
ffffffc01030a604: dmb ish
```

原厂 `stop_cpus`（`ffffffc01030a60c`）使用：
```asm
ffffffc01030a660: casa x8,x2,[x0]        ; LSE CAS 抢 stop_cpus_mutex
ffffffc01030a724: casl x8,x2,[x0]
; LL/SC 退路:
ffffffc01030a7b0: ldaxr x0,[x9]
ffffffc01030a7bc: stxr w10,x19,[x9]
```

mainline `multi_cpu_stop`（`ffffffc08016ccec`）与之一致：
```asm
ffffffc08016cd84: ldaddal w25,w1,[x21]
ffffffc08016cda0: dmb ishst
...
ffffffc08016ce00: ldxr w1,[x21]
ffffffc08016ce08: stlxr w0,w1,[x21]
ffffffc08016ce10: dmb ish
```

**结论：stop_machine 完成同步机制同源。**
- 都用 `ldaddal` / `ldxr-stlxr` 完成计数；
- 都依赖普通 cacheable 内存的 PoC 一致性；
- 原厂在 stop_machine 前 LSE system alternative 尚未 patch，所以实际走
  LL/SC 路径的可能性大；mainline 因提前 patch LSE，可能走 `ldaddal`。
  但两者都是架构正确实现。

---

## 任务 3：原厂对跨 cluster 一致性的处理

### 3.1 一致性互联符号

在 `work/stock_kallsyms.map` 中搜索：

| 类别 | 结果 |
|---|---|
| `cci_*` | 无（只有 crc_ccitt / UCSI CCI 等无关符号） |
| `ccn_*` | 无 |
| `cmn_*` | 无（只有 SCSI cmnd 等无关符号） |
| `dsu_*` | 无 |
| `scu_*` | 无（只有 SCSI scuttle 等无关符号） |
| `snoop_*` | 只有 USB snoop 调试，无互联 snoop |
| `ace/dvm` | 无互联控制符号 |

**原厂内核没有 CCI/CCN/CMN/DSU 初始化。** 一致性互联完全由 boot ROM /
SML（BL31）/ 硬件默认状态维护。

### 3.2 cache 维护函数

原厂核心 cache 维护全部是标准 ARMv8 序列，无 T770 特判：

`__clean_dcache_area_poc`（`ffffffc01008709c`）：
```asm
loop:
    dc cvac, x0
    ...
dsb sy
ret
```

`__flush_dcache_area`（`ffffffc010086fcc`）：
```asm
loop:
    dc civac, x0
    ...
dsb sy
ret
```

`__clean_dcache_area_pou`（`ffffffc010087004`）：
```asm
loop:
    dc cvau, x0
    ...
dsb ish
ret
```

`__flush_icache_range`（`ffffffc010086e40`）：
```asm
loop: dc cvau
dsb ish
loop: ic ivau
dsb ish
isb
```

**没有发现“对 A55 cluster 额外 clean”的 workaround。**

### 3.3 PSCI / cluster 电源

原厂存在标准 PSCI 接口：
```text
psci_cpu_on           ffffffc010f5ca3c
psci_cpu_off          ffffffc010f5c990
psci_cpu_suspend      ffffffc010f5c794
cpu_cluster_pm_enter  ffffffc01038013c
cpu_cluster_pm_exit   ffffffc010380218
```
这是 mainline 同款 CPU 电源管理，没有额外 cluster 一致性配置。

---

## 任务 4：SML（BL31）一致性字符串

对 `payloads/sml_a.bin` 的 `strings` 搜索：

- `snoop / coherency / DSU / CCI / CMN / ACE / DVM`：**无命中**；
- `SCU`：只有 `scu index reg init`（MPLL/SCU 调压索引寄存器，不是 Snoop
  Control Unit）；
- `cluster`：全部是 DVFS 报错字符串（cluster voltage/freq/index 等）；
- `cache`：无 cluster cache 初始化字符串。

**结论：SML 是 BL31/TF-A，负责 Qogir N6 Pro 电源与 DVFS，不初始化 CCI/CMN/DSU
一致性。T770 的 cache 一致性依赖硬件 DSU/互联默认状态，原厂内核也没有额外
配置。**

---

## 任务 5：当前方案可行性评估

### 5.1 `clean_poc + dsb` + 二级核 `inval_poc` 能否解决？

- 从 ARM 架构看：如果 flag 在 normal cacheable inner-shareable 内存，且所有
  CPU 在同一 PoC 域，`dcache_clean_poc` + 二级核 `dcache_inval_poc` 是**充分
  且略过度**的同步。
- 但原厂证明：**普通 `str` + `yield` 轮询已经足够**。因此如果 mainline 加了
  clean/inval 仍失败，不应继续怀疑 cache 指令不够，而应检查：
  1. 二级核是否真的进入 `__apply_alternatives_multi_stop`；
  2. 读取的 `all_alternatives_applied` 地址是否与 CPU0 写入地址完全一致；
  3. `stop_machine` 的 IPI/线程是否已经把任务派发给这些 CPU。

### 5.2 原厂更优雅的机制？

原厂没有 wfe/sev，也没有 IPI，就是：
```text
secondary: while (!all_alternatives_applied) cpu_relax(); isb();
cpu0:      __apply_alternatives(); all_alternatives_applied=1;
```
这依赖：
- stop_machine 状态机先把所有 CPU 拉到同一个 stop 状态；
- 普通 cacheable 内存的 PoC 一致性；
- `yield` 轮询避免 WFE 漏唤醒。

**这是最可移植、最符合 T770 原厂行为的方案。**

### 5.3 PoC 一致性是否硬件不可靠？

现有证据不支持“PoC 硬件坏”：
- 原厂 stop_machine / alternatives 使用普通 `str/ldr` 和原子完成计数；
- 原厂锁、调度、RCU、sysfs 大量跨 cluster 通信都能跑；
- 原厂 kernel 没有 CCI/CMN/DSU 特判。

因此更可能的是 mainline 在 stop_machine 调度/IPI 或 alternatives patch 本身上
存在状态机差异，而不是 cache 硬件。

---

## 推荐的可移植方案（具体到函数/指令级）

### 首选：完全照抄原厂 multi_stop 协议

`arch/arm64/kernel/alternative.c`：

```c
static int __init __apply_alternatives_multi_stop(void *unused)
{
    if (smp_processor_id()) {
        while (!READ_ONCE(all_alternatives_applied))
            cpu_relax();      /* arm64 上就是 yield */
        isb();
    } else {
        DECLARE_BITMAP(remaining_capabilities, ARM64_NCAPS);

        bitmap_complement(remaining_capabilities, boot_cpucaps,
                          ARM64_NCAPS);
        BUG_ON(all_alternatives_applied);
        __apply_alternatives(&kernel_alternatives, false,
                             remaining_capabilities);

        /* 原厂顺序：先 dsb(ish)，再普通写 flag */
        dsb(ish);
        WRITE_ONCE(all_alternatives_applied, 1);
    }
    return 0;
}
```

- 不要每轮 `dcache_inval_poc`；
- CPU0 可以保留一次 `dcache_clean_poc` 作为保险，但不要依赖它；
- 不要用 `wfe/sev`，用 `yield`。

### 次选：若仍怀疑 flag 可见性，用原子而不是普通 int

把 `all_alternatives_applied` 从 `int` 改为 `atomic_t`，CPU0：
```c
smp_store_release(&all_alternatives_applied, 1);
```
二级核：
```c
while (!smp_load_acquire(&all_alternatives_applied))
    cpu_relax();
```
这比 dcache clean/inval 更标准，也更容易证明可见性边界。

### 不建议

- 在二级核循环里每轮 `dcache_inval_poc`：浪费且掩盖真正问题；
- 试图实现“每个 CPU 各自执行 __apply_alternatives”：patch 同一代码时会有
  重复写、I-cache 和替代序列一致性问题，原厂没有这样做；
- 继续怀疑 CCI/CMN 未初始化：材料不支持。

---

## 还需要哪些分析

1. 在线探针确认所有 8 核都进入了 `__apply_alternatives_multi_stop`，还是只有
   A55/A76 其中一组进入；
2. 若只有一半进入：查 `stop_machine` / `queue_stop_cpus_work` 的 IPI 与
   `cpu_stopper_thread` 调度，不是 alternatives；
3. 若都进入但仍读不到 flag：打印每个 CPU 读到的 `&all_alternatives_applied`
   地址和值，排除不同 image/alias 地址问题；
4. 用 `atomic_t + smp_store_release/load_acquire` 做一次对照测试；
5. 恢复完整 `apply_alternatives_all` 后，再观察 repeat-TLBI / speculative-AT
   是否消除偶发 sysfs/device model 卡死。

