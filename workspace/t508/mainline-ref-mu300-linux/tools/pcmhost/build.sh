#!/bin/sh
# Build pcmhost as a static arm64 binary (no alsa-lib needed: it uses the kernel's ALSA ioctls directly).
#   tools/pcmhost/build.sh [OUT]      default: tools/pcmhost/pcmhost    - install it as /opt/mu300/bin/pcmhost
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); OUT=${1:-$HERE/pcmhost}
docker run --rm --platform linux/arm64 -v "$HERE":/w alpine sh -c \
  'apk add -q gcc musl-dev linux-headers && gcc -O2 -static -o /w/pcmhost.out /w/pcmhost.c'
mv "$HERE/pcmhost.out" "$OUT"; echo "built $OUT"
