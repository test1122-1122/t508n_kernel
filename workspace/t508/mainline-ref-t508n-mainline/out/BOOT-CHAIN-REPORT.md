# T508N 启动链与校验链联合分析报告

> 设备：TCL T508N（板级 OT5，FIH 代工），SoC **Unisoc UMS9620 / T770**（QogirN6Pro），
> 3 GB LPDDR4x，eMMC，Android 13，原厂内核 5.4.210。
> 日期：2026-10-11
> 范围：只分析**启动链**（谁把谁拉起来）和**校验链**（每一级拿什么验下一级），
> 以及 T508N 上 `no valid os found` 的因果闭环。
>
> 结论先行：T508N 的信任链是**两套机制并存**：
> 1. 引导器链 `splloader -> sml/trustos -> uboot` 走紫光 **DHTB + SIMGHDR RSA**；
> 2. Android 侧 `boot/vendor_boot/dtbo/vbmeta` 走标准 **AVB**。
> T508N 是 fused 设备，SPL 不能改；要让自定义 boot 启动，必须让 U-Boot 跳过 AVB
> （解锁或 magic64）。`no valid os found` 的直接原因是 AVB 强制生效时 A/B 自定义镜像均无法通过校验。

---

## 1. 启动链总览

```text
BootROM
  │  从 eMMC boot 区 / splloader 分区加载 SPL
  ▼
SPL（splloader / spl_a / spl_b）
  │  初始化 DDR、电源、eMMC
  │  加载并校验 SML / TrustOS / U-Boot
  ▼
SML + TrustOS（sml_a/b, trustos_a/b）
  │  EL3 安全监控 + Trusty TEE
  ▼
U-Boot / LK（uboot_a/b，镜像内部名为 sprdlk）
  │  选择 A/B 槽；加载 boot / vendor_boot / dtbo / vbmeta
  │  执行 AVB 校验
  ▼
Linux 内核（boot_a/b 内的 raw ARM64 Image）
  │  挂载 super 逻辑分区、vendor_boot ramdisk
  ▼
Android / OpenWrt / mainline
```

对应分区：

```text
BootROM      <- splloader / spl_a / spl_b
SPL          <- sml_a/b, trustos_a/b, uboot_a/b
U-Boot       <- boot_a/b, vendor_boot_a/b, dtbo_a/b, vbmeta*
Linux        <- boot ramdisk / vendor_boot ramdisk / super 逻辑分区
```

---

## 2. 校验链总览

| 阶段 | 加载者 | 被校验镜像 | 校验机制 | T508N 实际 |
|---|---|---|---|---|
| 0 | BootROM | SPL | DHTB SHA256；fused 设备还验 SIMGHDR RSA | fused，SPL 不可改 |
| 1 | SPL | SML / TrustOS / U-Boot | SIMGHDR RSA-2048 | U-Boot 必须保留原厂签名 |
| 2 | U-Boot | boot / vendor_boot / dtbo / vbmeta* | AVB | locked 强制；unlocked 跳过 |
| 3 | Linux | system / vendor / product 等 | dm-verity / AVB | 由 vbmeta 链控制 |

---

## 3. T508N 引导器镜像格式：DHTB + SIMGHDR

### 3.1 DHTB 头

`splloader / sml / trustos / uboot / teecfg` 都带 DHTB 头。

T508N `uboot_b.img` 实测：

```text
offset 0x00  magic        "DHTB"
offset 0x04  headversion  1
offset 0x08  data_hash    SHA256(payload)（由 dhtb_parse.py 实测校验通过）
offset 0x30  data_size    0x1ced10
offset 0x38  payload      原始签名载荷
```

DHTB 校验的直接证据：

```text
DHTB hash integrity: [OK]
```

### 3.2 SIMGHDR 尾

DHTB payload 之后是紫光签名结构：

```text
SIMGHDR offset = 0x1cef10
header_size    = 0x200
sig_struct_size= 0x458
RSA exponent   = 0x10001
RSA modulus    = b5c9d6f4ccf8485d7613850c7c9ba005...
```

实测：

```text
RSA signature present (image is formally signed)
Signing key: Unknown (modulus prefix b5c9d6f4)
```

这说明 T508N U-Boot 是：

```text
DHTB SHA256 + SIMGHDR RSA-2048 双重保护
```

### 3.3 T508N 真机 load base

来自真机 `uboot_log`（serial `T6PA03CJ3EH02ZW`）：

```text
load_buf      = 0xb4fffe00
sechdr_addr   = 0xb51ced10
sechdr_offset = 0x1cef10
cert_addr     = 0xb51ced70
sysdump-uboot = 0xb5000000 + 0x1000000
```

由此：

```text
LOAD_BASE = load_buf + 0x200 = 0xb5000000
```

T508N 上 DHTB + SIMGHDR 的布局与地址已经闭合，不再依赖推测。

---

## 4. SPL -> U-Boot 的校验

SPL 加载 U-Boot 时：

```text
1. 按 DHTB 读取 payload / data_size
2. 按 SIMGHDR footer 找 RSA cert
3. 对 payload 做 SHA256
4. 用内置公钥验 RSA 签名
5. 通过后才跳转到 U-Boot 入口
```

因此：

```text
U-Boot 的 payload 和 RSA 签名区不能被修改
```

T508N 是 fused 设备：

```text
SPL 本身也被 BootROM 用 RSA 校验
所以 SPL 也不能改
```

正确做法是保留原厂签名，在签名区之后追加运行期补丁，即 magic64。

---

## 5. U-Boot / LK 启动时序与校验链

真机 `uboot_log` 关键事件（normal 会话）：

| 时间 | 事件 | 校验/决策 |
|---:|---|---|
| ~900 ms | LK 启动，DDR 检测 | BootROM 已校验 SPL |
| ~930 ms | `welcome to lk/MP`，qogirn6pro / OT5 | 平台识别 |
| ~1793 ms | 读 `misc` ABC 数据 | `bootable slot 0 priority 15, tries 7` |
| 1816-2076 ms | 多源启动模式探测 | 最终 `NORMAL_MODE` |
| ~3129 ms | `secure_sign_verify` 加载 `vbmeta_a` | `SECUREBOOT_ENABLE` |
| ~3346 ms | 加载内核 `raw Image` | generic kernel size 38308352 |
| ~3383 ms | DTBO 合并 | entry id 0x2700 匹配 OT5 |
| ~13700 ms | `libavb avb_slot_verify` | 校验 boot/vendor_boot/dtbo/vbmeta 链 |
| ~14050 ms | 解锁状态判断 | `LOCK FLAG IS: UNLOCK, SKIP VERIFY` 或强制 AVB |
| ~14057 ms | Trusty 交互 | `pass_chip_uid_to_tos`、`lk_check_rpmb_key` |
| ~14464 ms | `start_linux` | PC 0x80080000，dt_addr 0xbbf00000 |

AVB 校验链实际覆盖：

```text
boot_a/b
vendor_boot_a/b
dtbo_a/b
vbmeta_system_a/b
vbmeta_vendor_a/b
vbmeta_odm_a/b
vbmeta_system_ext_a/b
vbmeta_product_a/b
```

紫光还把 AVB 扩展到部分 modem/固件分区：

```text
nr_modem_a
nr_phy_a
l_agdsp_a
pm_sys_a
```

---

## 6. AVB 与锁状态

U-Boot 的 AVB 行为由锁状态决定：

```text
locked:
  androidboot.flash.locked=1
  androidboot.vbmeta.device_state=locked
  androidboot.verifiedbootstate=green
  严格校验 A/B 槽

unlocked:
  androidboot.flash.locked=0
  androidboot.vbmeta.device_state=unlocked
  androidboot.verifiedbootstate=orange
  WARNNING: LOCK FLAG IS: UNLOCK, SKIP VERIFY!!!
  跳过 AVB
```

锁状态来源：

```text
miscdata 偏移 0x2000 的 64 字节
+ RPMB 权威副本
+ U-Boot get_lock_status() 运行期判断
```

T508N 实测：

```text
miscdata[0x2000] 解锁标记存在
U-Boot 日志出现 SKIP VERIFY
```

---

## 7. `no valid os found` 的因果闭环

```text
Android 修复/回滚
  -> U-Boot 恢复 locked / AVB 强制
  -> boot_a / boot_b 仍是自定义镜像
  -> AVB 校验失败
  -> A/B 两槽都不可启动
  -> LK 输出 no valid os found
```

与这一结论一致的 U-Boot 日志：

```text
ANDROID: bootable slot 0 priority: 15, tries_remaining: 7,
         verity_corrupted: 0, successful_boot: 1
ANDROID: unbootable slot 1 tries_remaining: 0, verity_corrupted: 0
ANDROID: Booting slot_a
```

B 槽在 `misc` `bootloader_control` 中被标记为 unbootable；fastboot/download 模式失败后会
reset 回 A 槽，这也解释了“fastboot 插电脑就重启进 A”的现象。

---

## 8. 解决路径

### 8.1 恢复一致 stock 启动链

先用原厂签名启动链填满：

```text
spl_a/b
uboot_a/b
sml_a/b
trustos_a/b
boot_a/b
vendor_boot_a/b
init_boot_a/b
dtb_a/b
dtbo_a/b
vbmeta_a/b + 所有 chained vbmeta_*
misc
miscdata
metadata
```

这一步用于让设备先能进入 Android。

### 8.2 让 U-Boot 跳过 AVB

两种路线：

```text
A. 标准解锁流程
   adb reboot autoloader
   exec_addr 0x65012f48 + fdl1/fdl2
   写 fdl2-cboot.bin 到 uboot_a/b
   发 spl-emmc-unlock.bin 0x65000800
   恢复本机 spl / uboot

B. magic64 签名保持
   保持原厂 signed payload 不动
   shellcode + patch table 追加在签名区之后
   运行期强制 get_lock_status = UNLOCK
```

T508N 上已验证 magic64 路径。

### 8.3 T508N magic64 patch 点

| Code offset | 原指令 | 目标 | 语义 |
|---:|---:|---:|---|
| `0xb068` | `0x54000180` | `0xd503201f` | 去掉解锁警告 + 电源键等待 |
| `0xb0e8` | `0xb946c813` | `0x52800033` | `get_lock_status` 强制返回 UNLOCK |
| `0xb324` | `0x9402ff42` | `0xd503201f` | 去掉 SKIP VERIFY UART 打印 |
| `0xb32c` | `0x94044594` | `0xd503201f` | 去掉 SKIP VERIFY 屏幕打印 |

```text
LOAD_BASE = 0xb5000000
g_DeviceStatus code offset = 0x1d88c8
```

### 8.4 实测结果

写入 `uboot_a/b = uboot_t508n_magic64_v1.img` 后，OpenWrt 启动：

```text
androidboot.flash.locked=0
androidboot.vbmeta.device_state=unlocked
androidboot.verifiedbootstate=orange
androidboot.slot_suffix=_a
```

说明：

```text
SPL 接受 magic64 的 payload_offset=0x210
shellcode 真机执行
AVB 被跳过
no valid os found 消失
```

### 8.5 A/B 槽控制

`misc` 0x800 的 `bootloader_control` 决定选槽：

```text
misc_slot_a.img -> A 槽 OpenWrt
misc_slot_b.img -> B 槽 mainline
```

不依赖 fastboot，可绕过“fastboot 插入电脑回退 A”的问题。

---

## 9. 从零恢复 no valid os found 的最短路径

```text
1. 进 BROM / SPD
2. 用 v3 解锁工具或 magic64 包，确保第一条 SPD 命令包含：
       exec_addr 0x65012f48
3. 恢复本机 SPL / U-Boot：
       spl.bin / uboota.bin / ubootb.bin
4. 写入 magic64 U-Boot：
       uboot_a = uboot_t508n_magic64_v1.img
       uboot_b = uboot_t508n_magic64_v1.img
5. 写 boot_a / boot_b 自定义镜像
6. 写 misc_slot_a.img 或 misc_slot_b.img 选择启动槽
7. reset
8. 启动后检查：
       androidboot.flash.locked=0
       androidboot.vbmeta.device_state=unlocked
       androidboot.verifiedbootstate=orange
```

---

## 10. 已确认与待确认

### 已确认（有真机/哈希/日志证据）

```text
DHTB SHA256(payload) 在 T508N uboot_b.img 上校验通过
SIMGHDR RSA-2048 存在
LOAD_BASE = 0xb5000000
magic64 在 T508N 真机生效
AVB 在 unlocked 状态被跳过
misc bootloader_control 决定 A/B 槽
```

### 待确认

```text
1. SPL 内部对每个镜像校验的完整调用路径（当前只从 U-Boot 侧验证）
2. Trusty/RPMB 对 lock flag 的最终权威性在每次冷启动时的表现
3. B 槽 mainline 6.18.55 test5 真机启动结果（已写入，待 06 脚本切槽验证）
4. 是否存在与 magic64 冲突的 U-Boot 变体
```

---

## 11. 相关文档

```text
t508/docs/reports/REPORT-T508N-启动流程分析.md
t508/docs/reports/REPORT-T508N-UBOOT-magic64.md
t508/docs/reports/REPORT-T508N-BOOT-SLOT-与救砖.md
t508n-mainline/BOOT-CHAIN.md
t508n-mainline/UBOOT-MAGIC64.md
t508n-mainline/SLOT-CONTROL.md
t508n-mainline/RECOVERY.md
```
