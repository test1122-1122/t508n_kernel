# T508N 会话压缩上下文（2026-08-16 22:35，turn 14 继续点）

> 来源：`/home/test/t508n/session.jsonl`（20,340 行，turn 12/13/14 因上下文超限失败）。
> 这份文件用于替换长对话历史，新 session 从这里继续。

## 1. 项目背景

- 设备：TCL T508N（OT5），Unisoc T770/UMS9620，4×A55 + 4×A76，Android 13，原厂内核 5.4.210。
- 目标：mainline 6.12.103 内核 bring-up。
- 调试限制：无 UART、无 USB；只有 LCD/framebuffer 探针 + fastboot。
- 初始卡点：8 核 SMP init 已通，但 `do_basic_setup → driver_init → memory_dev_init → add_memory_block → device_register` 间歇性卡死，卡点漂移（of_core_init sysfs、stop_machine 也卡过）。

## 2. 已确认、不要再重复验证的关键事实

### 2.1 反汇编映射（最重要）
- `work/System.map` 是 **mainline 6.12 旧构建产物，不是原厂符号表**，不能用。
- 已从 `stock_Image_full.bin` 内 kallsyms 恢复真实原厂符号表：
  `work/stock_kallsyms.map`，116,704 个符号。
- 原厂 `_text/_head = 0xffffffc010080000`，`_stext = 0xffffffc010081000`。
- 正确映射：`file_off = vaddr - 0xffffffc010080000`。
- 正确 objdump：
```bash
aarch64-none-linux-gnu-objdump -D -b binary -m aarch64 \
  --adjust-vma=0xffffffc010080000 \
  --start-address=<stock_kallsyms.map 地址> --stop-address=<+n> \
  work/stock_Image_full.bin
```
- 交叉工具链：`/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin/`。

### 2.2 原厂 vs mainline 核心结论
- 原厂 device model/sysfs 路径无 T770 特判：无额外 dsb/dmb/isb/dc/ic/tlbi。
- 原厂普通 cache 维护只用 `ic ivau`；但 `__apply_alternatives` 尾部有条件执行的 `ic ialluis`。
- 原厂 DTS/kallsyms/SML 均无 CCI/CMN/DSU/SCU/snoop 一致性初始化；不要加这些节点/驱动。
- mainline 当前曾整体跳过 `apply_alternatives_all()`，导致 12,074 条 `ARM64_HAS_LSE_ATOMICS`、99 条 repeat-TLBI、24 条 speculative-AT 等系统级 alternatives 未 patch；这是最可疑根因。
- 建议 config：保留 mainline 已开的 A55/A76 errata，额外开 `CONFIG_ARM64_ERRATUM_2441007=y`。
- LSE early patch 已移到 IRQ-off 单核阶段（`t508n_apply_lse_early()`），并用 `caches_clean_inval_pou(_stext,_etext)` 按 VA 失效 I-cache；方向正确。

### 2.3 原厂与 mainline 的 stop_machine/alternatives 同步（turn 12 最新结论）
- 原厂 `__apply_alternatives_multi_stop = 0xffffffc010114c0c`。
- 原厂二级核等待循环：`ldr` + `yield` 纯轮询，**没有 dcache/dsb/wfe/sev**；退出后 `isb`。
- 原厂 CPU0 路径：`__apply_alternatives()` 返回后普通 `str all_alternatives_applied=1`，无显式 dcache clean、无 sev；仅继承 `__apply_alternatives` 尾部的 `dsb ish / 条件 ic ialluis / dsb ish / isb`。
- 原厂 stop_machine 完成计数：LSE `ldaddal`/`casa` + LL/SC fallback；与 mainline 同源。
- 结论：原厂协议比当前 mainline 更裸却能跑，说明 T770 PoC 一致性本身可用；如果当前 `dcache_clean_poc + dcache_inval_poc` 版本仍只有一半 CPU 亮，**优先怀疑二级核是否进入该函数、flag 地址是否一致、stop_machine 派发/IPI 状态机**，而不是 cache 硬件坏了。
- 推荐方案：二级核 `while (!READ_ONCE(all_alternatives_applied)) cpu_relax(); isb();`；CPU0 最多保留一次 `dcache_clean_poc + dsb(ishst)`，不要每轮 invalidate；更稳妥可用 `atomic_t + smp_store_release/load_acquire`。

## 3. 已交付文件

| 文件 | 内容 |
|---|---|
| `work/stock_kallsyms.map` | 原厂 116,704 符号表（已恢复） |
| `work/recover_kallsyms.py` | kallsyms 恢复脚本 |
| `work/REPORT-T508N-stock-analysis.md` | A/B/C/D 四节：符号表纠正、device path、errata、CCI/CMN、alternatives |
| `work/REPORT-stock-vs-mainline-locks.md` | 锁/mutex/rwsem/多核同步/启动时序/config 对比 |
| `work/REPORT-stock-sync-mechanism.md` | **最新报告**：stop_machine/alternatives 同步机制 + 可移植方案 |
| `work/TASK-A-B-USB-DTS-C-离线成果.md` | USB 驱动/DTS/LSE patch 第一轮成果 |
| `work/TASK-A-deepdive-SSPHY-clock.md` | SS PHY 寄存器序列 + 时钟表 |
| `work/port-skeleton/` | sprd_usbm、SS PHY、dummy PHY、USB clock 最小驱动骨架 |
| `work/patch-0001-lse-early-correct.patch` | LSE early patch 时机修正补丁 |

## 4. 会话任务历史

1. **turn 1**（初始任务）：分析 stock 镜像，验证跨 cluster cache/同步问题；完成 A/B/C/D 大部分分析。
2. **turn 3-6**：纠正 System.map 错误、恢复真实 kallsyms、完成 `REPORT-T508N-stock-analysis.md`。
3. **turn 7-8**：接取 `WORK-适配材料清单.md` 任务 A/B/C；反汇编原厂 USB 模块（vendor_dlkm 中为未剥离 .ko），产出 USB/DTS/时钟/PHY 骨架。
4. **turn 9**：解释 HS PHY = USB 2.0 High-Speed PHY；T508N 有 MUSB+HS PHY 与 DWC3+SS PHY 两条 USB 路线。
5. **turn 10**（未完成，被新任务打断）：分析“除 USB 外还缺什么”。已查明 mainline `.config` 缺 `MMC_SDHCI_SPRD/SPRD_DMA/I2C_SPRD/SPI_SPRD/PINCTRL_SPRD_SC9860/GPIO_SPRD/EIC/DRM_SPRD` 等；PMIC/regulator 相关也未启用；最小 initramfs 仅 mount proc/sysfs/devtmpfs + devmem 关 WDT。
6. **turn 11**：执行 `prompt-stock-lock-analysis.md`，产出 `REPORT-stock-vs-mainline-locks.md`。
7. **turn 12**：执行 `prompt-stock-sync-analysis.md`，产出 `REPORT-stock-sync-mechanism.md`（22:32:46 写完），但**最终文字回复因上下文超限未发出**。
8. **turn 13/14**：用户粘贴上下文错误后说“继续”，仍需给出 turn 12 的最终汇报。

## 5. 当前继续点（范围：只跑通内核，忽略 USB 及一切外设）

- 最新用户意图：`prompt-stock-sync-analysis.md` 已执行，报告已写；当前唯一目标是
  **让内核本身跑通到 initramfs `/init`**。
- 当前代码：`__apply_alternatives_multi_stop` 已改为原厂式纯 `yield` 轮询，
  `apply_alternatives_all()` 已恢复完整 `stop_machine()`；
  `kernel/stop_machine.c` 又新增了完成路径探针（y=410/430/440/450）。
- **重要：`boot_linux.img`（08:57）不包含 09:04 的 stop_machine.c 修改，需重新
  `make Image` 并打包后再刷机。**
- 执行计划与决策树见 `work/HANDOFF-3-SESSIONS.md`。
- 不要重做已完成的 5 个任务；不要重跑大 objdump 分类；不要碰 USB/外设。

## 6. 下一步候选（仅内核跑通）

1. 重建 Image/boot_linux.img，刷机读屏，按 y=120/370/390/410/430/440/450
   判断 alternatives/stop_machine 是否通过。
2. 若通过仍卡：探针定位 `do_basic_setup → driver_init → memory_dev_init →
   device_register` 精确卡点；单核 `maxcpus=1` 做隔离对照。
3. 若 stop_machine 卡：先区分是派发/IPI 问题、flag 可见性问题还是完成计数问题；
   必要时再试 `atomic_t + smp_store_release/load_acquire`。
4. 单独开 `CONFIG_ARM64_ERRATUM_2441007=y` 做第二变量测试。
5. 不要继续怀疑 PoC/cache；USB/eMMC/pinctrl/PMIC 等外设全部归档，本阶段不处理。
