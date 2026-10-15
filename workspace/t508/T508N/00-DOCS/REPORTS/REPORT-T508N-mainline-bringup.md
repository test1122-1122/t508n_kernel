# T508N mainline Linux bring-up 汇总（截至 smp_init 之后）

> 本文记录 T508N（TCL OT5，Unisoc T770/UMS9620）上 mainline 6.12.103 内核的
> bring-up 进度、根因分析、临时 workaround 与调试方法。
> 与逆向报告 `REPORT-T508N-启动流程分析.md` 互补。

---

## 1. 当前进度（一个画面看清）

| 阶段 | 状态 |
|---|---|
| 二级核开 MMU + secondary_start_kernel | ✅ 完整走通（8 核全部上线进 idle） |
| smp_init / bringup_nonboot_cpus | ✅ 完成（进度条第 10 格亮） |
| setup_system_capabilities / alternatives | ⚠️ 跳过内核 alternatives 遍历 patch（workaround） |
| do_basic_setup → driver_init | ⚠️ 卡在 `memory_dev_init` → `add_memory_block` → `device_register` |
| exec `/init`（initramfs） | ❌ 未到达（屏幕底部无 4 条白线） |

**当前卡点**：`driver_init()` 里 `of_core_init` 已过，卡在
`memory_dev_init()` 的 `add_boot_memory_block()` 循环内的 `device_register()`。

---

## 2. 关键根因与 workaround（代码改动清单）

以下改动都在 `linux-6.12.103/` 中，是**让内核能走到 smp_init 之后**的关键。

### 2.1 `icache_inval_all_pou()` —— no-op（跳过 icache 失效）
- 文件：`arch/arm64/include/asm/cacheflush.h`
- 根因：mainline 用 `ic ialluis`（inner-shareable 广播全部失效），在 T770 上 **hang**。
  改成非广播 `ic iallu` 后指令能过，但"全部失效"会清掉 boot CPU 自身 I-cache，
  重新取指被 patch 代码时又卡。
- 原厂对照：反汇编 `stock_Image_full.bin`，**原厂 5.4 内核完全不用 `ic iallu`/`ic ialluis`**，
  只用 `ic ivau`（按 VA 逐个失效）。
- 处理：`icache_inval_all_pou()` 改成只 `dsb(ish)`，不失效 I-cache。
  （alternative 是可选优化，未 patch 代码也安全。）

### 2.2 `apply_alternatives_all()` —— 跳过内核 alternatives 遍历 patch
- 文件：`arch/arm64/kernel/alternative.c`
- 根因：`stop_machine()` 因 **跨 cluster cache 一致性 / 原子同步问题** 卡住
  （加探针能过、不加就卡，间歇性竞态）。绕过 stop_machine 直接调
  `__apply_alternatives()` 后，仍卡在**内核那一次的遍历循环**（VDSO 那次能过）。
- 处理：`apply_alternatives_all()` 里跳过内核 `__apply_alternatives()` 遍历，
  仅保留 `apply_alternatives_vdso()`。

### 2.3 `enable_cpu_capabilities()` 的 stop_machine —— 能过（无需改）
- `enable_cpu_capabilities()` 内部也调 `stop_machine(cpu_enable_non_boot_scope_capabilities)`,
  实测能正常返回（y=140 亮 2），未改。

### 2.4 早期已确认的 workaround（沿用）
- 关 PAC/MTE/SVE/EPAN/BTI；VA_BITS=39 / PA_BITS=48。
- `cpu_uninstall_idmap()` 延迟到 `paging_init()` 之后。
- noop_dtbo（board_id 0x2700）避免 stock DTBO merge 破坏 DT。
- EL2 virtual timer 移植（hyp-virt PPI 12，参考 `ums9620-ref/series-v3.mbox`）。

---

## 3. 根因推断（未最终定位）

二级核、8 核上线、smp_init 都已走通；剩下的卡点（`of_core_init` 的 sysfs attach、
`memory_dev_init` 的 `device_register`）都落在 **sysfs / device model 初始化**，
且呈**间歇性漂移**（每次启动卡点不同）。

推断与之前的 `stop_machine` 卡住同源：
**T770 的跨 cluster cache 一致性 / 原子操作 / 同步存在硬件/固件层面的问题**
（可能缺正确的 CCI/CMN 互联初始化，或某 errata workaround 未生效）。

待验证方向：
- 查 T770/UMS9620 的互联初始化（CCI/CMN）在 mainline/SML 中的配置。
- 检查跳过的 alternatives 里是否含与锁/原子/内存屏障相关的 errata patch。

---

## 4. 调试方法（无串口 framebuffer 探针法）

见配套 skill `SKILL-T508N-framebuffer-debug.md`。核心：

- 帧缓冲：**32bpp BGRA，320×480，stride 1280，base 0xb0000000，白=0xFFFFFFFF**。
- 探针 API：
  - `t508n_paint_bar(x, y, w, h)`（C，走 phys_to_virt + dcache_clean）
  - `t508n_sec_paint_xy x, y`（汇编，物理地址直接写，MMU 关时用）
  - `t508n_mark_row(y, n)`（行号标注：末尾 n 个小方块 = 第 n 行）
- 二分流程：加探针 → `make Image` → `sprd_mkbootimg.py` 打包 → fastboot 刷 boot_a → 读屏。

---

## 5. 文件来源区分（重要）

| 目录/文件 | 性质 |
|---|---|
| `work/`（stock_Image.bin、stock_Image_full.bin、System.map、kernel.config、stock_ums9620.dts、dtbo0-6.dtb、ot5.dtb） | **T508N 设备本身**的逆向产物（从设备分区镜像提取） |
| `~/ums9620-ref/`（series-v3.mbox、ums9620.dtsi、arm-arch-timer.yaml、patch-*.txt） | **UMS9620 参考开发板**的公开资料（Marc Zyngier 的 EL2 virtual timer patch series + 参考 DTS），**不是 T508N 的内容**，仅作移植参考 |

> ⚠️ 特别注意：`~/ums9620-ref/ums9620.dtsi` 是 UMS9620 参考平台的 DTS 片段，
> 与 T508N 板级 `linux-6.12.103/arch/arm64/boot/dts/sprd/ums9620-ot5.dts`
> （#include 主线的 `ums9620.dtsi`）是**两回事**，不要混淆。

---

## 6. 下一步

1. 定位 `device_register()` 卡住的子步骤（kobject_add / bus_add_device / sysfs 属性）。
2. 若确认是跨 cluster 同步问题，查互联初始化或 errata workaround。
3. 打通 USB（移植 SPRD USB PHY + DWC3 glue + DTS 节点）后可用 adb，替代屏幕观察。
