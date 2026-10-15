# 报告：vddusb33 变体矩阵假说 —— 离线核验（T1 前置结论）

> 依据：`work/prompt-vddusb33-variant-matrix.md` + `work/ARCHIVE-20260823.md` §23–25 +
> `work/GUIDE-vddusb33-stock-design.md` + 源码/DTB 工件离线核验。本会话无 bash/设备，
> 全部为离线证据；**T1（机上 DTB 实读）仍是必须的判门**。

---

## 0. 结论速览（TL;DR）

| # | 结论 | 置信 |
|---|---|---|
| 1 | **假说的机制层成立**：mfd-core 按 cell `of_compatible` 链 of_node（`mfd-core.c:184-187`），9621 矩阵 regulator cell = `"sprd,ump9621-regulator"`（`sprd-ump96xx-spi.c:124-125`），与 DT 的 `"sprd,ump962x-regulator"` 不匹配则链不上 → cell 设备存在但 of_node=NULL → ② NULL。 | 高（源码直接证实） |
| 2 | **但"运行时匹配了 9621 矩阵"这一前提，与库存 DTB 结构冲突**：原厂 DTB 是**三 PMIC 同总线**（`spi@400000/pmic@0`=ump9620、`pmic@1`=ump9621、`pmic@2`=ump9622）。运行时会**同时**选 9620+9621+9622 三张矩阵——9621 矩阵被选不是异常，是原厂设计。 | 高（`stock_ums9620.dts:2265/2904/2982` + match 表） |
| 3 | §25 的 platdev 证据（`ump9621-regulator` + `ump962x-syscon` + `ump9620-efuse`）**恰好是库存三 PMIC 设计的正常产物**，不能作为"主 PMIC 错配 9621"的证据。 | 高 |
| 4 | **真正判据 = 机上 booted DTB 的 `pmic@0` compatible + `syscon@2000` 存在性**。§23 ① 曾实测运行时 `of_find("sprd,ump962x-syscon")=NULL`，这与库存 DTB（含该节点）矛盾 → 要么当时 booted DTB 非库存，要么观测有误。**离线工件中没有任何 DTB 复现"无 syscon"状态**。 | 中（需 T1） |
| 5 | 若 T1 证实 booted DTB = 库存（pmic@0=9620 + syscon@2000 在）→ **假说排除**，② NULL 应另有其因（MFD/regmap 侧），走 §25 遗留疑点（需 kmsg/探针级证据）。 | — |
| 6 | 若 T1 证实 booted DTB 的 pmic@0 = `"sprd,ump9621"`（且 power-controller 仍为 `sprd,ump962x-regulator`）→ **假说坐实**，走 F1（本报告已备好 overlay 源码）。 | — |

---

## 1. 源码核验（全部为真）

| 声明（prompt/指南） | 实测位置 | 结果 |
|---|---|---|
| 9620 矩阵 regulator cell `ump962x-regulator` / `sprd,ump962x-regulator` | `sprd-ump96xx-spi.c:87-88` | ✓ |
| 9620 矩阵 syscon cell `ump962x-syscon` / `sprd,ump962x-syscon` | `sprd-ump96xx-spi.c:108-109` | ✓ |
| **9621 矩阵 regulator cell `ump9621-regulator` / `sprd,ump9621-regulator`（此名只存在于 9621 矩阵）** | `sprd-ump96xx-spi.c:124-125` | ✓ |
| 9621 矩阵 = 仅 3 cell（regulator/efuse/syscon） | `sprd-ump96xx-spi.c:122-133` | ✓ |
| 变体选择 = 纯 DT compatible match | `sprd-ump96xx-spi.c:317-329`（sprd,ump9620/9621/9622 → 三矩阵） | ✓ |
| regmap 挂在 SPI 设备（`devm_regmap_init(&spi->dev)`，MFD 建 cell 之前） | `sprd-ump96xx-spi.c:257` + `:299/:305` | ✓ |
| mfd-core 按 of_compatible 链 of_node | `mfd-core.c:184-187`（`for_each_child_of_node(parent->of_node)` + `of_device_is_compatible`） | ✓ |
| hsphy ① ② ③（§25 版 = 已含 §23 修复：regulator 优先 + syscon 兜底 + by-name 双名 + SPI 兜底） | `vendor-kernel-ums9620/drivers/usb/phy/phy-sprd-qogirn6pro.c:711-750` | ✓（tuned 树仍为旧版，勿混淆） |
| LDO_VDDUSB33 在 pmic@0/power-controller@2000 下（regulator-name "vddusb33"） | `stock_ums9620.dts:2742-2748` | ✓ |

---

## 2. 关键事实：库存 DTB = 三 PMIC 同总线（假说的盲区）

`stock_ums9620.dts`（= 原厂 FDT 160,099B 的等价 DTS）`/soc/aon/spi@400000` 下：

| 节点 | compatible | 关键子节点 | 行 |
|---|---|---|---|
| `pmic@0` (reg=0) | `sprd,ump9620` | `syscon@2000`=`sprd,ump962x-syscon`；`power-controller@2000`=`sprd,ump962x-regulator\0sprd,sc27xx-regulator`；内含 LDO_VDDUSB33 | 2265/2500-2501/2512-2513/2742 |
| `pmic@1` (reg=1) | `sprd,ump9621` | `syscon@2000`=`sprd,ump9621-syscon`；`power-controller@2000`=`sprd,ump9621-regulator` | 2904/2912-2913/2926-2927 |
| `pmic@2` (reg=2) | `sprd,ump9622` | `syscon@2000`=`sprd,ump9622-syscon`；`tsensor@20f8` | 2982/2991-2992/2996 |

推论（对假说的三个关键修正）：

1. **运行时同时选 9620 与 9621 矩阵**（还有 9622）。§25 在 platdev 里看到 `ump9621-regulator`
   （= pmic@1 的 9621 cell，of_node 应链到 pmic@1/power-controller@2000，正常）
   与 `ump962x-syscon`/`ump9620-efuse`（= pmic@0 的 9620 cell，正常）——**这正是库存设计**，
   不是"主 PMIC 错配"。
2. **若 booted DTB 是库存**，hsphy 的 ① `of_find("sprd,ump962x-regulator")` 命中 pmic@0/
   power-controller@2000；mfd-core 会把该节点链到 9620 矩阵的 `ump962x-regulator` cell；
   ② by-name/of_find_device_by_node 应命中；③ `dev_get_regmap(parent=pmic@0 SPI 设备)` 应拿到
   `devm_regmap_init` 的 regmap（`devm_regmap_init` 在 `mfd_add_devices` **之前**执行）。
   → 库存 DTB 下假说链**不应断**。
3. 因此 §25 "② 仍 NULL + by-name 找到 9621-regulator 但 regmap NULL" 与库存 DTB 相容性存疑。
   若真在库存 DTB 上复现，断点更可能在 §25 遗留疑点（cell of_node 链接条件 / regmap 所有权 /
   MFD probe 时序），需要 kmsg/探针级证据，而不是 DTB 兼容名。

---

## 3. 离线工件指纹（work/ 下 DTB 变体）

| 工件 | `sprd,ump9620` | `sprd,ump9621` | `ump962x-syscon` | `ump962x-regulator` | 判定 |
|---|---|---|---|---|---|
| `stock_ums9620.dts`（DTS，权威） | ✓ | ✓ | ✓ | ✓ | 库存三 PMIC |
| `stock_ums9620_v2.dts` / `v3.dts` | ✓ | ✓ | ✓ | ✓ | 同库存（v3 差 1 行=重编） |
| `work/vendor_boot.dtb` | ✓(4) | ✓(1) | ✓ | ✓ | 库存派生 |
| `work/ot5-vendor.dtb` | ✓ | ✓ | ✓ | ✓ | 库存派生 |
| `work/vendor_boot_a_usb2.img` / `a_v3.img` | ✓ | ✓ | ✓ | ✓ | 库存派生（内含同款 DTB） |
| `work/vendor_boot_linux_new.img` | ✗ | ✗ | ✗ | ✗ | mainline 精简 DTB（ot5-new，5823B） |
| `work/ot5.dtb` / `ot5-new.dtb` | ✗ | ✗ | ✗ | ✗ | mainline 精简 DTB |
| `work/dtbo0-6.dtb`（原厂 dtbo 七条 overlay） | ✗（无 pmic compatible） | — | — | dtbo0 有 8 处 power-controller/fragment 类字符串 | 非 pmic 补丁（DSI/电池等） |

**没有**任何离线工件复现 §23 ① 的"运行时 `sprd,ump962x-syscon` 不存在"状态。
→ 当前设备上 vendor_boot_a/dtbo_a 的实际内容（尤其是否被换过）**只能 T1 确认**。

---

## 4. 修正后的执行决策树（替代 prompt §2 的三选一）

```
T1（机上）：cat /proc/device-tree/.../pmic@0/compatible 等（命令见 §5）
│
├─ pmic@0 = "sprd,ump9621"
│    └─ 假说坐实 → F1：DTBO overlay 给 power-controller@2000 补 "sprd,ump9621-regulator"
│       （overlay 源码已备：work/dtbo-overlay-vddusb33-9621.dts）
│
├─ pmic@0 = "sprd,ump9620" 且 syscon@2000("sprd,ump962x-syscon") 在（= 库存 DTB）
│    ├─ 假说排除（9620 矩阵在跑，cell 应链 pmic@0/power-controller@2000）
│    ├─ 但先查 T3：当前 vendor_boot/dtbo 与库存一致性（若被换过 → F3 恢复库存镜像）
│    └─ 仍 ② NULL → 断点转移：mfd-core of_node 链接条件 / regmap 所有权 / MFD probe 时序
│       （需 kmsg/串口；§25 "严格 T508N-only 读取器" 是下一步必需）
│
└─ pmic@0 缺失或无 pmic 子树（精简 DTB 症状）
     └─ vendor_boot_a 仍是旧精简 DTB（Aug16 时代）→ F3：刷回原厂 vendor_boot_a + dtbo（noop 可）
```

> ⚠️ **F1 危险点（若误用于库存 DTB）**：给 pmic@0/power-controller@2000 加
> `sprd,ump9621-regulator` 后，9621 矩阵的 cell 也会链到该节点 → 两个 cell 平台设备共享
> 同一 of_node。`of_find_device_by_node` 返回首个（创建序=pmic@0 先）→ 通常仍 OK，但属
> 冗余改动，且不会修复"9620 cell 未链"以外的任何问题。**只在 T1 证实 pmic@0=9621 时用。**

---

## 5. 机上 T1/T2 命令（无串口，软件可判）

```sh
# T1 第一眼（booted DTB 实读）
ls /proc/device-tree/soc/aon/spi@400000/ 2>/dev/null          # 应见 pmic@0 pmic@1 pmic@2
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/compatible     # 判 9620 / 9621（关键!）
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/syscon@2000/compatible
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/power-controller@2000/compatible
cat /proc/device-tree/soc/aon/spi@400000/pmic@1/compatible

# T2 MFD 实际矩阵（platdev 快照）
ls /sys/bus/platform/devices/ | grep -iE 'ump96|regulator|syscon|efuse'
ls /sys/bus/spi/devices/                                       # 应见三个 pmic SPI 设备
dmesg | grep -iE 'ump96|pmic|mfd|regmap'                       # MFD probe 日志（若有）

# V1 验证（修复后）
ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/              # 64300000.hsphy bound?
for i in $(seq 1 20); do cat /sys/class/udc/musb-hdrc.0.auto/state; sleep 1; done
```

> 注意：当前设备为自建 Linux（Alpine rootfs），`/proc/device-tree` 直接可用；
> 若在官方 Android(b 槽) 下则需 root。

---

## 6. 刷机/回滚（t508n-flashing 技能要点）

```bash
# F3：恢复库存（vendor_boot_a = 原厂 DTB，dtbo_a = noop）
/mnt/c/platform-tools/fastboot.exe flash vendor_boot_a /home/test/t508n/Image/vendor_boot_a.img
/mnt/c/platform-tools/fastboot.exe flash dtbo_a /home/test/t508n/work/noop_dtbo.img   # 或原厂 dtbo.img
/mnt/c/platform-tools/fastboot.exe flash boot_a <boot_54vendor.img 对应版本>
/mnt/c/platform-tools/fastboot.exe reboot
# 回滚基线：boot_a=ed0ebdd6（§22）/ e75a61de（§23）/ 6db9c563（§24）；uboot=原厂 7fe2b5ea

# F1（仅 T1 证实 pmic@0=9621 后）：编译 overlay → 打包 dtbo_a → 刷入
dtc -@ -I dts -O dtb -o overlay.dtbo work/dtbo-overlay-vddusb33-9621.dts
# 打包 dtbo_a 用 scripts/mkdtboimg.py（Android 工具）或按 noop_dtbo.img 同格式重打
# ⚠️ fastboot 直写 dtbo 曾报 "Too many links" → 备选 adb+dd（需 Android b 槽）或 Linux 下 dd
```

---

## 7. 遗留（§25 疑点，若假说排除后仍 ② NULL）

1. mfd-core cell of_node 链接的运行时条件（`parent->of_node` 是否为 pmic@0；`of_device_is_compatible`
   在运行时 compatible 表上的表现）。
2. `dev_get_regmap(pd9->parent)` 为 NULL 的真实原因（devm_regmap_init 的 devres 归属；
   SPI 设备是否真的 probe 成功）。
3. 需要 MFD probe 内部日志 / 串口 / 严格 T508N-only kmsg 读取器。

---

## 8. 证据文件

- 本报告：`work/REPORT-vddusb33-variant-matrix-offline.md`
- F1 overlay 源码（条件使用）：`work/dtbo-overlay-vddusb33-9621.dts`
- F1 dtbo 打包脚本：`tools/build_vddusb33_9621_dtbo.py`
- 实机调试提示（下一 session 用）：`work/prompt-vddusb33-fastboot-debug.md`
- 复核材料：`work/vendor-kernel-ums9620/drivers/mfd/sprd-ump96xx-spi.c`、
  `.../drivers/usb/phy/phy-sprd-qogirn6pro.c`、`.../drivers/mfd/mfd-core.c`、
  `work/stock_ums9620.dts`、`work/ARCHIVE-20260823.md` §23-25
