#!/bin/sh
# Install a sprdwl_ng.ko.xz on disk (no live reload) and log it persistently.
set -eu
M=/lib/modules/$(uname -r)/kernel/drivers/net/wireless/uwe5622/unisocwifi/sprdwl_ng.ko.xz
[ -f "$M.orig" ] || cp -a "$M" "$M.orig"
if [ "$1" = "--restore" ]; then cp -a "$M.orig" "$M"; else cp "$1" "$M"; fi
depmod -a && sync
echo "$(date '+%F %T') installed $(md5sum "$M" | cut -c1-8) from ${1}" >> /root/wifi-swap.log
md5sum "$M"
