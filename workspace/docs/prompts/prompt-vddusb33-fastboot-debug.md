# 提示：vddusb33 变体矩阵 —— fastboot 实机调试（T1 → F1/F3 → V1-V3）

> 给下一 session（**带 bash/fastboot 环境**，如 WSL + `/mnt/c/platform-tools/fastboot.exe`）。
> 离线核验已完成，**勿重做**（见 `work/REPORT-vddusb33-variant-matrix-offline.md`）。
> 本提示 = 实机执行命令 + 判定 + 刷机/回滚。全程无串口：屏显(tlog/syscall 441) + pstore +
> `/home/*.log`(eMMC userdata) 回读是唯一设备侧通道（t508n-flashing 技能已加载）。

---

## 0. 一句话现状与目标

- 症状：自制链 attached 但主机不枚举；REG33 disabled；hsphy NO-DRIVER（② NULL）。
- 本提示假说：booted DTB 的 `pmic@0` 匹配了 **9621 矩阵**，其 regulator cell
  `"sprd,ump9621-regulator"` ≠ DT 节点 `power-controller@2000` 的 `"sprd,ump962x-regulator"`
  → mfd-core 不链 of_node → ② NULL。
- **离线核验修正**：库存 DTB 是**三 PMIC 同总线**（pmic@0=9620 / pmic@1=9621 / pmic@2=9622），
  9621 矩阵被选是原厂常态；§25 platdev 证据（ump9621-regulator + ump962x-syscon +
  ump9620-efuse）与库存设计一致，**不能**作为"主 PMIC 错配"证据。
- **假说成立的唯一前提 = 机上 booted DTB 的 `pmic@0/compatible == "sprd,ump9621"`**。
  ARCHIVE §5 记录当前 vendor_boot_a = `work/vendor_boot_a_v3.img`（PD 移除版，**非原厂**）
  与 §25"vendor_boot=原厂"的记录矛盾 → **T1 必须先实测，别信档案**。

---

## 1. 环境速查

| 项 | 路径/命令 |
|---|---|
| fastboot（Windows 侧） | `/mnt/c/platform-tools/fastboot.exe`（WSL interop） |
| 进 fastboot | 关机 → 音量下+电源 |
| 设备序列号 | T6PA03CJ3EH02ZW（历史）；`fastboot devices` 确认 |
| 内核源码 | `work/vendor-kernel-ums9620/`（当前 boot_a=31d8f08e 的树） |
| 内核构建 | `export LOCALVERSION=` + `make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8`（env 见 ARCHIVE §3） |
| boot 打包 | `python3 tools/sprd_mkbootimg.py --kernel work/vendor54_Image --ramdisk initramfs.cpio --output boot_54vendor.img --key tools/ot5_boot_key.pem` |
| initramfs 源 | `initramfs_root/`（init.c 内置 + rc.t508 Alpine 侧脚本） |
| eMMC 分区 | boot_a=**p36** / vendor_boot_a=**p38** / dtbo_a=**p44** / super=p46(/) / userdata=p75(/home) |
| dtc / dtbo 打包 | `dtc`（PATH）；`tools/build_noop_dtbo.py`（模板）；**`tools/build_vddusb33_9621_dtbo.py`（F1 专用，已备）** |

### 关键镜像/基线（md5 供校验）

| 文件 | 说明 | md5 |
|---|---|---|
| `work/boot_54vendor.img` | 当前 boot_a 打包（§25 前） | 5a13973190249828d519896898122692（§7 快照；31d8f08e 为 §25 最终版，如有新包以实际为准） |
| `work/vendor_boot_a_v3.img` | **当前 vendor_boot_a（PD 移除版，可能仍在机上）** | 7e1313305f219ea29915dc67bcc214cf |
| `Image/vendor_boot_a.img` | **原厂 vendor_boot（恢复用）** | — |
| `work/noop_dtbo.img` | 空 overlay dtbo（dtbo_a 当前应为它） | — |
| `work/dtbo.img` / `dtbo0-6.dtb` | 原厂 dtbo_a 内容（7 条目，勿动 pmic） | — |
| 回滚基线 | boot_a：ed0ebdd6(§22) / e75a61de(§23) / 6db9c563(§24) / **31d8f08e(§25 最终)**；uboot=原厂 7fe2b5ea | — |

---

## 2. T0（10 分钟，只读确认当前状态）

```bash
FB=/mnt/c/platform-tools/fastboot.exe
$FB devices
$FB getvar current-slot          # 应为 a
$FB getvar version-bootloader    # 记录
# 不要急着刷——先确认机上 boot_a/vendor_boot_a 的"实际"内容：
# 方法：fastboot 不读分区；从自建 Linux 下 dd 读（见 T1 注入法顺手做），
# 或直接按 §3 注入 T1 探针后开机读屏/log 判定。
```

> ⚠️ 若机上仍是 v3 vendor_boot（PD 移除版），且 MUSB 路线需要 `power-domains=<0x44>` 完整
> （REPORT-SELF-BUILT-USB-CHAIN.md §1：**勿用 -usb-nopd 变体**），先做 §5 F3 恢复再测，
> 否则会引入第二个变量。

---

## 3. T1：机上 booted DTB 实读（决定性判据）

**注入法**（唯一可靠通道：屏显/pstore/`/home` log）。给 `initramfs_root/rc.t508`
末尾追加 T1 块（init 每次启动会把 rc.t508 从 initramfs 同步到 /mnt/etc 后执行）：

```sh
# ---- T1 vddusb33 块（追加到 initramfs_root/rc.t508 末尾）----
{
  echo "=== T1 $(date) ==="
  echo "pmic0:   $(cat /proc/device-tree/soc/aon/spi@400000/pmic@0/compatible 2>/dev/null | tr -d '\0')"
  echo "pmic1:   $(cat /proc/device-tree/soc/aon/spi@400000/pmic@1/compatible 2>/dev/null | tr -d '\0')"
  echo "pmic2:   $(cat /proc/device-tree/soc/aon/spi@400000/pmic@2/compatible 2>/dev/null | tr -d '\0')"
  echo "syscon:  $(cat /proc/device-tree/soc/aon/spi@400000/pmic@0/syscon@2000/compatible 2>/dev/null | tr -d '\0')"
  echo "pwrctrl: $(cat /proc/device-tree/soc/aon/spi@400000/pmic@0/power-controller@2000/compatible 2>/dev/null | tr -d '\0')"
  echo "--- platdev (ump96/regulator/syscon/efuse) ---"
  ls /sys/bus/platform/devices/ 2>/dev/null | grep -iE 'ump96|regulator|syscon|efuse'
  echo "--- spi devices ---"
  ls /sys/bus/spi/devices/ 2>/dev/null
  echo "--- hsphy driver ---"
  ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/ 2>/dev/null
  echo "--- dmesg pmic/mfd/regmap (tail) ---"
  dmesg 2>/dev/null | grep -iE 'ump96|pmic|mfd|regmap' | tail -30
  echo "=== T1 END ==="
} >> /home/t1.log 2>&1
cat /home/t1.log >> /dev/pmsg0 2>/dev/null   # pstore 通道，下轮开机 init 回读上屏
# -----------------------------------------------
```

然后：重打 boot.img → 刷 boot_a → 开机 → **等 150s 软重启或 @F 冻结** →
下一轮 init 的 pstore 回读会把它打到屏幕；或直接进自建 Linux 后 `cat /home/t1.log`（userdata 持久）。

**判读**：

| pmic0 compatible | syscon@2000 | 结论 | 走 |
|---|---|---|---|
| `sprd,ump9621` | （任意） | **假说坐实** | §6 F1 |
| `sprd,ump9620` | 有 `sprd,ump962x-syscon` | 库存 DTB，假说排除 | §7（MFD/regmap 侧） |
| `sprd,ump9620` | 无/不同 | booted DTB 非库存（被改过） | §5 F3 恢复后重测 |
| 无 pmic 子树 | — | vendor_boot 是精简 DTB | §5 F3 恢复后重测 |

---

## 4. T2 交叉印证（同一次开机可顺带）

```sh
# MFD 实际建的 cell（§25 已见过 ump9621-regulator + ump962x-syscon + ump9620-efuse）
ls /sys/bus/platform/devices/ | grep -iE 'ump96'
# 若同时存在 ump962x-regulator（9620 矩阵 cell）→ 9620 矩阵在跑 → 假说排除
# 若只有 ump9621-* 而 无 ump962x-* → 主 pmic 确为 9621 → 假说坐实
dmesg | grep -iE 'sprd-ump96xx|ump96|pmic mfd' | tail -20   # MFD probe 日志
```

---

## 5. F3：恢复库存 DTB 基线（vendor_boot_a + dtbo_a + boot_a）

```bash
FB=/mnt/c/platform-tools/fastboot.exe
$FB flash vendor_boot_a /home/test/t508n/Image/vendor_boot_a.img   # 原厂 DTB（power-domains 完整）
$FB flash dtbo_a /home/test/t508n/work/noop_dtbo.img               # 空 overlay（防 merge 破坏）
$FB flash boot_a /home/test/t508n/boot_54vendor.img                # 当前/目标内核（31d8f08e 版优先）
$FB reboot
```
> fastboot 直写 dtbo 曾报 "Too many links" → 备选：自建 Linux 下
> `dd if=/tmp/vddusb33-9621.img of=/dev/mmcblk0p44 bs=2048`（先确认 p44=dtbo_a，见 ARCHIVE §2）；
> 或 b 槽 Android root 下 `adb shell dd if=/sdcard/x.img of=/dev/block/by-name/dtbo_a`。

---

## 6. F1：DTBO overlay 补 9621 regulator 兼容（仅 T1 坐实后）

已备好：
- overlay 源码：`work/dtbo-overlay-vddusb33-9621.dts`（给 pmic@0/power-controller@2000
  追加 `"sprd,ump9621-regulator"`，保留原有兼容；已含 sc-id/board_id 匹配规则）
- 打包脚本：`tools/build_vddusb33_9621_dtbo.py`（dt_table 大端头，board_id 0x2700，仿 noop 模板）

```bash
# 1) 构建（机器需有 dtc）
python3 tools/build_vddusb33_9621_dtbo.py \
  --overlay work/dtbo-overlay-vddusb33-9621.dts \
  --out /tmp/vddusb33-9621.img
# 2) 刷 dtbo_a（先试 fastboot，失败用 dd/Android adb）
FB=/mnt/c/platform-tools/fastboot.exe
$FB flash dtbo_a /tmp/vddusb33-9621.img || echo "Too many links? 用 dd 路径"
# 或 Linux 下：dd if=/tmp/vddusb33-9621.img of=/dev/mmcblk0p44 bs=2048
# 3) 保持 vendor_boot_a = 原厂（§5 已恢复）；boot_a 不变（hsphy 已含 §23 修复：regulator 优先+双名 by-name）
$FB reboot
```

> 修复后预期：mfd-core 把 pmic@0/power-controller@2000 链到 `ump9621-regulator` cell →
> hsphy by-name 拿到 cell → `dev_get_regmap(parent=pmic@0 SPI 设备)` 有值 → REG33 可开。

---

## 7. V1-V3 验证（F1/F3 后统一判定）

```sh
# V1 软件链（注入 rc.t508 或手测；同一台开机跑）
ls /sys/bus/platform/drivers/phy-sprd-qogirn6pro/                 # 64300000.hsphy = bound?
cat /sys/class/udc/musb-hdrc.0.auto/state                          # 期望 attached→powered→default→address→configured
for i in $(seq 1 20); do cat /sys/class/udc/musb-hdrc.0.auto/state; sleep 1; done
dmesg | grep -iE 'unable to get syscon platform device|T508N:' | tail -20
# REG33：dmesg/regulator sysfs（/sys/class/regulator/*/name==vddusb33 → status/state）

# V2 主机侧（PC）
lsusb                                   # T508N f_serial → CDC ACM（VID 0x1d6b PID 0x0104）
echo hello > /dev/ttyACM0               # 回环：设备侧 ttyGS0 应收

# V3 判定矩阵
#   hsphy bound + REG33 enabled + UDC configured + 主机见 CDC ACM → ✅ 收官
#   环闭合但停 attached → ⑥ 排除 → ⑧ DPDM/bc1p2 → ⑨ 模块来源核查
#   假说排除（pmic0=9620 且 cell 正常仍 ② NULL）→ ③ MFD regmap 所有权（需 MFD probe 内部 log/串口）
#   红屏 → 放行面过大 → 收敛精准放行（hsphy+dwc3）
```

---

## 8. 回滚（任何一步失败）

```bash
FB=/mnt/c/platform-tools/fastboot.exe
# boot_a 回滚基线（按需选择）
$FB flash boot_a <ed0ebdd6 / e75a61de / 6db9c563 对应 img>
# vendor_boot_a / dtbo_a 回滚
$FB flash vendor_boot_a /home/test/t508n/Image/vendor_boot_a.img   # 原厂
$FB flash dtbo_a /home/test/t508n/work/noop_dtbo.img               # 空 overlay
$FB reboot
```
> 全部补丁/探针/镜像已有 git 三重备份（vendor 树 + work-bak-local + t508n-repo）；
> TEE(trustos/teecfg) 受保护，**不要**动。

---

## 9. 关联材料

- 离线核验报告：`work/REPORT-vddusb33-variant-matrix-offline.md`（源码行号/工件指纹/决策树）
- 上一 session 提示：`work/prompt-vddusb33-variant-matrix.md`
- 原厂机制指南：`work/GUIDE-vddusb33-stock-design.md`（§3.1-3.5 决策树）
- 历程：`work/ARCHIVE-20260823.md` §22-25（§25 遗留疑点：cell of_node 链接条件 / regmap 所有权 / MFD 时序）
- 源码：`work/vendor-kernel-ums9620/drivers/mfd/sprd-ump96xx-spi.c`、
  `.../drivers/usb/phy/phy-sprd-qogirn6pro.c:698-750`（§23 修复版）、`.../drivers/mfd/mfd-core.c:184-187`
- DTS：`work/stock_ums9620.dts`（pmic@0 ~2265 / syscon@2000 ~2500 / power-controller@2000 ~2512 / LDO_VDDUSB33 ~2742）
- 刷机技能：`.dsh/skills/tool-operations.md` §t508n-flashing
