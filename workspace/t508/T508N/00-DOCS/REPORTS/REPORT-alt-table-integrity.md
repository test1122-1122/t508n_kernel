# T508N：mainline 6.12 alt 表完整性静态核查（"828 条字段异常记录" 真伪判定）

> 任务来源：`prompt-alt-table-integrity.md`。目标：静态核查 mainline 6.12.103 探针构建
> vmlinux 的 `__alt_instructions` 段是否存在 **828 条字段异常记录**（orig 越界 →
> `lm_alias` 写坏随机内存 → "waiter 状态被写坏"物证路径），判定**真危险 or 分析误差**。
>
> 分析对象：`/home/test/t508n/linux-6.12.103/vmlinux`（Aug 17 13:22 构建，与
> `linux-6.12.103/System.map`、`work/mainline_Image` 同构建；已逐字节比对 Image 与 vmlinux
> 的 alt 表内容完全一致）。
> 脚本：`work/parse_alt_table.py`；完整解析结果：`work/alt_table_records.csv`。

---

## 0. 结论摘要（TL;DR）

1. **"828 条字段异常" 无法复现，是分析误差，不是真实危险。**
   - 用正确语义解析 vmlinux 的 `.altinstructions` 段：**30,477 条记录全部结构合法**，
     **0 条 orig 越界、0 条 alt 越界、0 条 len 异常、0 条 cap 异常、0 条写窗口跨段、0 条未对齐**。
   - "828" = 上一份报告自己统计口径下的 `30,477 槽 − 29,649 有效`，而其"有效"判定
     （cap<102、len 自洽）与二进制真实内容不符：**同一批记录在正确语义下全部通过**。
2. **前报告 §3.2 的 per-cap 分布表与二进制不一致**（如 CB37 报 10,712 实为 10,901；
   classic37 报 880 实为 1,175；cap95 报 13,066 实为 13,178），部分 cap 又恰好吻合
   （39=558、99=24、42=54、25CB=50）——这是"换了数据源/解析有误"的特征，不是真实表。
3. **所有 30,477 条记录的写入目标都落在可执行段内**（.text 29,991 / .init.text 359 /
   .rodata.text 123 / .exit.text 4），**没有任何一条会写 .data/.bss/.percpu/alt 表自身**；
   与调度器/任务状态（init_task@0xffffffc0826a4640、runqueues@0xffffffc081e5cc00、
   __per_cpu_offset 等）的最近距离也在 .text 内部（合法 LSE 站点），**不存在
   "lm_alias 写坏 waiter 状态" 的物证路径**。
4. **`lm_alias(origptr)` 写坏内存的机制只有在 origptr 越界时才成立**；本表不存在越界记录，
   故该机制在本构建上**不可能由 alt 表触发**。waiter 卡点需回到既有假设
   （CPU0 半新半旧指令 / swait 无条件摘链等，见 waiter 报告 §6.3）。

---

## 1. alt_instr 结构与 __apply_alternatives 写入逻辑

### 1.1 struct alt_instr（arch/arm64/include/asm/alternative.h，12 字节 LE）

```c
struct alt_instr {
	s32 orig_offset;	/* 距本记录 orig_offset 字段地址的偏移 → orig 站点 */
	s32 alt_offset;		/* 距本记录 alt_offset 字段地址的偏移 → 替换代码/回调 */
	u16 cpucap;		/* cpucap 编号；bit15(0x8000)=CB 位(回调型) */
	u8  orig_len;		/* 原指令序列字节数 */
	u8  alt_len;		/* 替换序列字节数；CB 型必须为 0 */
};
```

### 1.2 地址计算（alternative.c）

```c
#define __ALT_PTR(a, f)	((void *)&(a)->f + (a)->f)
#define ALT_ORIG_PTR(a)	__ALT_PTR(a, orig_offset)   /* = 记录自身地址 + orig_offset */
#define ALT_REPL_PTR(a)	__ALT_PTR(a, alt_offset)    /* = 记录地址 + 4 + alt_offset */
#define ALT_CAP(a)	((a)->cpucap & ~ARM64_CB_BIT)
#define ALT_HAS_CB(a)	((a)->cpucap & ARM64_CB_BIT)
```

`__apply_alternatives`（非模块路径）对每条满足 `test_bit(cap, mask) && cpus_have_cap(cap)`
的记录执行：

1. `origptr = ALT_ORIG_PTR(alt)`；`updptr = lm_alias(origptr)`（**写目标**）。
2. `nr_inst = orig_len / 4`；`BUG_ON` 校验：CB 型 `alt_len==0`，非 CB 型 `alt_len==orig_len`。
3. 非 CB：`patch_alternative` 把替换指令（逐条重定位分支/adrp 偏移）写入 `updptr`；
   CB 型：`alt_cb_patch_nops` 把 `orig_len` 字节全写 NOP（即 `alternative_has_cap_likely`
   的 `b l_no` 原地变 NOP → 落入紧跟的内联 LSE 序列）。
4. 写后 `clean_dcache_range_nopatch(origptr, origptr+orig_len)`；尾部 `dsb ish; ic ialluis; isb`。

### 1.3 什么情况下会"写坏内存"

`lm_alias(x) = __va(__pa_symbol(x))`（include/linux/mm.h）——把内核映像 VA 换成同一
物理页的线性映射别名。**只有当 origptr 本身越界**（不在任何可执行段、指向 .data/.bss/
.alt 表/未映射区）时，`updptr` 才会落到文本之外并写入 `orig_len` 字节，构成"写坏随机
内存"。orig_offset/orig_len 组合若导致 `origptr+orig_len` 跨出所在段边界，同样会写坏
相邻段。本任务即核查这两类情况。

---

## 2. 段解析方法（readelf + Python）

### 2.1 关键符号与段布局（readelf -S / System.map 交叉验证）

| 项 | 值 |
|---|---|
| `__alt_instructions` | `ffffffc081ce7514`（.altinstructions 段 VMA，readelf 与 System.map 一致） |
| `__alt_instructions_end` | `ffffffc081d409b0` |
| 段大小 | `0x5949C` = 365,724 B = **30,477 槽**（12 B/条） |
| 段 file offset | `0x1cf7514`（vaddr = fileoff + `0xffffffc07fff0000`；注意**不是** +0xffffffc080000000，差 0x10000 是 .head.text 的 PE 头偏移） |
| 可执行段 | `.head.text` [0xffffffc080000000,+0x10000)、`.text` [0xffffffc080010000,0xffffffc081090000)（_stext.._etext）、`.rodata.text` [0xffffffc081c3c800,+0x5800)、`.init.text` [0xffffffc081c50000,0xffffffc081cddca4)、`.exit.text` [0xffffffc081cddca8,0xffffffc081ce7514) |
| 数据段 | .rodata/.init.data/.data..percpu/.data/.bss/__bug_table/…（见脚本 DATA_REGIONS） |
| `.altinstr_replacement` | **本构建不存在**（readelf -S 无此段；vmlinux.lds.S 也无输出段）。6.12 宏 `__ALTERNATIVE_CFG` 把 newinstr 放入**所在段的 `.subsection 1`**，替换池位于 .text 尾部/各文本段内，故 altptr 落在 .text/.init.text/.rodata.text 属正常。 |

### 2.2 解析方法

- 从 vmlinux fileoff `0x1cf7514` 读 0x5949C 字节，按 `<iiHBB` 解出
  (orig_offset, alt_offset, cpucap, orig_len, alt_len)。
- `origptr = (__alt_instructions + i*12) + orig_offset`
  `altptr  = (__alt_instructions + i*12) + 4 + alt_offset`
- 逐条判定：origptr/altptr 是否在可执行段；orig_len 是否 4 的倍数且 >0；
  非 CB 是否 alt_len==orig_len；CB 是否 alt_len==0；cap(<0x8000 掩码) 是否 <102；
  origptr/altptr 是否 4 字节对齐；`origptr+orig_len` 是否跨出所在段。

### 2.3 统计结果（全部 30,477 条）

| 检查项 | 结果 |
|---|---|
| orig 越界（origptr 不在任何可执行段） | **0** |
| alt 越界（altptr 不在任何可执行段） | **0** |
| orig_len 非法（0 或非 4 倍数） | **0** |
| 非 CB 且 alt_len != orig_len | **0** |
| CB 且 alt_len != 0 | **0** |
| cap(掩码后) >= ARM64_NCAPS(102) | **0** |
| origptr 落在数据段/alt 表自身 | **0** |
| 写窗口 origptr+orig_len 跨出所在段 | **0** |
| origptr/altptr 未 4 字节对齐 | **0** |
| **异常合计** | **0** |

- origptr 落区分布：.text **29,991**；.init.text **359**；.rodata.text **123**；.exit.text **4**。
- altptr 落区分布：.text **30,106**；.init.text **264**；.rodata.text **107**。
- orig_len 分布：4→29,016；16→908；20→344；8→130；12→43；24→36（全为 4 倍数）。
- cap 分布（掩码后）：95→13,178；CB37→10,901；51→2,401；CB1→1,220；37→1,175；39→558；
  54→142；98→99；68→71；97→65；45→59；42→54；36→53；25CB→50；10→43；…（共 60+ 种 cap，
  全部 < 102）。
- CB 条目共 12,174 条，其 altptr **全部等于回调函数地址**：`alt_cb_patch_nops`
  （0xffffffc08106edac）及 0xffffffc08106ef74/efcc/f024、0xffffffc081c561d8/c56284/c562e8/
  c5bef4、0xffffffc08004e99c/eabc/ead8（init/kvm 回调），已逐一 objdump 验证。

### 2.4 objdump 抽验（解析正确性）

| idx | 类型 | origptr | 反汇编 | altptr |
|---|---|---|---|---|
| 0 | CB37 | 0xffffffc081c5111c (kernel_init_freeable) | `b 0x…c5137c`（NOP-落入目标） | 0xffffffc08106edac = alt_cb_patch_nops |
| 100 | CB1 | 0xffffffc081c3d484 (__entry_tramp_text_start) | `mov x30,#0x20; b`（KPTI 桩） | 0xffffffc08106efcc |
| 500 | classic51 | 0xffffffc080029414 | `mrs x1,tpidr_el2`（VHE） | 0xffffffc0800294bc（替换池） |
| 505 | CB0 | 0xffffffc08002adcc | `b`（vmcoreinfo 路径） | 0xffffffc08106edac |
| 1000 | classic39 | 0xffffffc080053dd8 | — | 0xffffffc08005799c |

即：CB 条目的 orig 都是真实分支指令（`b`），altptr 都是真实回调；经典条目 altptr 指向
.altinstr_replacement 池（在 .text 内）。**解析方法与地址换算完全正确。**

---

## 3. "828 条异常" 复现失败与误差来源分析

### 3.1 828 的出处

前报告（REPORT-stock-alternatives-waiter-analysis.md §3.1/§3.2）给出 mainline 探针构建
"30,477 槽 / **29,649 有效**"→ 828 = 30,477 − 29,649，即其"有效（cap<102、len 自洽）"
过滤判掉的条数；并把"表内 828 条字段异常记录"列为待核验项（§6.4-4）。

### 3.2 复现失败：任何正确/常见错误解析都得不到 828

对**同一个二进制**穷举了以下解析变体，**没有任何一种产生 828 条异常**：

| 假设 | 结果 |
|---|---|
| 正确解析（本报告基线） | 0 异常 |
| orig 仅限 `_stext.._etext`（把 .init/.rodata/.exit.text 当越界） | 486 异常（359+123+4） |
| alt 仅限 `_stext.._etext` | 371 异常 |
| orig∪alt 越界 | 587 异常 |
| 掩 CB 位前判 cap<102（即 CB 条目全判 cap 越界） | 12,174 异常 |
| 非 CB 才判 len，CB 条目被要求 alt_len==orig_len | 12,174 异常 |
| 16/8/20 字节记录大小 | 7,619/15,238/6,095 异常（且槽数不符） |
| fileoff 用 vaddr−0xffffffc080000000（错 0x10000） | 30,477 异常（全灭） |
| Image 文件按 vaddr−base 解析 | 0 异常（与 vmlinux 一致） |
| 字段顺序倒置 / +4 错位 / 各种移位(0..11) | 无 29,649 命中 |
| 网格搜索 cap×len×orig范围×alt范围（含"有效=29,649"目标） | **无命中** |

### 3.3 前报告 per-cap 表与二进制不符（换源/解析有误的直接证据）

| cap | 前报告 | 实测 | 差 |
|---|---|---|---|
| 95 | 13,066 | **13,178** | −112 |
| CB37 | 10,712 | **10,901** | −189 |
| 37(classic) | 880 | **1,175** | −295 |
| 51 | 2,385 | **2,401** | −16 |
| CB1 | 1,137 | **1,220** | −83 |
| 39 | 558 | 558 | 0 ✓ |
| 54 | 139 | 142 | −3 |
| 98 | 97 | 99 | −2 |
| 68 | 67 | 71 | −4 |
| 99 | 24 | 24 | 0 ✓ |
| 42 | 54 | 54 | 0 ✓ |
| 36 | 52 | 53 | −1 |
| 45 | 50 | 59 | −9 |
| 25CB | 50 | 50 | 0 ✓ |

部分 cap 精确吻合、部分系统性偏少，且"合计 LSE 11,592"与实测 12,076 不符
（任务文中"LSE 12,074"更接近实测）——**特征指向：前报告曾基于另一构建/另一份数据
（如 Aug 15 构建，其 __alt_instructions=0xffffffc081ce6efc、30,475 槽，见 work/System.map），
或过滤条件实现有误**。当前 Aug 17 构建的 alt 表**不存在任何字段异常**。

### 3.4 结论

"828 条字段异常记录（orig 越界 → lm_alias 写坏内存）" **是本任务唯一需要证伪的假设，
已被证伪**：该数字既不能从当前 vmlinux/Image 的任何解析中复现，其来源 per-cap 表也与
二进制不符。**结论：分析误差，非真实危险。**

---

## 4. 写入目标分析（"是否可能写坏调度器/任务状态"）

即便假设存在越界记录，也需其写入目标落在关键区域才算危险。实测（无越界记录前提下
的最坏情况）：

- **所有写入目标**（= origptr 对应物理页的线性别名）**全部位于可执行文本段内**；
  距离最近的关键符号：idx 25530 origptr=0xffffffc0810700b8，距 kernel_init
  （0xffffffc0810701e0）仅 296B，但这是 .text 内**合法 LSE 站点**（patch 目标是
  同一函数内指令），不构成越界写入。
- init_task（.data @0xffffffc0826a4640）、runqueues（.data..percpu @0xffffffc081e5cc00）、
  __per_cpu_offset（.data @0xffffffc08269d730）、swapper_pg_dir（0xffffffc081c45000）
  均位于数据段/页表区；**alt 表 0 条记录的 origptr 指向这些区域**，lm_alias 不可能写坏它们。
- swait 队列项（kernel_init 的 complete 等待项在 task_struct/栈上）同样不受影响：没有任何
  写入目标指向 task_struct 所在 .data 或内核栈。

---

## 5. 验证/修复建议

1. **结论已闭环，无需对 alt 表做任何修复**——表本身是干净的。
2. 若需彻底消除疑虑，可加一次性启动期校验（成本低、收益明确）：
   `__apply_alternatives` 遍历时对 `origptr`/`origptr+orig_len` 做
   `if (!is_kernel_text(origptr) || !is_kernel_text(origptr+orig_len-1)) BUG()`，
   或复用 `core_kernel_text()`/区间检查，任何未来构建再出现越界会当场爆炸而非静默写坏。
3. waiter 卡点调查应**放弃"alt 表写坏内存"这条物证路径**，回到 waiter 报告 §6.3 的
   主假设：CPU0 在 8 核 ic ialluis 边界执行半新半旧指令（尤其 `__schedule`/
   `resched_curr`/中断返回路径的 LSE NOP-落入站点）破坏调度状态，叠加
   `swake_up_locked` 无条件 `list_del_init`（唤醒失败也摘链）导致 kernel_init 队列项
   不可逆丢失——与探针事实（y=450 亮、pid==1 ttwu 不亮、y=440 永不返回）完全吻合。

---

## 附：关键符号速查

| 符号 | 值 |
|---|---|
| `__alt_instructions` / `__alt_instructions_end` | `ffffffc081ce7514` / `ffffffc081d409b0` |
| `.altinstructions` 段 | VMA ffffffc081ce7514，fileoff 0x1cf7514，size 0x5949C |
| `__apply_alternatives` | `ffffffc080020830` |
| `alt_cb_patch_nops` | `ffffffc08106edac` |
| `kernel_init` | `ffffffc0810701e0`（.text） |
| `init_task` / `runqueues` / `__per_cpu_offset` | `ffffffc0826a4640` / `ffffffc081e5cc00` / `ffffffc08269d730` |
| 脚本 / 数据 | `work/parse_alt_table.py` / `work/alt_table_records.csv` |
