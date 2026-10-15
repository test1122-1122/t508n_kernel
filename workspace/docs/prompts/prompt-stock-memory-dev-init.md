# 任务：原厂 5.4 的 memory_dev_init / device_register 区域分析（对比 mainline 6.12 间歇卡点）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**已修复**：LSE static_key（SM#2 过）、NOMAP（set_zone_contiguous 过）、NUMA/SMT 关闭
（8 核竞态大幅消除）、of_core_init 循环内 phandle_cache 写注释（948 节点全过）。

**当前卡点（间歇竞态）**：`driver_init → memory_dev_init` 的 `device_register`：
- 探针：y=340 x=188（block_sz）/ x=224（subsys 后）亮、x=296（device_register 前）亮、
  y=380 x=8（device_register 返回）部分亮、y=340 x=260（memory_dev_init 完成）不亮
- 即：**部分 memory block 的 device_register 返回了，卡在后续某 block 的 device_register 里**
  （间歇：有时卡第几个 block 不同）
- 6.12 的路径：memory_dev_init（drivers/base/memory.c）→ for 循环 add_boot_memory_block(nr)
  → init_memory_block → device_register(&memory->dev) → device_add（drivers/base/core.c）
  → kobject_add（kernfs 创建 /sys/devices/...）→ bus_add_device（klist + uevent）→ 属性创建
- 3GB 内存 ≈ 48 个 memory block（64MB/block），循环内每个 block 一次完整 device_add
- 8 核在线（CPU0 跑 kernel_init，其他核 idle/运行 kthread）

需要分析：**原厂 5.4 在这个区域是怎么做的**，为什么原厂能过、6.12 间歇卡在 device_register。

## 材料
- /home/test/t508n/linux-6.12.103/             ← mainline 6.12.103 源码
  - drivers/base/memory.c（memory_dev_init/add_boot_memory_block/init_memory_block）
  - drivers/base/core.c（device_add/device_register/bus_add_device/kobject_add）
  - drivers/base/bus.c（bus_add_device/klist/uevent）
  - drivers/base/uevent.c（kobject_uevent）
- /home/test/t508n/work/linux-5.4.210/         ← 完整 v5.4.210 源码（drivers/base/memory.c 等）
- /home/test/t508n/work/stock_kallsyms.map + stock_Image_full.bin（原厂反汇编）
- /home/test/t508n/work/kernel.config（原厂 config——memory/hotplug 相关）

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。
原厂符号：stock_kallsyms.map 搜 memory_dev_init、add_boot_memory_block、init_memory_block、
device_register、device_add、bus_add_device、kobject_uevent。

## 任务

### 1. 原厂 5.4 的 memory_dev_init 实现对比
- 5.4 drivers/base/memory.c 的 memory_dev_init（git 拉取对比）
- 节点循环/block 数、add_boot_memory_block 的实现
- **5.4 vs 6.12 的差异**（函数体、调用顺序、属性、锁）

### 2. 原厂 device_register → device_add 路径对比
- 5.4 vs 6.12 的 device_add（drivers/base/core.c）：kobject_add、bus_add_device、
  uevent、属性创建的差异
- **多核交互点**：kernfs 锁、klist 锁、uevent（kobject_uevent_env → 网络/uevent helper）、
  devtmpfs（device_add 会创建 devtmpfs 节点！——kdevtmpfs 线程参与！）
- **devtmpfs 交互**：device_register 的 devtmpfs_create_node（唤醒 kdevtmpfs 处理
  devtmpfs 节点创建）——**这是 8 核间歇卡的最大嫌疑**（kdevtmpfs 线程协作）！
  - 5.4 vs 6.12 的 devtmpfs_create_node / devtmpfsd 处理差异

### 3. uevent / klist / kernfs 的锁路径
- device_add 的锁顺序（device_lock? dev->mutex? klist lock? kernfs）
- 5.4 vs 6.12 的锁/唤醒差异
- **间歇卡的机制**：什么锁/唤醒在 8 核下间歇失败

### 4. 反汇编原厂关键函数
- 原厂 memory_dev_init / init_memory_block / device_add（如果符号存在）反汇编验证

### 5. 综合
- 原厂能过、6.12 间歇卡在 device_register 的最可能原因（排序）
- 修复建议（具体到函数/config）

## 输出
单文件：/home/test/t508n/work/REPORT-stock-memory-dev-init.md

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码在 work/linux-5.4.210/
- 重点：device_add 的多核交互（devtmpfs 线程协作、uevent、klist/kernfs 锁）
