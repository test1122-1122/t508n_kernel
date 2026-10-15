# T508N（紫光展锐 T770 / UMS9620）启动流程逆向分析报告

> 镜像来源：`E:\rootphone\t508n\savebrock\Image`（78 个分区镜像，共 7.3GB）
> 分析环境：WSL2（Linux），镜像复制到 `/home/test/t508n/Image` 后离线解包
> 结论先行：这台 T508N 使用**紫光展锐 T770（UMS9620，平台代号 QogirN6Pro）**芯片，启动链为
> **BootROM → SPL(splloader) → [Trusty TOS + SML] → LK(sprdlk) → Android 内核 → init**，
> 全程存在大量紫光定制：DHTB 签名头、空置的 init_boot/dtb 分区、自定义 LZ4 ramdisk 容器、
> 加密 LP 元数据、AVB 扩展到 modem 分区等，与原生 AOSP 差异显著。

---

## 1. 设备与芯片识别

| 项目 | 值 | 证据来源 |
|---|---|---|
| 手机型号 | **TCL T508N**（5G 老人机） | `androidboot.cu=T508N-2ALECN12-1`（LK 日志） |
| 板级代号 | OT5（硬件 V2.3） | `androidboot.hardware=OT5`、DTBO `board_id=0x2700` |
| 芯片 | **紫光展锐 Unisoc T770 (UMS9620)** | `ro.soc.model=T770`、DTB `cpuinfo_hardware="Unisoc ums9620"` |
| 平台代号 | QogirN6Pro | LK 日志 `platform: qogirn6pro`、模块 `pinctrl-sprd-qogirn6pro.ko` |
| 厂商/代工 | TCL 品牌，**FIH（富士康移动）**代工 | build fingerprint `TCL/OT5_CM_FIH/OT5:13/.../508NCM5K:user/release-keys` |
| Android 版本 | **13**（TP1A.220624.014，SDK 33，patch 2023-08-05） | system/build.prop、vbmeta os_version=13 |
| 内核 | Linux **5.4.210**-android12-9（foxconn@NJBUILD04，clang 12，2024-01-16） | 内核字符串 + 内嵌 IKCONFIG |
| 硬件 | 3GB LPDDR4x、32GB eMMC(HS200)、PMIC UMP7520、充电 bq2560x | LK 日志 `ddrsize=3072M`、`pmic.chipid=7520` |
| 引导器版本 | SPRD LK（sprdlk），buildid `2024-01-16-17:41:42_LOCAL`，user 版 | LK 日志 |

---

## 2. 分区镜像清单（紫光特色布局）

```
引导链相关（全部双槽 A/B）：
  spl_a/b         4MB   SPL 引导器（splloader）        —— DHTB 签名
  uboot_a/b       3MB   ★内容实为紫光定制 LK(sprdlk)   —— DHTB 签名
  trustos_a/b     6MB   安全 OS（实为 Google Trusty）  —— DHTB 签名
  sml_a/b         1MB   EL3 安全监控（DVFS/电源）      —— DHTB 签名
  teecfg_a/b      1MB   TEE 配置                        —— DHTB 签名
  pm_sys_a/b      1MB   PMU 协处理器固件（Cortex-M33） —— 尾部 vbmeta
  boot_a/b        64MB  Android boot 镜像（内核+ramdisk）
  init_boot_a/b   8MB   ★全零！紫光不用 init_boot
  vendor_boot_a/b 100MB vendor ramdisk + 内核 DTB + recovery
  dtb_a/b         8MB   ★全零！内核 DTB 实际在 vendor_boot 里
  dtbo_a/b        8MB   DTBO（大端序 dt_table，7 个板级条目）
  vbmeta_a/b      1MB   AVB 根元数据
  vbmeta_{system,vendor,odm,product,system_ext}_a/b  链式 AVB
  avbmeta_rs_a/b  1MB   ★全零（预留）
Android 逻辑分区（super.img 内，全部 erofs）：
  system_a 1.64GiB / system_ext_a / vendor_a / product_a / odm_a / vendor_dlkm_a
  （b 槽 extent 数为 0 —— super 里 b 槽为空）
紫光特有分区：
  ch_sys_a/b      GNSS 芯片固件（非充电器！字符串：GNSS_FirmwareEntry_runtime）
  l_agdsp_a/b     音频 DSP 固件（"SharkL5_AUDCP_2022Y_VER_3125"）＋ vbmeta 尾部
  nr_modem/nr_phy/nr_fixnv1/2/nr_deltanv/nr_runtimenv1/2  5G NR modem 全套
  prodnv/calinv   modem 产线 NV
  ise/isedata     ★全零（预留，未启用）
  hypervsior_a/b  ★全零（hypervisor 预留，本机未启用）
  uboot_log       16MB 引导日志环形缓冲（★分析金矿）
  misc/miscdata   bootctrl(A/B) 与启动标志
  mmcblk0boot0/1  与 spl_a 内容完全相同（eMMC boot 区冗余副本）
```

---

## 3. 启动链总览

```
                     ┌──────────────────────────────────────────────┐
 eMMC                │  BootROM (芯片内置，不可改)                    │
   boot0/boot1 ──────┼──► 加载 SPL（spl 分区，校验 DHTB 签名）        │
                     └──────────────────────────────────────────────┘
                                      │
   spl_a (splloader, ~65KB)           ▼
     · DDR 初始化/扫描 (lpddr4x, bist)
     · 读 trustos_a → 装入 0xbd040000 (64MB, Trusty)
     · 读 sml_a     → 装入 0xbd000000 (256KB, EL3 DVFS 监控)
     · 读 uboot_a   → 装入 DDR 并跳转  ──────────────►  LK (sprdlk)
                                      │
   LK 阶段 (~13.5s，见第 5 节)         ▼
     · 硬件初始化（PMIC/I2C/charger/eMMC HS200）
     · 多源启动模式选择（normal/recovery/charger/fastboot/download…）
     · A/B 选择（misc 分区 bootctrl）
     · libavb 校验链：vbmeta→boot/vendor_boot/dtbo→system/vendor/…
       以及紫光扩展：nr_modem/nr_phy/l_agdsp/pm_sys（带 vbmeta 尾部）
     · Trusty IPC：keymint TA、RPMB key 检查、chip UID 传递
     · 加载内核(0x80080000) + DTB(vendor_boot) + DTBO 合并(0xbbf00000)
       + boot/vendor 双 ramdisk → 跳转内核
                                      │
   Android 阶段                        ▼
     · boot ramdisk 里的 /init = first-stage init（init_boot 不用！）
     · 挂载 vendor ramdisk、erofs 逻辑分区（AVB+dm-verity）
     · 启动紫光服务：keymint(Trusty)/tee_rpc/rpmbserver/engpc/
       modem_control/unisoc.rild/connmgr/wcn/gnss/charged…
```

---

## 4. DHTB —— 紫光安全启动签名头

`spl / uboot / trustos / sml / teecfg` 全部带 **DHTB** 头（紫光 BSP Sign，与 AVB 是两套并存机制）：

```
偏移    大小   内容
0x00    4      "DHTB" (44 48 54 42)
0x04    4      headversion = 1
0x08    32     签名/校验数据（32 字节，非明文哈希；md5/sha1/sha256 均不匹配，为紫光签名算法产物）
0x28    4      0xCCCCCCCC  （填充标记）
0x2C    4      0xAAAAAAAA  （填充标记）
0x30    4      载荷大小（LE，如 spl=0xff60、uboot=0x1ced10、trustos=0x2fe000）
0x34    4      保留
0x38    …      载荷（镜像本体）
```

载荷内部还有一层紫光镜像头（各镜像一致）：前 16 字节为零，`0x18` 处 = 载荷大小−8，
`0x1C` 处 = 载荷大小+0x698（内部代码段/校验段长度标记）。

- BootROM 用 DHTB 校验 SPL；SPL 用 DHTB 校验下一级（LK/TOS/SML）→ 芯片级信任链。
- Android 侧镜像（boot/vendor_boot/dtbo/逻辑分区）则走标准 AOSP **AVB**（vbmeta 链）。
- 外部资料印证：DHTB 即紫光 `imgheaderinsert` 工具的 "BSP sign" 头（[action_sprd_signer](https://github.com/ZH-XiJun/action_sprd_signer)）。

---

## 5. LK（uboot 分区里的 "sprdlk"）启动时序

`uboot_log.img` 保存了完整真实启动日志（多会话）。normal 会话关键时序：

| 时间(ms) | 事件 |
|---|---|
| ~900 | LK 启动："**LK (0, restored from storage)**"，dram 检测（2×1.5GB），timer，拉起 3 核 |
| 930 | "welcome to lk/MP"，平台/目标板信息（qogirn6pro/OT5，buildid 2024-01-16） |
| 1050-1800 | PMIC/I2C/时钟/charger（bq2560x、电量计 ump9620）/eMMC HS200 初始化 |
| 1793 | 读 misc 分区 ABC 数据："bootable slot 0 priority 15, tries 7"，**选择 boot_a** |
| 1816-2076 | **多源启动模式探测**（chipram_env→bat_low→miscdata→file_extend→watchdog→alarm→pctool→gpio→smpl→charger→keypad），最终 `NORMAL_MODE` |
| 2089 | "call smc to power on core1/core2"（经 SML 的 SMC 上电副核） |
| 2149 | 三线程并行：sprd_preload / sprd_lcd / self_monitor；LCD 点亮（st7365p，480x320） |
| 3129 | secure_sign_verify 线程：加载 vbmeta_a（0x99000000），`SECUREBOOT_ENABLE` |
| 3346 | 内核加载："generic kernel size 38308352"（raw 格式）→ 0x80080000；fix dtb；DTB 匹配 `sprd,sc-id=ums9620 1000 1000` |
| 3383 | DTBO 合并："entry id 0x2700"（=OT5 板级 ID），匹配 `ums9620 OT5 1000` |
| 13700 | **libavb avb_slot_verify**：逐个校验 boot_a→vendor_boot_a→dtbo_a→vbmeta_system/vendor/odm/system_ext/product→**nr_modem_a/nr_phy_a/l_agdsp_a/pm_sys_a**（紫光把 AVB 扩展到 modem/固件分区） |
| 13851 | l_agdsp 固件装入 0xafa00000；vboot 校验通过 |
| 14050 | 本会话设备已解锁：`LOCK FLAG IS: UNLOCK, SKIP VERIFY`；`boot_a: Public key ... does not match`（boot 被第三方重签，见第 9 节） |
| 14057 | **Trusty 交互**：`pass_chip_uid_to_tos()`、`lk_check_rpmb_key`（keymint TA，Get keymint version 3）、boot patch level 20230805 |
| 14176 | "enter mode normal"；组装最终 cmdline（含 androidboot.vbmeta.*/veritymode/slot_suffix 等 40+ 参数） |
| 14322 | vendor_boot ramdisk（23.4MB）→0xd0000000；boot ramdisk（2.06MB）紧随其后 |
| 14462 | "add bootconfig tailer:size=0"（bootconfig 为空）；audio cp boot ok |
| 14464 | **"[start_linux] start linux at PC:0x80080000, dt_addr:0xbbf00000"**；"lk consume time: 13563ms" |

日志还包含 **charger 会话**（androidboot.mode=charger，locked/green 状态）与一次
**autodloader/FDL 下载会话**（"reply exec act as Romcode"、`do_download`、刷写 boot_b 后重启）——
说明这批镜像本身是用紫光 ResearchDownload/FDL 协议 dump 出来的。

---

## 6. boot.img / vendor_boot.img 的紫光定制格式

### boot_a.img（header_version=4 但处处不同）

```
0x0000      ANDROID! v4 头（header_size=1580，标准 v4 应为 1584；cmdline 全空）
0x1000      ★内核从 0x1000 页对齐开始（标准 AOSP 从 header_size 开始），
            格式为 raw ARM64 Image（带 MZ/PE 头：.text 0x228b000 + .data 0x1fca00）
            内嵌 gzip IKCONFIG（内核 .config）
0x2485600   内核尾部附加 17KB 高熵数据块（无任何明文特征，疑似压缩/签名数据）
0x2489a00   ★ramdisk 区域：0x600 零填充 + 紫光 LZ4 容器
            容器 = { magic 0x184C2102, u32 尺寸, LZ4 raw 流 }（非 gzip、非标准格式！）
            LZ4 解压得 newc cpio（3.66MB，22 个条目）
0x2682000   AVB0 vbmeta 尾部（avbtool 1.1.0，key ea410c1b——与链中期望的 b9a83ffc 不符）
```

boot ramdisk 内容：`init`（**first-stage init 就放这里**）、`system/etc/ramdisk/build.prop`，
以及 **Magisk 痕迹**：`.backup/init`（原始 init 备份）、`overlay.d/sbin/{magisk64,magisk32,stub}.xz`。

### vendor_boot_a.img（v4）

```
0x0000      VNDRBOOT v4 头（header_size=0x850=2128，标准为 2112）
            cmdline: "console=ttyS1,115200n8 buildvariant=user"
0x1000      vendor ramdisk：同样的紫光 LZ4 容器（magic 0x184C2102），
            解压 54.7MB cpio，提取 584 个文件：
              first_stage_ramdisk/fstab.{OT5,ums9620_*,ums9157_*}、lib/modules（.ko 全套）、
              ueventd.OT5.rc、odm/、system/bin/recovery + recovery.fstab + 恢复 UI 资源
0x1663040   内核 DTB（160KB）："Spreadtrum UMS9620 SoC"，compatible "sprd,ums9620"
0x1662306   vendor ramdisk table（1 条目 108B）；bootconfig 为空
0x168c000   AVB0 尾部（avbtool 1.2.0，key 26ee376f——与 vbmeta_a 链一致 ✓）
```

### init_boot / dtb 分区为何是空的？

原生 Android 13 GKI 要求 first-stage init 放 `init_boot` 分区、内核 DTB 放 `vendor_boot`。
紫光 T770 这套方案：**init_boot、dtb 分区全部零填充**——first-stage init 放在 boot.img 的
ramdisk 里（init 是静态链接 aarch64 ELF），内核 DTB 放在 vendor_boot 里，dtbo 放 dtbo 分区。
这是紫光平台与原生 AOSP 最直观的差异之一。

### fstab（vendor ramdisk 内 first_stage_ramdisk/fstab.OT5）

```
system/system_ext/vendor/odm/product/vendor_dlkm → erofs ro, avb=vbmeta_*, logical, first_stage_mount
（vendor_dlkm 借 avb=vbmeta_system_ext；system 附加 q/r/s-gsi.avbpubkey）
/dev/block/by-name/metadata → f2fs/ext4 formattable
```

---

## 7. super.img —— 紫光定制的逻辑分区元数据

- 非 sparse。元数据**不是标准 liblp 布局**：
  - `0x1000 / 0x2000`：geometry（魔数 `gDla`，结构 52 字节：含 16 字节保留数据、
    metadata_max=0x10000、**3 个 metadata 槽**、块 4KB）
  - `0x3000 / 0x13000 / 0x23000`：metadata 头（`0PLA` v10.2，header_size=0x100），
    但 tables_size/描述符字段为乱码（**紫光加密/打乱**）；0x3050 处有明文的
    (offset, num, entry_size) 三元组描述符：partitions(0,12,52)、extents(624,6,24)、
    groups(768,3,48)、block_devices(912,1,64)
  - 分区表 12 条目（52B/条）；**b 槽分区 extent 数全为 0**（super 中 b 槽为空，
    与日志 "unbootable slot 1 tries_remaining: 0" 互相印证）
  - 分组：`default` + **`group_unisoc_a/b`**（紫光自定义组，上限各 5.86GB）
- 提取结果（全部 erofs）：system_a 1.64GiB / system_ext_a 301MB / vendor_a 492MB /
  product_a 142MB / odm_a 226MB / vendor_dlkm_a 7.8MB

---

## 8. Android 用户空间的紫光组件

vendor 分区 init 脚本（`vendor/etc/init/`）暴露了紫光服务生态：

- **安全**：`keymint@2.0-unisoc.service.trusty`、`gatekeeper@1.0-service.trusty`、
  `vendor.sprd.hardware.trusty-service`、`tee_rpc`、`rpmbserver`、`soter`、`vendor-oemlock`
- **引导/诊断**：`vendor.sprd.hardware.boot@1.2-service`（紫光 boot HAL）、`engpc`、
  `systemDebuggerd`、`autotest`、`phasecheckserver`、`refnotify`
- **modem/连接**：`unisoc.rild`、`modem_control`、`cp_diskserver`、`cplog_svc`、
  `slogmodem_vendor`、`connmgr`、`vendor.sprd.hardware.wcn`、`gnss@2.2`、`tsupplicant`
- **电源/充电**：`charge`、`charged`、`thermald`、`power.stats`、`dcxsrv`
- vendor_dlkm：143 个内核模块（`pinctrl-sprd-qogirn6pro.ko`、`sprd_cp_dvfs.ko`、`wcn_bsp.ko`…）
- product：TCL 定制（bootanimation、SettingsIntelligence 等）；odm：camera 算法库、MNN/NPU

---

## 9. 与原生 AOSP 启动流程差异对照（核心）

| 环节 | 原生 AOSP（Google/GKI 标准） | 紫光 T770（本机） |
|---|---|---|
| 引导器 | BootROM → **ABL（Little Kernel）** | BootROM → **SPL(splloader) → 紫光定制 LK**，多一个 SPL 阶段；LK 存放在名为 `uboot` 的分区里（内容不是 u-boot） |
| 引导器签名 | AVB（bootloader 自带 vendor key） | **DHTB 头**（紫光 BSP Sign，magic `DHTB`+CCCC/AAAA 填充）+ 载荷内二次镜像头 |
| TEE 启动时机 | ABL 或之后启动 Trusty | **SPL 就加载 Trusty(TOS@0xbd040000) 和 SML(EL3 DVFS 监控@0xbd000000)**；LK 通过 SMC 上电副核、与 keymint TA 通信 |
| 模式选择 | misc + 按键（简单） | **十几种来源级联探测**：chipram/bat_low/miscdata/watchdog/alarm/pctool/gpio/smpl/charger/keypad；模式多达 NORMAL/RECOVERY/CHARGE/FASTBOOT/DOWNLOAD/CALIBRATION/ENGTEST/AUTOTEST/APKMMI… |
| AVB 范围 | boot/vbmeta/逻辑分区 | 同左 **+ 扩展到 modem 域**：nr_modem/nr_phy/l_agdsp/pm_sys 均带 vbmeta 尾部，由 LK 统一校验 |
| AVB 状态传递 | bootconfig + kernel 属性 | 组装进 **cmdline**：`androidboot.vbmeta.*`、`androidboot.veritymode`、`androidboot.verifiedbootstate`… |
| init_boot | 必装 first-stage init | **分区存在但全零；init 放 boot ramdisk** |
| boot.img | kernel 紧跟 header；ramdisk 为 gzip cpio | kernel **0x1000 页对齐 + raw Image**（不压缩）；ramdisk 为**紫光 LZ4 容器**（magic 0x184C2102）；header_size 1580≠1584 |
| vendor_boot | header 2112；bootconfig 传递参数 | header 2176；**bootconfig 为空**；vendor ramdisk 同为紫光 LZ4 容器 |
| dtb 分区 | 一般不使用（DTB 在 vendor_boot） | 同右；另 dtbo 分区为**大端序 dt_table** |
| super 元数据 | 标准 liblp（geometry@0、2 槽） | **紫光变体**：geometry@0x1000（52B 结构）、**3 个 metadata 槽**、描述符三元组、**group_unisoc** 组、部分字段加密/打乱 |
| 文件系统 | 常见 ext4/erofs 混合 | 逻辑分区**全线 erofs**；vendor_dlkm 借 `vbmeta_system_ext` |
| 下载模式 | fastboot | 额外内置 **autodloader/FDL（ResearchDownload）**：可"act as Romcode" 走 USB DL 协议 |
| 诊断 | pstore/ramoops | 专用 **uboot_log 分区**持久化带 [ms][cpu] 时间戳的多会话引导日志 |
| 防回滚 | rollback index (RPMB) | 同左 + **OS 版本三重检查**（boot header/footer/config 三处）+ imgversion/modemimgver 写入 RPMB |
| 协处理器 | vendor 分区软件 | 大量独立固件分区：pm_sys(PMU-M33)、l_agdsp(音频 DSP)、ch_sys(GNSS)、5G NR modem 全套、ise/hypervisor（本机预留未启用） |

---

## 10. 重要取证发现

1. **该机已解锁并被 root**：vbmeta 会话显示 `verifiedbootstate=orange`、`flash.locked=0`；
   boot_a 的 vbmeta 尾部被第三方 key 重签（`ea410c1b`，链中期望 `b9a83ffc`）→
   LK 报 `ERROR_PUBLIC_KEY_REJECTED` 但因解锁而放行（"SKIP VERIFY"）。
2. **Magisk 已安装**：boot ramdisk 内 `.backup/init`（原始 init）、`overlay.d/sbin/magisk64.xz`
   等为 Magisk 修补痕迹；`/init` 已被 magiskinit 替换。
3. **日志含 3 种会话**：normal（解锁/橙色）、charger（锁定/绿色，`androidboot.mode=charger`，
   说明曾处于上锁出厂态关机充电）、download（FDL 刷写 boot_b 后重启）——
   时间线可推断：出厂锁定 → 某次充电 → 解锁刷 Magisk。
4. `avbmeta_rs`、`ise`、`hypervsior`、`init_boot`、`dtb` 分区全零：预留/未启用，
   不能仅凭分区存在判断功能启用。

---

## 11. 解包产物与工具（工作区目录）

```
/home/test/t508n/
├── Image/                     # 从 E: 复制的全部镜像
├── payloads/                  # DHTB 剥离后的 spl/uboot/trustos/sml/teecfg 载荷
├── tools/
│   ├── avbtool.py             # AOSP 官方 avbtool（googlesource 拉取）
│   ├── lz4dec.py              # 自写 LZ4 原始块解码器
│   ├── cpio_extract.py / cpio_extract2.py  # newc cpio 提取
│   └── lpunpack.py            # 紫光 LP 元数据解析（变体适配）
├── work/
│   ├── boot_ramdisk_cpio.bin / boot_ramdisk_out2/   # boot ramdisk（含 Magisk 痕迹）
│   ├── vendor_ramdisk_cpio.bin / vendor_ramdisk_out/ # vendor ramdisk（584 文件）
│   ├── vendor_boot.dtb / dtbo0.dtb / fixdtb.bin      # 设备树
│   ├── kernel.config         # 内核 IKCONFIG
│   ├── footers/*.vbmeta      # 各分区 vbmeta 尾部
│   ├── super_raw/*.img       # 6 个 erofs 逻辑分区镜像
│   └── fs/{system,vendor,product,odm,system_ext,vendor_dlkm}_a/  # 文件系统全量提取
└── /tmp/ubootlog.txt         # uboot_log 分区全部字符串日志
```

方法要点：`fsck.erofs --extract` 无 root 提取 erofs；`dtc` 反编译 DTB/DTBO（dtbo 大端序）；
自研脚本处理 DHTB/紫光 LZ4 ramdisk 容器/LP 变体元数据；`uboot_log` 字符串分析还原真实时序。

## 12. 参考资料

- 紫光 DHTB/BSP 签名与 AVB 签名方法：[ZH-XiJun/action_sprd_signer](https://github.com/ZH-XiJun/action_sprd_signer)
- T508N 机型背景：[B站火了一款可玩王者荣耀的老人机，T508N](https://post.smzdm.com/p/aeq05ol3/)、[130元全新5G老人机](https://post.smzdm.com/p/amvkqkqk/)
