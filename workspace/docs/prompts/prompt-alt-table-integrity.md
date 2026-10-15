# 任务：静态核查 mainline alt 表 828 条异常记录（orig 越界 → lm_alias 写坏内存？）

## 背景
T508N mainline 6.12.103 bring-up 卡点：apply_alternatives_all 的 stop_machine 完成后
complete() 被调用但 kernel_init 不被唤醒。子 agent（REPORT-stock-alternatives-waiter-analysis.md）
发现 mainline alt 表可能存在 **828 条字段异常记录**（orig 越界会让 `lm_alias` 写坏随机内存，
是"waiter 状态被写坏"的物证路径）。本任务：静态核查这些记录是否真实存在、具体是什么。

## 材料
- /home/test/t508n/linux-6.12.103/          ← mainline 源码
  - arch/arm64/kernel/alternative.c         ← __apply_alternatives / lm_alias 实现
  - arch/arm64/include/asm/alternative.h    ← struct alt_instr 定义
  - vmlinux（构建产物，含 __alt_instructions 段）或 System.map
- /home/test/t508n/work/REPORT-stock-alternatives-waiter-analysis.md  ← 先读，828 条异常的来源说明

## 任务

### 1. 理解 alt_instr 结构
- 读 arch/arm64/include/asm/alternative.h 的 struct alt_instr 定义（orig_offset, alt_offset,
  orig_len, alt_len, cpucap 等字段）和 __apply_alternatives 的实现（lm_alias、修复地址计算）
- 说明：什么情况下 orig_offset/orig_len 组合会导致"越界"（写坏 orig_offset 之外的内存）？

### 2. 解析 vmlinux 的 __alt_instructions 段
- 用 readelf/objdump 找到 __alt_instructions 到 __alt_instructions_end 的地址范围
- 写一个 Python 脚本解析每一条 alt_instr 记录（按结构体布局），统计：
  - 总条数
  - 字段异常记录：orig_offset + 修复地址是否超出合理范围（比如 orig 地址不在
    _stext.._etext 内、或 alt 地址不在 .altinstr_replacement 段内）
  - **重点复现"828 条异常"**：找出这些记录的共同特征（哪个 cpucap？orig_len 多少？
    是否集中在某个区域？）
- 注意：vmlinux 的段地址（链接地址）与文件偏移的换算（readelf -S 看 section 的
  VMA 和 file offset）

### 3. 判断异常记录是否真的会写坏内存
- 对每条异常记录，计算 __apply_alternatives 会写入的目标地址
- 判断目标地址是否落在 .text（合法 patch 目标）之外（比如 .bss/.data/其他）
- **特别检查：是否有异常记录的写入目标落在"调度器/任务状态"附近**（比如
  init_task/task_struct 区域、swait 队列项区域、或 CPU0 相关 per-cpu 区域）
- 结论：828 条异常是真实危险还是分析误差（比如段边界计算错误、vmlinux vs Image 差异）

### 4. 给出验证/修复建议
- 若确认真实危险：列出具体异常记录样本（orig 地址、cpucap、写入目标），
  判断是否可能解释"waiter 状态被写坏"，给出修复方案
- 若为分析误差：说明误差来源，给出正确的段解析方法

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-alt-table-integrity.md
- alt_instr 结构说明 + __apply_alternatives 写入逻辑
- 段解析方法 + 统计结果
- 异常记录样本（若有）+ 写入目标分析
- 结论：真危险 or 误差 + 建议

## 注意
- vmlinux 在 /home/test/t508n/linux-6.12.103/vmlinux（有符号的构建产物）
- 用 aarch64-linux-gnu-objdump / readelf 解析（/usr/bin/ 有）
- 脚本放 /home/test/t508n/work/ 下，报告放 /home/test/t508n/work/REPORT-alt-table-integrity.md
