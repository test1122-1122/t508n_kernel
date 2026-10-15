#!/bin/sh
# Build ctlset as a static arm64 binary (no alsa-lib needed: it uses the kernel's ALSA ioctls directly).
#   tools/ctlset/build.sh [OUT]      default: tools/ctlset/ctlset    - install it as /opt/mu300/bin/ctlset
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); OUT=${1:-$HERE/ctlset}
docker run --rm --platform linux/arm64 -v "$HERE":/w alpine sh -c \
  'apk add -q gcc musl-dev linux-headers && gcc -O2 -static -o /w/ctlset.out /w/ctlset.c'
mv "$HERE/ctlset.out" "$OUT"; echo "built $OUT"
