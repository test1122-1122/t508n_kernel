# T508N(UMS9620) 根文件系统 / initramfs / init 投递机制对比报告

> 目标：弄清**原厂 T508N** 与 **ZTE F50 内核树** 是如何把 rootfs 与 init 交给内核的，与我们
> 当前把**内置 initramfs**（busybox shell `/init`）编进内核的做法有何差异，并据此解释
> "内核已跑通到 `run_init_process("/init")`，但用户态 `/init` 不画屏"的现象。
> 纯离线分析，未改动任何文件/未构建。

---

## 0. 结论先行（TL;DR）

1. **原厂 T508N 与 ZTE F50 是同一套"外部 initrd"投递机制**：
   `CONFIG_BLK_DEV_INITRD=y` + `CONFIG_INITRAMFS_SOURCE=""`（无内置 rootfs）。
   内核**不内置**任何 rootfs，rootfs 完全依赖 bootloader（紫光定制 LK = sprldl）把
   `boot.img`/`vendor_boot.img` 里的 ramdisk 作为**外部 initrd** `initrd_start/initrd_end`
   传给内核，由 `init/initramfs.c:populate_rootfs()` 解包。这是二者**标准正确做法**。

2. **我们不同**：`CONFIG_INITRAMFS_SOURCE=/home/test/t508n/initramfs_root`（把 busybox rootfs
   编译进 vmlinux，`CONFIG_INITRAMFS_FORCE` 理论上游离 + `CONFIG_CMDLINE_EXTEND`）。
   内核自带 rootfs，不再依赖 bootloader 的 initrd。**注意：当前 `work/vendor-kernel-ums9620/.config`
   里 `CONFIG_INITRAMFS_FORCE` 与 `CONFIG_CMDLINE_EXTEND` 均已丢失**（见 §4.1），这是与配方
   `T508N-VENDOR-BUILD-RECIPE.md` 不一致的地方，风险极高。

3. **原厂/F50 为何到 `run_init_process("/init")` 能成功而外部容器在"我们"这边失败**：
   内核 `lib/decompress.c:decompress_method()` 只按**前 2 字节**选解压器；紫光 LZ4 容器
   `magic 0x184C2102` 的**小端前两字节 = 0x02 0x21**，恰好命中内核 `compressed_formats[]`
   里 `lz4` 的 `{0x02,0x21}` 条目 → 内核把它当 LZ4 解压。但 `lib/decompress_unlz4.c`
   期望的是 **LZ4 legacy 分块帧**（每块前有 4B 长度、magic 前无 size 字段、且是多块），
   而紫光容器是 **magic + size_field + 单个 LZ4 原始块**。二者不匹配 → `unlz4` 失败，
   `populate_rootfs` 走 `clean_rootfs()` + `populate_initrd_image()`，**会清空内置 rootfs**。
   原厂能启动，只能说明**紫光 LK 在传给内核前自行把容器解压（或按不同帧组合）成标准 cpio**；
   这正是"我们直接复用容器镜像"失败、改走"内置 + FORCE"才能成功的根因。

4. **"内核探针 7 段全通过 / `/init` 不画屏"的判定**：根投递已经成功（内置 initramfs 已展开、
   `/init` 存在且可执行、`run_init_process` 已到达）。**不画屏是用户态问题**，主要候选：
   - `CONFIG_DEVTMPFS` 在三个 config 里都**关闭**（`# CONFIG_DEVTMPFS is not set`）→
     `/dev` 为空，`/dev/mem`、`/dev/console`、`/dev/kmsg` 都未被内核自动创建；
     而 `/init` 画屏走 `devmem`（需 `/dev/mem`），`mount -t devtmpfs ... /dev` 因内核没
     DEVTMPFS 而必失败，退到 `mdev -s`，但 `mdev -s` 不会生成 `/dev/mem` → `devmem` 静默失败。
   - 内核侧探针（`t508n_dbg`）是**内核内直接写物理帧缓冲**，不需要 `/dev/mem`，所以能亮 7 格；
     用户态 `/init` 用 `devmem` 却缺 `/dev/mem` —— 这正好解释"内核探针亮、用户态不亮"。
   - `/init` 第一段先做 `mount proc/sys/devtmpfs/tmpfs` 再做 `W 64`；`mount -t devtmpfs`
     失败后 `mdev -s` 扫描 sysfs，时长/顺序改变，或 `mount -t devpts devpts /dev/pts`
     因缺 `CONFIG_DEVPTS` 失败，都可能拖慢/改变到达 `W 64` 的路径。
   - 帧缓冲几何参数（见 §5.3）在脚本里可能是**错误的 stride/分辨率**，把 8x8 块写到
     屏外/不可见区域。

---

## 1. 材料与证据来源（全文引用均以本工作区为准）

| 编号 | 材料 | 路径 |
|---|---|---|
| A | F50 内核树（ZTE 开源 UMS9620 5.4.210 fork） | `work/vendor-kernel-ums9620/` |
| B | T508N 原厂 config（IKCONFIG 提取） | `work/kernel.config` |
| C | 我们构建产物 .config | `work/vendor-kernel-ums9620/.config` |
| D | 5.4-custom（pristine 参照 / 旧版能跑） | `work/linux-5.4-custom/.config` |
| E | 内置 initramfs（busybox rootfs，/init 为 shell 脚本） | `/home/test/t508n/initramfs_root/` |
| F | 原厂 vendor ramdisk 提取 | `work/vendor_ramdisk_out/` |
| G | 原厂 boot ramdisk 提取（magiskinit 首段 init） | `work/boot_ramdisk_out2/` |
| H | 启动流程逆向报告 | `REPORT-T508N-启动流程分析.md` |
| I | 真厂内核构建配方 | `work/T508N-VENDOR-BUILD-RECIPE.md` |

> 内核启动链：**BootROM → SPL(splloader) → [Trusty TOS + SML] → LK(sprdlk) → 内核 → init**。
> LK 在 `uboot_a/b`（内容为紫光定制 LK），Android 侧走标准 AVB；`init_boot/dtb` 分区为全零。
> 见 H §2/§3。

---

## 2. 问题 1 —— f50_defconfig 与原厂 kernel.config 的 rootfs 相关差异

### 2.1 关键项逐条对比（grep 三份 config）

| 配置项 | **f50_defconfig** (A) | **原厂 kernel.config** (B) | **我们 .config** (C) | 5.4-custom (D) |
|---|---|---|---|---|
| `CONFIG_BLK_DEV_INITRD` | `=y` (183) | `=y` (183) | `=y` (181) | `=y` (169) |
| `CONFIG_INITRAMFS_SOURCE` | `=""` (184) | `=""` (184) | `="/home/test/t508n/initramfs_root"` (182) | 同左 (170) |
| `CONFIG_INITRAMFS_ROOT_UID/GID` | N/A | N/A | `=0/=0` (183/184) | `=0/=0` (171/172) |
| `CONFIG_INITRAMFS_FORCE` | **未设** | **未设** | **未设（丢失，见 §4.1）** | 未设 |
| `CONFIG_CMDLINE_EXTEND` | **未设** | **未设** | **未设（丢失，见 §4.1）** | 未设 |
| `CONFIG_INITRAMFS_COMPRESSION` | 无 | 无 | `=".gz"` (191) | `=".gz"` (179) |
| `CONFIG_RD_GZIP` | `=y` (185) | `=y` (185) | `=y` (185) | `=y` |
| `CONFIG_RD_LZ4` | `=y` (190) | `=y` (190) | `=y` (190) | `=y` |
| `CONFIG_RD_*`(bzip2/lzma/xz/lzo) | 全部 `is not set` | 同 | 同 | 同 |
| `CONFIG_DEVTMPFS` | **`# is not set`** (1603) | **`# is not set`** (1593) | **`# is not set`** (1589) | 未设 |
| `CONFIG_BLK_DEV_RAM` | `=y` (1684) | `=y` (1674) | `=y` (1670) | `=y` |
| `CONFIG_BLK_DEV_RAM_COUNT/SIZE` | `16/8192` (1685/1686) | `16/8192` (1675/1676) | `16/8192` (1671/1672) | 同 |
| `CONFIG_TMPFS` | `=y` (5762) | `=y` (5572) | `=y` (5702) | `=y` |
| `CONFIG_CMDLINE` | `=""` (462) | `=""` (461) | `=""` (463) | `=""` |

**结论 ①：f50_defconfig 与原厂 kernel.config 的 rootfs 相关配置完全一致**，且都：
- 用 `BLK_DEV_INITRD=y` + **空** `INITRAMFS_SOURCE` → **外部 initrd，无内置 rootfs**；
- 开启 `RD_LZ4`（+`RD_GZIP`）用于解外部 initrd，**没有** INITRAMFS_FORCE；
- 关闭 `DEVTMPFS`；
- 启用 `BLK_DEV_RAM`(16×8192K) 与 `TMPFS`。

> 即 **F50 与原厂 T508N 在"把 rootfs 交给内核"这件事上用的是同一条标准 Linux 外部 initrd 路径**，
> 差别只在分发内容（T508N 是 Android 首段 init+magi skinit；F50 是 ZTE 路由器/非完整 Android 用户态）。

**结论 ②：我们（C/D）与它们的唯一根级差异就是 `CONFIG_INITRAMFS_SOURCE` 非空** —— 我们
vmlinux **内置** rootfs。这一差异直接决定了后续所有行为。

> 佐证：两份 f50_defconfig（`kernel-ums9620-tuned` 与 `vendor-kernel-ums9620`）的 rootfs 关键项
> `diff` 分文不差 → F50 的正确做法是稳定、可复现的。

---

## 3. 问题 2 —— F50 的实际 boot 流程（从内核树线索推断）

### 3.1 内核侧"无紫光特判"——init 代码是完全的上游 5.4.210

在 vendor 内核树 `init/*.c`（main.c / do_mounts.c / do_mounts_rd.c / do_mounts_initrd.c /
initramfs.c）全文搜索 `androidboot|midware|vendor|sprd|unisoc|qogirn|first_stage|ramdisk`，
**除 arm64 的 `t508n_*` 调试探针外没有任何紫光/展锐/Kernel 层定制**（`init/` 目录结果为
标准 upstream 内容：`ramdisk_execute_command`、`rd_load_image`、"RAMDISK: ..." 等皆原生）。
→ **紫光没有在 Linux init 层加任何"容器/ramdisk"特判**，投递完全走标准机制，靠的是
**bootloader(LK) 先处理好，再喂标准外部 initrd 给内核**。

### 3.2 内核解容器的路径（LZ4 magic 0x184C2102 被谁消费）

`unpack_to_rootfs()`（`init/initramfs.c:443`）对每个非 `'0'` 开头的数据调用
`decompress_method(buf,len,&name)`（`lib/decompress.c:58`）选解压器。magic 表（
`lib/decompress.c:47-56`）**只用前 2 字节**匹配：

```c
static const struct compress_format compressed_formats[] __initconst = {
    { {0x1f,0x8b}, "gzip",  gunzip },
    ...
    { {0x02,0x21}, "lz4",   unlz4 },
    { {0,0},       NULL,    NULL }
};
```

紫光容器 `magic 0x184C2102` 的小端字节序为 `02 21 4C 18`，**前 2 字节 `02 21` 恰与
`lz4` 条目吻合** → `unpack_to_rootfs` 会把该容器交给 `unlz4`。

接着 `lib/decompress_unlz4.c:unlz4()` 按 **LZ4 legacy 分块帧**处理：

```c
#define ARCHIVE_MAGICNUMBER 0x184C2102        /* unlz4.c:29 */
chunksize = get_unaligned_le32(inp);          /* 读第一个 u32 */
if (chunksize == ARCHIVE_MAGICNUMBER) { inp+=4; size-=4; }  /* 跳过 magic */
...
for (;;) {
    chunksize = get_unaligned_le32(inp);      /* 当作"块长度" */
    if (chunksize == ARCHIVE_MAGICNUMBER) { ...continue; }   /* 块头仍是 magic */
    if (chunksize == 0) goto exit_3;          /* 0 结尾 */
    ...
    inp += 4; ...
    ret = LZ4_decompress_safe(inp, outp, chunksize, dest_len);  /* 解一次"一块" */
}
```

但紫光容器（`tools/sprd_ramdisk.py` 忠实还原的 stock 格式）是：

```
[0x600 零填充] + magic 0x184C2102 + size_field(u32) + 单个 LZ4 原始块 + [4B 零尾]
  size_field = 0x604 + len(lz4)
```

即：**magic 之后是一个 "size_field"（总长度/未压缩长度），随后是"单个、连续的 LZ4 原始块"，
没有逐块 4B 长度前缀的多块帧结构**。验证：用 `sprd_ramdisk.py:lz4_block_decode` 对
`boot_54vendor.img` 容器 `buf[8:]`（跳过 magic+size_field）解码，得 1,205,760 字节、
起始 `b'070701'`（newc cpio）—— 证明该段确实是**一个合法的 LZ4 规范块**，但其**帧无关
size_field 语义**。

而 `unlz4` 把 `size_field` 当作第一块长度、再按 legacy 逐块长度帧解读 → **帧不匹配，解出
错误或报错**（`error("Decoding failed")` / `error("data corrupted")`）。因此：

> **内核 `decompress_method` 因前两字节 `0x02 0x21` 误认此容器为 `lz4`，但 `unlz4` 的 legacy
> 分块帧与紫光容器不符 → 外部 initrd 解包失败。** 这正是配方 I 里所写的
> "外部 boot-ramdisk 是紫光 LZ4 容器(magic 0x184C2102)，unpack_to_rootfs 只认 cpio 必败"。

### 3.3 populate_rootfs 的正确分支 vs 我们误触发的分支

`init/initramfs.c:646` `populate_rootfs()`：

```c
649:  char *err = unpack_to_rootfs(__initramfs_start, __initramfs_size);   /* 先解内置 */
650:  if (err) panic(...);                                                  /* 内置解包失败即 panic */
654:  { ...t508n (80=initrd_start 非零)... }
655:  if (!initrd_start || IS_ENABLED(CONFIG_INITRAMFS_FORCE)) goto done;     /* ★关键分支 */
658:  if (IS_ENABLED(CONFIG_BLK_DEV_RAM)) ...
663:  err = unpack_to_rootfs((char *)initrd_start, initrd_end - initrd_start); /* 解外部 initrd */
664:  if (err) {
665:      clean_rootfs();               /* ★★ 会删除刚展开的内置 rootfs！ */
666:      populate_initrd_image(err);   /* 把原始 initrd 写成 /initrd.image */
667:  }
```

- 原厂/F50（`INITRAMFS_SOURCE=""`）→ 内置为空，`initrd_start` 由 LK 给出 → 走 663 行解外部
  标准 initrd → 得到 boot ramdisk 里的首段 init。**这是标准正确路径。**
- **我们**（内置 initramfs）→ 若**没设 `INITRAMFS_FORCE`** 且 boot 镜像带外部 initrd
  （我们的 `boot_54vendor.img` ramdisk_size=`0xd782e` > 0，LK 必然传 initrd_start），则
  660-667 会尝试解外部容器 → `unlz4` 失败 → `clean_rootfs()` **把内置 rootfs 全删** → `/init`
  没了。而当前 `.config` **没有 FORCE**（见 §4.1），**这正是本次最危险的差别**。

> 结论：**F50/原厂用"外部 initrd"，把解容器的工作交给 bootloader；我们改"内置 initramfs"，
> 本应靠 `INITRAMFS_FORCE` 忽略外部容器，但当前 config 丢了 FORCE，导致"内置又被外部容器
> 解包失败连累清空"的隐患。**

---

## 4. 问题 3 —— 原厂 T508N 的 init 机制（首段 init 在 boot ramdisk，vendor ramdisk 由谁挂载）

### 4.1 我们 config 的 FORCE/CMDLINE_EXTEND 现状（§2 表格已在 C 列标注）

`grep -nE "CMDLINE_EXTEND|CMDLINE_FORCE|INITRAMFS_FORCE" work/vendor-kernel-ums9620/.config`
（与 `.config.old`）**都无结果** → 我们当前 .config 丢失了配方 I §2 步骤 3 要求的
`CONFIG_INITRAMFS_FORCE=y` 与 `CONFIG_CMDLINE_EXTEND=y`。因为 `olddefconfig` 会因依赖链丢弃
`INITRAMFS_FORCE`（配方 I §5.2 已警告）。**这是当前构建的实质差异点，需在报告中用于解释
"外部容器连累内置 rootfs"的风险。**

### 4.2 首段 init：boot ramdisk 里的静态 ELF（magiskinit）

`work/boot_ramdisk_out2/init`：
```
ELF 64-bit LSB executable, ARM aarch64, statically linked, stripped   (685,568 b)
```
并伴有 `.backup/init`（原始 init 备份）与 `overlay.d/sbin/{magisk64,magisk32,stub}.xz`（详见
H §6/§9/§10）→ **`/init` = Magisk 修补后的 magiskinit，是静态链接的 aarch64 ELF**，作为
**first-stage init** 放在 `boot.img` 的 ramdisk 里。原厂 `init_boot/dtb` 分区全零，不参与
（H §6，§9 表："init_boot 分区存在但全零；init 放 boot ramdisk"）。

### 4.3 vendor ramdisk 由首段 init 挂载（first_stage_mount + erofs + AVB）

vendor ramdisk 位于 `vendor_boot.img`（同样紫光 LZ4 容器，H §6），解包为
`work/vendor_ramdisk_out/`（584 文件）。其首段 init 读取 `first_stage_ramdisk/fstab.OT5`：

```
# vendor_ramdisk_out/first_stage_ramdisk/fstab.OT5
system     /system      erofs ro wait,avb=vbmeta_system,logical,first_stage_mount,avb_keys=...,slotselect
system_ext /system_ext  erofs ro wait,avb=vbmeta_system,logical,first_stage_mount,slotselect
vendor     /vendor      erofs ro wait,avb=vbmeta_vendor,logical,first_stage_mount,slotselect
odm        /odm         erofs ro wait,avb=vbmeta_odm,logical,first_stage_mount,slotselect
product    /product     erofs ro wait,avb=vbmeta_product,logical,first_stage_mount,slotselect
vendor_dlkm /vendor_dlkm erofs ro wait,avb=vbmeta_system_ext,logical,first_stage_mount,slotselect
/dev/block/by-name/metadata /metadata f2fs ... first_stage_mount,check
```

- `vendor_ramdisk_out/init` 是一个 **16 字节重定向文件**，内容 `/system/bin/init`
  （`xxd`：`2f73 7973 7465 6d2f 6269 6e2f 696e 6974` = `/system/bin/init`）→
  **second-stage init 由 system 分区的 `/system/bin/init` 担任**。
- 首段 init（magiskinit）做 AVB(first_stage_mount)+`logical` 挂载，`fsck/metadata` 工具在
  `first_stage_ramdisk/system/bin/{e2fsck,fsck.f2fs,snapuserd}`。

### 4.4 平台 init 差异（F50 路由器 vs FIH T508N）

F50 是 ZTE 5G CPE 路由器，其内核与 T508N 同为 UMS9620 fork、同样 `BLK_DEV_INITRD` 外部投递；
但 F50 的用户态**很可能不是完整 Android init**（无 `init_boot` 概念、无 `fstab.OT5` 的线性
挂载表），而是更精简的 init。内核树（A）已带 `android/`、`build.config.gki_unisoc.aarch64` 等
GKI 骨架，说明**内核本身是 Android/GKI 底座**，而 rootfs 内容（是否 Android init、rc 脚本）
由**外部 initrd** 决定。原厂 T508N 的 `vendor/etc/init/*.rc`（init.rc / init.sprd.*.service）
与 `first_stage_ramdisk/fstab.OT5` 才真正暴露平台 init 生态（H §8）。

> 关键：**原厂 rc/fstab 都在"外部 initrd + erofs 逻辑分区"之上**，而我们的内置 initramfs
> 完全没有这些分区挂载逻辑 + 只依赖 `/init` shell。**这不是"缺某个 rc 文件"，而是两套完全不同的
> 用户态模型。**

---

## 5. 问题 4 —— 我们做法的差异点归纳（CONTRAST）

### 5.1 对比总表

| 维度 | **原厂 T508N / F50（正确做法）** | **我们（当前做法）** |
|---|---|---|
| rootfs 投递 | **外部 initrd**：`BLK_DEV_INITRD=y`、`INITRAMFS_SOURCE=""` | **内置 initramfs**：`INITRAMFS_SOURCE=/home/test/t508n/initramfs_root` 编进 vmlinux |
| rootfs 载体 | bootloader(LK) 把 `boot.img` ramdisk 作为 `initrd_start/end` 传给内核；vendor ramdisk 在 `vendor_boot.img` | vmlinux 内嵌 cpio（`CONFIG_INITRAMFS_COMPRESSION=".gz"`），`__initramfs_start/__initramfs_size` |
| 解压责任 | 主要靠 **LK 预处理**（容器→标准 cpio），内核只 `unpack_to_rootfs` | 内核直接 `unpack_to_rootfs`（内置 cpio，newc） |
| 首段 init | `boot.img` ramdisk `/init` = **magiskinit（静态 aarch64 ELF）** | 内置 initramfs `/init` = **busybox shell 脚本** |
| second-stage | vendor ramdisk `/init`→`/system/bin/init`，首段挂 vendor/erofs/AVB | 无分区挂载，/init 直接画屏+insmod eMMC |
| DEVTMPFS | **关闭**（需用户态 `mdev`/手动建节点） | **关闭**（同左，但我们的 `/init` 依赖 `/dev/mem`） |
| INITRAMFS_FORCE | 未设（用外部 initrd，正常） | **配方要求设 y（忽略外部容器）；当前 .config 已丢** |
| `CMDLINE_EXTEND` | 未设 | **配方要求设 y；当前 .config 已丢** |
| 内核 init 特判 | 无（上游 5.4.210） | 无（仅 `t508n_*` 调试探针 + `t508n_dbg.c`） |

### 5.2 `/init` 不画屏的可能原因（按证据强度排序）

1. **根投递本身成功但不是"画屏阻塞点"**：探针确认 `ksys_access("/init",0)==0`（6 格不亮）
   与 `run_init_process("/init")`（7 格亮）都成立 → 内置 initramfs 已展开、`/init` 可在、
   内核已在拉起用户态 init。**不是"rootfs 没交给内核"的问题**。

2. **`/dev/mem` 缺失导致 `devmem` 静默失败（最可能）**：三份 config `CONFIG_DEVTMPFS`
   全部 `# is not set`。内核不会自动给 `/dev` 建节点；`/init` 第一段 `mount -t devtmpfs devtmpfs /dev`
   因内核无 DEVTMPFS 必然失败 → 回退 `mdev -s`，但 `mdev -s`（从 sysfs）不生成 `/dev/mem`、
   `/dev/console`、`/dev/kmsg` → `devmem` 打不开 `/dev/mem`，画屏函数 `W/R/G` 全部失效。
   **佐证对比**：内核侧 `t508n_dbg` 探针直接写物理帧缓冲（不需要 `/dev/mem`），所以"内核探针亮、
   用户态 `/init` 不亮"。

3. **`/init` 第一段在画屏前先做一串 mount**：`mount proc/sysfs`（需 procfs/sysfs，通常 OK）、
   `mount -t devtmpfs ... /dev`（必败）、`mdev -s`（扫 sysfs，耗时/可能 long）、
   `mount -t tmpfs ... /tmp`（需 TMPFS，OK）、`mount -t devpts devpts /dev/pts`（需 `CONFIG_DEVPTS`；
   若未配置则失败）。任一"卡/慢/需要缺失设备节点"都会拖住 `W 64`。

4. **帧缓冲几何/stride 疑似错误**：`/init` 用 `BASE=$((0xb0000000 + 30*1280))`，以 **1280 字节/行**
   作为 stride 处偏移；但平台 LCD 为 `480x320`（H §5 `st7365p, 480x320`）或配方 I 写 `320x480`
   BGRA。若真实 fb 宽度/format 与 `1280`（≈320px×4B）或 `480`（≈480px×4B）不符，8x8 块会被写到
   屏外/不可见区。**旧版 init 曾出白色线、改版后消失**，与几何参数（stride/坐标）被改动高度相关。

5. **`insmod`/模块依赖**：`/lib/modules/*.ko`（mmc_swcq/mmc_hsq/sdhci-sprd）位于**内置 initramfs**，但
   `/lib/modules` 不在 `depmod` 由内核 `modules.builtin/modules.order` 管理；脚本手动 `insmod`，
   若无符号/依赖报错不影响画屏（W/R 会画红/白），故**不是**"不画屏"主因。

6. **是否需要 `initramfs` 里缺的路径/目录**（原厂/F50 有而我们缺的）：
   原厂 boot ramdisk 有 `system/etc/ramdisk/build.prop`、`overlay.d/`、`.backup/init`；
   vendor ramdisk 有 `first_stage_ramdisk/`、`odm/`、`vendor_dlkm/`、`ueventd.OT5.rc`、`lib/modules` 等。
   但**这些是"外部 initrd + Android 分区模型"的配套**；我们的内置 initramfs 只要
   `/init` + `/bin/busybox` + 模块即可工作。**我们缺的不是某个原厂目录，而是"外部 initrd 这套
   自包含 rootfs 模型"**。此外请注意：原厂 `fstab.OT5` 里**没有** `devtmpfs`/动态根节点逻辑，
   而是 `first_stage_mount` 静态 + `mdev`/ueventd 建节点 —— 我们的 `/init` 需要同等补齐
   `/dev` 节点创建策略。

### 5.3 建议的排查/修复方向（不改文件，仅结论）
- **首要**：确认当前 `.config` 确实缺 `CONFIG_INITRAMFS_FORCE`（与配方 I 矛盾）；若要在带
  boot-image 外部 initrd 的情形下继续用内置 initramfs，**必须设 `INITRAMFS_FORCE=y`**
  （并在设 `CMDLINE_EXTEND=y` 之后再设 FORCE，最后**不要再跑 olddefconfig**）。
- **次要**：`/init` 画屏前先验证 `/sys/class/graphics/fb0` 与可用的帧缓冲分辨率/stride，
  直接写 `devmem`，并确保 `/dev/mem` 存在（或在 init 内 `mknod /dev/mem c 1 1`、
  `/dev/console c 5 1`、`/dev/kmsg c 1 11`）。
- **替代方案**：若想真正复刻原厂/F50 的做法，应改用**外部 initrd** 并把 rootfs 打成
  **标准 cpio（newc）/ gzip** 喂给 LK，而不是"内置 + 依赖 FORCE 忽略容器"。

---

## 6. 问题 5 —— 结论

### 原厂/F50 的标准做法（一句话）
> **根文件系统与 init 完全由 bootloader(LK) 以"外部 initrd"方式交给内核**：内核 `CONFIG_BLK_DEV_INITRD=y`、
> `CONFIG_INITRAMFS_SOURCE=""`（零内置 rootfs），LK 把 `boot.img`(＋`vendor_boot.img`) 里的 ramdisk
> 处理/解压成标准 cpio 后经 `initrd_start/initrd_end` 传入，`init/initramfs.c:populate_rootfs()` 解包并执行
> `ramdisk_execute_command`；T508N 其首段 `/init` 是 boot ramdisk 里的静态 ELF magiskinit，再由它挂
> vendor ramdisk + erofs 逻辑分区 + AVB，切到 `/system/bin/init`。**紫光不在内核 init 层做任何特判。**

### 我们应改什么
1. **立即**：确认并补回 `CONFIG_INITRAMFS_FORCE=y`（+ `CONFIG_CMDLINE_EXTEND=y`），
   否则当前带外部 initrd 的 `boot_54vendor.img` 会让 `populate_rootfs` 在 660-667 行因
   外部紫光容器解包失败而 `clean_rootfs()` 清空内置 rootfs（`/init` 消失）。
2. **若坚持内置 initramfs**：`/init` 需补齐 `/dev` 节点（`/dev/mem`、`/dev/console`、`/dev/kmsg`），
   修正帧缓冲 stride/分辨率，而非依赖"内核没开 DEVTMPFS 时的 `mdev -s`"。
3. **若想贴合原厂/F50 标准**：改用外部 initrd，把 rootfs 打成标准 cpio（gzip/newc）交给 LK，
   去掉 `INITRAMFS_SOURCE`/`FORCE`，回到 `BLK_DEV_INITRD` 外部投递这条标准路径。

---

## 7. 关键证据行号速查

| 证据 | 文件 | 行 |
|---|---|---|
| F50/原厂 外部 initrd：`INITRAMFS_SOURCE=""` + `BLK_DEV_INITRD=y` | `arch/arm64/configs/f50_defconfig` / `kernel.config` | 183/184 |
| 我们内置 initramfs | `vendor-kernel-ums9620/.config` | 182 |
| DEVTMPFS 关闭（三份 config） | f50:1603 / kernel.config:1593 / .config:1589 | — |
| `populate_rootfs` FORCE 分支（`!initrd_start || FORCE → done`，否则解外部→clean_rootfs） | `init/initramfs.c` | 655-667 |
| 解压器 magic 表（`{0x02,0x21}="lz4"`） | `lib/decompress.c` | 54 |
| LZ4 legacy magic 0x184C2102 + 逐块帧 | `lib/decompress_unlz4.c` | 29-192 |
| 纯上游 init 代码、无紫光特判 | `init/*.c` 全目录 | 全文 |
| 内核实现 `ramdisk_execute_command="/init"` + `ksys_access` 失败→prepare_namespace | `init/main.c` | 1486-1494 |
| 内核 `ksys_open("/dev/console")`（DEVTMPFS 关时可能失败） | `init/main.c` | 1476 |
| 首段 init = 静态 aarch64 ELF(magiskinit) | `boot_ramdisk_out2/init` | `file` 输出 |
| vendor ramdisk fstab 首段挂载 erofs+AVB | `vendor_ramdisk_out/first_stage_ramdisk/fstab.OT5` | 全文 |
| `vendor_ramdisk_out/init`→`/system/bin/init` | `vendor_ramdisk_out/init` | 16B 文件 |
| 紫光容器格式（magic+size_field+单 LZ4 块） | `tools/sprd_ramdisk.py` | 全文 |
| 我们的 boot 镜像确实带外部 ramdisk（`ramdisk_size=0xd782e`） | `boot_54vendor.img` header | offset 12 |
| 配方要求 FORCE+CMDLINE_EXTEND 但当前 .config 丢失 | `work/T508N-VENDOR-BUILD-RECIPE.md` §2 / §5.2 | 全文 |
