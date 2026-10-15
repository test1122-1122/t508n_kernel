# T508N full workspace mirror

Author: deepseek  
Target: https://github.com/test1122-1122/t508n_kernel

This repository is a filtered mirror of the local T508N development workspace.

```text
mainline/    curated mainline 6.18.55 bring-up (kernel, modules, images, userspace)
workspace/   filtered source/documentation snapshot of the local workspace
```

## What is included

- mainline kernel ports, T508N overlays, out-of-tree module sources and `.ko`
- test40 boot image and `Image.lk`
- WiFi / Bluetooth / BlueZ helper scripts and offline packages
- documents, reports, build tools, small git repository snapshots
- small local git repositories as `import/<name>` branches

## What is excluded

- vendor firmware, stock Android images, U-Boot/trustos
- kernel build trees and objects (`vmlinux`, `.o`)
- files above GitHub's size limits
- private keys and credentials

See:

```text
workspace/EXCLUDED-MANIFEST.md
mainline/README.md
```
