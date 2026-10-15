# Workspace mirror manifest

Generated: 2026-10-15T23:09:20

This directory is a filtered snapshot of the local T508N workspace.

- Files below 50 MB are included when they are source/text/config.
- Vendor firmware, full Android images, kernel build trees and private keys are excluded.
- Small local git repositories are imported as `import/<name>` branches when their pack size is below GitHub limits.


## Excluded top-level trees

| Path | Size | Reason |
|---|---:|---|
| `/home/test/t508n/v50` | 21G | build outputs / vendor images / kernel source |
| `/home/test/t508n/shared` | 1.5G | build outputs / vendor images / kernel source |
| `/home/test/t508n/t508/firmware` | 21G | build outputs / vendor images / kernel source |
| `/home/test/t508n/t508/work` | 26G | build outputs / vendor images / kernel source |
| `/home/test/t508n/t508/build` | 9.6G | build outputs / vendor images / kernel source |
| `/home/test/t508n/t508/src` | 17G | build outputs / vendor images / kernel source |
| `/home/test/t508n/t508/T508N` | 1.5G | build outputs / vendor images / kernel source |
| `/home/test/t508n/publish` | 406M | build outputs / vendor images / kernel source |

## Local git repositories

| Repo | HEAD | Branch | Size | Max pack | Import |
|---|---|---|---:|---:|---|
| `/home/test/t508n/research/VN300E-UMS9620-root` | `90b193b7ca67` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/bsp_device_qogirn6pro` | `e6fc9e4cfab2` | `W21.24.3` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/bsp_module_wcn_jing` | `b3ddb2e42ba6` | `W21.24.3` | 3 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-audio` | `3df74a1b2d61` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-common-camera` | `cb0fbb1e14ef` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-display` | `67bb150a70b5` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-touchscreen-novatek_nt36xxx` | `31d07f689c66` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-touchscreen-synaptics_td4320_spi` | `adff6c63b29b` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/sprd-kernel-modules-video` | `bb51830f9fd6` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/uwe5622-5ghz-fix` | `73d67061ec3c` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/research/vendor_sprd_modules_audio` | `059714f9f094` | `W21.24.3` | 7 MB | 1 MB | yes |
| `/home/test/t508n/research/vendor_sprd_modules_sensors` | `c853c4cceab6` | `W21.24.3` | 3 MB | 0 MB | yes |
| `/home/test/t508n/research/zte-f50-ufi-openwrt-guide` | `39afcb8ac729` | `main` | 0 MB | 0 MB | yes |
| `/home/test/t508n/t508/mainline-ref/mu300-linux` | `aff105fa3ed9` | `main` | 28 MB | 6 MB | yes |
| `/home/test/t508n/t508/src/fih_OT5_device_tree` | `4f8a7740da0a` | `main` | 55 MB | 17 MB | yes |
| `/home/test/t508n/t508/src/linux-5.4-custom` | `c4bd9f7ae561` | `pristine` | 2942 MB | 186 MB | no (too large) |
| `/home/test/t508n/t508/src/linux-6.12.103` | `cb8e725f31bc` | `master` | 3018 MB | 256 MB | no (too large) |
| `/home/test/t508n/t508/src/lk-upstream-ref` | `3d4f278d44a6` | `master` | 110 MB | 12 MB | yes |
| `/home/test/t508n/t508/src/t508n-repo` | `3da3a196b322` | `main` | 8 MB | 0 MB | yes |
| `/home/test/t508n/t508/src/twrp_OT5_device_tree` | `7156bbc638e5` | `main` | 54 MB | 17 MB | yes |
| `/home/test/t508n/t508/src/vendor-kernel-ums9620` | `4b81213a34fe` | `main` | 5570 MB | 563 MB | no (too large) |
| `/home/test/t508n/t508/work/refs/vendor_sprd_modules_gps_jing` | `2e85ae472f53` | `W21.24.3` | 0 MB | 0 MB | yes |
| `/home/test/t508n/t508/work/work-bak-local` | `cd2affa6e9af` | `master` | 0 MB | 0 MB | yes |
| `/home/test/t508n/v50/src/kernel-ums9620-tuned` | `fc8eb92797e4` | `main` | 1111 MB | 195 MB | no (too large) |
| `/home/test/t508n/v50/src/mu300-linux` | `cd5017d79a99` | `master` | 8718 MB | 4 MB | no (too large) |
| `/home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux` | `294deb810b78` | `main` | 6 MB | 1 MB | yes |

## Imported branches

The following refs are pushed to the GitHub repo:

```text
import/VN300E-UMS9620-root  <- /home/test/t508n/research/VN300E-UMS9620-root  90b193b7ca67 (main)
import/bsp_device_qogirn6pro  <- /home/test/t508n/research/bsp_device_qogirn6pro  e6fc9e4cfab2 (W21.24.3)
import/bsp_module_wcn_jing  <- /home/test/t508n/research/bsp_module_wcn_jing  b3ddb2e42ba6 (W21.24.3)
import/sprd-kernel-modules-audio  <- /home/test/t508n/research/sprd-kernel-modules-audio  3df74a1b2d61 (main)
import/sprd-kernel-modules-common-camera  <- /home/test/t508n/research/sprd-kernel-modules-common-camera  cb0fbb1e14ef (main)
import/sprd-kernel-modules-display  <- /home/test/t508n/research/sprd-kernel-modules-display  67bb150a70b5 (main)
import/sprd-kernel-modules-touchscreen-novatek_nt36xxx  <- /home/test/t508n/research/sprd-kernel-modules-touchscreen-novatek_nt36xxx  31d07f689c66 (main)
import/sprd-kernel-modules-touchscreen-synaptics_td4320_spi  <- /home/test/t508n/research/sprd-kernel-modules-touchscreen-synaptics_td4320_spi  adff6c63b29b (main)
import/sprd-kernel-modules-video  <- /home/test/t508n/research/sprd-kernel-modules-video  bb51830f9fd6 (main)
import/uwe5622-5ghz-fix  <- /home/test/t508n/research/uwe5622-5ghz-fix  73d67061ec3c (main)
import/vendor_sprd_modules_audio  <- /home/test/t508n/research/vendor_sprd_modules_audio  059714f9f094 (W21.24.3)
import/vendor_sprd_modules_sensors  <- /home/test/t508n/research/vendor_sprd_modules_sensors  c853c4cceab6 (W21.24.3)
import/zte-f50-ufi-openwrt-guide  <- /home/test/t508n/research/zte-f50-ufi-openwrt-guide  39afcb8ac729 (main)
import/mu300-linux  <- /home/test/t508n/t508/mainline-ref/mu300-linux  aff105fa3ed9 (main)
import/fih_OT5_device_tree  <- /home/test/t508n/t508/src/fih_OT5_device_tree  4f8a7740da0a (main)
import/lk-upstream-ref  <- /home/test/t508n/t508/src/lk-upstream-ref  3d4f278d44a6 (master)
import/t508n-repo  <- /home/test/t508n/t508/src/t508n-repo  3da3a196b322 (main)
import/twrp_OT5_device_tree  <- /home/test/t508n/t508/src/twrp_OT5_device_tree  7156bbc638e5 (main)
import/vendor_sprd_modules_gps_jing  <- /home/test/t508n/t508/work/refs/vendor_sprd_modules_gps_jing  2e85ae472f53 (W21.24.3)
import/work-bak-local  <- /home/test/t508n/t508/work/work-bak-local  cd2affa6e9af (master)
import/mu300-linux  <- /home/test/t508n/v50/src/mu300-linux-snapshots/mu300-linux  294deb810b78 (main)
```

## Private material excluded

- `tools/ot5_boot_key.pem`
- `tools/test_key.pem`
- any other `*.pem` / `*.key` / SSH private keys
- vendor firmware and stock boot/U-Boot/trustos images
- local PATs and git remote credentials
