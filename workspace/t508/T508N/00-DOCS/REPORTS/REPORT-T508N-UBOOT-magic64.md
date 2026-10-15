# T508N U-Boot magic64 解锁报告

> 设备：TCL T508N / OT5，Unisoc UMS9620（T770），3GB eMMC
> 日期：2026-10-09
> 结论先行：T508N 是 fused UMS9620，SPL 不可改；已用**签名保持 magic64**方案
> 在 `uboot_a/uboot_b` 上完成解锁补丁，实机启动 OpenWrt 后 `androidboot.flash.locked=0`、
> `verifiedbootstate=orange`，AVB 已被 U-Boot 跳过。T508N 专属 patch 点已全部定位。

---

## 1. T508N 校验链回顾

```text
BootROM
  -> splloader / spl_a/b
       DHTB SHA256；fused 设备还验 SIMGHDR RSA

SPL
  -> sml_a/b, trustos_a/b, uboot_a/b
       SIMGHDR RSA-2048

U-Boot
  -> boot_a/b, vendor_boot_a/b, dtbo_a/b, vbmeta*
       AVB；unlocked 时跳过

Linux / Android / OpenWrt
```

T508N 上：

```text
SPL 不可替换（没有私钥）
U-Boot 必须保留原厂 RSA 签名
自定义 boot 只能在 U-Boot 解锁/跳过 AVB 后启动
```

---

## 2. magic64 原理

```text
0x000  DHTB header (0x200)
0x200  entry jump -> shellcode
0x210  原厂 signed payload（逐字节不动，RSA 仍有效）
...    shellcode + patch table
...    SIMGHDR footer 整体 +0x100
...    RSA cert 整体 +0x100
```

SPL 先按原样校验 payload，再跳到 `0x200`：

```text
shellcode
  -> 把 payload 搬回 LOAD_BASE
  -> 按 patch table 修改运行期指令
  -> 刷 cache
  -> 跳回真实 _start
```

---

## 3. T508N load base 证据

来自真机 `uboot_log`（serial `T6PA03CJ3EH02ZW`）和 `uboot_b.img`：

```text
target: OT5
project: OT5
buildid: 2024-01-16-17:41:42_LOCAL
load_buf      = 0xb4fffe00
sechdr_addr   = 0xb51ced10
sechdr_offset = 0x1cef10
cert_addr     = 0xb51ced70
sysdump-uboot = 0xb5000000 + 0x1000000
```

因此：

```text
LOAD_BASE = 0xb4fffe00 + 0x200 = 0xb5000000
```

镜像结构：

```text
DHTB data_size = 0x1ced10
SIMGHDR offset = 0x1cef10
partition size = 0x300000
RSA exponent   = 0x10001
```

---

## 4. T508N 专属 patch 点

所有 offset 为 DHTB payload 内 code offset（file offset = code offset + 0x200）。

| Code offset | 原指令 | 目标 | 函数/语义 |
|---:|---:|---:|---|
| `0xb068` | `0x54000180` `b.eq` | `0xd503201f` `nop` | 去掉 `INFO: LOCK FLAG IS : UNLOCK!!!` + 等电源键/倒计时 |
| `0xb0e8` | `0xb946c813` `ldr w19,[x0,#0x6c8]` | `0x52800033` `mov w19,#1` | `get_lock_status` 强制返回 UNLOCK |
| `0xb324` | `0x9402ff42` `bl` | `0xd503201f` `nop` | 去掉 SKIP VERIFY UART 打印 |
| `0xb32c` | `0x94044594` `bl` | `0xd503201f` `nop` | 去掉 SKIP VERIFY 屏幕打印 |

相关全局：

```text
g_DeviceStatus code offset = 0x1d88c8
runtime                    = 0xb51d88c8
read sites                 = 0xb05c, 0xb0e8
```

---

## 5. 构建与离线验证

工具：

```text
t508/work/srag-refs/UnisocBypass/tools/magic_pack_ums9620.py
```

命令：

```sh
python3 magic_pack_ums9620.py uboot_b.img uboot_t508n_magic64_v1.img \
  --patch 0xb068=0xd503201f \
  --patch 0xb0e8=0x52800033 \
  --patch 0xb324=0xd503201f \
  --patch 0xb32c=0xd503201f
```

离线验证结论：

```text
DHTB 有效
payload 在 0x210 处逐字节等于原厂
SIMGHDR 平移，payload_offset 0x200 -> 0x210
RSA cert 内容不变，仅整体平移
shellcode 精确修改 4 个目标 word
控制流正确回到 0xb5000000 的真实 _start
```

验证器中一个 `everything after cert is zero padding` 的 FAIL 是其 cert 偏移计算误报；
手工检查 stock 与 patched 的 cert 之后均为零。

---

## 6. 产物与实机结果

| 产物 | 大小 | MD5 |
|---|---:|---|
| `uboot_t508n_magic64_v1.img` | 3 MiB | `39960804d6cc18cadacf5fc239b08225` |
| `uboot_t508n_magic64_probe.img` | 3 MiB | `fa4a30e4cec556eeb1ba263f76f6141c` |
| 原厂 `uboot_a/b.img` | 3 MiB | `7fe2b5eaeb68b9b7792ef806ce553e61` |

写入 `uboot_a` 和 `uboot_b` 后，OpenWrt 启动，`/proc/cmdline` 包含：

```text
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
androidboot.slot_suffix=_a
```

证明：

- SPL 接受 magic64 的 payload_offset=0x210；
- shellcode 在真机执行；
- U-Boot 已跳过 AVB，自定义 `boot_a` 可以启动。

---

## 7. Windows 一键包

```text
E:\Download\ums9620_tcl_t508n_magic64\
E:\Download\ums9620_tcl_t508n_magic64.zip
```

脚本：

```text
01-一键解锁并写magic64.bat
02-仅写magic64-uboot.bat
03-探针验证.bat
04-备份uboot.bat
05-恢复原厂uboot.bat
```

回滚：

```text
05-恢复原厂uboot.bat
```

或使用 BROM 写回原厂 `uboot_a_stock.img` / `uboot_b_stock.img`。
