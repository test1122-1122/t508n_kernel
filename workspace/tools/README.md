# 紫光 T770/UMS9620 (T508N/OT5) 启动集成工具包

基于对 stock 镜像的逆向成果，提供可在 Linux 下复刻紫光引导格式的工具。
全部工具不依赖 root，在 WSL 上即可运行。

> 本目录由原 `/home/test/t508n/tools/` 整体迁入，另并入 `restore_stock.bat`、
> `fetch_ums9620_ref.sh`。兼容软链 `/home/test/t508n/tools` 仍指向此处。

## 0. 文件清单

| 文件/目录 | 用途 |
|---|---|
| `sprd_ramdisk.py` | 紫光 LZ4 ramdisk 容器打包/解包（见 §1） |
| `sprd_mkbootimg.py` | 复刻紫光 boot.img 布局并可选追加 vbmeta footer（见 §2） |
| `patch_vendor_boot.py` | 修改 vendor_boot（原位替换 DTB 等） |
| `avbtool.py` | AVB/vbmeta 工具（Google 官方脚本） |
| `lpunpack.py` | 紫光 LP（super）逻辑分区解包 |
| `lz4dec.py` | LZ4 解压辅助 |
| `cpio_extract.py` / `cpio_extract2.py` | cpio 解包 |
| `build_kernel.sh` / `build_boot_linux.sh` / `build_initramfs.sh` | 构建脚本 |
| `build_noop_dtbo.py` / `build_vddusb33_9621_dtbo.py` | 生成空/定制 DTBO overlay |
| `collect_evidence.sh` | 证据收集 |
| `dts/` | 设备树源（`ums9620-ot5.dts` 等） |
| `ot5_boot_key.pem` / `test_key.pem` | **签名私钥（敏感，勿外传）** |
| `restore_stock.bat` | 恢复原厂（Windows） |
| `fetch_ums9620_ref.sh` | 拉取 UMS9620/LK 参考源 |
| `winadb/` / `winfastboot/` | Windows 下的 adb/fastboot 便携工具 |

## 1. sprd_ramdisk.py —— 紫光 ramdisk 容器（LZ4）打包/解包

紫光 boot/vendor ramdisk 不是 gzip cpio，而是自定义容器：
`magic 0x184C2102 + u32 size_field + LZ4 原始块流`（size_field = 0x604 + lz4 长度）。

```bash
# 解包 boot_a.img 里的 ramdisk（偏移/大小来自头字段）
python3 tools/sprd_ramdisk.py unpack Image/boot_a.img 0x248a000 0x1f79b9 out.cpio

# 把自定义 cpio 打成容器（输出 raw 容器, 供 sprd_mkbootimg.py 使用）
python3 tools/sprd_ramdisk.py pack my_initramfs.cpio ramdisk.bin

# 自检（与 stock 回环比对）
python3 tools/sprd_ramdisk.py test
```

## 2. sprd_mkbootimg.py —— 复刻紫光 boot.img 布局

LK 只吃 raw ARM64 Image（`The kernel is in raw format!`，**不能是 Image.gz**），
内核必须放在 0x1000 页对齐处，ramdisk 容器紧贴页对齐内核末尾：

```bash
python3 tools/sprd_mkbootimg.py \
    --kernel arch/arm64/boot/Image \
    --ramdisk my_initramfs.cpio \
    --output boot_t508n.img \
    [--key mykey.pem]        # 可选: 用 avbtool 追加 vbmeta footer(自签)
```

输出布局与 stock boot_a.img 逐字节结构一致（已用 stock 内核回环验证）。
- 解锁态设备可省略 `--key`（LK: `Device Status is unlock, skip boot image verify`）；
- 上锁态需配合自签 vbmeta 链（见第 4 节）。

## 3. vendor_boot 与 DTB/DTBO

- **不要动 vendor_boot**（内核 DTB + vendor ramdisk + recovery 都在里面，
  LK 按 `sprd,sc-id` 匹配 DTB、按 board_id 0x2700 匹配 DTBO）。
- mainline 内核用本仓库 `tools/dts/ums9620-ot5.dts`（串口起步版）编译
  自己的 DTB；**保留 stock vendor_boot 不动**时，LK 仍会加载 stock DTB——
  因此 mainline 起步建议把内核 DTB 直接**追加进 boot.img**（LK 支持
  boot.img 内 fix dtb），或替换 vendor_boot 里的 DTB 段。

## 4. vbmeta 与上锁/解锁策略

设备解锁（fastboot flashing unlock 或紫光 ResearchDownload）后 LK 容忍校验失败，
这是 bring-up 期最省事的状态。

上锁态完整链（可选，全部用自签 key）：
```bash
openssl genrsa -out keys/boot.pem 4096
# 1) 给 boot.img 打 footer（sprd_mkbootimg.py --key 已做）
# 2) 重建 vbmeta 链:
python3 tools/avbtool.py make_vbmeta_image \
    --chain_partition boot:1:keys/boot.pem \
    --chain_partition vendor_boot:17:keys/vboot.pem \
    --chain_partition dtbo:6:keys/dtbo.pem \
    --key keys/root.pem --algorithm SHA256_RSA4096 --output vbmeta_custom.img
# 3) flash vbmeta_custom.img 到 vbmeta_a/b
```

## 5. 刷入方式（二选一）

```bash
# fastboot（LK 自带 fastboot 模式: 开机按住音量键组合）
fastboot flash boot_a boot_t508n.img
fastboot reboot

# 或紫光 ResearchDownload / FDL（设备 dump 出来时用的就是这条路）
```

## 6. 调试手段

- 串口：`console=ttyS1,115200n8`（LK 注入），uart1 在 ap-apb 0x20210000；
- `uboot_log` 分区会保留每次引导日志（[ms][cpu] 格式），
  读回方法与本次逆向相同（strings 分析）。
