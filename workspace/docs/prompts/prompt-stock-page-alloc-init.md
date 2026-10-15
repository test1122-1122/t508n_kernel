# 任务：原厂 5.4 的 page_alloc_init_late 区域分析（对比 mainline 6.12 卡点）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**重大进展**：已用原厂式 LSE（static_key/jump_label 替代 asm-goto NOP-落入）解决
stop_machine 卡点（SM#2 通过、smp_init 完成、进度条 10 格）。

**当前新卡点**：`kernel_init_freeable()` 里 `page_alloc_init_late()` **卡住**：
- 探针实测：sched_init_smp 后 ✓、workqueue_init_topology 后 ✓、async_init 后 ✓、
  padata_init 后 ✓、**page_alloc_init_late 后 ✗**（卡在函数内）
- 6.12 的 page_alloc_init_late（mm/mm_init.c:2291）执行：
  mem_init_print_info → buffer_init → memblock_discard → shuffle_free_memory →
  set_zone_contiguous → page_alloc_sysctl_init
- CONFIG_DEFERRED_STRUCT_PAGE_INIT 未开（deferred 部分跳过）
- 6.12 的 .config 里 CONFIG_SHUFFLE_PAGE_ALLOCATOR 状态待查

需要分析：**原厂 5.4 在这个区域是怎么做的**，与 mainline 6.12 的差异。

## 材料
- /home/test/t508n/work/kernel.config         ← 原厂 5.4 config（重点：SHUFFLE/DEFERRED/HOTPLUG 相关）
- /home/test/t508n/work/stock_kallsyms.map    ← 原厂符号表
- /home/test/t508n/work/stock_Image_full.bin  ← 原厂内核，可反汇编
- /home/test/t508n/linux-6.12.103/            ← mainline 6.12.103 源码
  - mm/mm_init.c（page_alloc_init_late）
  - mm/page_alloc.c（shuffle_free_memory、set_zone_contiguous）
  - fs/buffer.c（buffer_init）
- /home/test/t508n/work/stock-5.4.210/        ← 已拉取的 5.4 源码片段（可再拉 mm 相关）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务

### 1. 原厂 config 对比（先做，可能直接定位）
对比 work/kernel.config 与 mainline .config 的：
- CONFIG_SHUFFLE_PAGE_ALLOCATOR
- CONFIG_DEFERRED_STRUCT_PAGE_INIT
- CONFIG_MEMORY_HOTPLUG / CONFIG_MEMORY_BLOCK_SIZE / CONFIG_SPARSEMEM*
- CONFIG_BLOCK / CONFIG_BLK_DEV_INITRD（buffer_init 依赖）
- 其他可能影响 page_alloc_init_late 的选项
**结论：原厂是否开了/没开这些选项（差异可能导致卡点）**

### 2. 原厂 5.4 的 page_alloc_init_late 实现
- 从上游拉 5.4 的 mm/page_alloc.c（或 mm/mm_init.c——5.4 的 page_alloc_init_late 在 mm/page_alloc.c）
- 对比 6.12：函数体各步骤是否相同？顺序是否不同？
- 原厂是否有 T770/SPRD 特判？

### 3. 原厂 buffer_init / shuffle_free_memory / set_zone_contiguous
- 5.4 与 6.12 的实现差异（逐函数对比）
- 特别：shuffle_free_memory 的随机化逻辑（5.4 vs 6.12）、set_zone_contiguous 的遍历方式
- 这些函数的多核交互（锁/TLB/cache 维护）差异

### 4. 反汇编原厂 page_alloc_init_late（如果符号在 stock_kallsyms.map）
- 找到原厂符号地址，反汇编验证实现
- 确认原厂该函数是否正常执行（从实现看有没有 T770 敏感的多核操作）

### 5. 综合
- 原厂 vs mainline 在该区域的关键差异（config/实现）
- 最可能解释"mainline 卡在 page_alloc_init_late、原厂能过"的点
- 可操作的修复建议（改 config 或改代码）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-page-alloc-init.md
- 每任务一节：对比证据 + 结论
- 最后：最可疑差异 + 修复建议

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码可用 git 拉取（work/stock-5.4.210 已是 git 仓库，git checkout v5.4.210 -- <path>）
