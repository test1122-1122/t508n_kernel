#!/bin/bash
# UMS9620 mainline 适配资料下载脚本
# 多镜像源 + 重试 + IPv4 强制, 应对 lore/git.kernel.org 被墙或 IPv6 问题
set -u
mkdir -p ~/ums9620-ref && cd ~/ums9620-ref

fetch() {
    # $1=目标文件名  $2..=源URL(按顺序尝试)
    local out="$1"; shift
    local url
    for url in "$@"; do
        echo "  尝试: $url"
        if curl -4 -sSL --retry 3 --retry-delay 2 --connect-timeout 15 -o "$out" "$url"; then
            if [ -s "$out" ]; then
                echo "  [OK] $out ($(wc -c < "$out") bytes)"
                return 0
            fi
        fi
        echo "  [失败] 换下一个源"
    done
    echo "  [全部失败] $out"
    return 1
}

echo "=== 1. mainline 最新 UMS9620 dtsi (含 timer 节点) ==="
fetch ums9620.dtsi \
  "https://raw.githubusercontent.com/torvalds/linux/master/arch/arm64/boot/dts/sprd/ums9620.dtsi" \
  "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/arch/arm64/boot/dts/sprd/ums9620.dtsi" \
  "https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/master/arch/arm64/boot/dts/sprd/ums9620.dtsi?format=TEXT"

echo ""
echo "=== 2. 绑定文档 interrupt-names ==="
fetch arm-arch-timer.yaml \
  "https://raw.githubusercontent.com/torvalds/linux/master/Documentation/devicetree/bindings/timer/arm%2Carch_timer.yaml" \
  "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/Documentation/devicetree/bindings/timer/arm%2Carch_timer.yaml"

echo ""
echo "=== 3. Marc Zyngier patch 系列 (完整 mbox) ==="
fetch series-v3.mbox.gz \
  "https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-1-maz@kernel.org/t.mbox.gz" \
  "https://patchew.org/linux/20260523140242.586031-1-maz@kernel.org/mbox.gz"
if [ -s series-v3.mbox.gz ]; then
    gzip -df series-v3.mbox.gz && echo "series-v3.mbox 已解压"
fi

echo ""
echo "=== 4. 关键单个 patch (raw) ==="
# 03: arch_timer 驱动改动
fetch patch-03-arch-timer.txt \
  "https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-4-maz@kernel.org/raw" \
  "https://lore.kernel.org/all/20260523140242.586031-4-maz@kernel.org/raw"
# 16: sprd/UMS9620 DTS 加 EL2 virtual timer
fetch patch-16-sprd-dts.txt \
  "https://lore.kernel.org/linux-arm-kernel/20260523140242.586031-17-maz@kernel.org/raw" \
  "https://lore.kernel.org/all/20260523140242.586031-17-maz@kernel.org/raw"

echo ""
echo "=== 完成, 文件列表 ==="
ls -la
echo ""
echo "关键查看命令:"
echo "  grep -A8 'arm,armv8-timer' ums9620.dtsi"
echo "  cat patch-16-sprd-dts.txt"
echo "  cat patch-03-arch-timer.txt"
