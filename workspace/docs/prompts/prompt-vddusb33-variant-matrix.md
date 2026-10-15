# 提示：vddusb33 regmap 环 —— 变体矩阵假说验证与修复（含测试/验证方法）

> 给下一 session。症状：自制链 attached 但主机不枚举；REG33 disabled；hsphy NO-DRIVER
> （② `of_find_device_by_node` = NULL）。§23–25 已排除：① 兼容名（已修）、cell 缺失（§24 作废，
> cell 存在）。
> **本提示的假说**：运行时 PMIC 匹配了 **9621 变体矩阵** → 其 regulator cell of_compatible
> `"sprd,ump9621-regulator"` **≠** DT 节点 `power-controller@2000` 的兼容 `"sprd,ump962x-regulator"`
> → mfd-core 不链 of_node → cell 设备存在但 of_node=NULL → ② NULL → hsphy unbound → PHY 未初始化
> + REG33 disabled。
> 证据线索：§25 实测 platdev 里有 **ump9621-regulator**（此名只存在于 9621 矩阵，sprd-ump96xx-spi.c:125）。

---

## 1. 测试步骤（按序，每条 ≤10 分钟可判）

### T1 确认 booted DTB 的 pmic 变体与节点兼容 ★（第一眼）
- 设备上（优先）：
  ```sh
  # 挂载设备树（若 /proc/device-tree 已挂可直接看）
  ls /proc/device-tree/soc/aon/spi@400000/pmic@0/ 2>/dev/null
  cat /proc/device-tree/soc/aon/spi@400000/pmic@0/compatible
  cat /proc/device-tree/soc/aon/spi@400000/pmic@0/power-controller@2000/compatible
  cat /proc/device-tree/soc/aon/spi@400000/pmic@0/syscon@2000/compatible
  # 或 dump 全量 FDT：cat /sys/firmware/fdt > /home/fdt.dtb（经 fb 日志/pstore 回读）
  ```
- 离线兜底：反编译当前 vendor_boot DTB + DTBO 合并结果（`dtc -I dtb -O dts`）查上述节点。
- **判读**：
  - pmic@0 = `sprd,ump9621` → **假说坐实路径**（走 §2-F1/F2）；
  - pmic@0 = `sprd,ump9620` 但 power-controller 兼容与 9620 矩阵 cell 一致 → 假说排除，转 §4。

### T2 确认 MFD 实际选了哪张矩阵
- 设备上：
  ```sh
  dmesg | grep -iE 'ump96|pmic|mfd|regmap'
  ls /sys/bus/platform/devices/ | grep -iE 'regulator|syscon|efuse'
  ls /sys/bus/spi/devices/                    # 确认 pmic SPI 设备（id 与 of_node）
  ```
- **判读**：出现 `ump9621-regulator` → 9621 矩阵被选（坐实）；只有 `ump962x-regulator` → 9620 矩阵。

### T3 对照 stock：自制链 booted DTB 是否与原厂一致
- 比较 T1 结果与 stock 预期（原厂 boot 的 pmic 兼容、power-controller/syscon 节点存在性与兼容）。
- **判读**：不一致 → 先修 DTB 一致性（这才是"按原厂设计"的前提）；一致 → 差异在别处（矩阵/驱动侧）。

---

## 2. 修复（假说坐实后，三选一）

| 修复 | 做法 | 评价 |
|---|---|---|
| **F1 对齐 DT（首选）** | 给 `power-controller@2000` 加实际矩阵的 regulator 兼容（如 `"sprd,ump9621-regulator"`），经 **DTBO overlay**（dtbo_a 8MB 可重排，勿动 vendor_boot DTB）→ mfd-core 链 of_node → ② 闭合 | 不动厂商源码；最贴近原厂机制 |
| F2 改矩阵 cell | `sprd-ump96xx-spi.c` 把所选矩阵的 regulator/syscon cell of_compatible 改为与 DT 一致（`"sprd,ump962x-regulator"`） | 厂商源码一行；需重编模块 + 世代一致 |
| F3 保证 9620 矩阵 | 若设备本就该匹配 9620（pmic@0 = sprd,ump9620），查为何运行时选了 9621（DTBO 覆盖？多 pmic 节点？）并修正 | 治本，但先要 T1/T2 定位 |

---

## 3. 验证方法（无串口，全部软件可判）

### V1 软件链（设备侧，STEP 探针 + sysfs）
```sh
# ① hsphy 绑定（② 闭合的直接证据）
ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/          # 出现 64300000.hsphy = bound
# ② REG33 状态（regulator 使能证据）
#    init 探针输出 / dmesg | grep -iE 'regulator|vddusb|T508N'
# ③ UDC 状态推进（主机可见性证据）——轮询直到稳定：
for i in $(seq 1 20); do cat /sys/class/udc/musb-hdrc.0.auto/state; sleep 1; done
#    期望序列: attached → powered → default → address → configured
# ④ dmesg 无错误：
dmesg | grep -iE 'unable to get syscon platform device|T508N: unable'
```

### V2 主机侧（PC）
- `lsusb`：出现 T508N（f_serial → CDC ACM，VID 0x1d6b / PID 0x0104，gadget 脚本设定）；
- Windows 设备管理器：`USB Serial Device (COMx)`；
- 若枚举成功：`echo hello > /dev/ttyACM0`（主机侧）→ 设备 `ttyGS0` 应收到（回环测试）。

### V3 判定矩阵
| 现象 | 判定 | 下一步 |
|---|---|---|
| hsphy bound + REG33 enabled + UDC 到 configured + 主机见 CDC ACM | ✅ **通** | USB 战役收官（attached→枚举全链达成） |
| 环闭合（② 过、regmap 有）但停在 attached | ⑥ 排除 | 转 ⑧ DPDM/bc1p2 终态（读 hsphy DPDM 开关）→ ⑨ 模块来源核查 |
| 假说排除（pmic=9620 且 cell of_node 正常仍 ② NULL） | 转向 | ③ regmap 所有权（需 MFD probe 内部 log —— 严格 T508N-only kmsg 读取器已布，或串口） |
| 红屏 | 放行面过大 | 收敛精准放行（hsphy+dwc3） |

---

## 4. 设备状态与回滚

- 当前：boot_a=**31d8f08e**（§25 最终）；回滚基线 **ed0ebdd6 / e75a61de**（§22/§23）；
  uboot=原厂 7fe2b5ea；vendor_boot=原厂 DTB；dtbo=按当前阶段（T1 需确认）。
- 回滚：`fastboot flash boot_a <基线.img>`（其余分区未动）；全部补丁/探针已 git 三重备份。

---

## 5. 关联材料

- 源码：`work/vendor-kernel-ums9620/drivers/mfd/sprd-ump96xx-spi.c`
  （9620 矩阵 regulator cell :42-43 / syscon cell :63-64；9621 矩阵 :122-133；regmap :257；变体表 :145-164；match :317-329）
- hsphy 三环：`drivers/usb/phy/phy-sprd-qogirn6pro.c:709-729`
- DTS：`work/stock_ums9620.dts`（pmic@0 ~2266；syscon@2000 ~2501；power-controller@2000 ~2510；LDO_VDDUSB33 2743）
- 历程：`ARCHIVE-20260823.md` §23/§24/§25；指南：`work/GUIDE-vddusb33-stock-design.md`
- 打包技能：`.dsh/skills/tool-operations.md` §t508n-flashing（刷机/回滚命令）
