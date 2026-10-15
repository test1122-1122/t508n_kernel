# GUIDE：按原厂设计补 vddusb33 regmap 环（MFD cell 路径）

> 给另一 session 的执行指南。目标：按原厂机制（MFD cell → of_node → 平台设备 → 父 regmap）
> 闭合 hsphy 的 ②③ 环，使 REG33: enabled → HS-PHY 3.3V → 主机枚举。
> **这不是绕道，是让原厂设计在自制链上成立**——regulator/syscon cell 在厂商源码表里就存在，
> mfd-core 链 of_node 是内核标准行为。§24 的"bus-match 论 / regmap 私有论"与源码矛盾，
> 更可能是 **MFD 链没 probe** 的表象（见 §2 复核判据）。

---

## 1. 原厂机制（30 秒）

```
MFD sprd-ump96xx-spi (SPI 驱动, match "sprd,ump9620") probe
  ├─ devm_regmap_init(&spi->dev, &sprd_pmic_regmap, &spi->dev, &sprd_pmic_config)   ← regmap 挂 SPI 设备
  └─ mfd_add_devices → cell 平台设备（mfd-core 按 of_compatible 链 of_node）:
       { "ump962x-regulator", of_compatible "sprd,ump962x-regulator" }  ← sprd-ump96xx-spi.c:42-43
       { "ump962x-syscon",    of_compatible "sprd,ump962x-syscon"    }  ← sprd-ump96xx-spi.c:63-64
       匹配 pmic@0 直接子节点: power-controller@2000 (regulator) / syscon@2000 (syscon)

hsphy probe (phy-sprd-qogirn6pro.c:709-729):
  ① regmap_np = of_find_compatible_node("sprd,ump962x-regulator")   [兜底 syscon，已修]
  ② regmap_pdev = of_find_device_by_node(regmap_np)   ← cell 平台设备；NULL=硬失败(-ENODEV)
  ③ phy->pmic = dev_get_regmap(regmap_pdev->dev.parent)  ← 父=PMIC SPI 设备，regmap 可及
```

**为什么自制链断**：cell 平台设备不存在（MFD 没 probe / 没建 cell / 时序早于 hsphy）→ ② NULL →
hsphy unbound → PHY 从未初始化 + REG33 disabled → attached 但主机不枚举。

---

## 2. 现场复核（10 分钟，先定位堵点）

| # | 判据 | 命令 | 结果含义 |
|---|---|---|---|
| A | pmic@0 是否成 SPI 设备 | `ls /sys/bus/spi/devices/` | 无 pmic@0 → **spi-sprd-adi 没跑**（堵点在 ADI 链） |
| B | cell 平台设备是否存在 | `ls /sys/bus/platform/devices/ \| grep -iE 'ump96\|regulator'` | 无 ump962x-regulator/syscon → **MFD 没建 cell** |
| C | MFD probe 日志 | `dmesg \| grep -iE 'ump96|pmic|mfd|regmap\|sprd-ump96xx'` | 报错/无日志 → 定位 probe 失败点 |
| D | hsphy 绑定态 | `ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/` | 无 64300000.hsphy → probe 失败（②/③） |

判读：
- **A 缺** → 走 §3.1；**B 缺**（A 在）→ 走 §3.2；**B 在但 D 缺** → 走 §3.3/§3.4。

---

## 3. 按原厂执行的步骤（决策树）

### 3.1 pmic@0 SPI 设备缺失 → 修 spi-sprd-adi 链
- 模块已在链（mods[] 188-192：clk-sprd / ums9620-clk / spi-sprd-adi / sprd-ump96xx-spi / ump9620-regulator）；
- 查 `dmesg | grep -i adi`：spi-sprd-adi probe 是否失败（依赖：adi 时钟 = clk-sprd/ums9620-clk 已在链）；
- probe 成功后 SPI core 自动 `of_register_spi_devices` 注册 pmic@0（DTS: /soc/aon/spi@400000/pmic@0, compatible "sprd,ump9620"）。

### 3.2 MFD 没建 cell → 查 sprd-ump96xx-spi probe
- SPI 驱动 match "sprd,ump9620"（sprd-ump96xx-spi.c:318-319）与 pmic@0 compatible ✓；
- probe 顺序：`devm_regmap_init`（line 257）→ irq chip（line 291）→ `devm_mfd_add_devices`（line 299/305）；
- 检查 `mfd_add_devices` 返回值（失败会打印）；变体表选择：pmic_ump9620_matrix（line 145）含 regulator/syscon cell。

### 3.3 cell 在但 hsphy ② 仍 NULL → 时序
- hsphy probe 早于 cell 建立 → `phy-sprd-qogirn6pro.c:719-723` 把 `ret = -ENODEV` 改为
  **`return -EPROBE_DEFER`**（让内核在 cell 就绪后重试——这是"让原厂设计成立"的修正，不是绕道）；
- 或调整 init 模块加载顺序：MFD 组先于 musb 组。

### 3.4 ② 命中但 ③ NULL → regmap 获取
- `dev_get_regmap(regmap_pdev->dev.parent, NULL)`——parent 应为 PMIC SPI 设备；
  regmap 由 `devm_regmap_init(&spi->dev,...)` attach（若 MFD probe 成功则必可及）；
- 若 parent 不是 SPI 设备（of_platform 化产生的 parent 不同），改为
  `of_find_compatible_node(NULL,NULL,"sprd,ump9620")` → spi 总线找设备 → `dev_get_regmap(&spi_dev->dev)`。

### 3.5 全就绪 → 验证
- hsphy bound ✓（判据 D）；
- **REG33: enabled**（init 探针或 regulator sysfs）；
- UDC state 推进：attached → default → address → configured（`cat /sys/class/udc/musb-hdrc.0.auto/state` 轮询）；
- 主机 `lsusb` 出现 T508N（f_serial → CDC ACM / ttyACM）。

---

## 4. 最终判定

| 结果 | 判定 | 下一步 |
|---|---|---|
| hsphy bound + REG33 enabled + UDC state 到 configured + 主机见 CDC ACM | ✅ 通 | USB 战役收官 |
| 环闭合但停在 attached | ⑥排除 | ⑧ DPDM/bc1p2 终态 → ⑨ 模块来源核查 |
| 红屏 | 放行面过大 | 收敛精准放行（hsphy+dwc3） |

---

## 5. 风险与回滚

- 不改强制角色/域唤醒逻辑（防 §20 协商压过强制回归）；
- 改动面 = 加载顺序 / 一行 EPROBE_DEFER / 必要时 MFD 探测依赖——均属"补原厂机制"；
- 回滚：boot_a 刷回 6db9c563（§24 已备份版）或 ea9c949e（§23 回归版）。

---

## 6. 关联材料

- 机制源码：`work/vendor-kernel-ums9620/drivers/mfd/sprd-ump96xx-spi.c`（cell 表 :42-43/:63-64、
  regmap :257、变体表 :145-150、match :318-319）
- hsphy 三环：`drivers/usb/phy/phy-sprd-qogirn6pro.c:709-729`
- regulator 驱动：`drivers/regulator/ump9620-regulator.c`（compatible "sprd,ump962x-regulator"）
- DTS 节点：`work/stock_ums9620.dts`（syscon@2000 ~line 2501；power-controller@2000 ~line 2510；
  LDO_VDDUSB33 line 2743）
- 背景：`ARCHIVE-20260823.md` §23/§24；`work/REPORT-USB-CONSOLIDATED.md` §3.4/§3.5
