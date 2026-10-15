#!/bin/bash
# Phase 2: split work/ into T508 / V50 / shared / cross
ROOT=/home/test/t508n
cd "$ROOT" || exit 1
LOG="$ROOT/.reorg/move.log"
exec >>"$LOG" 2>&1
echo "===== move2 $(date -Is) ====="

mov() { # src dst
  local s="$1" d="$2"
  if [ ! -e "$s" ] && [ ! -L "$s" ]; then echo "MISS  $s"; return 0; fi
  if [ -e "$d" ] || [ -L "$d" ]; then echo "SKIP  $d (exists)"; return 0; fi
  mkdir -p "$(dirname "$d")"
  if mv "$s" "$d"; then echo "MOVE  $s -> $d"; else echo "FAIL  $s -> $d"; fi
}
movw() { mov "work/$1" "$2"; }

# ---------- T508 directories ----------
movw DISASM-C-irq-中断GIC          t508/work/disasm/DISASM-C-irq-中断GIC
for d in g h i j k l m n; do movw "disasm_$d" "t508/work/disasm/disasm_$d"; done
movw boot_ramdisk_out             t508/work/ramdisk/boot_ramdisk_out
movw boot_ramdisk_out2            t508/work/ramdisk/boot_ramdisk_out2
movw device                       t508/firmware/device-dumps
movw evidence_203711              t508/work/evidence/evidence_203711
movw fih_OT5_device_tree          t508/src/fih_OT5_device_tree
movw twrp_OT5_device_tree         t508/src/twrp_OT5_device_tree
movw fih_kernel_check             t508/work/fih_kernel_check
movw footers                      t508/firmware/footers
movw fs                           t508/firmware/extracted-fs
movw initramfs                    t508/src/initramfs
movw linux-5.4-custom             t508/src/linux-5.4-custom
movw linux-5.4.210                t508/src/linux-5.4.210
movw linux-6.12-clean             t508/src/linux-6.12-clean
movw observations                 t508/work/observations
movw payloads                     t508/firmware/payloads-work
movw port-skeleton                t508/src/port-skeleton
movw sml                          t508/work/sml
movw srag                         t508/work/srag
movw stock-5.4.210                t508/src/stock-5.4.210
movw super_raw                    t508/firmware/extracted-super
movw tmp                          t508/work/tmp
movw uboot_analysis               t508/work/uboot/analysis
movw vendor-kernel-ums9620        t508/src/vendor-kernel-ums9620
movw vendor_ramdisk_out           t508/firmware/vendor_ramdisk_out
movw vddusb33-decomp-handoff      t508/work/vddusb33/decomp-handoff
movw vddusb33-decomp-handoff-full t508/work/vddusb33/decomp-handoff-full

# ---------- V50 directories ----------
movw dtb              v50/firmware/dtb
movw f50-b09          v50/firmware/f50-b09
movw f50-b16          v50/firmware/f50-b16
movw kernel-ums9620-tuned v50/src/kernel-ums9620-tuned
movw mu300-linux      v50/src/mu300-linux-snapshots/mu300-linux
movw mu300-linux-latest v50/src/mu300-linux-snapshots/mu300-linux-latest
movw prebuilt         v50/build/prebuilt
movw rev              v50/work/rev
movw ufitools         v50/tools/ufitools
movw v50-backup       v50/firmware/v50-backup
movw v50-full         v50/firmware/v50-full
movw v50-image-test   v50/work/v50-image-test
movw zte-oss          v50/src/zte-oss

# ---------- V50 loose files ----------
movw SHA256SUMS.tmp                         v50/work/SHA256SUMS.tmp
movw DIAG-github-mu300-拉取失败诊断.md       v50/docs/reports/DIAG-github-mu300-拉取失败诊断.md
movw REPORT-F50-B09固件与B16备份分析.md      v50/docs/reports/REPORT-F50-B09固件与B16备份分析.md
movw REPORT-V50-能否使用mu300-linux.md       v50/docs/reports/REPORT-V50-能否使用mu300-linux.md
movw REPORT-mu300-linux-项目理解.md          v50/docs/reports/REPORT-mu300-linux-项目理解.md
movw mu300-linux-main.tar.gz                v50/src/mu300-linux-snapshots/mu300-linux-main.tar.gz
movw "mu300-linux-update-294deb8..aec90a8.diff" v50/src/mu300-linux-snapshots/mu300-linux-update-294deb8..aec90a8.diff

# ---------- cross-project ----------
movw REPORT-mu300-linux-对T508N价值评估.md  docs/cross/REPORT-mu300-linux-对T508N价值评估.md

# ---------- T508 reports ----------
for f in \
  ARCHIVE-20260823.md FINAL-WRAPUP-20260821.md FLASH-20260821-P1.md \
  GUIDE-vddusb33-stock-design.md HANDOFF-3-SESSIONS.md \
  PATCH-diag-sgi-kfence.md PATCH-mem-sysfs-order.md PATCH-nohz-kick.md \
  REPORT-54-custom-build.md REPORT-54-probe-port.md REPORT-A76-requirements.md \
  REPORT-AGENTS-GUIDE-analysis.md REPORT-CURRENT-RACE-ANALYSIS.md \
  REPORT-LSE-atomic-completion-analysis.md REPORT-ROOTFS-COMPARE.md \
  REPORT-SELF-BUILT-USB-CHAIN.md REPORT-SML-psci-cpu-on.md \
  REPORT-STOCK-USB-STACK-MECHANISM.md REPORT-T508N-stock-analysis.md \
  REPORT-UNISOC-MODS-ON-612.md REPORT-UNISOC-ONLY-CONFIGS.md \
  REPORT-USB-BOOT-FLOW.md REPORT-USB-CONSOLIDATED.md REPORT-alt-table-integrity.md \
  REPORT-mainline-completion-wakeup-analysis.md \
  REPORT-stock-alternatives-waiter-analysis.md REPORT-stock-crosscluster-visibility.md \
  REPORT-stock-driver-init.md REPORT-stock-memory-dev-init.md REPORT-stock-of-core-init.md \
  REPORT-stock-page-alloc-init.md REPORT-stock-sync-mechanism.md \
  REPORT-stock-vs-mainline-completion.md REPORT-stock-vs-mainline-locks.md \
  REPORT-stock-wakeup-chain-analysis.md REPORT-stock-wakeup-nohz.md \
  REPORT-stock-zone-memory.md REPORT-vddusb33-variant-matrix-offline.md \
  SESSION-CONTEXT-COMPACT.md SESSION-SPLIT-AND-WORKFLOW.md SUMMARY-ALL-ANALYSIS.md \
  T508N-VENDOR-BUILD-RECIPE.md TASK-A-B-USB-DTS-C-离线成果.md \
  TASK-A-deepdive-SSPHY-clock.md TASK-vddusb33-regulator-fix.md \
  UNISOC-DETAILED.md UNISOC-MODS-AND-CONFIG.md unisoc_ums9620.md ; do
  movw "$f" "t508/docs/reports/$f"
done

# ---------- T508 prompts ----------
for f in prompt-vddusb33-fastboot-debug.md prompt-vddusb33-runtime-evidence.md \
         prompt-vddusb33-variant-matrix.md ; do
  movw "$f" "t508/docs/prompts/$f"
done

# ---------- T508 disasm workspace ----------
for f in \
  DISASM-A-cpu-boot.md DISASM-B-sched-调度器.md DISASM-D-mm-内存管理.md \
  DISASM-E-devm-驱动模型sysfs.md DISASM-F-fs-文件系统VFS.md DISASM-G-drv-clk-时钟.md \
  DISASM-H-drv-pmic-电源.md DISASM-I-drv-ufs.md DISASM-J-drv-timer-wdt.md \
  DISASM-K-mm-slub.md DISASM-L-rcu.md DISASM-M-workqueue.md DISASM-N-cpuidle-cpufreq.md \
  E-devm-bus_add_device.dump E-devm-device_add.dump E-devm-kernfs_add_one.dump \
  E-devm-kernfs_new_node.dump E-devm-kobject_add_internal.dump E-devm-memory.dump \
  E-devm-symbols.txt E-devm-uevent_env.dump E-devm-memory.dump \
  vmlinux_sections.txt vmlinux_segments.txt alt_table_records.csv ; do
  movw "$f" "t508/work/disasm/$f"
done

# ---------- T508 stock firmware material ----------
for f in \
  stock_Image.bin stock_Image_full.bin stock_kallsyms.map kernel.config System.map \
  stock_ums9620.dts stock_ums9620_v2.dts stock_ums9620_v3.dts \
  ot5.dtb ot5-new.dtb ot5-vendor.dtb vendor_boot.dtb \
  dtbo0.dtb dtbo1.dtb dtbo2.dtb dtbo3.dtb dtbo4.dtb dtbo5.dtb dtbo6.dtb \
  boot_footer.vbmeta vendor_boot_footer.vbmeta uboot_t508n_lk_orig.bin ; do
  movw "$f" "t508/firmware/stock/$f"
done

# ---------- T508 built images ----------
for f in mainline_Image stock54-custom_Image vendor54_Image \
         boot_linux_612-probes-backup.img boot_linux_P1-backup.img ; do
  movw "$f" "t508/build/images/$f"
done

# ---------- T508 uboot workspace ----------
for f in uboot_log.img uboot_log_2.img uboot_log_3.img uboot_log_new.img \
         uboot_patched_usbon.img uboot_patched_usbon_v2.img uboot_patched_usbon_v3w2.img \
         uboot_patched_v4.img uboot_patched_v5.img uboot_payload_strings.txt ; do
  movw "$f" "t508/work/uboot/images/$f"
done

# ---------- T508 patches ----------
for f in diff-612-arch.patch diff-612-include.patch diff-612-memory.patch \
         diff-612-mm.patch diff-612-sched.patch diff-612-unisoc-mods.patch \
         final-612-diff.patch patch-0001-lse-early-correct.patch ; do
  movw "$f" "t508/work/patches/$f"
done

# ---------- T508 logs ----------
for f in build-54-custom.log build-612-clean.log console-2.log \
         console-ramoops-current.log panic-dmesg.log pmsg-ramoops-current.log pstore-list.txt ; do
  movw "$f" "t508/work/logs/$f"
done

# ---------- T508 screenshots ----------
for f in screen1.jpg screen1.raw screen2.jpg screen2.raw ; do
  movw "$f" "t508/work/screenshots/$f"
done

# ---------- T508 analysis scripts ----------
for f in bootimg_ramdisk.py clean_screen2.py disasm_helpers.sh dt_from_vendorboot.py \
         parse_alt_table.py recover_kallsyms.py strip_probes.py ; do
  movw "$f" "t508/work/scripts/$f"
done

# ---------- T508 boot artifacts ----------
for f in boot_ramdisk.cpio boot_ramdisk_cpio.bin vendor_ramdisk_cpio.bin \
         vendor_bootconfig.bin vendor_boot_a_usb2.img vendor_boot_a_v3.img \
         vendor_boot_linux_new.img dtbo.img noop_dtbo.img \
         dtbo-overlay-vddusb33-9621.dts ; do
  movw "$f" "t508/build/boot-artifacts/$f"
done

# ---------- T508 source tarballs / handoff archives ----------
movw linux-5.4.210.tar.xz            t508/src/tarballs/linux-5.4.210.tar.xz
movw vddusb33-decomp-handoff.zip     t508/work/vddusb33/vddusb33-decomp-handoff.zip
movw vddusb33-decomp-handoff-full.zip t508/work/vddusb33/vddusb33-decomp-handoff-full.zip

echo "===== move2 done $(date -Is) ====="
echo "leftover in work/:"
find work -mindepth 1 -maxdepth 1 | sort
