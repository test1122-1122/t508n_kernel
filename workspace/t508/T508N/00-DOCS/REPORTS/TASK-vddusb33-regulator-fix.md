# TASK（§23 续）：vddusb33 上电 —— hsphy regmap 链修复（② MFD cell of_node 单点）

> 状态：vddusb33 定位已达最深一层（ARCHIVE-20260823 §23，2026-08-25 19:47）。
> **REG33: disabled 已实测确认**（rail 确实没上电，C1 前提成立）。
> ⚠️ 原任务书（本文件旧版）假设"链缺 PMIC"**已证伪**：链内 mods[] 188-192 本就有
> clk-sprd / ums9620-clk / spi-sprd-adi / sprd-ump96xx-spi / ump9620-regulator。
> **当前唯一卡点 = hsphy probe 的 regmap 三环中的 ②：MFD cell 的 of_node 链接。**

---

## 0. 一句话目标

让 hsphy probe 拿到 PMIC regmap（原厂式三环：① compatible 名字 → ② MFD 子平台设备 →
③ 父 regmap），使 `regulator_enable(vddusb33)` 真正执行 → **REG33: enabled → HS-PHY 3.3V → 主机枚举**。

## 1. 已确认事实（§23，勿重踩）

| 项 | 事实 |
|---|---|
| REG33 | **disabled（实测）**——rail 未上电，C1 前提成立 |
| hsphy 真名 | **64300000.hsphy**（非 300000.phy）；不在 devices_deferred（非 EPROBE_DEFER） |
| ① 兼容名 | **已修复**：probe 找 `sprd,ump962x-syscon` 运行时找不到（运行时 = **`sprd,ump962x-regulator`**，dtbo 覆盖/基座差异）；修复 = 优先 regulator、syscon 兜底；已合入 boot_a=**ea9c949e** |
| **② MFD cell of_node** | **当前卡点**：`of_find_device_by_node(regmap_np) = NULL` → `dev_get_regmap(pdev->dev.parent)` 拿不到 → regmap 环断（MFD_UMP96XX_PMIC=m ✓，sprd-ump96xx-spi.ko 链内 ✓；剩余 = cell of_node 时序/链接） |
| ③ regmap | 待②（原厂式 = ①名字 ②MFD 子平台设备 ③父 regmap 三环） |
| 回归状态 | **STEP3 attached 已回归 ✓**（强制逻辑未破坏） |

## 2. 任务（§24 定稿）：让 power-controller@2000 节点获得平台设备（② 闭合）

**根因（§24 实测）**：运行时兼容 `sprd,ump962x-regulator` 不在 `of_platform_default_bus_match`
（simple-bus/soc/syscon…）→ **无平台设备 → `of_find_device_by_node = NULL`**；
原厂 `sprd,ump962x-syscon` 是 bus-match ✓ → of_platform 化 ✓ → 有设备 ✓。

**主路线 R1（DTBO 双兼容，推荐）**：在 **dtbo overlay** 里给 `power-controller@2000` 加回
`"sprd,ump962x-syscon"` 第二兼容（bus-match 项）→ of_platform 化 → 平台设备出现 →
hsphy ② 命中 → ③ `dev_get_regmap(parent)` 闭合。
- 载体 = **dtbo_a（8MB，可重排）**；不要动 vendor_boot DTB（那边重排 +3400 槽位不足，已试败）。
- 注意保留 regulator 兼容（驱动匹配靠它）：双兼容 `"sprd,ump962x-regulator" + "sprd,ump962x-syscon"`。
- 备选 R2：MFD 暴露私有 regmap（`sprd-ump96xx-spi.c` 的 `ddata->regmap` 显式传给子设备）。

验证：dmesg hsphy probe 无 regmap 错误；**REG33: enabled**；UDC state 从 attached 推进到
default/address/configured；主机 `lsusb` 见 CDC ACM（ttyACM）。

## 3. 弯路记录（防重踩，§23）

- `syscon_regmap_lookup_by_compatible(regulator/syscon)` 失败（该节点**非 syscon 类**）；
- pmic 真名 = **`sprd,ump9620`**（非 sprd,ump96xx-pmic / sc27xx-pmic）；且 pmic = **SPI 设备（非
  platform）**——`of_find_device_by_node` 平台侧拿不到；
- 加 7 件 PMIC 模块 = **重复+冲突**（已回滚；EEXIST = 成功处理保留）；
- 全量放行 = 红屏（其他设备 SError）；**精准放行（hsphy+dwc3）= 安全**。

---

## 4. regmap 环分析（代码级，2026-08-25 本会话补充）

### 4.1 环的结构（三段代码）

```
regmap 持有者 = PMIC SPI 设备 (pmic@0, sprd-ump96xx-spi, SPI 驱动)
   devm_regmap_init(&spi->dev, &sprd_pmic_regmap, &spi->dev, &sprd_pmic_config)  ← regmap 挂 SPI 设备
   devm_mfd_add_devices(&spi->dev, ..., sprd_ump9620_devs, ...)                  ← 建 cell 平台设备
        cell.of_compatible = "sprd,ump962x-regulator"   (sprd-ump96xx-spi.c:88)
        mfd-core: 在 pmic@0 的直接子节点里匹配兼容 → 链 of_node
        ✓ DT 侧确认: power-controller@2000 (pmic@0 直子) compatible="sprd,ump962x-regulator"

hsphy probe (phy-sprd-qogirn6pro.c:709-729):
   ① regmap_np = of_find_compatible_node("sprd,ump962x-regulator")   [兜底 syscon，已修复]
   ② regmap_pdev = of_find_device_by_node(regmap_np)   ★ NULL → dev_err + ret=-ENODEV + goto（硬失败！）
   ③ phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL)   [父 = PMIC SPI 设备]
```

### 4.2 环断的后果链（这就是"attached 但主机不枚举"的软件根因）

- ② 返回 NULL → **hsphy probe 硬失败（-ENODEV，非 defer）→ hsphy 驱动 unbound**
  （§23 step-0 的 "NODRV + no-defer" 正是这个）；
- → HS-PHY 从未初始化（眼图/DPDM/trim 全没写）→ **D+/D− 无驱动信号**；
- → **REG33: disabled**（没人调 regulator_enable——regmap 没拿到）；
- → musb 数字部分照常 → **attached 成立**；主机看不到设备 → 不枚举。

### 4.3 ② 为 NULL 的候选原因（按概率）

1. **MFD cell 平台设备不存在或其 of_node 未链**：mfd-core 只在 pmic@0 **直接子节点**匹配。
   需查：① sprd-ump96xx-spi 是否真的 probe（pmic@0 是否被 spi-sprd-adi 注册为 SPI 设备——
   否则 MFD 根本没跑）；② 变体表选择（pmic@0 compatible="sprd,ump9620" → ump9620_devs ✓ 含
   regulator cell）；③ power-controller@2000 兼容名（已确认 ✓ 直子 ✓）——DT/表都齐，
   **失败大概率在"MFD 未 probe 或 mfd_add_devices 未执行"**。
2. **时序**：hsphy probe 早于 MFD cell 创建 → NULL；且驱动用 -ENODEV（不 defer）→ 永久 unbound。
3. mfd_add_devices 失败（id 冲突/返回值未查）。

### 4.4 修复方向（§24 实测后更新）

> ⚠️ **§24 实测否决**：SPI-bus 兜底（旧方案 A）✗——**MFD regmap 是展锐私有 `ddata->regmap` 保管，
> 不在 dev 注册表**（与源码 `devm_regmap_init` 有张力，但以实测为准）；vendor_boot DTB 加双兼容 ✗
> （重排 +3400 槽位不足）。② 根因 = **运行时兼容非 bus-match → 无平台设备**（原厂 syscon 是 bus-match）。

- **R1 ★ 推荐：DTBO 双兼容/repack**：给 `power-controller@2000` 加回 **`"sprd,ump962x-syscon"`** 第二兼容
  （bus-match 项）→ of_platform 化 → 平台设备出现 → ② `of_find_device_by_node` 命中 → 原厂式三环闭合。
  **载体用 DTBO（dtbo_a 8MB 可重排）**，不动 vendor_boot DTB（槽位不足的坑在那边）。
- **R2 备选：MFD 暴露私有 regmap**：改 `sprd-ump96xx-spi.c`，把 `ddata->regmap` 显式传给子设备
  （mfd_cell 字段 / dev_set_drvdata / 导出 getter）——改厂商源码契约，hsphy 侧相应改用新入口。
- **R3 兜底：串口日志通道**就绪后，从 MFD probe 内部直接传递 regmap。
- 判据（无论哪条）：hsphy bound ✓（`/sys/bus/platform/drivers/phy-sprd-qogirn6pro/64300000.hsphy`）→
  **REG33: enabled** → UDC state 推进到 default/address/configured → 主机 `lsusb` 见 CDC ACM。

## 5. 判定标准

| 结果 | 判定 | 下一步 |
|---|---|---|
| REG33: enabled + UDC state 到 default/address/configured + 主机见 CDC ACM | ✅ 通 | USB 战役收官：attached→枚举全链达成 |
| regmap 环闭合但仍停在 attached | ⑥排除 | 转 ⑧ DPDM/bc1p2 终态（读 hsphy DPDM 开关）→ ⑨ 模块来源核查 |
| 红屏 | 放行面过大 | 收敛到精准放行（hsphy+dwc3） |

## 6. 风险与回滚

- 只动 regmap/名字相关（内核补丁或 init 侧），**不改强制角色/域唤醒逻辑**（防 §20 协商压过强制回归）；
- 回滚：boot_a 刷回 **ed0ebdd6**（§22 基线）或 ea9c949e（§23 已回归版）。

## 7. 关联材料

- 定位记录：`ARCHIVE-20260823.md` §23（三步定位 + 弯路）
- MFD 源码：`work/vendor-kernel-ums9620/drivers/mfd/sprd-ump96xx-spi.c`
- regulator 源码：`work/vendor-kernel-ums9620/drivers/regulator/ump9620-regulator.c`
- hsphy 驱动：`phy-sprd-qogirn6pro`（probe 的 syscon/regmap 查找逻辑，`TASK-A-B-USB-DTS-C-离线成果.md` §1.2）
- 背景：`work/REPORT-USB-CONSOLIDATED.md` §3.4（C1 候选）/ §3.5（差异表）
