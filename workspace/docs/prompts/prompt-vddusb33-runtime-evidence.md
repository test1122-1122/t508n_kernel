# 任务：vddusb33 regmap 环 —— 运行时证据定位与修复（H1/H2/H3）

> 给另一 session。症状：自制链 attached 但主机不枚举；REG33 disabled；hsphy NO-DRIVER
> （② `of_find_device_by_node` = NULL）。dtbo/dtb 均已确认为原厂，**情况不变**。
> 静态面已全部排除（见 §1）——环不闭的答案只能在**运行时状态**里，用 §2 的 5 条证据定位。

---

## 0. 一句话

静态证据全部指向"代码与 DTB 都该能工作"，但环不闭 → 断点 = 运行时 of_node 链接没发生。
三个候选机制：**H1** pmic SPI 设备 of_node=NULL（mfd 父节点无 of_node → 全部 cell 不链）；
**H2** 运行时选了 9621 变体矩阵 → cell 兼容匹配不上 DT 节点；**H3** 链内 hsphy 是 §23 补丁版
（regulator 优先搜索），可能把环从能通的 syscon 路径切到了断的 regulator 路径。

---

## 1. 已静态排除（勿重做）

| 项 | 证据 |
|---|---|
| DTB（dtbo+dtb） | 原厂（用户确认）→ merged DTB 应 = stock |
| mfd-core.c of_node 链接 | **vendor 与 upstream diff 为空**（drivers/mfd/mfd-core.c）——逻辑完全相同 |
| MFD cell 表 | **T508N stock .ko 与链内 F50 版字符串一致**（均含 sprd,ump962x-regulator / ump962x-syscon / ump9621-*） |
| 模块血统 | 链内 MFD/ADI/regulator = F50 重编（hash 10831ca3a788…），但 cell 内容等价于 stock |

关键代码（mfd-core.c:184-187，链接条件）：
```c
if (parent->of_node && cell->of_compatible) {
    for_each_child_of_node(parent->of_node, np) {
        if (of_device_is_compatible(np, cell->of_compatible)) { pdev->dev.of_node = np; break; }
    }
}
```
→ cell 存在但 of_node 未链，只可能是：parent->of_node 为 NULL（H1），或 cell of_compatible
匹配不上任何直子节点（H2），或 hsphy 找错了节点（H3）。

---

## 2. 运行时验证 5 条（设备上，全部一行，替代盲测）

```sh
# ① booted DTB 的 pmic 变体（决定 MFD 选哪张矩阵）
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/compatible
#   期望/参考: "sprd,ump9620"（base DTS line 2266）；若为 "sprd,ump9621" → 9621 矩阵被选（H2 成立）

# ② booted DTB 的 regulator/syscon 节点兼容（是否与所选矩阵的 cell 匹配）
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/power-controller@2000/compatible
cat /proc/device-tree/soc/aon/spi@400000/pmic@0/syscon@2000/compatible
#   参考: base = "sprd,ump962x-regulator" / "sprd,ump962x-syscon"（stock_ums9620.dts 2510/2501）
#   若 DTBO0 覆盖成 "sprd,ump9621-regulator" → 与 9621 矩阵匹配（stock 即如此工作）

# ③ pmic SPI 设备有没有 of_node（H1 直接判据）
ls -la /sys/bus/spi/devices/*/of_node

# ④ cell 平台设备谁的 of_node 有、谁没有（H2 直接判据）
ls -la /sys/bus/platform/devices/*ump96*/of_node

# ⑤ 链内 hsphy 的搜索顺序（H3 判据）
strings <initramfs 内 phy-sprd-qogirn6pro.ko 路径> | grep -E 'ump962x-(regulator|syscon)'
#   链内应为补丁版: regulator 在前（§23 修复）；stock 原版未知，需与 vendor_ramdisk 对照
```

---

## 3. 分流与修复

| 观察 | 结论 | 动作 |
|---|---|---|
| ③ 无 of_node（pmic SPI 设备） | **H1 坐实** | 查链内 spi-sprd-adi（F50 重编，hash 88e9b2e2…）注册 pmic SPI 设备是否走 `of_register_spi_devices`（of_node 是否设置）；与 stock .ko（hash 56a14e0b…）行为对照；必要时换回 stock .ko（vermagic 对齐） |
| ④ regulator cell 无 of_node 但 syscon cell 有 | **H2/H3** | ① 若 ② 显示 regulator 兼容 ≠ 所选矩阵 cell 的 of_compatible → **F1 对齐 DT**（DTBO 给 power-controller@2000 加实际矩阵兼容）；② **H3：把 hsphy ① 改回 syscon 优先**（stock 原版顺序，phy-sprd-qogirn6pro.c:709-711 两行），syscon 路径若能链则直接闭合 |
| ④ 全无 of_node | H1 | 同上 |
| ④ 全有 of_node 但仍 ② NULL | H3 | 核对 hsphy ① 找到的节点 与 ② 目标是否同一（搜索顺序/节点选择差异） |

> 优先级建议：**先跑 ①②③④⑤ → H3 一行试改（syscon 优先）最便宜 → H1 查 ADI 注册路径 → H2 对齐 DT**。
> 全程不改强制角色/域唤醒逻辑（防 §20 回归）。

---

## 4. 验证判据（改完）

```sh
# ① hsphy 绑定（② 闭合的直接证据）
ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/          # 出现 64300000.hsphy
# ② REG33 状态（rail 上电证据）
dmesg | grep -iE 'regulator|vddusb|T508N'                  # REG33: enabled
# ③ UDC 状态推进（主机可见性证据）
for i in $(seq 1 20); do cat /sys/class/udc/musb-hdrc.0.auto/state; sleep 1; done
#    期望: attached → powered → default → address → configured
# ④ 主机侧
lsusb                                                      # 出现 T508N（CDC ACM, VID 0x1d6b/PID 0x0104）
```

| 现象 | 判定 |
|---|---|
| hsphy bound + REG33 enabled + UDC 到 configured + 主机见 CDC ACM | ✅ 通（收官） |
| 环闭合仍停 attached | ⑥ 排除 → ⑧ DPDM/bc1p2 终态 |
| ①②③④⑤ 全部正常但 ② 仍 NULL | 需 MFD probe 内部 log（严格 kmsg 读取器已布 / 串口） |

---

## 5. 设备状态与回滚

- 当前：boot_a=31d8f08e（§25）；回滚基线 ed0ebdd6 / e75a61de（§22/§23）；
  uboot=原厂 7fe2b5ea；vendor_boot=原厂 DTB；dtbo=原厂（本轮确认）；
- 回滚：`fastboot flash boot_a <基线.img>`；补丁/探针已 git 三重备份；
- 链内模块目录：`initramfs_root/lib/modules/5.4.210-android12-9-ga1d5c173d86b-ab1/`
  （MFD=F50 重编 hash 10831ca3a788；stock 对照 = `work/vendor_ramdisk_out/lib/modules/`，hash 1340e41660f7）。

---

## 6. 关联材料

- 链接条件：`drivers/mfd/mfd-core.c:184-187`（vendor 与 upstream 一致，已核实）
- hsphy 三环：`drivers/usb/phy/phy-sprd-qogirn6pro.c:709-729`（① 搜索顺序 :709-711 是 H3 改动点）
- MFD：`drivers/mfd/sprd-ump96xx-spi.c`（9620 矩阵 cell :42-43/:63-64；9621 矩阵 :122-133；match :317-329）
- DTS：`work/stock_ums9620.dts`（pmic@0 ~2266；syscon@2000 ~2501；power-controller@2000 ~2510）
- 历程：`ARCHIVE-20260823.md` §23/§24/§25；指南：`work/GUIDE-vddusb33-stock-design.md`
- 刷机：`.dsh/skills/tool-operations.md` §t508n-flashing（fastboot/spd_dump 命令）
