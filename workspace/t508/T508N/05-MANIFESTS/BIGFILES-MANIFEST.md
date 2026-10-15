# BIGFILES-MANIFEST：大文件登记（只登记，未复制）

> 规则：>256 MiB 的文件不进入归档；>1 GiB 一律不复制。
> 全量 SHA256 见 05-MANIFESTS/CHECKSUMS-T508N.sha256。

| 大小 | 是否 >1GiB | SHA256(前16) | 用途 | 原位置 |
|---|---:|---|---|---|
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrock\flash_all_spd_dump\super.img` |
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrock\Image\super.img` |
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrocka\flash_all_spd_dump\super.img` |
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrocka\Image\super.img` |
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/vendor_dlkm_b.img` |
| 5.47 GiB | 是 | `8738931e4f1df065` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/stock-images/super.img` |
| 2.73 GiB | 是 | `f436ec5dca133d4f` | 分区/文件系统镜像 | `/home/test/t508n/t508/work/android_payload2.erofs` |
| 2.73 GiB | 是 | `1dd217108c871614` | 分区/文件系统镜像 | `C:\platform-tools\android_payload2.simg` |
| 2.73 GiB | 是 | `1dd217108c871614` | 分区/文件系统镜像 | `/home/test/t508n/t508/work/android_payload2.simg` |
| 2.72 GiB | 是 | `86b7c5a513fe3938` | 分区/文件系统镜像 | `C:\platform-tools\android_payload.erofs` |
| 2.72 GiB | 是 | `86b7c5a513fe3938` | 分区/文件系统镜像 | `/home/test/t508n/t508/work/android_payload.erofs` |
| 2.72 GiB | 是 | `743ea3385cc2e0cb` | 分区/文件系统镜像 | `C:\platform-tools\android_payload.simg` |
| 2.72 GiB | 是 | `743ea3385cc2e0cb` | 分区/文件系统镜像 | `/home/test/t508n/t508/work/android_payload.simg` |
| 2.49 GiB | 是 | `b92a80ff8063792b` | 整机备份压缩包 | `E:\rootphone\t508n\savebrock\2a备份(苦力怕制作).zip` |
| 2.49 GiB | 是 | `b92a80ff8063792b` | 整机备份压缩包 | `E:\rootphone\t508n\savebrocka\2a备份(苦力怕制作).zip` |
| 1.64 GiB | 是 | `476a9e2c33163d53` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrocka\out\system_a.img` |
| 1.64 GiB | 是 | `476a9e2c33163d53` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/system_a.img` |
| 1.32 GiB | 是 | `16ca1fbc49a01f79` | 日志/证据 | `/home/test/t508n/t508/work/lpunpack2.log` |
| 0.49 GiB |  | `19f6ff0da7fcec67` | stock 分区镜像 | `E:\rootphone\t508n\savebrock\flash_all_spd_dump\blackbox.img` |
| 0.49 GiB |  | `19f6ff0da7fcec67` | stock 分区镜像 | `E:\rootphone\t508n\savebrock\Image\blackbox.img` |
| 0.49 GiB |  | `19f6ff0da7fcec67` | stock 分区镜像 | `E:\rootphone\t508n\savebrocka\flash_all_spd_dump\blackbox.img` |
| 0.49 GiB |  | `19f6ff0da7fcec67` | stock 分区镜像 | `E:\rootphone\t508n\savebrocka\Image\blackbox.img` |
| 0.48 GiB |  | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrocka\out\vendor_a.img` |
| 0.48 GiB |  | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/system_ext_b.img` |
| 0.48 GiB |  | `e2eddbc2a964fc04` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/vendor_a.img` |
| 0.29 GiB |  | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `E:\rootphone\t508n\savebrocka\out\system_ext_a.img` |
| 0.29 GiB |  | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/system_ext_a.img` |
| 0.29 GiB |  | `4ad1cf4a13c3a6a2` | 分区/文件系统镜像 | `/home/test/t508n/t508/firmware/extracted-super/system_b.img` |

共 28 个，合计 65.76 GiB。它们均已存在于原始位置（Windows/WSL），归档内不保留第二份副本。
