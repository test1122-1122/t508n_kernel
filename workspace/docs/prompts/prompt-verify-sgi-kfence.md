# 任务：验证未验证项 4+5 — Unisoc 死循环门移植 + KFENCE 诊断

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210）mainline 6.12.103 bring-up。
**先读**：
- /home/test/t508n/work/UNISOC-MODS-AND-CONFIG.md（第 4 节未验证项 4/5、第 1.2 节 C1）
- /home/test/t508n/work/DISASM-C-irq-中断GIC/Timer.md（死循环门反汇编）

当前：memory_dev_init 的 device_register 间歇竞态仍现。
**未验证项 4**：原厂 ipi_handler case0（IPI_RESCHEDULE）带 Unisoc 死循环门：
`if (ipi_cpu(0x123e91ec)==cpu && state(0x1279e7d8)==1) while(1);`（sprd-ipi-thread/hang-detect）。
**未验证项 5**：原厂开启 KFENCE（5.12+ 回移，sample_interval=100）——mainline 6.12 **原生支持**
KFENCE（CONFIG_KFENCE=y 即可），是诊断"内存被踩"的现成工具。

## 任务
1. **死循环门移植设计**（诊断 T770 SGI 丢失）：
   - 在 mainline 6.12 的 do_handle_IPI case IPI_RESCHEDULE（arch/arm64/kernel/smp.c）设计
     一个等价检测：记录每个核收到 RESCHEDULE 的次数 + 检测"发送方等待无响应"？
   - 或者更简单：在 IPI_RESCHEDULE 处理里加"进入即画屏"的探针（已有类似）+ 计数器？
   - 评估：能否用死循环门确认"T770 的 SGI 间歇丢失"（即：竞态时某核是否没收到本应到达的 IPI）
2. **KFENCE 诊断方案**：
   - 评估开启 CONFIG_KFENCE=y 的可行性（6.12 原生；内存池配置 kfence_sample_interval）
   - **关键**：KFENCE 能检测什么（越界写/释放后使用——"内存被踩"的直接证据）？
     开启后如何从无 console 环境获取报告（KFENCE 输出到 dmesg——无 console 看不到！
     能否用 framebuffer 探针/KFENCE 的 fault 路径挂钩显示？）
   - 给出开启方案（config + 可能的 framebuffer 挂钩）
3. 给出**可实施的移植/开启方案**到 /home/test/t508n/work/PATCH-diag-sgi-kfence.md

## 输出
单文件：/home/test/t508n/work/PATCH-diag-sgi-kfence.md（含设计 + 补丁/config + 判定方法）

## 注意
- 只写自己的输出文件
- 方案要可在无 console 的 framebuffer 环境下判定结果
