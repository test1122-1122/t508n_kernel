# T508N (UMS9620/T770) stock 内核跨 cluster 同步问题分析报告

## 0. 材料修正：work/System.map 不是原厂 5.4 符号表

`work/System.map` 与 `linux-6.12.103/System.map` 有 127,458 个同名符号，且含有
maple tree(`mas_empty_area`)、`t508n_paint_*` 等 5.4 不可能存在的符号。
它是较早一次 mainline 6.12 构建的产物，不能用于 stock_Image_full.bin。

已从 `stock_Image_full.bin` 内 kallsyms 数据恢复真实原厂符号表：
`work/stock_kallsyms.map`（116,704 个符号）。

关键地址：
| 项 | 地址/偏移 |
|---|---:|
| `_head` / `_text` | `ffffffc010080000` |
| `_stext` | `ffffffc010081000` |
| kallsyms_offsets | file `0x19de418` |
| kallsyms_relative_base | file `0x1a50398` = `ffffffc010080000` |

正确反汇编：
```bash
aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<stock_kallsyms.map 地址> --stop-address=<+n> \
  work/stock_Image_full.bin
```
文件偏移与虚拟地址关系：`file_off = vaddr - 0xffffffc010080000`。

---

## A. 原厂 device_register / sysfs 路径：没有 T770 特判

真实原厂地址：
| 符号 | 地址 |
|---|---:|
| `device_register` | `ffffffc010b75afc` |
| `device_initialize` | `ffffffc010b75b2c` |
| `device_add` | `ffffffc010b75d04` |
| `dev_set_name` | `ffffffc010b77180` |
| `kobject_add` | `ffffffc011683da4` |
| `bus_add_device` | `ffffffc010b7f58c` |
| `memory_dev_init` | `ffffffc01218ec8c` |
| `create_memory_block_devices` | `ffffffc010bc134c` |
| `init_memory_block` | `ffffffc010bc1610` |
| `devices_init` | `ffffffc01218dcd8` |
| `driver_init` | `ffffffc01218e794` |
| `of_core_init` | `ffffffc0121a00a8` |

### A.1 device_register / device_add / kobject_add

`device_register` 是标准 5.4 wrapper：
```asm
ffffffc010b75afc:  str x30,[x18],#8
ffffffc010b75b00:  stp x29,x30,[sp,#-32]!
ffffffc010b75b0c:  mov x19,x0
ffffffc010b75b10:  bl  0xffffffc010b75b2c   ; device_initialize
ffffffc010b75b18:  bl  0xffffffc010b75d04   ; device_add
```

`device_add` 入口与 5.4 mainline 形态一致：
```asm
ffffffc010b75d04:  sub sp,sp,#0x70
ffffffc010b75d08:  str x30,[x18],#8
ffffffc010b75d0c:  stp x29,x30,[sp,#32]
...
ffffffc010b75d38:  bl  0xffffffc011682c54   ; get_device/kobject_get
ffffffc010b75d58:  bl  0xffffffc010b77180   ; dev_set_name
ffffffc010b75d88:  bl  0xffffffc010b771f8   ; get_device_parent
ffffffc010b75e64:  bl  0xffffffc011683da4   ; kobject_add
ffffffc010b75e84:  bl  0xffffffc010b9586c   ; device_create_file/...
```

`kobject_add` 入口也是标准实现：
```asm
ffffffc011683da4:  sub sp,sp,#0xa0
ffffffc011683da8:  str x30,[x18],#8
ffffffc011683dac:  stp x29,x30,[sp,#112]
...
ffffffc011683e24:  bl  0xffffffc01168329c   ; kobject_add_varg/internal
```

### A.2 memory_dev_init

原厂 5.4 路径为：
```text
memory_block_size_bytes() -> subsys_register(memory_subsys)
  -> mutex_lock -> init_memory_block() -> mutex_unlock
```
反汇编关键调用：
```asm
ffffffc01218ecb4:  bl  0xffffffc010bc0e4c   ; memory_block_size_bytes
ffffffc01218ed20:  bl  0xffffffc010b7d1bc   ; subsys_register
ffffffc01218ed34:  bl  0xffffffc0116a336c   ; mutex_lock
ffffffc01218edb8:  bl  0xffffffc010bc1610   ; init_memory_block
ffffffc01218edf4:  bl  0xffffffc0116a2548   ; mutex_unlock
```
注意：5.4 原厂没有 mainline 的 `add_memory_block/__add_memory_block` 符号；
mainline 6.12 才把该流程拆成 `add_memory_block -> device_register`。

### A.3 barrier/cache/errata 扫描结果

对上述所有函数逐条扫描：
- 无 `dsb`、`dmb`、`isb`、`sb`；
- 无 `dc` / `ic` / `tlbi`；
- 只有普通 `nop` 和错误路径 `brk #0x800`；
- 未发现 errata alternative 插入痕迹。

mainline 6.12 同名函数同样没有 barrier/cache 指令。

**结论 A：原厂没有对 device model / sysfs 初始化做跨 cluster 特殊处理。
mainline 卡在 memory_dev_init/device_register 不是“原厂加料”的差异。**

### A.4 原厂 I-cache 维护的准确情况

原厂普通 cache flush 路径确实只按 VA：

`__flush_icache_range = ffffffc010086e40`：
```asm
ffffffc010086e94:  dc cvau, x4
ffffffc010086ea4:  dsb ish
ffffffc010086ecc:  ic ivau, x3
ffffffc010086edc:  dsb ish
ffffffc010086ee0:  isb
```
`invalidate_icache_range = ffffffc010086f20`：
```asm
ffffffc010086f78:  ic ivau, x3
ffffffc010086f88:  dsb ish
ffffffc010086f8c:  isb
```
没有 `ic iallu/ialluis`。

**但要修正一点**：原厂 `__apply_alternatives = ffffffc010114cd8` 的末尾有一个
条件执行的 `ic ialluis`：
```asm
ffffffc010114e94:  dsb ish
ffffffc010114e98:  ...            ; runtime capability 判断
ffffffc010114ea4:  ic ialluis
ffffffc010114ea8:  dsb ish
ffffffc010114eac:  isb
```
它由 `cpu_hwcaps` 的一个 runtime bit（bit 28，在 `ffffffc01250c600`）决定是否跳过。
因此“原厂从不用 ialluis”应精确表述为：
**普通 cache 维护路径不用 ialluis；alternatives 收尾有一个受 capability 门控的 ialluis。**
用户当前 mainline 已把 `icache_inval_all_pou()` 改成 `dsb(ish)`，方向正确。

---

## B. config errata 差异

T770 的 CPU 组合：**4×Cortex-A55 + 3×Cortex-A76 + 1×Cortex-A76**（DTS）。

### B.1 与 T770 直接相关且原厂没有、mainline 有的项

| mainline 配置 | errata / CPU | 作用 |
|---|---|---|
| `CONFIG_ARM64_ERRATUM_1530923=y` | A55 1530923 | speculative AT 后 TLBI 错误，加 ISB |
| `CONFIG_ARM64_ERRATUM_3194386=y` | **A76 3324349** | MSR SSBS 不自同步，加 SB/ISB |
| `CONFIG_ARM64_ERRATUM_4118414=y` | **A76 4193800** | TLBI 完成不保证访存完成，repeat-TLBI |

这些 mainline 已经打开，**不应关闭**。3194386/4118414 是 5.4 之后才合入的
A76 workaround，所以原厂 config 没有是正常的。

### B.2 建议额外打开

```kconfig
CONFIG_ARM64_ERRATUM_2441007=y
```
A55 2441007 也 select `ARM64_WORKAROUND_REPEAT_TLBI`，与 A76 1286807/4193800
是同一类“TLBI 完成不可靠”问题。mainline 当前是 `# ... is not set`。

### B.3 原厂有、mainline 没有

- `CONFIG_ARM64_ERRATUM_834220=y`：Cortex-A57 errata，T770 无关，可忽略。
- `CONFIG_ARM64_UAO / SSBD / SW_TTBR0_PAN`：5.4 feature 开关差异，非根因。

### B.4 两个内核都开的关键项

`CONFIG_ARM64_ERRATUM_1024718/1165522/1286807/1418040/1463225`，
`CONFIG_ARM64_LSE_ATOMICS=y`，`CONFIG_ARM64_WORKAROUND_REPEAT_TLBI/CLEAN_CACHE=y`。

---

## C. CCI/CMN 互联初始化

### C.1 DTS/DTBO

在 `stock_ums9620.dts` 与全部 `dtbo0~6.dtb`、`vendor_boot.dtb` 中：
- 无 `arm,cci-400`、`arm,ccn`、`arm,cmn`、`arm,dsu-pmu`；
- 无 DSU/CCI/CMN/ACE/snoop 控制节点；
- CPU 节点为 `arm,cortex-a55` / `arm,cortex-a76`，`enable-method="psci"`，无 `next-level-cache`；
- 唯一与 coherency 相关属性是 GPU 节点：
  `gpu@23140000 { system-coherency = <0x1f>; ... }`。

### C.2 原厂内核符号

真实原厂 kallsyms 中没有 `cci_*`、`ccn_*`、`cmn_*`、`mc_send_command` 等
一致性互联初始化符号。DMC 相关只有：
- `sprd_dmc_probe` / `sprd_get_ddr_cur_freq` / `sprd_ddr_size_show`
  这些是 DDR DVFS/debug 驱动，不是 cache 一致性互联配置。

### C.3 SML / SPL / U-Boot / TrustOS

- `sml_a.bin` 是 BL31/TF-A，字符串主要是：
  `Init the Qogir N6 Pro power control and dvfs logic`、cluster DVFS、
  `mpll/scu index reg init` 等调压调频逻辑；
- 没有 CCI/CMN/DSU/ACE/snoop 初始化字符串或明显寄存器配置痕迹。

### C.4 结论 C

**原厂内核和 DTS 都没有做 CCI/CMN 初始化；这不是 mainline 漏掉的驱动。
T770 的一致性互联由 boot 阶段既有状态（或 SoC 内部 DynamIQ/DSU）维持，mainline
无需、也不应尝试去初始化一个不存在的 CCI/CMN 节点。**
“SML 没配好”从现有材料看不成立：SML 主要管电源/DVFS，不碰一致性互联。

---

## D. mainline 6.12 kernel_alternatives / __alt_instructions

### D.1 表结构

- `__alt_instructions`: `ffffffc081ce74dc` .. `ffffffc081d40960`
- 大小 `0x59484`，条目 **30,475**
- 每项 12B：`{s32 orig_offset; s32 alt_offset; u16 cpucap; u8 orig_len; u8 alt_len}`

### D.2 errata/workaround 相关条目统计

| cpucap | 条目数 | 对 T770 |
|---|---:|---|
| `ARM64_HAS_LSE_ATOMICS` | **12,074** | **是，原子指令 patch** |
| `ARM64_WORKAROUND_DEVICE_LOAD_ACQUIRE` | 13,178 | 否（A57） |
| `ARM64_WORKAROUND_REPEAT_TLBI` | **99** | **是（A76 1286807/4193800）** |
| `ARM64_WORKAROUND_QCOM_FALKOR_E1003` | 65 | 否 |
| `ARM64_WORKAROUND_1508412` | 32 | 否（A77） |
| `ARM64_WORKAROUND_CAVIUM_TX2_219_PRFM` | 32 | 否 |
| `ARM64_WORKAROUND_SPECULATIVE_AT` | **24** | **是（A55 1530923 / A76 1165522）** |
| `ARM64_HAS_CACHE_DIC/IDC` | 13 | feature |
| `ARM64_WORKAROUND_CLEAN_CACHE` | 5 | 否（A53） |
| 其他 Cavium/Ampere/Nvidia/TSB 等 | 少量 | 否 |

`ARM64_WORKAROUND_SPECULATIVE_SSBS` 没有 alternative 条目；它是
`cpufeature.c` 里的 runtime branch，不受“跳过 alternatives”影响。

### D.3 关键条目内容

LSE atomics 例子（`enable_debug_monitors+0x50`）：
```asm
; orig: LL/SC
ffffffc08001410c:  ldxr w21,[x1]
ffffffc080014110:  add  w21,w21,w0
ffffffc080014114:  stxr w4,w21,[x1]
ffffffc080014118:  cbnz w4,...
; repl: LSE
ffffffc080014784:  ldadd w0,w21,[x1]
```

REPEAT_TLBI 例子：
```asm
ffffffc080025bb4:  nop            ; 未 patch 时什么都不做
ffffffc080025bfc:  b   repeat_tlbi_sequence
```
SPECULATIVE_AT 例子：
```asm
ffffffc08005f464:  nop            ; 未 patch 时无 ISB
ffffffc08005fd48:  isb
```

### D.4 跳过 apply_alternatives_all 的直接后果

当前 `arch/arm64/kernel/alternative.c:286 apply_alternatives_all()` 被 T508N 调试
代码改成跳过 `__apply_alternatives(&kernel_alternatives, ...)`。

关键机制：
- `apply_boot_alternatives()` 只 patch `boot_cpucaps`；
- `ARM64_HAS_LSE_ATOMICS` 是 `ARM64_CPUCAP_SYSTEM_FEATURE`（`SCOPE_SYSTEM`），**不在 boot_cpucaps**；
- `ARM64_WORKAROUND_REPEAT_TLBI`、`ARM64_WORKAROUND_SPECULATIVE_AT` 是
  `ARM64_CPUCAP_LOCAL_CPU_ERRATUM`（`SCOPE_LOCAL_CPU`），**也不在 boot_cpucaps**。

因此跳过第二次 patch 后：
1. **12,074 个 LSE atomics alternative 全部未生效**，内核实际运行 LL/SC
   （`ldxr/stxr`），而原厂 5.4 正常启动时会 patch 成 LSE（`ldadd/cas/swp`）。
2. **99 个 repeat-TLBI 和 24 个 speculative-AT workaround 未生效**；
   对 T770 意味着 A76 TLBI 完成序问题、A55/A76 speculative AT 问题没有
   内核侧缓解。
3. `ARM64_HAS_LSE_ATOMICS` 的 capability 在 `setup_system_capabilities()` 中
   已被置位，但置位本身不会 patch 代码；代码路径继续走 LL/SC。

这与“原厂能跑、mainline 间歇卡死”的现象最吻合：
**原厂 8 核跑 LSE 单副本原子指令；当前 mainline 因跳过 alternatives 跑的是
LL/SC exclusive 序列。**

---

## 最终结论

1. **不是 device model 特判缺失**。原厂 `device_register/device_add/kobject_add/
   memory_dev_init` 与 mainline 一样，没有额外 barrier/errata。
2. **不是 CCI/CMN 未初始化**。原厂内核/DTS/DTBO 没有 CCI/CMN 节点，SML 不负责
   一致性互联配置；不要往 DTS 里加不存在的互联节点。
3. **最可能根因：mainline 第二次 alternatives patch 被整体跳过。**
   - LSE atomics 12,074 个 patch 点未生效，整个内核从原厂 LSE 模式退化为
     LL/SC 模式；
   - A76/A55 的 TLBI 和 speculative-AT workaround 未生效；
   - 这足以解释间歇性、随核数/时序漂移的卡死。
4. **`ic ialluis` 广播 hang 是真实限制，但不是唯一根因。**
   原厂普通路径只用 `ic ivau`；mainline 已改为 `dsb(ish)`，这个 patch 应保留，
   但不能因此把整个 alternatives patch 关掉。

---

## 可操作的下一步

1. **不要把整个 `apply_alternatives_all` 跳过。** 最小实验：恢复
   `__apply_alternatives(&kernel_alternatives, false, remaining_capabilities)`，
   同时保留 `icache_inval_all_pou() = dsb(ish)`。
2. **先只 patch LSE atomics。** 在 `setup_boot_cpu_capabilities()` 之后、SMP 上线前，
   加一个实验函数：
   ```c
   if (read_cpuid(ID_AA64ISAR0_EL1) 的 ATOMIC 字段 != 0)
       apply_selected_early_alternatives(BIT(ARM64_HAS_LSE_ATOMICS));
   ```
   因为所有 T770 CPU 都支持 LSE，boot CPU 判断即可；SMP 未上线，全局 patch 安全。
   如果这样 patch 后 `stop_machine/device_register` 不卡了，根因就坐实。
3. **恢复 local-errata patch。** 对于 repeat-TLBI / speculative-AT，若 stop_machine
   仍不可用，可在 bring-up 阶段按 SoC compatible 强制置位这些 cap 并在 boot
   stage patch；正式修法仍是修复 stop_machine 后走完整 `apply_alternatives_all`。
4. **打开 `CONFIG_ARM64_ERRATUM_2441007=y`**，与 A76 repeat-TLBI 一起测试。
5. **保持现有 I-cache 策略**：普通 cache 维护只用 `ic ivau`；如果恢复完整
   alternatives，参考原厂 `__apply_alternatives`：
   `dc civac` 逐行 clean -> `dsb ish` -> capability 门控的 `ic ialluis`/跳过。
   不要无条件 `ic ialluis`。
6. **不要添加 CCI/CMN/DSU 初始化代码或 DTS 节点**，现有材料不支持该方向。
