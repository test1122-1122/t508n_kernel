# 任务：driver_init 链后续卡点分析（devices_init 及之后，对比原厂 5.4）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**已解决的两个卡点**：
1. stop_machine（SM#2）：原厂式 LSE（static_key/jump_label 替代 asm-goto NOP-落入）→ 通过
2. page_alloc_init_late → set_zone_contiguous：原厂式 NOMAP 检查（memblock_is_map_memory）→ 通过

**当前进度**（探针实测）：
- smp_init 完成（进度条 10 格）
- page_alloc_init_late 完成
- do_basic_setup 进入（cpuset_init_smp 后 ✓）
- **driver_init() 卡住**：devtmpfs_init **完整通过**（y=270 内部 4 步全亮：
  vfs_kern_mount ✓ register_filesystem ✓ kthread_run ✓ wait_for_completion ✓）
- **卡在 devtmpfs_init 之后**：driver_init 的下一个调用是 devices_init()
  （y=260 x=44 探针待确认）——历史卡点区域（of_core_init 的 sysfs attach、
  memory_dev_init 的 device_register 曾间歇卡住，与多核 sysfs/device model 相关）

需要分析：**devices_init 及 driver_init 链后续（buses_init/classes_init/firmware_init/
hypervisor_init/of_core_init/platform_bus_init/auxiliary_bus_init/memory_dev_init/
node_dev_init/cpu_dev_init/container_dev_init）在 6.12 的实现、多核交互、潜在卡点，
以及原厂 5.4 对应部分的差异**。

## 材料
- /home/test/t508n/linux-6.12.103/             ← mainline 6.12.103 源码
  - drivers/base/base.c（devices_init）
  - drivers/base/bus.c（buses_init）
  - drivers/base/class.c（classes_init）
  - drivers/base/firmware.c、drivers/base/hypervisor.c
  - drivers/of/base.c（of_core_init）
  - drivers/base/platform.c（platform_bus_init）
  - drivers/base/node.c（node_dev_init）、drivers/base/cpu.c（cpu_dev_init）、
    drivers/base/container.c（container_dev_init）
  - drivers/base/memory.c（memory_dev_init）
  - drivers/base/core.c（device_add/kobject 路径）
- /home/test/t508n/work/stock-5.4.210/         ← 5.4.210 源码 git 仓库（可拉 drivers/base 对比）
- /home/test/t508n/work/kernel.config          ← 原厂 config
- /home/test/t508n/work/stock_kallsyms.map     ← 原厂符号表
- /home/test/t508n/work/stock_Image_full.bin   ← 原厂内核，可反汇编

## 反汇编方法
```bash
/usr/bin/aarch64-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<符号地址> --stop-address=<+n> work/stock_Image_full.bin
```
file_off = vaddr - 0xffffffc010080000。

## 任务

### 1. devices_init 分析（drivers/base/base.c）
- 6.12 的 devices_init 实现：做什么（kobject 子系统初始化？kset 创建？）
- 多核交互点（锁、sysfs、kobject 注册）
- **潜在卡点**：什么情况下会卡住（比如 kobject_create_and_add、sysfs 创建、kset 注册的锁竞争）

### 2. driver_init 链后续逐个分析（buses_init/classes_init/of_core_init 等）
- 每个函数的实现要点 + 多核交互 + 卡点嫌疑（按"历史卡过"排序：of_core_init 的 sysfs attach、
  memory_dev_init 的 device_register 优先）
- 6.12 vs 5.4 的实现差异（git 拉 5.4 drivers/base 对比）

### 3. 原厂 5.4 的 driver_init 链（时机/顺序/实现差异）
- 原厂 driver_init 的调用顺序 vs 6.12（6.12 有 firmware_init/hypervisor_init 等新增？）
- 原厂 devices_init/classes_init/buses_init 的实现（反汇编或源码）
- **原厂在这些函数里有没有 T770 特有处理**（应该没有——之前报告确认原厂 device path 无特判）

### 4. 多核 sysfs/device model 的同步机制
- kobject_add → sysfs_create_dir 的锁路径（kobj->parent 的锁？sysfs 的 kernfs 锁）
- device_add 的锁路径（device_lock？dev->mutex？）
- **这些锁在多核下的死锁/长等待可能**——与 T770 已修复的 LSE 问题的关联（LSE 修复后锁应该正常）

### 5. 综合
- 最可能卡在 driver_init 链的哪个函数、什么机制
- 验证探针建议（一次刷机一个变量的顺序）
- 原厂 vs mainline 的关键差异

## 输出
Markdown 报告：/home/test/t508n/work/REPORT-stock-driver-init.md
- 每任务一节：分析 + 证据
- 最后：卡点假设排序 + 验证方案 + 修复建议

## 注意
- 符号表用 work/stock_kallsyms.map
- 5.4 源码用 git 拉取（work/stock-5.4.210 是 git 仓库）
- 重点：driver_init 链在 post-SMP（8 核在线）语境下的多核同步问题
