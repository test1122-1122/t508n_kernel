# 任务：验证未验证项 2+3 — mem_sysfs_mutex 恢复 + memory_dev_init 顺序对齐

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**先读**：
- /home/test/t508n/work/UNISOC-MODS-AND-CONFIG.md（第 4 节未验证项 2/3）
- /home/test/t508n/work/DISASM-E-devm-驱动模型sysfs.md（原厂 memory_dev_init 反汇编：
  mem_sysfs_mutex@0x123f1070 全程持有；原厂 driver_init 顺序
  devices→buses→classes→firmware→hypervisor→of_core→platform→cpu→memory→container）
- /home/test/t508n/work/REPORT-stock-memory-dev-init.md

当前：memory_dev_init 的 device_register 间歇竞态仍现。6.12 的差异之一：
① 6.12 **删除了 mem_sysfs_mutex**（原厂 memory_dev_init 全程持有它）；
② 6.12 的 memory_dev_init 排在 cpu_dev_init **之前**（原厂在 cpu_dev_init 之后，memory 是"第二个"device_register 风暴）。

## 任务
1. 读 5.4.210（work/linux-5.4.210/drivers/base/memory.c）与 6.12（linux-6.12.103/drivers/base/memory.c）
   的 memory_dev_init / init_memory_block：mem_sysfs_mutex 的获取/释放点、保护范围
2. 设计恢复 mem_sysfs_mutex 的补丁：
   - 6.12 的 memory.c 需要加回 mutex（定义 + memory_dev_init 全程持有？或每个 block？）
   - **关键**：原厂 mem_sysfs_mutex 保护什么（memory block 注册与 /sys 访问的互斥）？
     6.12 用什么替代（如果有）？恢复的影响（死锁风险？与现有锁的交互）？
3. 设计 memory_dev_init 顺序对齐（挪回 cpu_dev_init 之后）：
   - 6.12 drivers/base/init.c：auxiliary→memory→node→cpu→container
   - 原厂：platform→cpu→memory→container
   - 调整后依赖检查（memory_dev_init 是否依赖 node/cpu 先建？）
4. 给出**可直接实施的补丁**到 /home/test/t508n/work/PATCH-mem-sysfs-order.md

## 输出
单文件：/home/test/t508n/work/PATCH-mem-sysfs-order.md（含分析 + 补丁 + 风险）

## 注意
- 只写自己的输出文件
- 补丁具体到函数/行
