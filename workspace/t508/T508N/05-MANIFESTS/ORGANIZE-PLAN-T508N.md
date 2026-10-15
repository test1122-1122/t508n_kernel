# T508N 项目文件夹整理方案（先计划，后执行）

> 阶段：阶段一（只读盘点）已完成；**阶段二（B2-精简归档）已于 2026-10-09 执行完成**：
> 归档根 `/home/test/t508n/t508/T508N/`，253 个文件 / 1.48 GiB，逐文件 SHA256 校验 253/253 通过。
> 生成时间：2026-10-09
> 依据：`INVENTORY-T508N.md`、`DUPLICATES-T508N.md`、`CONFLICTS-T508N.md`、`CHECKSUMS-T508N.sha256`

---

## 0. 红线复述（整个方案不得违反）

1. 不删除任何文件，不格式化，不刷机，不覆盖镜像。
2. 不移动关键 bin/img/zip/bat/exe；除非已复制并 SHA256 校验成功。
3. 保留脚本相对路径依赖：`unlock_emmc.bat`、`spd_dump` 工具目录、magic64 包、`savebrock` 目录的相对结构不得改变。
4. Windows 中文/空格路径小心处理；工具不得从 WSL UNC 路径运行（全部 Windows 侧脚本保持原地、在 Windows 本地执行）。
5. 去重/移动/重命名一律「先计划，后执行」，并保留 `_backup` / `_ARCHIVE` 副本。
6. 不读取、不复制、不外传私钥/敏感文件（`.pem` / key / 校准数据只登记）。
7. 大文件（>1 GiB）只登记、分类、计算哈希，不复制。

---

## 1. 现状（阶段一盘点结论）

```text
扫描根：E:\rootphone\t508n、E:\Download（T508N 相关）、C:\platform-tools、/home/test/t508n/t508、/home/test/t508n 顶层
登记文件 8061 个 / 126.3 GiB（其中折叠目录 21 个 34.09 GiB 未逐条展开）
已计算 SHA256 8029 个 / 92.14 GiB（0 失败）
内容完全相同组 1695 个，冗余副本 2995 个，理论可回收 58.02 GiB
同名不同内容/大小 284 组（其中值得注意的 60 组）
关键构件 1336 个
```

问题本质：**同一批构件在 Windows 与 WSL 两侧各存 4~6 份**，信息重复但用途不同，属于「灾备冗余」而不是「垃圾」。
因此默认方案是**原地保留 + manifest 索引**，不做物理去重。

---

## 2. 整理原则

1. **原地保留优先**：所有 `E:\` 与 `C:\platform-tools` 下的工具、脚本、镜像一律不动。理由：Windows 一键包靠相对路径调用 `spd_dump.exe` 与 `fdl*`，一旦移动会破坏「双击即可救砖」。
2. **索引而非搬家**：新建 `/home/test/t508n/t508/T508N/` 作为**目录索引与文档主副本**，用 `05-MANIFESTS/` 的 CSV/指针指向权威副本。
3. **复制先于归档**：如确需把某文件放进新结构，顺序固定为 复制 → SHA256 比对 → 写 MANIFEST → 原文件保持原位；仅当用户明确要求"腾空间"时才在原目录留 `_ARCHIVE-README.txt` 标记（仍不删除）。
4. **大文件不搬**：`>256 MiB` 默认只登记不复制；`>1 GiB` 一律不复制（用户明确要求除外）。去重后 >256 MiB 的独立大文件只有 11 个 / 23.09 GiB。
5. **敏感件隔离**：32 个敏感文件（`.pem`、key、校准类）只登记路径与大小，不读取内容、不哈希、不复制。

---

## 3. 建议目录结构

```text
/home/test/t508n/t508/T508N/            ← 新建，索引与文档主副本（WSL，剩余空间 725 GiB）
  00-DOCS/          文档索引（指向 t508/docs 的入口 + 本整理产物）
  01-TOOLS/
    unlock-v2/      v2 解锁工具        → E:\rootphone\t508n\2、解BL锁工具ums9620_tcl_t508n_v2\
    unlock-v3/      v3 解锁工具        → E:\Download\ums9620_tcl_t508n_v3 (1)\
    magic64/        magic64 一键包     → E:\Download\ums9620_tcl_t508n_magic64\
    spd-dump/       SPD/BROM 工具链    → C:\platform-tools\srag-v6\、srag-v3\、srag\
  02-FIRMWARE/
    stock/          stock 启动链镜像   → E:\rootphone\t508n\savebrock\flash_all_spd_dump\、C:\platform-tools\stock\
    device-backup/  真机原始备份       → E:\rootphone\t508n\2unlock\（唯一 A!=B 的真备份）
    mainline/       mainline 镜像      → t508/build/boot-images/、mainline-ref/t508n-mainline/out/
    openwrt/        OpenWrt 镜像       → C:\platform-tools\boot_54openwrt_*.img（当前 auto15）
  03-ROOTFS/        p75 rootfs         → C:\platform-tools\openwrt-p75-112m.img / 256m
  04-REPORTS/       整理报告           → t508/docs/reports/
  05-MANIFESTS/     INVENTORY/CHECKSUMS/DUPLICATES/CONFLICTS + MANIFEST.csv
  06-ARCHIVE/       历史变体归档（可选，只放复制件，不接管原件）
```

**关键取舍**：`01-TOOLS/` 与 `02-FIRMWARE/` 默认**只放 manifest 指针与 SHA256**，不复制 exe/bat（避免出现第二份会被误用的工具链）；需要离线携带时再执行第 5 节的复制批次。

---

## 4. 权威副本矩阵（关键构件）

| 构件 | **权威副本（唯一真源）** | 其他副本 | SHA256(前16) | 归属目录 |
|---|---|---|---|---|
| magic64 U-Boot 正式版 | `E:/Download/ums9620_tcl_t508n_magic64/uboot_t508n_magic64_v1.img` | C:/platform-tools ×2、~/t508/work/uboot | `d4961f76747369a2` | 01-TOOLS/magic64 |
| magic64 探针版 | `E:/Download/ums9620_tcl_t508n_magic64/uboot_t508n_magic64_probe.img` | C:/platform-tools ×2 | `fde6b307b86cc946` | 01-TOOLS/magic64 |
| 原厂 U-Boot（回滚用） | `E:/Download/ums9620_tcl_t508n_magic64/uboot_a_stock.img` | savebrock/Image/uboot_a.img（同哈希） | `8b5a6131946b7e91` | 01-TOOLS/magic64 |
| 槽位 A 镜像 | `E:/Download/ums9620_tcl_t508n_magic64/misc_slot_a.img` | 无 | `82bc63d7f0bb9907` | 01-TOOLS/magic64 |
| 槽位 B 镜像 | `E:/Download/ums9620_tcl_t508n_magic64/misc_slot_b.img` | 无 | `4948bee107cb8b3e` | 01-TOOLS/magic64 |
| 解锁用 misc | `.../magic64/misc-fastbootd.bin`（84 B） | 6 份完全相同 | `0a23c55ca21259d9` | 01-TOOLS/magic64 |
| stock misc（A 槽元数据） | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/misc.img` | C:/platform-tools/stock/misc.img | `7db67f82f318dc58` | 02-FIRMWARE/stock |
| stock miscdata | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/miscdata.img` | C:/platform-tools/stock ×2 | `288619bd309dab10` | 02-FIRMWARE/stock |
| SPD 工具（最新） | `C:/platform-tools/srag-v6/spd_dump.exe`（80896 B） | srag、t508n-uboot-magic64 | `2e4c117eb22ff800` | 01-TOOLS/spd-dump |
| SPD 工具（v3，实机验证） | `E:/Download/ums9620_tcl_t508n_v3 (1)/spd_dump.exe`（40448 B） | magic64、srag-v3 | `5c75c4ea64d61226` | 01-TOOLS/unlock-v3 |
| SPD 工具（v2，老版） | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/spd_dump.exe`（36352 B） | 2unlock | `2d2864b2a557c74a` | 01-TOOLS/unlock-v2（**标注：第一条命令缺 exec_addr，勿用于 BROM exploit**） |
| v3 解锁脚本 | `E:/Download/ums9620_tcl_t508n_v3 (1)/unlock_emmc.bat` | srag-v3、~/t508/work/srag/t508n-v3 | `41d1f9f905a16b6a` | 01-TOOLS/unlock-v3 |
| v2 解锁脚本 | `E:/rootphone/t508n/2、解BL锁工具ums9620_tcl_t508n_v2/unlock_emmc.bat` | 2unlock | `27a631cd2f232b1b` | 01-TOOLS/unlock-v2 |
| 真机原始 boot A | `E:/rootphone/t508n/2unlock/boota.img` | 无（其他 boota 都不同） | `5901a6e5e54248d2` | 02-FIRMWARE/device-backup |
| 真机原始 boot B | `E:/rootphone/t508n/2unlock/bootb.img` | 无 | `75d2347389ff2d82` | 02-FIRMWARE/device-backup |
| 真机原始 SPL | `E:/rootphone/t508n/2unlock/spl.bin` | 无（v3/magic64 的 spl.bin 是另一份） | `354dadd5f23bcf4d` | 02-FIRMWARE/device-backup |
| 真机原始 uboot A/B | `E:/rootphone/t508n/2unlock/uboot{a,b}.bin` | v3、magic64/backup 各一套 | `403a9a89cf418674` / `f7fe95924f49c7e1` | 02-FIRMWARE/device-backup |
| stock boot_a/b | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/boot_a.img` | 20 份完全相同（含 A=B 复制品） | `f252428edfe0c512` | 02-FIRMWARE/stock |
| stock super | `E:/rootphone/t508n/savebrock/flash_all_spd_dump/super.img` | 6 份完全相同（含 firmware/extracted-super/vendor_dlkm_b.img） | `8738931e4f1df065` | 02-FIRMWARE/stock（大文件，只登记） |
| mainline test5 | `/home/test/t508n/t508/build/boot-images/boot_mainline_6.18.55_t508n_test5.img` | C:/platform-tools | `0c6011ead098ea14` | 02-FIRMWARE/mainline |
| OpenWrt 当前 A 槽 | `C:/platform-tools/boot_54openwrt_hotspot_luci_rndis_auto15_hotspot_net.img` | t508/build/boot-images（同哈希） | `73a61f5cc410dc20` | 02-FIRMWARE/openwrt |
| p75 rootfs 112M | `C:/platform-tools/openwrt-p75-112m.img` | 无 | `c84c556072c70058` | 03-ROOTFS |
| p75 rootfs 256M | `C:/platform-tools/openwrt-p75-256m.img` | 无 | `c8d075e9b18f6d70` | 03-ROOTFS |
| 整机备份压缩包 | `E:/rootphone/t508n/savebrock/2a备份(苦力怕制作).zip` | savebrocka 同哈希 | `b92a80ff8063792b` | 02-FIRMWARE/stock（大文件，只登记） |

---

## 5. 阶段二执行批次（**待确认后执行**）

| 批次 | 内容 | 复制量 | 是否必需 |
|---|---|---:|---|
| B0 | 创建 `T508N/` 全部子目录（`mkdir -p`） | 0 | 是 |
| B1 | 工具链清单与校验和（manifest 指针，不复制 exe/bat） | 0 | 是 |
| B2 | 复制「关键且 ≤256 MiB」去重后文件到对应目录 | 553 个 / 7.89 GiB（含文档） | 可选 |
| B3 | 复制当前运行集（magic64、misc_slot、test5、auto15、p75） | ≈ 0.5 GiB | 建议 |
| B4 | 生成 `05-MANIFESTS/MANIFEST.csv` 并全量校验 | 0 | 是 |
| B5 | 折叠目录/历史变体归档到 `06-ARCHIVE/`（复制，不删原件） | 视用户选择（34.09 GiB） | 否 |

复制动作的标准姿势（每个文件都走同一段逻辑）：

```sh
# 1) 复制（不覆盖任何已存在文件）
test -e "$DST" && { echo "SKIP exists: $DST"; exit 1; }
cp -p -- "$SRC" "$DST"
# 2) 校验
a=$(sha256sum "$SRC" | cut -d' ' -f1); b=$(sha256sum "$DST" | cut -d' ' -f1)
[ "$a" = "$b" ] || { echo "HASH MISMATCH $SRC"; exit 2; }
# 3) 记录到 MANIFEST.csv：src,dst,sha256,size,mtime,verdict
```

**Windows 侧不动**：`E:\rootphone\t508n`、`E:\Download\ums9620_tcl_t508n_magic64`、`C:\platform-tools` 的目录结构与文件名保持与盘点时完全一致。

---

## 6. 校验与回滚

- 校验：`sha256sum -c --quiet /home/test/t508n/t508/docs/CHECKSUMS-T508N.sha256`（8029 条，0 失败）。
- 每个复制件额外做一次源/目标哈希比对，比对失败的**不写入 MANIFEST**并保留原件。
- 回滚：阶段二只新增 `/home/test/t508n/t508/T508N/`，不修改任何原始文件；`rm -rf /home/test/t508n/t508/T508N` 即可完全还原（Windows 侧零改动）。

---

## 7. 需要用户确认的问题

1. 是否执行 B2（553 个文件 / 7.89 GiB 复制到 `T508N/`）？还是仅要 manifest 索引（B1+B4，0 复制）？
2. 归档根目录是否定在 `/home/test/t508n/t508/T508N/`？（WSL 剩余 725 GiB）
3. 是否需要把 OpenWrt 200+ 个 `boot_54openwrt_*` 历史变体移入 `06-ARCHIVE/`（**复制**，原件不动）？
4. 是否要更新 `E:\Download\ums9620_tcl_t508n_magic64\README-中文.txt`（Windows 侧写入，当前未改）？
5. 大文件（`super.img` 等 11 个 / 23.09 GiB）确认只登记不复制？

---

## 8. 附录

### 8.1 折叠目录展开命令（需要逐文件清单时）

```sh
# Windows 侧
find "/mnt/e/rootphone/t508n/savebrocka/out/decompiled" -type f -printf '%s\t%p\n' > /tmp/decompiled.txt
# WSL 侧
find /home/test/t508n/t508/src -maxdepth 2 -type d | while read d; do echo "$d: $(find "$d" -type f | wc -l)"; done
```

### 8.2 重新生成盘点产物

```sh
cd /home/test/t508n/t508/work/folder-audit
./scan_find.sh          # 只读元数据（≈70 s）
python3 inventory.py    # 规范化 + 分类 -> entries.json
python3 hash.py         # SHA256（增量，可重复执行）
python3 gen_inventory.py && python3 gen_inventory_md.py && python3 gen_analysis_docs.py
```

### 8.3 Windows 侧注意事项

- `spd_dump.exe` 必须在 Windows 本地目录运行，**不要**从 `\\wsl.localhost\...` UNC 路径调用。
- 含中文/空格的路径（如 `2、解BL锁工具ums9620_tcl_t508n_v2`）在各 .bat 中以 `cd /d "%~dp0"` 相对定位，保持目录名不变即可继续工作。
- 写入 `misc` 会改变 A/B 槽选择：先跑 `01-一键解锁并写magic64.bat`，之后再用 `06/07` 写 `misc_slot_*.img`。
