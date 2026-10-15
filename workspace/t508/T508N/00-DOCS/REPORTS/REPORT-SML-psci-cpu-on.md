# REPORT-SML-psci-cpu-on.md — SML EL3 固件 PSCI CPU_ON 路径分析

> 目标：判定 T508N（Unisoc T770 / UMS9620）的 EL3 安全固件 SML 在 PSCI CPU_ON
> （0xc4000003）释放二级核后，**是否等待二级核的启动握手/确认**。
> 结论先行：**存在握手机制（每核状态机 flag），且 mainline 的 SMC64 CPU_ON 确实进入 SML 的
> PSCI 分发路径；但受限于固件文件不完整（分发查表表缺失），无法 100% 定位到 CPU_ON 处理器
> 内部的确切轮询等待循环，因此对"是否等待握手"的最终判定为「大概率是 / 无法完全确认」**。
> 下面给出全部证据与卡点。

---

## 1. SML 载荷格式 / 入口 / 基础信息

| 项目 | 值 | 说明 |
|---|---|---|
| 文件 | `payloads/sml_a.bin` | DHTB 剥离后的载荷 |
| `file` | `data`（raw binary，无符号表） | 非 ELF，需 `-b binary` 反汇编 |
| 文件大小 | 140528 字节 = 0x224F0 | 报告称 SML 区 256KB（0xbd000000–0xbd040000），文件仅含代码段 |
| 镜像头 | 0x14 = 0x224e8（=size−8），0x18 = 0x22b88（=size+0x698） | 紫光镜像头（前 16 字节全 0） |
| 代码起点 | **0xbd0001c8**（文件偏移 0x1c8） | 首个有效指令：`adrp x20,0xbd01b000; add x20,x20,#0x828; str x0,[x20]...` |
| 代码区 | 0xbd0001c8 – 0xbd0224e8 | ~137KB 代码 |
| VBAR_EL3 | **0xbd00e9c8**（0xbd000218 处 `msr vbar_el3,x0`） | 异常向量表 |
| 版本字符串 | `v1.4, qogirn6pro, bddeffbc(release)` | 确认是 QogirN6Pro 的 SML |
| 定位 | `NOTICE: Init the Qogir N6 Pro power control and dvfs logic` | SML = EL3 电源/DVFS 监控 |
| 关键字符串 | `SPRD System Reset/Off: operation not handled`、`Get core power state error`、`Power level error`、`The core id is error, id=%u` | 说明 SML 实现了 PSCI 子集 |

反汇编命令：
```
/usr/bin/aarch64-linux-gnu-objdump -b binary -m aarch64 --adjust-vma=0xbd000000 -D payloads/sml_a.bin
```

---

## 2. SMC 分发 —— **mainline 的 SMC64 CPU_ON 确实进入 SML 的 PSCI 分发**

### 2.1 异常向量表（VBAR_EL3 = 0xbd00e9c8）

向量表偏移 +0x400 是「低 EL（AArch64）同步异常」入口，即内核（EL1 AArch64）`smc` 后陷阱到 EL3 的位置：

```
bd00edc8:  msr    daifclr, #0x4
bd00edcc:  str    x30, [sp, #240]
bd00edd0:  mrs    x30, esr_el3
bd00edd4:  ubfx   x30, x30, #26, #6       ; EC = ESR_EL3[31:26]
bd00edd8:  cmp    x30, #0x13              ; EC 0x13 = SMC32 (来自 AArch32)
bd00eddc:  b.eq   0xbd00d4e4              ; SMC32 -> 0xbd00d4e4 (带 bit30 检查)
bd00ede0:  cmp    x30, #0x17              ; EC 0x17 = SMC64 (来自 AArch64)
bd00ede4:  b.eq   0xbd00d4fc              ; SMC64 -> 0xbd00d4fc (直进分发,无 bit30 检查)
bd00ede8:  b      0xbd00d604              ; 其他 -> 通用异常
```

**重要纠错（相对常见误读）**：arm64 内核 `esr.h` 明确：
- `ESR_ELx_EC_SMC32 = 0x13`（SMC 来自 AArch32）
- `ESR_ELx_EC_SMC64 = 0x17`（SMC 来自 AArch64）

因此 mainline（AArch64）`smc #0xc4000003` 产生的 EC = **0x17**，走 `0xbd00d4fc` **直接进入分发**，
**不会**被 `0xbd00d4e4` 的 `tbnz w0,#30`（SMC64 拒绝）拦截。这一点至关重要。

### 2.2 分发逻辑（0xbd00d4fc → 0xbd00d574）

```
bd00d4fc:  stp x4,x5,[sp,#32] ... (保存寄存器)
bd00d52c:  ubfx x16,x0,#24,#6        ; x16 = func_id[29:24]
bd00d530:  ubfx x15,x0,#31,#1        ; x15 = bit31
bd00d534:  orr  x16,x16,x15,lsl #6   ; index = func_id[29:24] | (bit31<<6)
bd00d538:  adr  x11, 0xbd013528      ; 函数指针表（32 字节步长）
bd00d53c:  adr  x14, 0xbd026f18      ; 查表表（字节表）
bd00d540:  ldrb w15,[x14,x16]        ; w15 = lookup[index]
bd00d548:  tbnz w15,#7,0xbd00d57c    ; 若 bit7 置位 -> 返回 -1
bd00d54c:  lsl  w10,w15,#5
bd00d550:  ldr  x15,[x11,w10,uxtw]   ; handler = func_table[w15*32]
...
bd00d574:  blr  x15                  ; 调用 handler
```

对 `0xC4000003`（SMC64 CPU_ON）：
- index = `(0xC4000003>>24)&0x3F | (1<<6)` = `0x04 | 0x40` = **0x44**。
- 与 `0x84000003`（SMC32 CPU_ON）映射到**同一个 index 0x44**。

### 2.3 mainline 内核确认使用 SMC64（0xC4000003）

`linux-6.12.103/vmlinux`（有符号）的 `psci_0_2_cpu_on` 反汇编：

```
ffffffc080d5c348:  mov  x0, #0x3
ffffffc080d5c350:  movk x0, #0xc400, lsl #16      ; x0 = 0xC4000003
ffffffc080d5c354:  ...
ffffffc080d5c35c:  blr  x4                        ; -> __arm_smccc_smc（即 SMC 指令）
```

且 `ums9620.dtsi` 的 psci 节点 `compatible="arm,psci-0.2"; method="smc"`。
**结论：mainline 用 `smc #0xc4000003`，该调用会被 SML 分发（EC 0x17），不是被拒绝。**

---

## 3. CPU_ON 处理器定位 —— 卡点：分发查表表不在文件中

SML 的 SMC 分发依赖两级表：
- **查表表** `lookup[0xbd026f18 + index]`（字节表，index 0–127）。
- **函数指针表** `func_table[0xbd013528 + w15*32]`（32 字节步长）。

对 index 0x44，查表表位置为 `0xbd026f18 + 0x44 = 0xbd026f5c`。
**该地址超出文件**：文件仅 0x224F0 字节，而 0x26f5c 对应文件偏移 0x26f5c，远超文件尾。
SML 实际映射到 0xbd000000–0xbd040000（256KB）区，但 `sml_a.bin` 只捕获了 ~137KB 代码段，
**数据表（查表表、部分 rodata）落在文件未捕获区域**，因此无法解析 `w15`，进而无法直接
定位到 index 0x44 对应的 CPU_ON 处理器。

佐证：`func_table`（0xbd013528）的首批条目为 `0x00000000 / 0x00013c32 / 0x00013d3d / 0x00010202 / 0x410fd0f0...`
其中 `0x410fd0f0`（= Cortex-A55 MIDR 类值）、`0x00010202`（位于 rodata 字符串区）**并非代码指针**，
说明该区域混有 CPU 信息/数据，不是纯函数指针表，进一步印证分发表与数据区交织、无法直接反查。

---

## 4. 握手机制证据 —— SML 存在"每核状态 flag"状态机

虽然无法解析 CPU_ON 处理器内部轮询，但 SML 中存在明确的**每核状态机**，是握手的基础：

### 4.1 二级核启动代码写入 flag

二级核被上电后进入 SML 的启动代码（0xbd007e84，读自己的 MPIDR、初始化 EL2）：

```
bd007f44:  mrs  x0, tpidr_el3
bd007f48:  mov  w1, #0x1
bd007f4c:  str  w1, [x0, #376]        ; tpidr_el3+376 = 1  (二级核标记"正在启动")
```

### 4.2 每核状态 flag 的读取/校验

`0xbd007fa0` 附近（读 `tpidr_el3` 基址 + 376）：

```
bd007fcc:  ldr  w0, [x0, #376]        ; 读每核状态
bd007fd0:  cbz  w0, 0xbd007fe4        ; ==0 -> w20=0xfffffffc (-4)   (未上电)
bd007fd4:  cmp  w0, #0x2
bd007fd8:  b.ne 0xbd007fec            ; !=2 -> (==1) 继续
bd007fdc:  mov  w20, #0xfffffffb      ; ==2 -> w20=0xfffffffb (-5)   (已运行)
```

即每核状态：**0 = 关 / 1 = 启动中 / 2 = 运行**。二级核启动时置 1，随后（0xbd007f4c 流程）
转 2。这是典型的**上电握手状态机**。

### 4.3 相关字符串映射到 PSCI 处理器

- `Get core power state error`（0xbd0102df）
- `The core id is error, id = %u`（0xbd0117ed）
- `Power level error`（0xbd010231）/ `Get power state error`（0xbd010258）
- `SPRD System Reset/Off: operation not handled`

这些证明 SML 有专门的 PSCI 电源状态/核 ID 校验处理器，而非只做 DVFS。

---

## 5. 附带发现：TrustOS 也有一层"bootstrap completion"握手

`payloads/trustos_a.bin`（3MB，raw，Trusty EL1 安全 OS）字符串：
- `creating bootstrap completion thread for cpu %d`
- `bootstrap2` / `secondarybootstrap2`
- `Invalid secondary cpu num %d, SMP_MAX_CPUS %d, secondary_bootstrap_thread_count %d`
- `fail to get power state 0x%x`
- `sm_secondary_init` / `thread_secondary_cpu_init_early`

说明**二级核启动在 TrustOS 侧也有一层"引导完成线程"（bootstrap completion）握手**。
因此该平台二级核上电后可能先进入 EL3（SML）再进 EL1 secure（TrustOS）建立 bootstrap 线程，
最后才交接给非安全内核 `secondary_start_kernel`。这是「握手」的第二个可能层次。

> 注：TrustOS 全文反汇编时 objdump 报错（core dumped，可能因文件过大/格式），未完成 TrustOS
> 的完整 SMC 分发定位；以上仅基于字符串证据。

---

## 6. 结论

**核心问题：SML 的 PSCI CPU_ON 是否在释放二级核后等待二级核的启动握手/确认？**

### 明确的判定（能确定的）：
1. **mainline 的 SMC64 CPU_ON（0xc4000003）确实进入 SML 的 PSCI 分发**（EC 0x17 → 0xbd00d4fc → 分发），
   不会被 SML 以 SMC64 拒绝。这推翻了"mainline 因 SMC64 被 SML 拒绝而失败"的可能。
2. SML **存在每核上电握手状态机**（tpidr_el3+376：0=关/1=启动中/2=运行），二级核启动时置 1。
3. TrustOS 另有一层 `bootstrap completion` 引导完成握手。

### 对"是否等待握手"的最终判定：
**大概率是（存在每核握手状态机），但无法完全确认 CPU_ON 处理器内部是否用该 flag 轮询等待。**
具体原因是 **SML 文件不完整**：分发用的查表表（0xbd026f18）超出 137KB 文件（SML 区 256KB，
但载荷只捕获代码段），无法解析 index 0x44 对应的 CPU_ON 处理器，从而无法确认处理器内部
是否存在 `ldr flag; cmp; b.eq` 的轮询等待循环。因此不能给出 100% 的"是"。

### 若判定为"是"，握手地址与 mainline 需满足的条件（推断）：
- 握手地址/条件：SML 每核结构体 `tpidr_el3` 基址 + **偏移 0x178（376）** 的 32 位状态字段；
  期望值由 0（关）→ 1（启动中）。
- mainline 需要满足：二级核被上电后必须**先进入 SML（EL3）并执行启动代码**（0xbd007e84 流程，
  含 `str w1,[x0,#376]` 置 1），再经 TrustOS bootstrap 交接回非安全内核。若 mainline 的
  `secondary_start_kernel` 入口 / 内核启动协议与原厂不同，导致二级核**从未进入 SML 启动路径**
  或**从未到达内核入口**，则该 flag 停在 0，CPU_ON 侧可能永久等待 → CPU0 的 SMC 永不返回。

### 卡点/下一步建议：
1. 需要**完整的 256KB SML 镜像**（当前 `sml_a.bin` 只有 ~137KB 代码段），补齐 0xbd0224f0–0xbd040000
   区域，即可解析 0xbd026f18 查表表，定位 index 0x44 的 CPU_ON 处理器并确认轮询等待。
2. 需对比原厂 5.4 内核与 mainline 6.12 内核的 `secondary_start_kernel` 入口地址/启动协议，
   确认两者传给 CPU_ON 的 entry_point 是否一致、二级核上电后是否走相同的 EL3→TrustOS→内核路径。

---

## 附录：关键地址速查

| 内容 | 地址（vaddr） | 文件偏移 |
|---|---|---|
| 代码入口/复位 | 0xbd0001c8 | 0x1c8 |
| VBAR_EL3（异常向量表） | 0xbd00e9c8 | 0xe9c8 |
| 向量 +0x400（低EL AArch64 同步=内核SMC入口） | 0xbd00edc8 | 0xedc8 |
| SMC32 分发（EC 0x13） | 0xbd00d4e4 | 0xd4e4 |
| SMC64 分发（EC 0x17） | 0xbd00d4fc | 0xd4fc |
| 分发函数指针表 | 0xbd013528 | 0x13528 |
| 分发查表表 | 0xbd026f18 | **0x26f18（超出文件！）** |
| 二级核启动代码 | 0xbd007e84 | 0x7e84 |
| 每核状态写 1（tpidr+376） | 0xbd007f4c | 0x7f4c |
| 每核状态读/校验 | 0xbd007fa0 起 | 0x7fa0 |
| "The core id is error" 字符串 | 0xbd0117ed | 0x117ed |
