# 任务：原厂内核全面反编译 — 子系统 D-mm：内存管理

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**先读** /home/test/t508n/work/SUMMARY-ALL-ANALYSIS.md（已确认事实/反汇编成果/已修复/已排除）。
当前未解决：`memory_dev_init` 的 `device_register` 间歇竞态（8 核在线、NUMA/SMT 关、
ttwu_queue_cond 恒 false 后仍现）。

## 本任务（子系统 D-mm：内存管理）
对原厂内核**全面反汇编**该子系统，对比 mainline 6.12，找出差异。

## 材料
- /home/test/t508n/work/stock_Image_full.bin  ← 原厂内核（38MB，可 objdump）
- /home/test/t508n/work/stock_kallsyms.map    ← 原厂符号表（116,704 符号）
- /home/test/t508n/work/linux-5.4.210/        ← 完整 5.4.210 源码
- /home/test/t508n/linux-6.12.103/            ← mainline 6.12.103 源码

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<stock_kallsyms.map 符号地址> --stop-address=<+n> \
  /home/test/t508n/work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。链接基址 _text=0xffffffc010080000。

## 反汇编范围（子系统 D-mm 符号，从 stock_kallsyms.map grep 近似名）
关键词：page_alloc|slub|kmem_cache|memblock|vmemmap|pfn|zone|mm_init|buddy|free_page
（grep stock_kallsyms.map，提取 50-150 个代表性符号地址，逐个反汇编）

## 任务
1. 从 stock_kallsyms.map 提取符号地址（grep 近似名，选代表性符号）
2. 逐个反汇编（大函数分段），记录：指令序列要点、与原厂 5.4 源码的对应、与 mainline 6.12 的差异
3. **重点找差异**：与 5.4 源码/6.12 源码不同的实现细节、T770 特有处理、
   可能影响 8 核并发/设备注册竞态的机制
4. 输出**单文件** /home/test/t508n/work/DISASM-D-mm-内存管理.md：
   - 符号+地址+反汇编要点表
   - 原厂实现特点（相对 mainline 6.12 的差异清单）
   - 与当前竞态（memory_dev_init 间歇卡）可能相关的发现

## 注意
- 只写自己的输出文件（DISASM-D-mm-内存管理.md），不写其他文件
- 符号表一律用 stock_kallsyms.map
- 反汇编量大时聚焦关键函数（核心路径优先）
