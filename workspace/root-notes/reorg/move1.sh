#!/bin/bash
# Phase 1: skeleton + shared + T508 top-level + V50 top-level
ROOT=/home/test/t508n
cd "$ROOT" || exit 1
LOG="$ROOT/.reorg/move.log"
exec >>"$LOG" 2>&1
echo "===== move1 $(date -Is) ====="

mov() { # src dst
  local s="$1" d="$2"
  if [ ! -e "$s" ] && [ ! -L "$s" ]; then echo "MISS  $s"; return 0; fi
  if [ -e "$d" ] || [ -L "$d" ]; then echo "SKIP  $d (exists)"; return 0; fi
  mkdir -p "$(dirname "$d")"
  if mv "$s" "$d"; then echo "MOVE  $s -> $d"; else echo "FAIL  $s -> $d"; fi
}

# ---- skeleton ----
mkdir -p t508/docs/reports t508/docs/prompts t508/docs/session \
         t508/src t508/firmware t508/build/boot-images t508/build/initramfs \
         t508/build/packages t508/build/images t508/tools t508/work
mkdir -p v50/docs/reports v50/docs/rev v50/src/mu300-linux-snapshots \
         v50/firmware v50/build v50/tools v50/work
mkdir -p shared/toolchains shared/dl shared/host shared/packages shared/tools
mkdir -p docs/cross

# ---- shared ----
mov aarch64-linux-musl-cross shared/toolchains/aarch64-linux-musl-cross
mov arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu shared/toolchains/arm-gnu-toolchain-13.2
mov dl shared/dl
mov openssl-local shared/host/openssl-local
mov libssl-dev_3.5.5-1ubuntu3_amd64.deb shared/packages/libssl-dev_3.5.5-1ubuntu3_amd64.deb
mov make_4.3-4.1build2_amd64.deb shared/packages/make_4.3-4.1build2_amd64.deb
# host helpers currently inside work/
mov work/host-libs shared/host/host-libs
mov work/host-openssl shared/host/host-openssl
mov work/pipcache shared/tools/pipcache
mov work/pylib shared/tools/pylib
mov work/tools7z shared/tools/tools7z

# ---- T508 top-level ----
mov linux-6.12.103 t508/src/linux-6.12.103
mov t508n-repo t508/src/t508n-repo
mov lk-upstream-ref t508/src/lk-upstream-ref
mov busybox-1.36.1 t508/src/busybox-1.36.1
mov Image t508/firmware/stock-images
mov payloads t508/firmware/payloads
mov initramfs_root t508/firmware/initramfs_root
mov initramfs.cpio t508/build/initramfs/initramfs.cpio
for f in boot_54custom.img boot_54custom_builtin.img boot_54custom_probe.img \
         boot_54vendor.img boot_54vendor.img.bak.* boot_linux.img vendor_boot_linux.img; do
  mov "$f" "t508/build/boot-images/$f"
done
mov tools t508/tools
mov decomp-analysis t508/work/decomp-analysis
mov work-bak-local t508/work/work-bak-local
mov session.jsonl t508/docs/session/session.jsonl
mov restore_stock.bat t508/tools/restore_stock.bat
mov fetch_ums9620_ref.sh t508/tools/fetch_ums9620_ref.sh
for f in alpine-minirootfs-3.24.1-aarch64.tar.gz e2fsprogs-libs.apk e2fsprogs-static.apk \
         e2fsprogs.apk libblkid.apk libcom_err.apk libeconf.apk libuuid.apk; do
  mov "$f" "t508/build/packages/$f"
done

# ---- top-level markdown (all T508) ----
for f in *.md; do
  [ -e "$f" ] || continue
  case "$f" in
    prompt-*)                        mov "$f" "t508/docs/prompts/$f" ;;
    REPORT-T508N-*|REPORT-UMS9620-*) mov "$f" "t508/docs/reports/$f" ;;
    *)                               mov "$f" "t508/docs/$f" ;;
  esac
done

# ---- V50 top-level ----
mov v50-linux v50/src/mu300-linux

echo "===== move1 done $(date -Is) ====="
