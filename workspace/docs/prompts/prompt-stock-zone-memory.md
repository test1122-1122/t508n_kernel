# 任务：原厂 5.4 的 set_zone_contiguous / 内存布局分析（对比 mainline 6.12 卡点）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**已解决**：原厂式 LSE（static_key/jump_label）修复 stop_machine 卡点（SM#2 通过、smp_init 完成）。

**当前卡点**：`page_alloc_init_late()` → `set_zone_contiguous()` **卡死**：
- 探针实测：mem_init_print_info ✓、buffer_init ✓、memblock_discard ✓、shuffle_free_memory ✓
- `set_zone_contiguous`（mm/mm_init.c:2268）遍历 zone 的 pageblock 调
  `__pageblock_pfn_to_page`（mm/page_alloc.c）——**卡在遍历中途**（去掉 cond_resched 仍卡，
  排除调度嫌疑 → 卡在 __pageblock_pfn_to_page 的某 pfn 处）
- 设备 3GB LPDDR4x；**DTS 按 3GB 声明**（stock DTS 是 8GB 占位，LK 裁剪）
- 6.12 的 CONFIG_ZONE_DMA=y（mainline 开了）、CONFIG_NUMA=y

需要分析：**原厂 5.4 在这个区域（set_zone_contiguous / 内存布局 / vmemmap）是怎么做的**，
为什么原厂能过、mainline 卡在遍历中途。

## 材料
- /home/test/t508n/work/kernel.config          ← 原厂 5.4 config
- /home/test/t508n/work/stock_kallsyms.map     ← 原厂符号表
- /home/test/t508n/work/stock_Image_full.bin   ← 原厂内核，可反汇编
- /home/test/t508n/work/stock_ums9620.dts      ← 原厂完整 DTS（内存节点、内存布局）
- /home/test/t508n/work/stock-5.4.210/         ← 5.4.210 源码 git 仓库（可拉 mm 相关）
- /home/test/t508n/linux-6.12.103/             ← mainline 6.12.103 源码
  - mm/mm_init.c（set_zone_contiguous）
  - mm/page_alloc.c（__pageblock_pfn_to_page、zone 初始化）
  - arch/arm64/mm/init.c（mem_init、zone 布局）
- /home/test/t508n/work/REPORT-stock-page-alloc-init.md  ← 先读（已有部分结论）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。
已知：原厂 set_zone_contiguous ≈ 0xffffffc0104975ec（page_alloc_init_late 主循环 bl 它）。

## 任务

### 1. 原厂 set_zone_contiguous + __pageblock_pfn_to_page 实现对比
- 5.4 的 mm/page_alloc.c 拉取（git checkout v5.4.210 -- mm/page_alloc.c mm/internal.h）
- 对比 5.4 vs 6.12 的 set_zone_contiguous、__pageblock_pfn_to_page：
  - 遍历逻辑是否相同？cond_resched 是否有？
  - __pageblock_pfn_to_page 的检查项差异（pfn_to_online_page? page_zone? mem_section?）
- **反汇编原厂 set_zone_contiguous（0x4975ec）**验证

### 2. 原厂内存布局（关键！）
- 读 work/stock_ums9620.dts 的 memory 节点（reg 属性）——**原厂声明的物理内存范围**（起始地址 + 大小）
- 对比我们的 3GB DTS（linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620-ot5.dts）
- **T770 3GB 的物理布局**：起始地址？是否连续？有没有 2GB 边界/保留区域（GPU、外设、SML 保留）？
- **原厂 zone 布局**：从原厂 config + 反汇编推断（ZONE_DMA/DMA32/NORMAL 的边界）
- **重点回答：mainline 卡在遍历中途（疑似 2GB 附近），原厂的 3GB 内存布局是否与 mainline 的 zone 划分不匹配**（比如 mainline 的 ZONE_DMA 边界/zone_end_pfn 计算与 T770 实际布局不一致）

### 3. 原厂 config 对比（zone/vmemmap 相关）
- CONFIG_ZONE_DMA / CONFIG_ZONE_DMA32 / CONFIG_NUMA / CONFIG_SPARSEMEM / CONFIG_SPARSEMEM_VMEMMAP / CONFIG_FORCE_MAX_ZONEORDER / CONFIG_ARM64_4K_PAGES / CONFIG_HUGETLB_PAGE
- 原厂 vs mainline 的差异 → 可能影响 zone 划分/pageblock 大小的选项

### 4. vmemmap 映射（pfn_to_page 路径）
- 6.12 的 vmemmap 映射在 T770 3GB 布局下是否完整（arch/arm64/mm/mmu.c vmemmap 区域）
- pfn_to_online_page / pfn_to_page 对"2GB 之后"的 pfn 的处理（mem_section 存在性）
- **如果 vmemmap/页表对某区域未映射 → __pageblock_pfn_to_page 访问坏地址 → 挂死**——原厂怎么避免的？

### 5. 综合
- 原厂能过、mainline 卡在 set_zone_contiguous 遍历中途的最可能原因
- 可操作的修复建议（config 对齐 / DTS 内存节点调整 / 代码修改）

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-zone-memory.md
- 每任务一节：对比证据（源码/DTS/反汇编）+ 结论
- 最后：最可疑差异 + 修复建议

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码用 git 拉取（work/stock-5.4.210 是 git 仓库）
- 重点：T770 3GB 物理内存布局与 mainline zone 划分的匹配性
