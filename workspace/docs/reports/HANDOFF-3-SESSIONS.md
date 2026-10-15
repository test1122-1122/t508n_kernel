# T508N 内核跑通专项交接文档（范围：忽略 USB 及一切外设）

更新：2026-08-17 09:15。
范围修正：**本阶段只做“内核本身跑通”，不碰 USB、eMMC/SD、I2C/SPI、pinctrl/GPIO、
DRM/DSI、PMIC/regulator、watchdog 等外设。** 前面积累的 USB/DTS 外设资料全部归档，
等内核跑通后再启用。

## 0. 目标定义

“内核跑通”的验收标准：

1. boot CPU + 8 个二级核上线；
2. LSE early patch 与系统级 `apply_alternatives_all()` 完整执行；
3. `do_basic_setup → driver_init → memory_dev_init → device_register` 不再卡住；
4. 其余 initcall 走完，`kernel_init` 成功挂载内置 initramfs 并执行 `/init`；
5. 屏幕上出现 initramfs 的启动标记（`init` 已写 "T508N init started" 到 kmsg）
   或探针显示等价位置。

外设驱动**不要求**：USB、eMMC、MMC/SD、I2C、SPI、pinctrl、GPIO、DRM、PMIC、WDT
都不需要为使内核到达 `/init` 而启用。它们不是当前阻塞点。

## 1. 当前阻塞点与已知事实（不要重复验证）

- 8 核 SMP init 已通；卡点曾出现在 `apply_alternatives_all()/stop_machine`，
  以及 `do_basic_setup → driver_init → memory_dev_init → add_memory_block →
  device_register`，且间歇漂移。
- `work/System.map` 是 mainline 旧构建产物，不能用于 stock 分析；正确符号表是
  `work/stock_kallsyms.map`（116,704 个原厂符号）。
- stock 反汇编映射：`_text=0xffffffc010080000`，
  `file_off = vaddr - 0xffffffc010080000`，
  objdump 加 `--adjust-vma=0xffffffc010080000`。
- stock device/sysfs 路径**没有** T770 特判；stock 内核/DTS/SML 也**没有**
  CCI/CMN/DSU/SCU 一致性初始化，不要补这些。
- stock 普通 cache 维护只用 `ic ivau`；`__apply_alternatives` 尾部有一个
  条件执行的 `ic ialluis`。mainline 侧继续采用原厂式 `ic ivau`。
- stock `__apply_alternatives_multi_stop` 二级核等待是 `ldr + yield` 纯轮询，
  **无 dcache/dsb/wfe/sev**；CPU0 普通 `str` 置位。
  这证明 T770 PoC 一致性本身可用，不要再怀疑 cache 硬件。
- mainline 最大嫌疑：之前跳过系统级 `apply_alternatives_all()`，导致
  12,074 条 LSE atomics、99 条 repeat-TLBI、24 条 speculative-AT 未 patch。
  当前源码已恢复 `stop_machine()` 全量 patch。
- LSE early patch 已在 `smp_prepare_boot_cpu()` 的 IRQ-off 单核阶段执行，
  并用 `caches_clean_inval_pou(_stext,_etext)` 做 VA I-cache 失效。

## 2. 当前 mainline 工作区状态（先读这里）

- HEAD：`cb8e725f3`（framebuffer probes + LSE early patch + stop_machine instrumentation）。
- 未提交修改：
  - `arch/arm64/kernel/alternative.c`：`__apply_alternatives_multi_stop` 已改为
    stock 式 `READ_ONCE + cpu_relax()` 纯 yield 轮询；每核 y=370/390 探针。
  - `kernel/stop_machine.c`：y=410/430/440/450 探针，标记最后一个 `complete()`。
- **构建状态：`work/mainline_Image` 与 `boot_linux.img` 是 08:57 构建，只包含
  alternative.c 修改；不包含 09:04 的 stop_machine.c 修改。必须先重新构建。**
- 探针坐标：
  - y=120：apply_alternatives_all 路径
    （x=8 vdso后；x=80 CPU0 `__apply_alternatives` 返回；x=44 stop_machine 返回）。
  - y=370：每个 CPU 进入 `__apply_alternatives_multi_stop`。
  - y=390：每个 CPU 的 multi_stop fn 完成。
  - y=410：`cpu_stopper_thread` 中每个 CPU fn 返回。
  - y=430：每个 CPU `cpu_stop_signal_done()` 后。
  - y=450：最后一个把 `nr_todo` 减到 0、调用 `complete()` 的 CPU。
  - y=440：`__stop_cpus` 中 `wait_for_completion` 前/后。

## 3. 内核跑通专项计划（按顺序执行，单变量验证）

### Phase 0：重建当前版本并刷机
```bash
cd /home/test/t508n/linux-6.12.103
make Image -j$(nproc)
cp arch/arm64/boot/Image /home/test/t508n/work/mainline_Image
cd /home/test/t508n
python3 tools/sprd_mkbootimg.py --kernel work/mainline_Image \
  --ramdisk initramfs.cpio --output boot_linux.img --key tools/ot5_boot_key.pem
# fastboot flash boot_a boot_linux.img && fastboot reboot
```
记录：进度条格数、每行亮点（特别是 y=120/370/390/410/430/440/450）。

### Phase 1：先判定 alternatives/stop_machine 是否通过

按读屏结果走决策树：

1. **8 个 CPU 都亮 y=370、y=390，y=410/430 全亮，y=450 亮，y=440 前后都亮，
   且 y=120 三个点都亮** → alternatives 阶段通过，进入 Phase 2。
2. **有 CPU 没有 y=370** → 该核没有进入 multi_stop，问题在 stop_machine 派发：
   查 `__stop_cpus/queue_stop_cpus_work/cpu_stopper_thread` 与 IPI；
   检查 `cpu_online_mask`、stop 队列、stopper 线程是否正常唤醒。
3. **有 CPU 亮 y=370 但不亮 y=390** → 该核卡在
   `while (!all_alternatives_applied) cpu_relax()`。
   先打印/探针确认它读到的 flag 地址和值；若地址一致仍读不到，
   再试 `atomic_t + smp_store_release/load_acquire` 对照版。
4. **y=390 全亮但 y=410/430 不全亮** → 卡在 `cpu_stopper_thread` 返回路径或
   `cpu_stop_signal_done()`，查 stop_machine 完成状态机，不是 alternatives。
5. **y=430 全亮但 y=450 不亮、y=440 只亮前面** → 完成计数/complete 同步问题，
   检查 `atomic_dec_and_test` 与 LSE patch 是否真正生效。

结论必须落到：是“派发/IPI 问题”还是“flag 可见性问题”还是“完成计数问题”。

### Phase 2：alternatives 通过后，若仍卡 driver core，继续定位卡点

1. 在 core 路径增加 framebuffer 探针：
   `do_basic_setup → driver_init → memory_dev_init → add_memory_block →
   device_register → device_add → kobject_add → bus_add_device`。
   一次只标一个阶段，避免 `.idmap.text` 超限和屏幕过密。
2. 对照 stock 反汇编结论：这些函数 stock 无 dsb/dmb/dc/ic/tlbi 特判，
   mainline 也不应有额外屏障；先定位精确卡点，再判断是同步问题还是驱动问题。
3. 单核隔离实验：bootargs 加 `maxcpus=1`（或临时不启动二级核）验证
   driver core 是否单核可过。
   - 单核可过、8 核卡 → 跨核同步/调度窗口问题；
   - 单核也卡 → 与 SMP 同步无关，查 initcall/driver core 本身。
4. 确认系统级 alternatives 真的应用成功：
   - 检查 `system_cpucaps` 中 `ARM64_HAS_LSE_ATOMICS`、
     `ARM64_WORKAROUND_REPEAT_TLBI`、`ARM64_WORKAROUND_SPECULATIVE_AT`
     是否置位；
   - 必要时在 `__apply_alternatives` 完成后打印已应用 cap。

### Phase 3：最小 config 收尾（只动内核跑通相关项）

- 保持已开：`CONFIG_ARM64_ERRATUM_1530923/3194386/4118414`。
- 待验证项：`CONFIG_ARM64_ERRATUM_2441007=y`（A55 repeat-TLBI）。
  建议先测当前版本，再单独开这项重测，避免变量混淆。
- 确认 initramfs 路径所需项已开：`CONFIG_BLK_DEV_INITRD`、
  `CONFIG_DEVTMPFS`、`CONFIG_DEVTMPFS_MOUNT`。
- **不要**为了跑通内核去开 USB/eMMC/pinctrl/PMIC 等外设 config。
- 不要新增 CCI/CMN/DSU DTS 节点；不要恢复无条件 `ic ialluis`。

### Phase 4：验收与记录

- 记录测试矩阵：git diff 版本、config、Image 构建时间、屏幕读数。
- 一旦 `/init` 执行并输出启动标记，视为本阶段完成。
- 更新 `REPORT-stock-sync-mechanism.md` 的“还需要哪些分析”章节，
  把在线实测结果回填。

## 4. 当前成果（仅保留内核跑通相关）

- `work/stock_kallsyms.map`：原厂 116,704 符号。
- `work/REPORT-T508N-stock-analysis.md`：A/B/C/D 四节，核心结论见上。
- `work/REPORT-stock-vs-mainline-locks.md`：锁/原子/多核同步/启动时序对比。
- `work/REPORT-stock-sync-mechanism.md`：stop_machine/alternatives 同步协议，
  原厂纯 yield 轮询结论和推荐方案。
- `work/patch-0001-lse-early-correct.patch`：LSE early patch 修正。
- `SKILL-T508N-framebuffer-debug.md`：探针 API、刷机读屏约定、已知坑。

以下资料**归档，本阶段不启用**：
- `work/TASK-A-B-USB-DTS-C-离线成果.md`
- `work/TASK-A-deepdive-SSPHY-clock.md`
- `work/port-skeleton/`
- `WORK-适配材料清单.md` 中任务 A（USB）、任务 B（USB DTS）及所有外设相关小节。

## 5. 红线

- 不用 `work/System.map` 做 stock 分析。
- 不重新做已完成的反汇编分类和 kallsyms 恢复。
- 不碰 USB、eMMC、pinctrl、GPIO、PMIC、DRM 等外设。
- 不加 CCI/CMN/DSU 初始化。
- 不用无条件 `ic ialluis`；I-cache 按 VA（`ic ivau`）失效。
- 不在一次刷机里同时改多个变量。
