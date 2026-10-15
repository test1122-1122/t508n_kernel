# 任务：原厂 5.4 的 of_core_init / __of_attach_node_sysfs 区域分析（对比 mainline 6.12 卡点）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**已修复**：LSE static_key（SM#2 过）、NOMAP（set_zone_contiguous 过）、NUMA/SMT 关闭（8 核竞态消失）。

**当前卡点（稳定，非竞态）**：`driver_init → of_core_init()` 卡死：
- 探针：y=210 x=8（kset 创建后）亮、x=44 计数 0 格、x=296（完成）不亮
- **卡在节点循环的第一个节点**（`__of_attach_node_sysfs(np)`，根节点 "/" 或第一个子节点）
- 6.12 的 of_core_init（drivers/of/base.c:167）：
  `of_platform_register_reconfig_notifier → kset_create_and_add("devicetree", NULL, firmware_kobj)
  → for_each_of_allnodes(np) { __of_attach_node_sysfs(np); phandle_cache 写入 } → proc_symlink`
- phandle_cache 是 static 数组（非 NULL，写 NULL 排除）
- 我们的 DTS 有 no-map 节点（sipc-mem/cp-modem 等，之前为 set_zone_contiguous 加的）

需要分析：**原厂 5.4 在这个区域是怎么做的**，为什么原厂能过、6.12 卡在第一个节点。

## 材料
- /home/test/t508n/linux-6.12.103/             ← mainline 6.12.103 源码
  - drivers/of/base.c（of_core_init）
  - drivers/of/sysfs.c（__of_attach_node_sysfs）
  - drivers/of/kobj.c（of_node_kobj 相关）
- /home/test/t508n/work/linux-5.4.210/         ← 完整 v5.4.210 源码
  - drivers/of/base.c、drivers/of/sysfs.c、drivers/of/kobj.c
- /home/test/t508n/work/stock_kallsyms.map + stock_Image_full.bin（原厂反汇编）
- /home/test/t508n/work/stock_ums9620.dts（原厂 DTS——节点结构参考）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。
原厂符号：在 stock_kallsyms.map 搜 of_core_init、__of_attach_node_sysfs、of_node_kobj、kset_create_and_add。

## 任务

### 1. 原厂 5.4 的 of_core_init 实现对比
- 5.4 drivers/of/base.c 的 of_core_init（git 拉取对比）
- 逐步骤对比 6.12：of_platform_register_reconfig_notifier、kset_create_and_add、
  节点循环、phandle_cache、proc_symlink
- **差异点**：原厂的节点循环有什么不同？原厂是否跳过某些节点？

### 2. 原厂 5.4 的 __of_attach_node_sysfs 实现对比
- 5.4 vs 6.12 的 drivers/of/sysfs.c __of_attach_node_sysfs（逐行对比）
- **kobject_add 的 parent 处理**、名称处理（full_name/basename）
- **根节点 "/" 的处理**：原厂怎么处理根节点的 sysfs attach？（6.12 的第一个节点是根节点）
- 6.12 相对 5.4 的新增/变化（比如 kobject_add 的 flags、kernfs 交互）

### 3. of_node_kobj / kobject_add 的锁与依赖
- kobject_add → sysfs_create_dir_ns → kernfs 创建的锁路径
- **6.12 的 kobject_add 在 of_core_init 语境（8 核在线、of_mutex 持有）下的死锁/卡点可能**
- 原厂 5.4 的对应路径差异

### 4. 反汇编原厂 of_core_init（验证）
- 从 stock_kallsyms.map 找 of_core_init 地址，反汇编
- 确认原厂的节点循环/attach 方式（与 5.4 源码一致？）

### 5. 综合
- 原厂能过、6.12 卡在第一个节点 __of_attach_node_sysfs 的最可能原因
- 修复建议（代码改动，具体到函数）

## 输出
单文件：/home/test/t508n/work/REPORT-stock-of-core-init.md

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码在 work/linux-5.4.210/
- 重点：第一个节点（根节点）attach 的差异
