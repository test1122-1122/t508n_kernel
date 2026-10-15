# 路径映射表（旧 → 新）

> 2026-09-29 工作区重组后生成。**历史报告/文档中的旧绝对路径一律保留原文**（不篡改溯源），
> 按本表换算即可。脚本类的硬编码路径已同步修复（见文末）。

## 1. 顶层路径映射

除下表外，下列两项是“内容合并”而非整体改名：

| 旧路径 | 新路径 | 说明 |
|---|---|---|
| `/home/test/t508n/dl/` | `/home/test/t508n/shared/dl/` | 下载缓存，4 个压缩包并入既有目录 |
| `/home/test/t508n/tools/` | `/home/test/t508n/t508/tools/` | 构建工具并入，`restore_stock.bat`、`fetch_ums9620_ref.sh` 也归入此处 |

| 旧路径 | 新路径 |
|---|---|
| `/home/test/t508n/AGENTS-GUIDE.md` | `/home/test/t508n/t508/docs/AGENTS-GUIDE.md` |
| `/home/test/t508n/FINAL-BRINGUP-INDEX.md` | `/home/test/t508n/t508/docs/FINAL-BRINGUP-INDEX.md` |
| `/home/test/t508n/Image` | `/home/test/t508n/t508/firmware/stock-images` |
| `/home/test/t508n/KERNEL-适配方案.md` | `/home/test/t508n/t508/docs/KERNEL-适配方案.md` |
| `/home/test/t508n/REPORT-T508N-mainline-bringup.md` | `/home/test/t508n/t508/docs/reports/REPORT-T508N-mainline-bringup.md` |
| `/home/test/t508n/REPORT-T508N-启动流程分析.md` | `/home/test/t508n/t508/docs/reports/REPORT-T508N-启动流程分析.md` |
| `/home/test/t508n/REPORT-UMS9620-mainline适配.md` | `/home/test/t508n/t508/docs/reports/REPORT-UMS9620-mainline适配.md` |
| `/home/test/t508n/SKILL-T508N-framebuffer-debug.md` | `/home/test/t508n/t508/docs/SKILL-T508N-framebuffer-debug.md` |
| `/home/test/t508n/WORK-适配材料清单.md` | `/home/test/t508n/t508/docs/WORK-适配材料清单.md` |
| `/home/test/t508n/aarch64-linux-musl-cross` | `/home/test/t508n/shared/toolchains/aarch64-linux-musl-cross` |
| `/home/test/t508n/alpine-minirootfs-3.24.1-aarch64.tar.gz` | `/home/test/t508n/t508/build/packages/alpine-minirootfs-3.24.1-aarch64.tar.gz` |
| `/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu` | `/home/test/t508n/shared/toolchains/arm-gnu-toolchain-13.2` |
| `/home/test/t508n/boot_54custom.img` | `/home/test/t508n/t508/build/boot-images/boot_54custom.img` |
| `/home/test/t508n/boot_54custom_builtin.img` | `/home/test/t508n/t508/build/boot-images/boot_54custom_builtin.img` |
| `/home/test/t508n/boot_54custom_probe.img` | `/home/test/t508n/t508/build/boot-images/boot_54custom_probe.img` |
| `/home/test/t508n/boot_54vendor.img` | `/home/test/t508n/t508/build/boot-images/boot_54vendor.img` |
| `/home/test/t508n/boot_54vendor.img.bak.1787922677` | `/home/test/t508n/t508/build/boot-images/boot_54vendor.img.bak.1787922677` |
| `/home/test/t508n/boot_linux.img` | `/home/test/t508n/t508/build/boot-images/boot_linux.img` |
| `/home/test/t508n/busybox-1.36.1` | `/home/test/t508n/t508/src/busybox-1.36.1` |
| `/home/test/t508n/decomp-analysis` | `/home/test/t508n/t508/work/decomp-analysis` |
| `/home/test/t508n/e2fsprogs-libs.apk` | `/home/test/t508n/t508/build/packages/e2fsprogs-libs.apk` |
| `/home/test/t508n/e2fsprogs-static.apk` | `/home/test/t508n/t508/build/packages/e2fsprogs-static.apk` |
| `/home/test/t508n/e2fsprogs.apk` | `/home/test/t508n/t508/build/packages/e2fsprogs.apk` |
| `/home/test/t508n/fetch_ums9620_ref.sh` | `/home/test/t508n/t508/tools/fetch_ums9620_ref.sh` |
| `/home/test/t508n/initramfs.cpio` | `/home/test/t508n/t508/build/initramfs/initramfs.cpio` |
| `/home/test/t508n/initramfs_root` | `/home/test/t508n/t508/firmware/initramfs_root` |
| `/home/test/t508n/libblkid.apk` | `/home/test/t508n/t508/build/packages/libblkid.apk` |
| `/home/test/t508n/libcom_err.apk` | `/home/test/t508n/t508/build/packages/libcom_err.apk` |
| `/home/test/t508n/libeconf.apk` | `/home/test/t508n/t508/build/packages/libeconf.apk` |
| `/home/test/t508n/libssl-dev_3.5.5-1ubuntu3_amd64.deb` | `/home/test/t508n/shared/packages/libssl-dev_3.5.5-1ubuntu3_amd64.deb` |
| `/home/test/t508n/libuuid.apk` | `/home/test/t508n/t508/build/packages/libuuid.apk` |
| `/home/test/t508n/linux-6.12.103` | `/home/test/t508n/t508/src/linux-6.12.103` |
| `/home/test/t508n/lk-upstream-ref` | `/home/test/t508n/t508/src/lk-upstream-ref` |
| `/home/test/t508n/make_4.3-4.1build2_amd64.deb` | `/home/test/t508n/shared/packages/make_4.3-4.1build2_amd64.deb` |
| `/home/test/t508n/openssl-local` | `/home/test/t508n/shared/host/openssl-local` |
| `/home/test/t508n/payloads` | `/home/test/t508n/t508/firmware/payloads` |
| `/home/test/t508n/prompt-54-probe-port.md` | `/home/test/t508n/t508/docs/prompts/prompt-54-probe-port.md` |
| `/home/test/t508n/prompt-a76-requirements.md` | `/home/test/t508n/t508/docs/prompts/prompt-a76-requirements.md` |
| `/home/test/t508n/prompt-alt-table-integrity.md` | `/home/test/t508n/t508/docs/prompts/prompt-alt-table-integrity.md` |
| `/home/test/t508n/prompt-build-54-custom.md` | `/home/test/t508n/t508/docs/prompts/prompt-build-54-custom.md` |
| `/home/test/t508n/prompt-completion-wakeup-compare.md` | `/home/test/t508n/t508/docs/prompts/prompt-completion-wakeup-compare.md` |
| `/home/test/t508n/prompt-current-race-analysis.md` | `/home/test/t508n/t508/docs/prompts/prompt-current-race-analysis.md` |
| `/home/test/t508n/prompt-disasm-B-sched.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-B-sched.md` |
| `/home/test/t508n/prompt-disasm-C-irq.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-C-irq.md` |
| `/home/test/t508n/prompt-disasm-D-mm.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-D-mm.md` |
| `/home/test/t508n/prompt-disasm-E-devm.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-E-devm.md` |
| `/home/test/t508n/prompt-disasm-F-fs.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-F-fs.md` |
| `/home/test/t508n/prompt-disasm-a-cpu.md` | `/home/test/t508n/t508/docs/prompts/prompt-disasm-a-cpu.md` |
| `/home/test/t508n/prompt-final-index.md` | `/home/test/t508n/t508/docs/prompts/prompt-final-index.md` |
| `/home/test/t508n/prompt-lse-atomic-completion.md` | `/home/test/t508n/t508/docs/prompts/prompt-lse-atomic-completion.md` |
| `/home/test/t508n/prompt-mainline-completion-wakeup.md` | `/home/test/t508n/t508/docs/prompts/prompt-mainline-completion-wakeup.md` |
| `/home/test/t508n/prompt-stock-alternatives-waiter.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-alternatives-waiter.md` |
| `/home/test/t508n/prompt-stock-crosscluster-visibility.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-crosscluster-visibility.md` |
| `/home/test/t508n/prompt-stock-driver-init.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-driver-init.md` |
| `/home/test/t508n/prompt-stock-lock-analysis.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-lock-analysis.md` |
| `/home/test/t508n/prompt-stock-memory-dev-init.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-memory-dev-init.md` |
| `/home/test/t508n/prompt-stock-of-core-init.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-of-core-init.md` |
| `/home/test/t508n/prompt-stock-page-alloc-init.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-page-alloc-init.md` |
| `/home/test/t508n/prompt-stock-sync-analysis.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-sync-analysis.md` |
| `/home/test/t508n/prompt-stock-wakeup-chain.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-wakeup-chain.md` |
| `/home/test/t508n/prompt-stock-wakeup-nohz.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-wakeup-nohz.md` |
| `/home/test/t508n/prompt-stock-zone-memory.md` | `/home/test/t508n/t508/docs/prompts/prompt-stock-zone-memory.md` |
| `/home/test/t508n/prompt-summary-all.md` | `/home/test/t508n/t508/docs/prompts/prompt-summary-all.md` |
| `/home/test/t508n/prompt-unisoc-mods-on-612.md` | `/home/test/t508n/t508/docs/prompts/prompt-unisoc-mods-on-612.md` |
| `/home/test/t508n/prompt-unisoc-only-configs.md` | `/home/test/t508n/t508/docs/prompts/prompt-unisoc-only-configs.md` |
| `/home/test/t508n/prompt-verify-kick-clean.md` | `/home/test/t508n/t508/docs/prompts/prompt-verify-kick-clean.md` |
| `/home/test/t508n/prompt-verify-mem-order.md` | `/home/test/t508n/t508/docs/prompts/prompt-verify-mem-order.md` |
| `/home/test/t508n/prompt-verify-nohz-kick.md` | `/home/test/t508n/t508/docs/prompts/prompt-verify-nohz-kick.md` |
| `/home/test/t508n/prompt-verify-sgi-kfence.md` | `/home/test/t508n/t508/docs/prompts/prompt-verify-sgi-kfence.md` |
| `/home/test/t508n/restore_stock.bat` | `/home/test/t508n/t508/tools/restore_stock.bat` |
| `/home/test/t508n/session.jsonl` | `/home/test/t508n/t508/docs/session/session.jsonl` |
| `/home/test/t508n/t508n-repo` | `/home/test/t508n/t508/src/t508n-repo` |
| `/home/test/t508n/v50-linux` | `/home/test/t508n/v50/src/mu300-linux` |
| `/home/test/t508n/vendor_boot_linux.img` | `/home/test/t508n/t508/build/boot-images/vendor_boot_linux.img` |
| `/home/test/t508n/work-bak-local` | `/home/test/t508n/t508/work/work-bak-local` |
| `/home/test/t508n/无串口验证指南.md` | `/home/test/t508n/t508/docs/无串口验证指南.md` |

## 2. `work/` 内条目映射

原 `work/` 同时混放两个项目的资料。下表为逐项归位结果。

| 旧路径 | 新路径 |
|---|---|
| `/home/test/t508n/work/ARCHIVE-20260823.md` | `/home/test/t508n/t508/docs/reports/ARCHIVE-20260823.md` |
| `/home/test/t508n/work/DIAG-github-mu300-拉取失败诊断.md` | `/home/test/t508n/v50/docs/reports/DIAG-github-mu300-拉取失败诊断.md` |
| `/home/test/t508n/work/DISASM-A-cpu-boot.md` | `/home/test/t508n/t508/work/disasm/DISASM-A-cpu-boot.md` |
| `/home/test/t508n/work/DISASM-B-sched-调度器.md` | `/home/test/t508n/t508/work/disasm/DISASM-B-sched-调度器.md` |
| `/home/test/t508n/work/DISASM-C-irq-中断GIC` | `/home/test/t508n/t508/work/disasm/DISASM-C-irq-中断GIC` |
| `/home/test/t508n/work/DISASM-D-mm-内存管理.md` | `/home/test/t508n/t508/work/disasm/DISASM-D-mm-内存管理.md` |
| `/home/test/t508n/work/DISASM-E-devm-驱动模型sysfs.md` | `/home/test/t508n/t508/work/disasm/DISASM-E-devm-驱动模型sysfs.md` |
| `/home/test/t508n/work/DISASM-F-fs-文件系统VFS.md` | `/home/test/t508n/t508/work/disasm/DISASM-F-fs-文件系统VFS.md` |
| `/home/test/t508n/work/DISASM-G-drv-clk-时钟.md` | `/home/test/t508n/t508/work/disasm/DISASM-G-drv-clk-时钟.md` |
| `/home/test/t508n/work/DISASM-H-drv-pmic-电源.md` | `/home/test/t508n/t508/work/disasm/DISASM-H-drv-pmic-电源.md` |
| `/home/test/t508n/work/DISASM-I-drv-ufs.md` | `/home/test/t508n/t508/work/disasm/DISASM-I-drv-ufs.md` |
| `/home/test/t508n/work/DISASM-J-drv-timer-wdt.md` | `/home/test/t508n/t508/work/disasm/DISASM-J-drv-timer-wdt.md` |
| `/home/test/t508n/work/DISASM-K-mm-slub.md` | `/home/test/t508n/t508/work/disasm/DISASM-K-mm-slub.md` |
| `/home/test/t508n/work/DISASM-L-rcu.md` | `/home/test/t508n/t508/work/disasm/DISASM-L-rcu.md` |
| `/home/test/t508n/work/DISASM-M-workqueue.md` | `/home/test/t508n/t508/work/disasm/DISASM-M-workqueue.md` |
| `/home/test/t508n/work/DISASM-N-cpuidle-cpufreq.md` | `/home/test/t508n/t508/work/disasm/DISASM-N-cpuidle-cpufreq.md` |
| `/home/test/t508n/work/E-devm-bus_add_device.dump` | `/home/test/t508n/t508/work/disasm/E-devm-bus_add_device.dump` |
| `/home/test/t508n/work/E-devm-device_add.dump` | `/home/test/t508n/t508/work/disasm/E-devm-device_add.dump` |
| `/home/test/t508n/work/E-devm-kernfs_add_one.dump` | `/home/test/t508n/t508/work/disasm/E-devm-kernfs_add_one.dump` |
| `/home/test/t508n/work/E-devm-kernfs_new_node.dump` | `/home/test/t508n/t508/work/disasm/E-devm-kernfs_new_node.dump` |
| `/home/test/t508n/work/E-devm-kobject_add_internal.dump` | `/home/test/t508n/t508/work/disasm/E-devm-kobject_add_internal.dump` |
| `/home/test/t508n/work/E-devm-memory.dump` | `/home/test/t508n/t508/work/disasm/E-devm-memory.dump` |
| `/home/test/t508n/work/E-devm-symbols.txt` | `/home/test/t508n/t508/work/disasm/E-devm-symbols.txt` |
| `/home/test/t508n/work/E-devm-uevent_env.dump` | `/home/test/t508n/t508/work/disasm/E-devm-uevent_env.dump` |
| `/home/test/t508n/work/FINAL-WRAPUP-20260821.md` | `/home/test/t508n/t508/docs/reports/FINAL-WRAPUP-20260821.md` |
| `/home/test/t508n/work/FLASH-20260821-P1.md` | `/home/test/t508n/t508/docs/reports/FLASH-20260821-P1.md` |
| `/home/test/t508n/work/GUIDE-vddusb33-stock-design.md` | `/home/test/t508n/t508/docs/reports/GUIDE-vddusb33-stock-design.md` |
| `/home/test/t508n/work/HANDOFF-3-SESSIONS.md` | `/home/test/t508n/t508/docs/reports/HANDOFF-3-SESSIONS.md` |
| `/home/test/t508n/work/PATCH-diag-sgi-kfence.md` | `/home/test/t508n/t508/docs/reports/PATCH-diag-sgi-kfence.md` |
| `/home/test/t508n/work/PATCH-mem-sysfs-order.md` | `/home/test/t508n/t508/docs/reports/PATCH-mem-sysfs-order.md` |
| `/home/test/t508n/work/PATCH-nohz-kick.md` | `/home/test/t508n/t508/docs/reports/PATCH-nohz-kick.md` |
| `/home/test/t508n/work/REPORT-54-custom-build.md` | `/home/test/t508n/t508/docs/reports/REPORT-54-custom-build.md` |
| `/home/test/t508n/work/REPORT-54-probe-port.md` | `/home/test/t508n/t508/docs/reports/REPORT-54-probe-port.md` |
| `/home/test/t508n/work/REPORT-A76-requirements.md` | `/home/test/t508n/t508/docs/reports/REPORT-A76-requirements.md` |
| `/home/test/t508n/work/REPORT-AGENTS-GUIDE-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-AGENTS-GUIDE-analysis.md` |
| `/home/test/t508n/work/REPORT-CURRENT-RACE-ANALYSIS.md` | `/home/test/t508n/t508/docs/reports/REPORT-CURRENT-RACE-ANALYSIS.md` |
| `/home/test/t508n/work/REPORT-F50-B09固件与B16备份分析.md` | `/home/test/t508n/v50/docs/reports/REPORT-F50-B09固件与B16备份分析.md` |
| `/home/test/t508n/work/REPORT-LSE-atomic-completion-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-LSE-atomic-completion-analysis.md` |
| `/home/test/t508n/work/REPORT-ROOTFS-COMPARE.md` | `/home/test/t508n/t508/docs/reports/REPORT-ROOTFS-COMPARE.md` |
| `/home/test/t508n/work/REPORT-SELF-BUILT-USB-CHAIN.md` | `/home/test/t508n/t508/docs/reports/REPORT-SELF-BUILT-USB-CHAIN.md` |
| `/home/test/t508n/work/REPORT-SML-psci-cpu-on.md` | `/home/test/t508n/t508/docs/reports/REPORT-SML-psci-cpu-on.md` |
| `/home/test/t508n/work/REPORT-STOCK-USB-STACK-MECHANISM.md` | `/home/test/t508n/t508/docs/reports/REPORT-STOCK-USB-STACK-MECHANISM.md` |
| `/home/test/t508n/work/REPORT-T508N-stock-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-T508N-stock-analysis.md` |
| `/home/test/t508n/work/REPORT-UNISOC-MODS-ON-612.md` | `/home/test/t508n/t508/docs/reports/REPORT-UNISOC-MODS-ON-612.md` |
| `/home/test/t508n/work/REPORT-UNISOC-ONLY-CONFIGS.md` | `/home/test/t508n/t508/docs/reports/REPORT-UNISOC-ONLY-CONFIGS.md` |
| `/home/test/t508n/work/REPORT-USB-BOOT-FLOW.md` | `/home/test/t508n/t508/docs/reports/REPORT-USB-BOOT-FLOW.md` |
| `/home/test/t508n/work/REPORT-USB-CONSOLIDATED.md` | `/home/test/t508n/t508/docs/reports/REPORT-USB-CONSOLIDATED.md` |
| `/home/test/t508n/work/REPORT-V50-能否使用mu300-linux.md` | `/home/test/t508n/v50/docs/reports/REPORT-V50-能否使用mu300-linux.md` |
| `/home/test/t508n/work/REPORT-alt-table-integrity.md` | `/home/test/t508n/t508/docs/reports/REPORT-alt-table-integrity.md` |
| `/home/test/t508n/work/REPORT-mainline-completion-wakeup-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-mainline-completion-wakeup-analysis.md` |
| `/home/test/t508n/work/REPORT-mu300-linux-对T508N价值评估.md` | `/home/test/t508n/docs/cross/REPORT-mu300-linux-对T508N价值评估.md` |
| `/home/test/t508n/work/REPORT-mu300-linux-项目理解.md` | `/home/test/t508n/v50/docs/reports/REPORT-mu300-linux-项目理解.md` |
| `/home/test/t508n/work/REPORT-stock-alternatives-waiter-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-alternatives-waiter-analysis.md` |
| `/home/test/t508n/work/REPORT-stock-crosscluster-visibility.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-crosscluster-visibility.md` |
| `/home/test/t508n/work/REPORT-stock-driver-init.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-driver-init.md` |
| `/home/test/t508n/work/REPORT-stock-memory-dev-init.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-memory-dev-init.md` |
| `/home/test/t508n/work/REPORT-stock-of-core-init.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-of-core-init.md` |
| `/home/test/t508n/work/REPORT-stock-page-alloc-init.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-page-alloc-init.md` |
| `/home/test/t508n/work/REPORT-stock-sync-mechanism.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-sync-mechanism.md` |
| `/home/test/t508n/work/REPORT-stock-vs-mainline-completion.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-vs-mainline-completion.md` |
| `/home/test/t508n/work/REPORT-stock-vs-mainline-locks.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-vs-mainline-locks.md` |
| `/home/test/t508n/work/REPORT-stock-wakeup-chain-analysis.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-wakeup-chain-analysis.md` |
| `/home/test/t508n/work/REPORT-stock-wakeup-nohz.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-wakeup-nohz.md` |
| `/home/test/t508n/work/REPORT-stock-zone-memory.md` | `/home/test/t508n/t508/docs/reports/REPORT-stock-zone-memory.md` |
| `/home/test/t508n/work/REPORT-vddusb33-variant-matrix-offline.md` | `/home/test/t508n/t508/docs/reports/REPORT-vddusb33-variant-matrix-offline.md` |
| `/home/test/t508n/work/SESSION-CONTEXT-COMPACT.md` | `/home/test/t508n/t508/docs/reports/SESSION-CONTEXT-COMPACT.md` |
| `/home/test/t508n/work/SESSION-SPLIT-AND-WORKFLOW.md` | `/home/test/t508n/t508/docs/reports/SESSION-SPLIT-AND-WORKFLOW.md` |
| `/home/test/t508n/work/SHA256SUMS.tmp` | `/home/test/t508n/v50/work/SHA256SUMS.tmp` |
| `/home/test/t508n/work/SUMMARY-ALL-ANALYSIS.md` | `/home/test/t508n/t508/docs/reports/SUMMARY-ALL-ANALYSIS.md` |
| `/home/test/t508n/work/System.map` | `/home/test/t508n/t508/firmware/stock/System.map` |
| `/home/test/t508n/work/T508N-VENDOR-BUILD-RECIPE.md` | `/home/test/t508n/t508/docs/reports/T508N-VENDOR-BUILD-RECIPE.md` |
| `/home/test/t508n/work/TASK-A-B-USB-DTS-C-离线成果.md` | `/home/test/t508n/t508/docs/reports/TASK-A-B-USB-DTS-C-离线成果.md` |
| `/home/test/t508n/work/TASK-A-deepdive-SSPHY-clock.md` | `/home/test/t508n/t508/docs/reports/TASK-A-deepdive-SSPHY-clock.md` |
| `/home/test/t508n/work/TASK-vddusb33-regulator-fix.md` | `/home/test/t508n/t508/docs/reports/TASK-vddusb33-regulator-fix.md` |
| `/home/test/t508n/work/UNISOC-DETAILED.md` | `/home/test/t508n/t508/docs/reports/UNISOC-DETAILED.md` |
| `/home/test/t508n/work/UNISOC-MODS-AND-CONFIG.md` | `/home/test/t508n/t508/docs/reports/UNISOC-MODS-AND-CONFIG.md` |
| `/home/test/t508n/work/alt_table_records.csv` | `/home/test/t508n/t508/work/disasm/alt_table_records.csv` |
| `/home/test/t508n/work/boot_footer.vbmeta` | `/home/test/t508n/t508/firmware/stock/boot_footer.vbmeta` |
| `/home/test/t508n/work/boot_linux_612-probes-backup.img` | `/home/test/t508n/t508/build/images/boot_linux_612-probes-backup.img` |
| `/home/test/t508n/work/boot_linux_P1-backup.img` | `/home/test/t508n/t508/build/images/boot_linux_P1-backup.img` |
| `/home/test/t508n/work/boot_ramdisk.cpio` | `/home/test/t508n/t508/build/boot-artifacts/boot_ramdisk.cpio` |
| `/home/test/t508n/work/boot_ramdisk_cpio.bin` | `/home/test/t508n/t508/build/boot-artifacts/boot_ramdisk_cpio.bin` |
| `/home/test/t508n/work/boot_ramdisk_out` | `/home/test/t508n/t508/work/ramdisk/boot_ramdisk_out` |
| `/home/test/t508n/work/boot_ramdisk_out2` | `/home/test/t508n/t508/work/ramdisk/boot_ramdisk_out2` |
| `/home/test/t508n/work/bootimg_ramdisk.py` | `/home/test/t508n/t508/work/scripts/bootimg_ramdisk.py` |
| `/home/test/t508n/work/build-54-custom.log` | `/home/test/t508n/t508/work/logs/build-54-custom.log` |
| `/home/test/t508n/work/build-612-clean.log` | `/home/test/t508n/t508/work/logs/build-612-clean.log` |
| `/home/test/t508n/work/clean_screen2.py` | `/home/test/t508n/t508/work/scripts/clean_screen2.py` |
| `/home/test/t508n/work/console-2.log` | `/home/test/t508n/t508/work/logs/console-2.log` |
| `/home/test/t508n/work/console-ramoops-current.log` | `/home/test/t508n/t508/work/logs/console-ramoops-current.log` |
| `/home/test/t508n/work/device` | `/home/test/t508n/t508/firmware/device-dumps` |
| `/home/test/t508n/work/diff-612-arch.patch` | `/home/test/t508n/t508/work/patches/diff-612-arch.patch` |
| `/home/test/t508n/work/diff-612-include.patch` | `/home/test/t508n/t508/work/patches/diff-612-include.patch` |
| `/home/test/t508n/work/diff-612-memory.patch` | `/home/test/t508n/t508/work/patches/diff-612-memory.patch` |
| `/home/test/t508n/work/diff-612-mm.patch` | `/home/test/t508n/t508/work/patches/diff-612-mm.patch` |
| `/home/test/t508n/work/diff-612-sched.patch` | `/home/test/t508n/t508/work/patches/diff-612-sched.patch` |
| `/home/test/t508n/work/diff-612-unisoc-mods.patch` | `/home/test/t508n/t508/work/patches/diff-612-unisoc-mods.patch` |
| `/home/test/t508n/work/disasm_g` | `/home/test/t508n/t508/work/disasm/disasm_g` |
| `/home/test/t508n/work/disasm_h` | `/home/test/t508n/t508/work/disasm/disasm_h` |
| `/home/test/t508n/work/disasm_helpers.sh` | `/home/test/t508n/t508/work/scripts/disasm_helpers.sh` |
| `/home/test/t508n/work/disasm_i` | `/home/test/t508n/t508/work/disasm/disasm_i` |
| `/home/test/t508n/work/disasm_j` | `/home/test/t508n/t508/work/disasm/disasm_j` |
| `/home/test/t508n/work/disasm_k` | `/home/test/t508n/t508/work/disasm/disasm_k` |
| `/home/test/t508n/work/disasm_l` | `/home/test/t508n/t508/work/disasm/disasm_l` |
| `/home/test/t508n/work/disasm_m` | `/home/test/t508n/t508/work/disasm/disasm_m` |
| `/home/test/t508n/work/disasm_n` | `/home/test/t508n/t508/work/disasm/disasm_n` |
| `/home/test/t508n/work/dt_from_vendorboot.py` | `/home/test/t508n/t508/work/scripts/dt_from_vendorboot.py` |
| `/home/test/t508n/work/dtb` | `/home/test/t508n/v50/firmware/dtb` |
| `/home/test/t508n/work/dtbo-overlay-vddusb33-9621.dts` | `/home/test/t508n/t508/build/boot-artifacts/dtbo-overlay-vddusb33-9621.dts` |
| `/home/test/t508n/work/dtbo.img` | `/home/test/t508n/t508/build/boot-artifacts/dtbo.img` |
| `/home/test/t508n/work/dtbo0.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo0.dtb` |
| `/home/test/t508n/work/dtbo1.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo1.dtb` |
| `/home/test/t508n/work/dtbo2.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo2.dtb` |
| `/home/test/t508n/work/dtbo3.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo3.dtb` |
| `/home/test/t508n/work/dtbo4.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo4.dtb` |
| `/home/test/t508n/work/dtbo5.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo5.dtb` |
| `/home/test/t508n/work/dtbo6.dtb` | `/home/test/t508n/t508/firmware/stock/dtbo6.dtb` |
| `/home/test/t508n/work/evidence_203711` | `/home/test/t508n/t508/work/evidence/evidence_203711` |
| `/home/test/t508n/work/f50-b09` | `/home/test/t508n/v50/firmware/f50-b09` |
| `/home/test/t508n/work/f50-b16` | `/home/test/t508n/v50/firmware/f50-b16` |
| `/home/test/t508n/work/fih_OT5_device_tree` | `/home/test/t508n/t508/src/fih_OT5_device_tree` |
| `/home/test/t508n/work/fih_kernel_check` | `/home/test/t508n/t508/work/fih_kernel_check` |
| `/home/test/t508n/work/final-612-diff.patch` | `/home/test/t508n/t508/work/patches/final-612-diff.patch` |
| `/home/test/t508n/work/footers` | `/home/test/t508n/t508/firmware/footers` |
| `/home/test/t508n/work/fs` | `/home/test/t508n/t508/firmware/extracted-fs` |
| `/home/test/t508n/work/host-libs` | `/home/test/t508n/shared/host/host-libs` |
| `/home/test/t508n/work/host-openssl` | `/home/test/t508n/shared/host/host-openssl` |
| `/home/test/t508n/work/initramfs` | `/home/test/t508n/t508/src/initramfs` |
| `/home/test/t508n/work/kernel-ums9620-tuned` | `/home/test/t508n/v50/src/kernel-ums9620-tuned` |
| `/home/test/t508n/work/kernel.config` | `/home/test/t508n/t508/firmware/stock/kernel.config` |
| `/home/test/t508n/work/linux-5.4-custom` | `/home/test/t508n/t508/src/linux-5.4-custom` |
| `/home/test/t508n/work/linux-5.4.210` | `/home/test/t508n/t508/src/linux-5.4.210` |
| `/home/test/t508n/work/linux-5.4.210.tar.xz` | `/home/test/t508n/t508/src/tarballs/linux-5.4.210.tar.xz` |
| `/home/test/t508n/work/linux-6.12-clean` | `/home/test/t508n/t508/src/linux-6.12-clean` |
| `/home/test/t508n/work/mainline_Image` | `/home/test/t508n/t508/build/images/mainline_Image` |
| `/home/test/t508n/work/mu300-linux` | `/home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux` |
| `/home/test/t508n/work/mu300-linux-latest` | `/home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux-latest` |
| `/home/test/t508n/work/mu300-linux-main.tar.gz` | `/home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux-main.tar.gz` |
| `/home/test/t508n/work/mu300-linux-update-294deb8..aec90a8.diff` | `/home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux-update-294deb8..aec90a8.diff` |
| `/home/test/t508n/work/noop_dtbo.img` | `/home/test/t508n/t508/build/boot-artifacts/noop_dtbo.img` |
| `/home/test/t508n/work/observations` | `/home/test/t508n/t508/work/observations` |
| `/home/test/t508n/work/ot5-new.dtb` | `/home/test/t508n/t508/firmware/stock/ot5-new.dtb` |
| `/home/test/t508n/work/ot5-vendor.dtb` | `/home/test/t508n/t508/firmware/stock/ot5-vendor.dtb` |
| `/home/test/t508n/work/ot5.dtb` | `/home/test/t508n/t508/firmware/stock/ot5.dtb` |
| `/home/test/t508n/work/panic-dmesg.log` | `/home/test/t508n/t508/work/logs/panic-dmesg.log` |
| `/home/test/t508n/work/parse_alt_table.py` | `/home/test/t508n/t508/work/scripts/parse_alt_table.py` |
| `/home/test/t508n/work/patch-0001-lse-early-correct.patch` | `/home/test/t508n/t508/work/patches/patch-0001-lse-early-correct.patch` |
| `/home/test/t508n/work/payloads` | `/home/test/t508n/t508/firmware/payloads-work` |
| `/home/test/t508n/work/pipcache` | `/home/test/t508n/shared/tools/pipcache` |
| `/home/test/t508n/work/pmsg-ramoops-current.log` | `/home/test/t508n/t508/work/logs/pmsg-ramoops-current.log` |
| `/home/test/t508n/work/port-skeleton` | `/home/test/t508n/t508/src/port-skeleton` |
| `/home/test/t508n/work/prebuilt` | `/home/test/t508n/v50/build/prebuilt` |
| `/home/test/t508n/work/prompt-vddusb33-fastboot-debug.md` | `/home/test/t508n/t508/docs/prompts/prompt-vddusb33-fastboot-debug.md` |
| `/home/test/t508n/work/prompt-vddusb33-runtime-evidence.md` | `/home/test/t508n/t508/docs/prompts/prompt-vddusb33-runtime-evidence.md` |
| `/home/test/t508n/work/prompt-vddusb33-variant-matrix.md` | `/home/test/t508n/t508/docs/prompts/prompt-vddusb33-variant-matrix.md` |
| `/home/test/t508n/work/pstore-list.txt` | `/home/test/t508n/t508/work/logs/pstore-list.txt` |
| `/home/test/t508n/work/pylib` | `/home/test/t508n/shared/tools/pylib` |
| `/home/test/t508n/work/recover_kallsyms.py` | `/home/test/t508n/t508/work/scripts/recover_kallsyms.py` |
| `/home/test/t508n/work/rev` | `/home/test/t508n/v50/work/rev` |
| `/home/test/t508n/work/screen1.jpg` | `/home/test/t508n/t508/work/screenshots/screen1.jpg` |
| `/home/test/t508n/work/screen1.raw` | `/home/test/t508n/t508/work/screenshots/screen1.raw` |
| `/home/test/t508n/work/screen2.jpg` | `/home/test/t508n/t508/work/screenshots/screen2.jpg` |
| `/home/test/t508n/work/screen2.raw` | `/home/test/t508n/t508/work/screenshots/screen2.raw` |
| `/home/test/t508n/work/sml` | `/home/test/t508n/t508/work/sml` |
| `/home/test/t508n/work/srag` | `/home/test/t508n/t508/work/srag` |
| `/home/test/t508n/work/stock-5.4.210` | `/home/test/t508n/t508/src/stock-5.4.210` |
| `/home/test/t508n/work/stock54-custom_Image` | `/home/test/t508n/t508/build/images/stock54-custom_Image` |
| `/home/test/t508n/work/stock_Image.bin` | `/home/test/t508n/t508/firmware/stock/stock_Image.bin` |
| `/home/test/t508n/work/stock_Image_full.bin` | `/home/test/t508n/t508/firmware/stock/stock_Image_full.bin` |
| `/home/test/t508n/work/stock_kallsyms.map` | `/home/test/t508n/t508/firmware/stock/stock_kallsyms.map` |
| `/home/test/t508n/work/stock_ums9620.dts` | `/home/test/t508n/t508/firmware/stock/stock_ums9620.dts` |
| `/home/test/t508n/work/stock_ums9620_v2.dts` | `/home/test/t508n/t508/firmware/stock/stock_ums9620_v2.dts` |
| `/home/test/t508n/work/stock_ums9620_v3.dts` | `/home/test/t508n/t508/firmware/stock/stock_ums9620_v3.dts` |
| `/home/test/t508n/work/strip_probes.py` | `/home/test/t508n/t508/work/scripts/strip_probes.py` |
| `/home/test/t508n/work/super_raw` | `/home/test/t508n/t508/firmware/extracted-super` |
| `/home/test/t508n/work/tmp` | `/home/test/t508n/t508/work/tmp` |
| `/home/test/t508n/work/tools7z` | `/home/test/t508n/shared/tools/tools7z` |
| `/home/test/t508n/work/twrp_OT5_device_tree` | `/home/test/t508n/t508/src/twrp_OT5_device_tree` |
| `/home/test/t508n/work/uboot_analysis` | `/home/test/t508n/t508/work/uboot/analysis` |
| `/home/test/t508n/work/uboot_log.img` | `/home/test/t508n/t508/work/uboot/images/uboot_log.img` |
| `/home/test/t508n/work/uboot_log_2.img` | `/home/test/t508n/t508/work/uboot/images/uboot_log_2.img` |
| `/home/test/t508n/work/uboot_log_3.img` | `/home/test/t508n/t508/work/uboot/images/uboot_log_3.img` |
| `/home/test/t508n/work/uboot_log_new.img` | `/home/test/t508n/t508/work/uboot/images/uboot_log_new.img` |
| `/home/test/t508n/work/uboot_patched_usbon.img` | `/home/test/t508n/t508/work/uboot/images/uboot_patched_usbon.img` |
| `/home/test/t508n/work/uboot_patched_usbon_v2.img` | `/home/test/t508n/t508/work/uboot/images/uboot_patched_usbon_v2.img` |
| `/home/test/t508n/work/uboot_patched_usbon_v3w2.img` | `/home/test/t508n/t508/work/uboot/images/uboot_patched_usbon_v3w2.img` |
| `/home/test/t508n/work/uboot_patched_v4.img` | `/home/test/t508n/t508/work/uboot/images/uboot_patched_v4.img` |
| `/home/test/t508n/work/uboot_patched_v5.img` | `/home/test/t508n/t508/work/uboot/images/uboot_patched_v5.img` |
| `/home/test/t508n/work/uboot_payload_strings.txt` | `/home/test/t508n/t508/work/uboot/images/uboot_payload_strings.txt` |
| `/home/test/t508n/work/uboot_t508n_lk_orig.bin` | `/home/test/t508n/t508/firmware/stock/uboot_t508n_lk_orig.bin` |
| `/home/test/t508n/work/ufitools` | `/home/test/t508n/v50/tools/ufitools` |
| `/home/test/t508n/work/unisoc_ums9620.md` | `/home/test/t508n/t508/docs/reports/unisoc_ums9620.md` |
| `/home/test/t508n/work/v50-backup` | `/home/test/t508n/v50/firmware/v50-backup` |
| `/home/test/t508n/work/v50-full` | `/home/test/t508n/v50/firmware/v50-full` |
| `/home/test/t508n/work/v50-image-test` | `/home/test/t508n/v50/work/v50-image-test` |
| `/home/test/t508n/work/vddusb33-decomp-handoff` | `/home/test/t508n/t508/work/vddusb33/decomp-handoff` |
| `/home/test/t508n/work/vddusb33-decomp-handoff-full` | `/home/test/t508n/t508/work/vddusb33/decomp-handoff-full` |
| `/home/test/t508n/work/vddusb33-decomp-handoff-full.zip` | `/home/test/t508n/t508/work/vddusb33/vddusb33-decomp-handoff-full.zip` |
| `/home/test/t508n/work/vddusb33-decomp-handoff.zip` | `/home/test/t508n/t508/work/vddusb33/vddusb33-decomp-handoff.zip` |
| `/home/test/t508n/work/vendor-kernel-ums9620` | `/home/test/t508n/t508/src/vendor-kernel-ums9620` |
| `/home/test/t508n/work/vendor54_Image` | `/home/test/t508n/t508/build/images/vendor54_Image` |
| `/home/test/t508n/work/vendor_boot.dtb` | `/home/test/t508n/t508/firmware/stock/vendor_boot.dtb` |
| `/home/test/t508n/work/vendor_boot_a_usb2.img` | `/home/test/t508n/t508/build/boot-artifacts/vendor_boot_a_usb2.img` |
| `/home/test/t508n/work/vendor_boot_a_v3.img` | `/home/test/t508n/t508/build/boot-artifacts/vendor_boot_a_v3.img` |
| `/home/test/t508n/work/vendor_boot_footer.vbmeta` | `/home/test/t508n/t508/firmware/stock/vendor_boot_footer.vbmeta` |
| `/home/test/t508n/work/vendor_boot_linux_new.img` | `/home/test/t508n/t508/build/boot-artifacts/vendor_boot_linux_new.img` |
| `/home/test/t508n/work/vendor_bootconfig.bin` | `/home/test/t508n/t508/build/boot-artifacts/vendor_bootconfig.bin` |
| `/home/test/t508n/work/vendor_ramdisk_cpio.bin` | `/home/test/t508n/t508/build/boot-artifacts/vendor_ramdisk_cpio.bin` |
| `/home/test/t508n/work/vendor_ramdisk_out` | `/home/test/t508n/t508/firmware/vendor_ramdisk_out` |
| `/home/test/t508n/work/vmlinux_sections.txt` | `/home/test/t508n/t508/work/disasm/vmlinux_sections.txt` |
| `/home/test/t508n/work/vmlinux_segments.txt` | `/home/test/t508n/t508/work/disasm/vmlinux_segments.txt` |
| `/home/test/t508n/work/zte-oss` | `/home/test/t508n/v50/src/zte-oss` |

## 3. 旧路径兼容符号链接

以下**顶层**符号链接被创建，使旧的顶层入口路径继续可用（均指向真实新位置，可安全删除）：

```
AGENTS-GUIDE.md -> t508/docs/AGENTS-GUIDE.md
FINAL-BRINGUP-INDEX.md -> t508/docs/FINAL-BRINGUP-INDEX.md
Image -> t508/firmware/stock-images
aarch64-linux-musl-cross -> shared/toolchains/aarch64-linux-musl-cross
arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu -> shared/toolchains/arm-gnu-toolchain-13.2
boot_54custom.img -> t508/build/boot-images/boot_54custom.img
boot_54custom_builtin.img -> t508/build/boot-images/boot_54custom_builtin.img
boot_54custom_probe.img -> t508/build/boot-images/boot_54custom_probe.img
boot_54vendor.img -> t508/build/boot-images/boot_54vendor.img
boot_54vendor.img.bak.1787922677 -> t508/build/boot-images/boot_54vendor.img.bak.1787922677
boot_linux.img -> t508/build/boot-images/boot_linux.img
busybox-1.36.1 -> t508/src/busybox-1.36.1
decomp-analysis -> t508/work/decomp-analysis
dl -> shared/dl
initramfs.cpio -> t508/build/initramfs/initramfs.cpio
initramfs_root -> t508/firmware/initramfs_root
linux-6.12.103 -> t508/src/linux-6.12.103
lk-upstream-ref -> t508/src/lk-upstream-ref
openssl-local -> shared/host/openssl-local
payloads -> t508/firmware/payloads
session.jsonl -> t508/docs/session/session.jsonl
t508n-repo -> t508/src/t508n-repo
tools -> t508/tools
v50-linux -> v50/src/mu300-linux
vendor_boot_linux.img -> t508/build/boot-images/vendor_boot_linux.img
work -> t508/work
work-bak-local -> t508/work/work-bak-local
```

> 说明：`work`、`tools`、`Image`、`payloads`、`initramfs_root`、`decomp-analysis`、`work-bak-local`、
> `linux-6.12.103`、`t508n-repo`、`lk-upstream-ref`、`busybox-1.36.1`、`v50-linux` 等均可用。
> 但 `work/<单文件>`（如 `work/REPORT-*.md`）**不再逐一建链**——它们已按 §2 归入
> `t508/docs/`、`t508/firmware/` 或 `v50/docs/`，请按表换算。

## 4. `work/` 内部条目路径

原 `work/` 下的子目录（如 `work/super_raw`、`work/rev`）**不再建索引符号链接**，
以保证 `t508/work/` 只含 T508 内容、`v50/work/` 只含 V50 内容。旧路径请按 §2 换算，
例如：

| 旧路径 | 新路径 |
|---|---|
| `/home/test/t508n/work/super_raw/` | `/home/test/t508n/t508/firmware/extracted-super/` |
| `/home/test/t508n/work/rev/` | `/home/test/t508n/v50/work/rev/` |
| `/home/test/t508n/work/zte-oss/` | `/home/test/t508n/v50/src/zte-oss/` |
| `/home/test/t508n/work/kernel-ums9620-tuned/` | `/home/test/t508n/v50/src/kernel-ums9620-tuned/` |
| `/home/test/t508n/work/linux-6.12-clean/` | `/home/test/t508n/t508/src/linux-6.12-clean/` |
| `/home/test/t508n/work/vendor-kernel-ums9620/` | `/home/test/t508n/t508/src/vendor-kernel-ums9620/` |

## 5. 已修复的脚本（硬编码绝对路径 → 新路径）

| 脚本 | 修复内容 |
|---|---|
| `t508/work/disasm/disasm_h/decode_mfd.py` / `decode_mfd2.py` | `work/vendor_ramdisk_out` → `t508/firmware/vendor_ramdisk_out` |
| `t508/work/disasm/disasm_h/decode_reg.py` | 同上 |
| `t508/work/disasm/disasm_l/dump_rcu_fn.sh` | `work/stock_Image_full.bin`、`work/stock_kallsyms.map` → `t508/firmware/stock/` |
| `t508/work/scripts/disasm_helpers.sh` | `work/stock_Image_full.bin` → `t508/firmware/stock/` |
| `t508/work/scripts/strip_probes.py` | `linux-6.12.103/` → `t508/src/linux-6.12.103/` |
| `t508/work/scripts/clean_screen2.py` | 同上 |
| `t508/work/scripts/parse_alt_table.py` | `linux-6.12.103/vmlinux`、`work/alt_table_records.csv` → 新位置 |
| `t508/work/scripts/bootimg_ramdisk.py` | `work/tools7z` → `shared/tools/tools7z` |
| `t508/work/uboot/analysis/rdis.sh` | 工具链路径 → `shared/toolchains/arm-gnu-toolchain-13.2` |
| `v50/firmware/v50-backup/dt_extract.py` / `lp_extract.py` / `lp_dump.py` / `vb_parse.py` | `work/v50-backup` → `v50/firmware/v50-backup` |

## 6. 文档路径策略

* `docs/` 与各项目 `docs/` 下的**历史报告保持原文**（含旧绝对路径），便于溯源；
  需要定位时查 §1/§2。
* 新写的索引文档（本文件、各 `README.md`、`docs/INDEX.md`）**一律使用新路径**。
