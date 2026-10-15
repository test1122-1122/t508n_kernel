#!/bin/bash
# Phase 3: compatibility symlinks so old absolute paths keep resolving.
ROOT=/home/test/t508n
cd "$ROOT" || exit 1
LOG="$ROOT/.reorg/move.log"
exec >>"$LOG" 2>&1
echo "===== links $(date -Is) ====="

lnk() { # linkpath target
  local l="$1" t="$2"
  if [ -e "$l" ] || [ -L "$l" ]; then echo "SKIP  $l (exists)"; return 0; fi
  mkdir -p "$(dirname "$l")"
  ln -s "$t" "$l" && echo "LINK  $l -> $t" || echo "FAIL  $l -> $t"
}

# ---- top-level ----
lnk work                    t508/work
lnk tools                   t508/tools
lnk Image                   t508/firmware/stock-images
lnk payloads                t508/firmware/payloads
lnk initramfs_root          t508/firmware/initramfs_root
lnk initramfs.cpio          t508/build/initramfs/initramfs.cpio
lnk decomp-analysis         t508/work/decomp-analysis
lnk work-bak-local          t508/work/work-bak-local
lnk linux-6.12.103          t508/src/linux-6.12.103
lnk t508n-repo              t508/src/t508n-repo
lnk lk-upstream-ref         t508/src/lk-upstream-ref
lnk busybox-1.36.1          t508/src/busybox-1.36.1
lnk v50-linux               v50/src/mu300-linux
lnk aarch64-linux-musl-cross shared/toolchains/aarch64-linux-musl-cross
lnk arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu shared/toolchains/arm-gnu-toolchain-13.2
lnk dl                      shared/dl
lnk openssl-local           shared/host/openssl-local
lnk session.jsonl           t508/docs/session/session.jsonl
lnk AGENTS-GUIDE.md         t508/docs/AGENTS-GUIDE.md
lnk FINAL-BRINGUP-INDEX.md  t508/docs/FINAL-BRINGUP-INDEX.md
for f in boot_54custom.img boot_54custom_builtin.img boot_54custom_probe.img \
         boot_54vendor.img boot_54vendor.img.bak.1787922677 boot_linux.img vendor_boot_linux.img ; do
  lnk "$f" "t508/build/boot-images/$f"
done

# ---- inside t508/work: old work/<name> paths ----
cd "$ROOT/t508/work" || exit 1
# V50 items that used to live in work/
lnk rev                     ../../v50/work/rev
lnk v50-backup              ../../v50/firmware/v50-backup
lnk v50-full                ../../v50/firmware/v50-full
lnk v50-image-test          ../../v50/work/v50-image-test
lnk zte-oss                 ../../v50/src/zte-oss
lnk prebuilt                ../../v50/build/prebuilt
lnk ufitools                ../../v50/tools/ufitools
lnk dtb                     ../../v50/firmware/dtb
lnk f50-b09                 ../../v50/firmware/f50-b09
lnk f50-b16                 ../../v50/firmware/f50-b16
lnk mu300-linux             ../../v50/src/mu300-linux-snapshots/mu300-linux
lnk mu300-linux-latest      ../../v50/src/mu300-linux-snapshots/mu300-linux-latest
lnk kernel-ums9620-tuned    ../../v50/src/kernel-ums9620-tuned
# T508 items relocated within the tree
lnk fih_OT5_device_tree     ../src/fih_OT5_device_tree
lnk twrp_OT5_device_tree    ../src/twrp_OT5_device_tree
lnk linux-5.4-custom        ../src/linux-5.4-custom
lnk linux-5.4.210           ../src/linux-5.4.210
lnk linux-6.12-clean        ../src/linux-6.12-clean
lnk stock-5.4.210           ../src/stock-5.4.210
lnk vendor-kernel-ums9620   ../src/vendor-kernel-ums9620
lnk initramfs               ../src/initramfs
lnk port-skeleton           ../src/port-skeleton
lnk device                  ../firmware/device-dumps
lnk footers                 ../firmware/footers
lnk fs                      ../firmware/extracted-fs
lnk super_raw               ../firmware/extracted-super
lnk vendor_ramdisk_out      ../firmware/vendor_ramdisk_out
lnk payloads                ../firmware/payloads-work
lnk DISASM-C-irq-中断GIC    disasm/DISASM-C-irq-中断GIC
for d in g h i j k l m n ; do lnk "disasm_$d" "disasm/disasm_$d" ; done
lnk uboot_analysis          uboot/analysis
lnk vddusb33-decomp-handoff      vddusb33/decomp-handoff
lnk vddusb33-decomp-handoff-full vddusb33/decomp-handoff-full

echo "===== links done $(date -Is) ====="
