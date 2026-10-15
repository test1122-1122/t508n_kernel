# T508N 工作区指引（给其他 agent 的入口文档）

> 生成时间：2026-08-17 09:20
> 当前阶段唯一目标：**让 mainline 内核本身跑通到 initramfs `/init`**。
> USB 及其他外设一律不处理，相关资料已归档。

---

## 1. 你先读哪几份

按顺序读，不要跳：

1. `/home/test/t508n/work/HANDOFF-3-SESSIONS.md`
   - 当前**主执行计划**。定义“内核跑通”验收标准、当前阻塞点、
     Phase 0~4 操作顺序、stop_machine 探针读屏决策树、红线。
   - 这是最近一次范围收敛后的文档，以它为准。

2. `/home/test/t508n/work/SESSION-CONTEXT-COMPACT.md`
   - 中断会话的压缩历史。包含项目背景、已确认事实、每轮任务产出、
     当前继续点和下一步候选。
   - 用于快速恢复上下文，不需要读原始 20,340 行 `session.jsonl`。

3. `/home/test/t508n/work/REPORT-stock-sync-mechanism.md`
   - 最新的深度分析报告：原厂 vs mainline 的
     `__apply_alternatives_multi_stop` / stop_machine 同步机制。
   - 回答原厂二级核等待循环为什么是纯 `yield` 轮询，以及可移植方案。

---

## 2. 最近刚写/改了什么

| 文件 | 刚做/改了什么 | 作用 |
|---|---|---|
| `/home/test/t508n/work/HANDOFF-3-SESSIONS.md` | 重写为“内核跑通专项” | 主执行计划，替代旧的 3 副 session 分工 |
| `/home/test/t508n/work/SESSION-CONTEXT-COMPACT.md` | 更新第 5/6 节为“只跑内核”范围 | 压缩上下文，给其他 agent 恢复记忆 |
| `/home/test/t508n/AGENTS-GUIDE.md` | 本文件 | 工作区索引和入口 |

不要把上述文件当普通笔记覆盖。继续任务前先读它们。

---

## 3. 所有成果在哪（完整地图）

### 3.1 当前活动文档（本轮只使用这些）

| 路径 | 内容 |
|---|---|
| `/home/test/t508n/work/HANDOFF-3-SESSIONS.md` | 内核跑通专项计划、决策树、红线 |
| `/home/test/t508n/work/SESSION-CONTEXT-COMPACT.md` | 会话压缩上下文 |
| `/home/test/t508n/work/REPORT-stock-sync-mechanism.md` | stop_machine/alternatives 同步机制报告 |
| `/home/test/t508n/work/REPORT-stock-vs-mainline-locks.md` | 锁/原子/多核同步/启动时序对比 |
| `/home/test/t508n/work/REPORT-T508N-stock-analysis.md` | stock A/B/C/D 四节分析 |
| `/home/test/t508n/SKILL-T508N-framebuffer-debug.md` | framebuffer 探针 API、刷机/读屏约定、已知坑 |
| `/home/test/t508n/work/stock_kallsyms.map` | 116,704 个真实原厂符号 |

### 3.2 归档资料（当前范围明确不处理）

| 路径 | 内容 |
|---|---|
| `/home/test/t508n/work/TASK-A-B-USB-DTS-C-离线成果.md` | USB 驱动/DTS/LSE 第一轮分析 |
| `/home/test/t508n/work/TASK-A-deepdive-SSPHY-clock.md` | SS PHY 寄存器序列和 USB 时钟表 |
| `/home/test/t508n/work/port-skeleton/` | USB 相关最小驱动骨架 |
| `/home/test/t508n/WORK-适配材料清单.md` | 原三线任务书；其中 USB/DTS/外设部分当前不启用 |
| `/home/test/t508n/prompt-stock-lock-analysis.md` | lock 分析任务书，已完成 |
| `/home/test/t508n/prompt-stock-sync-analysis.md` | sync 分析任务书，已完成 |
| `/home/test/t508n/work/recover_kallsyms.py` | kallsyms 恢复脚本，已完成使命 |
| `/home/test/t508n/work/patch-0001-lse-early-correct.patch` | LSE early patch 修正补丁，供追溯 |

### 3.3 原始材料

| 路径 | 内容 |
|---|---|
| `/home/test/t508n/work/stock_Image_full.bin` | 原厂 38MB 完整 Image，可直接反汇编 |
| `/home/test/t508n/work/stock_Image.bin` | 原厂 8MB boot Image |
| `/home/test/t508n/work/stock_ums9620.dts` | 原厂 DTB 反编译 DTS |
| `/home/test/t508n/work/kernel.config` | 原厂 5.4 config |
| `/home/test/t508n/work/dtbo0.dtb` ~ `dtbo6.dtb` | 原厂 DTBO |
| `/home/test/t508n/payloads/` | SML/SPL/U-Boot/TrustOS 等固件 |
| `/home/test/t508n/session.jsonl` | 原始中断会话日志，一般不要重读 |

---

## 4. 当前 mainline 工作区状态

源码目录：`/home/test/t508n/linux-6.12.103`

- HEAD commit：`cb8e725f3`
  “T508N mainline baseline: framebuffer probes + LSE early patch +
  stop_machine instrumentation”。
- 未提交修改：
  - `arch/arm64/kernel/alternative.c`
    - `__apply_alternatives_multi_stop` 已改原厂式纯 `yield` 轮询；
    - 每核 y=370/390 探针。
  - `kernel/stop_machine.c`
    - 新增 y=410/430/440/450 探针；
    - `cpu_stop_signal_done()` 返回 `bool` 标记最后一个 complete。
- 构建状态：
  - `work/mainline_Image` 与 `boot_linux.img` 是 **08:57** 构建；
  - `kernel/stop_machine.c` 在 **09:04** 才修改；
  - 所以当前 boot image **不包含最新 stop_machine 探针**。
  - 下一步必须先重建，命令见 `HANDOFF-3-SESSIONS.md` Phase 0。

---

## 5. 关键事实（其他 agent 不要重复验证）

- `work/System.map` 不是 stock 符号表；正确的是 `work/stock_kallsyms.map`。
- stock 反汇编映射：
  `_text = 0xffffffc010080000`，`file_off = vaddr - 0xffffffc010080000`。
- stock 无 CCI/CMN/DSU/SCU 一致性初始化；不要加。
- stock 二级核等待 alternatives flag：`ldr + yield`，无 dcache/dsb/wfe/sev。
- T770 PoC 一致性本身可用；不要再怀疑 cache 硬件。
- mainline 已恢复完整 `apply_alternatives_all()` 和 LSE early patch。
- 当前只差：重建刷机、读屏判定、必要时定位 driver core 卡点。

---

## 6. 其他 agent 的第一批动作建议

1. 读 `HANDOFF-3-SESSIONS.md` 全文。
2. 看 git 状态：
   ```bash
   cd /home/test/t508n/linux-6.12.103
   git status --short
   git diff -- arch/arm64/kernel/alternative.c kernel/stop_machine.c
   ```
3. 确认是否已有人重新构建：
   ```bash
   stat /home/test/t508n/work/mainline_Image /home/test/t508n/boot_linux.img
   ```
   如果构建时间仍早于 `stop_machine.c` 修改时间，则不要直接刷机。
4. 按 `HANDOFF-3-SESSIONS.md` 的 Phase 0~4 执行。
5. 不要自行扩展范围到 USB/eMMC/pinctrl/PMIC 等外设。
