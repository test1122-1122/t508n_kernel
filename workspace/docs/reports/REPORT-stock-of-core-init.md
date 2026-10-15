# REPORT：原厂 5.4 vs mainline 6.12 — of_core_init / __of_attach_node_sysfs 区域对比

> 任务：找出"6.12 卡在节点循环第一个节点（根节点）的 `__of_attach_node_sysfs`、原厂能过"的关键差异。
> 材料：`linux-6.12.103/drivers/of/`（6.12 源码 + built `base.o/kobj.o`）、`work/linux-5.4.210/`（v5.4.210 上游源码）、
> `work/stock_kallsyms.map` + `work/stock_Image_full.bin`（原厂反汇编）、`work/stock_ums9620.dts` + `work/vendor_boot.dtb`（实机 DTB）。
> 日期：2025-08（T508N bring-up 会话）

---

## 0. 结论摘要（TL;DR）

1. **`__of_attach_node_sysfs` 在 5.4 与 6.12 之间源码逐行相同**（两棵树都没有 `drivers/of/sysfs.c`，实现都在 `drivers/of/kobj.c:115`）。
   原厂二进制的 `__of_attach_node_sysfs` 反汇编与 6.12 built 版本逻辑也完全等价。
2. **`of_core_init` 的差异只有 4 处**（见 §1.2）：6.12 多了开头 `of_platform_register_reconfig_notifier()`；
   5.4 先在循环外 `of_populate_phandle_cache()`（全树 2 遍 walk + kcalloc + 填 cache），6.12 改为循环内联填 static cache；
   6.12 循环体多一句 phandle_cache 写；其余（of_mutex 持锁、`kset_create_and_add("devicetree",NULL,firmware_kobj)`、
   `for_each_of_allnodes → __of_attach_node_sysfs`、`proc_symlink`）完全一致。
3. **kobject/kernfs/sysfs 底层逐函数 diff：全部功能等价**（`kobject_add_internal`、`kobject_kset_join`、
   `kernfs_add_one`、`kernfs_new_node`、`sysfs_create_dir_ns`、`sysfs_create_bin_file` 5.4 == 6.12）。
   唯一结构性差异：kernfs 锁从 5.4 全局 `kernfs_mutex` 变成 6.12 每 root `kernfs_rwsem`——本阶段只有 CPU0 触碰，无影响。
4. **探针语义（用 built `base.o` 反汇编验证）**："计数 0 格"(x=44) 亮 ⟺ `probe_i` 到达 64 ⟺ **已成功 attach 64 个节点（含根节点 "/"）**。
   因此**根节点 attach 已经成功**；卡点在第 65 个节点（`for_each_of_allnodes` 第 64 号，0-based）的 `__of_attach_node_sysfs`。
   用实机 DTB `work/vendor_boot.dtb`（948 节点，即全量 stock DTS）推算，**该节点 = `i2c@2260000`（/soc/ap-ahb 下）**。
5. **关键差异结论**：of/ 代码本身不存在能解释"卡住"的 5.4→6.12 差异；`__of_attach_node_sysfs(i2c@2260000)` 的 12 个属性
   与相邻节点（62/63/65…）完全相同，故卡点不是"节点数据特异"，而是**迭代到 ~64 时的系统状态问题**
   （最可疑：早期内存/分配器状态损坏在重分配循环中确定性发作；其次：循环期间 IRQ/tick 路径；再次："慢"而非"死"——6.12 关了 softlockup）。
6. 修复建议见 §5：一组可判别的实验（细粒度探针 / maxcpus=1 / 关 IRQ / slub_debug / 开 softlockup / 还原 5.4 式 cache 预填充）。

---

## 1. 源码对比

### 1.1 说明：文件位置（重要勘误）

任务描述提到 `drivers/of/sysfs.c`，但 **5.4 与 6.12 两棵树都没有 `drivers/of/sysfs.c`**：
`__of_attach_node_sysfs` / `__of_detach_node_sysfs` / `__of_add_property_sysfs` / `safe_name` 全部在
**`drivers/of/kobj.c`**（5.4.210 与 6.12.103 都是 165 行，内容除 `struct kobj_type of_node_ktype` 加 `const` 外逐字节相同）。

### 1.2 `of_core_init`（5.4 base.c:230-251 vs 6.12 base.c:167-204）

| 步骤 | 原厂 stock 5.4（源码 + 反汇编验证） | mainline 6.12（源码 + built base.o 验证） |
|---|---|---|
| 0 | — | `of_platform_register_reconfig_notifier()`（**6.12 移入**；5.4 在 `platform_bus_init()` 末尾调用，drivers/base/platform.c:1305） |
| 1 | **`of_populate_phandle_cache()`**：`raw_spin_lock_irqsave(devtree_lock)` → 全树遍历数 phandle（跳过 0 与 0xdeadbeef）→ `roundup_pow_of_two` → `kcalloc(GFP_ATOMIC)` → 再遍历填 `phandle_cache[phandle & mask]`（源码层含 `of_node_get`）→ 解锁 | —（phandle_cache 是 **static 128 项数组**，无预填充） |
| 2 | `mutex_lock(&of_mutex)` | 同 |
| 3 | `of_kset = kset_create_and_add("devicetree", NULL, firmware_kobj)`；失败 → unlock + pr_err + return | 同 |
| 4 | `for_each_of_allnodes(np) __of_attach_node_sysfs(np);` | `for_each_of_allnodes(np) { __of_attach_node_sysfs(np); if (np->phandle && !phandle_cache[hash(np->phandle)]) phandle_cache[hash] = np; }`（多一句内联 cache 写） |
| 5 | `mutex_unlock(&of_mutex)` | 同 |
| 6 | `if (of_root) proc_symlink("device-tree", NULL, "/sys/firmware/devicetree/base");` | 同 |

**差异点小结**：
- **原厂没有跳过任何节点**：`for_each_of_allnodes` 从 `of_root` 开始 DFS 前序遍历全部节点（含根节点），
  原厂反汇编确认循环内**没有** phandle cache 写、没有节点过滤/跳过。
- 6.12 的差异是：notifier 注册提前、cache 预填充取消（改循环内联写 static 数组）、循环体多一句平凡 store。
  这三处没有一处含锁等待/循环/可挂死逻辑。
- 附带发现：stock 二进制里 `__of_attach_node_sysfs` **结尾没有 `of_node_get(np)`**（vendor fork 删掉了），6.12 保留。

### 1.3 `__of_attach_node_sysfs`（5.4 == 6.12，逐行）

```c
int __of_attach_node_sysfs(struct device_node *np)
{
	const char *name;
	struct kobject *parent;
	struct property *pp;
	int rc;

	if (!IS_ENABLED(CONFIG_SYSFS) || !of_kset)
		return 0;

	np->kobj.kset = of_kset;
	if (!np->parent) {
		/* 根节点 "/" 的处理（两版完全相同）：
		 * 名字用 "base"，parent = NULL → kobject_add 时经 kset 落到
		 * of_kset 的目录下 → /sys/firmware/devicetree/base */
		name = safe_name(&of_kset->kobj, "base");
		parent = NULL;
	} else {
		name = safe_name(&np->parent->kobj, kbasename(np->full_name));
		parent = &np->parent->kobj;
	}
	if (!name)
		return -ENOMEM;

	rc = kobject_add(&np->kobj, parent, "%s", name);
	kfree(name);
	if (rc)
		return rc;

	for_each_property_of_node(np, pp)      /* 每属性 1 个 sysfs bin 文件 */
		__of_add_property_sysfs(np, pp);

	of_node_get(np);                       /* 6.12 有；stock 二进制无（vendor 删） */
	return 0;
}
```

- 根节点：`safe_name(&of_kset->kobj, "base")` → `kobject_add(&np->kobj, NULL, "%s", "base")` →
  `kobject_add_internal` 里 `parent = kobject_get(&of_kset->kobj)` → 目录建在 of_kset 下。两版一致。
- 非根节点：`kbasename(full_name)`（strrchr '/'）+ `safe_name` 防重名（`name#1..#15` 重试，最多 16 次，有界）。
- 6.12 相对 5.4 的**新增/变化：无**（kernfs 交互相同；`safe_name` 相同；属性 bin 文件创建相同）。

### 1.4 kobject/kernfs/sysfs 底层（逐函数 diff 结果）

| 函数 | 5.4 vs 6.12 | 备注 |
|---|---|---|
| `kobject_add` → `kobject_add_internal`（lib/kobject.c） | **逐行相同** | name 检查 → `kobject_get(parent)` → `kobj_kset_join`（`spin_lock(kset->list_lock)`，短暂）→ `create_dir` |
| `sysfs_create_dir_ns`（fs/sysfs/dir.c） | 相同（6.12 多 `kobject_get_ownership` 一样有） | parent=NULL 时用 `sysfs_root_kn` |
| `kernfs_create_dir_ns` / `kernfs_new_node` / `kernfs_add_one`（fs/kernfs/dir.c） | **逐行相同** | 唯一差异：5.4 用全局 `kernfs_mutex`，6.12 用 per-root `kernfs_rwsem`（`down_write` + rb 插入）；本阶段单 CPU 触碰，无影响 |
| `sysfs_create_bin_file` → `sysfs_add_file_mode_ns` / `sysfs_add_bin_file_mode_ns`（fs/sysfs/file.c） | 等价 | 6.12 拆出 bin 专用函数，内容相同（`__kernfs_create_file`） |
| `kobject_kset_join` | **相同** | spinlock 短临界区 |

**死锁/长等待评估（6.12 语境：8 核在线、PREEMPT=y、of_mutex 持有）**：
- 链内所有 kernfs/sysfs 锁只有 CPU0 触碰；`of_mutex` 与 kernfs 锁之间无反向获取路径；
- 二级核此时 idle / kthreadd / kworker 睡眠，不碰 kernfs、不碰 of_mutex；
- → **代码层面不存在死锁**。卡住只能是：循环/重试出不来（状态损坏）、分配器问题、中断路径问题、或"慢"。

---

## 2. 原厂反汇编验证（stock_Image_full.bin，file_off = vaddr − 0xffffffc010080000）

### 2.1 `of_core_init` @ 0xffffffc0121a00a8

```
1a00a8  str x30,[x18],#8            ; SCS 栈（原厂开 SHADOW_CALL_STACK）
1a00b8  bl  0xffffffc010feacb8      ; = of_populate_phandle_cache()   ← 循环前预填充（5.4 式）
1a00c4  bl  0xffffffc0116a336c      ; = mutex_lock(of_mutex)
1a00dc  bl  0xffffffc011682f40      ; = kset_create_and_add("devicetree",NULL,firmware_kobj)
1a00e4  str x0, [x8,#3808]          ; of_kset = 结果
1a00e8  cbz → 失败路径
1a00f0  x19 = of_root               ; 循环起点 = 根节点
1a00fc  bl  0xffffffc010ff1e8c      ; = __of_attach_node_sysfs(np)
1a0100..0120  for_each_of_allnodes 走位（parent@0x60, child@0x68, sibling@0x70，
          含"walk up while parent && !sibling"回溯）→ DFS 前序，循环内无 cache 写
1a012c  bl  0xffffffc0116a2548      ; = mutex_unlock(of_mutex)
1a014c  bl  0xffffffc01061c354      ; = proc_symlink("device-tree",...)
```

**结论**：原厂 `of_core_init` == 5.4 源码（`of_populate_phandle_cache` 先行 → kset → 全节点 attach → proc_symlink），
**没有** `of_platform_register_reconfig_notifier`，**没有** 循环内 phandle cache 写。循环遍历**全部节点、无跳过**。

### 2.2 `of_populate_phandle_cache` @ 0xffffffc010feacb8

```
feacb8  entry（devtree_lock irqsave：mrs daif / msr daifset）
fead38  np = of_root；w9 = 0xdeadbeef (OF_PHANDLE_ILLEGAL)
fead50  遍历：x11=[np+0x68](child) / 回溯 [np+0x60](parent)/[np+0x70](sibling)
fead60  w11 = [np+8]  ← phandle（vendor struct：name@0x00, phandle@0x08, full_name@0x10）
        cmp 0xdeadbeef / cbz 0 → 跳过；否则 w8++ 计数
fead90  roundup_pow_of_two → w9 = mask-1 存 [x23,#3772]
feadc4  bl kcalloc（GFP_ATOMIC=0xb20）→ [x21,#3776] = phandle_cache
feade8.. 第二遍遍历：phandle_cache[phandle & mask] = np（纯 store，未见 of_node_get）
feae38  stlrb + msr daif,x20（解锁/恢复 IRQ）
```

**注意**：stock 二进制该函数**没有 `of_node_get`**（与 vanilla 5.4.210 源码不同）→ 说明原厂内核是**厂商 fork**，
`work/linux-5.4.210` 是 vanilla 参考。这不影响本报告的对比结论（对比基准以"原厂二进制行为"为准）。

### 2.3 `__of_attach_node_sysfs` @ 0xffffffc010ff1e8c

```
ff1ea4  x8 = of_kset；cbz → return 0
ff1eb4  [np+0x98] = of_kset          ; np->kobj.kset = of_kset（kobj@0x78，kset@+0x20）
ff1eb8  x9 = [np+0x60]（parent）；cbz → 根节点分支
ff1ebc  x20 = [np+0x10]（full_name）；strrchr('/') → basename
ff1edc  bl safe_name(&np->parent->kobj, basename)
ff1f38  根节点分支：safe_name(&of_kset->kobj(+0x18), "base")，parent = NULL
ff1f00  bl kobject_add（0xffffffc011683da4）
ff1f0c  bl kfree(name)；rc!=0 → return
ff1f14  属性循环：pp=[np+0x50]；bl __of_add_property_sysfs（0xffffffc010ff1b30）；pp=[pp+0x18]
ff1f30  return 0   ← 结尾无 of_node_get（vendor fork 删除）
```

**结论**：原厂 `__of_attach_node_sysfs` 与 5.4/6.12 源码逐条对应，**根节点与普通节点处理方式与 6.12 完全一致**。

### 2.4 6.12 built 对象验证（drivers/of/base.o / kobj.o）

- built `of_core_init`：进入即 paint(276,210) → `bl of_platform_register_reconfig_notifier` → `mutex_lock` →
  `kset_create_and_add` → paint(8,210) → 循环：计数 paint 逻辑 + `bl __of_attach_node_sysfs` +
  phandle 读 `[x19,#8]` + `mul w0,w0,#0x61c88647; lsr #25`（hash_32）→ cache 写 → 走位 → paint(296,210) → unlock。
  **探针语义实锤**：`add w24,w1,#1; tst x1,#0x3f; b.ne` → 当旧 `probe_i & 63 == 0` 时 `asr w0,w24,#6; sub #1`
  → cell0(x=44) 在旧 probe_i=64（第 65 个节点）时点亮。
- built `__of_attach_node_sysfs`（kobj.o:0x2dc）：与 stock 二进制逻辑相同（kobj@0x80、parent@0x68、properties@0x58、
  kset@+0x20；root → safe_name("base")；kobject_add；属性循环；**结尾 of_node_get**）。

---

## 3. 实机 DTB 与"卡住节点"身份

- `work/vendor_boot.dtb` = **948 节点**（160163 B，与 dtc 编译 stock_ums9620.dts 的 160099 B 几乎一致 → 实机跑的就是全量 stock DTS）。
  （`vendor_boot_linux.img` 内嵌的 5658 B DTB = 精简 57 节点 ot5，**不足以点亮 x=44**，说明实机刷的 DTB 是全量版。）
- 按 built 探针语义，卡点 = for_each 第 64 号（0-based）节点：**`/soc/ap-ahb/i2c@2260000`**（12 个属性：
  compatible/reg/interrupts/clock-frequency/#address-cells/#size-cells/status/reset-names/resets/clock-names/clocks/phandle=0x130）。
- 该节点与 62 号 `i2c@2240000`、63 号 `i2c@2250000`、65 号 `i2c@2270000` **结构完全相同**（62/63 已成功 attach）→
  **卡点与节点数据无关**，与"迭代到 64 附近的系统状态"有关。
- 若观测的"计数 0 格"实际来自更早/不同的探针版本（cell0 在节点 0 点亮），则按"第一个节点=根节点"理解：
  根节点 attach = `safe_name(&of_kset->kobj,"base")` + `kobject_add(&np->kobj,NULL,"base")`，
  该路径 5.4 与 6.12 **同样完全一致**（§1.3/§2.3），仍无代码差异可解释。

---

## 4. 关键差异（5.4→6.12 的真实剩余差异，均不在 attach 代码内）

| # | 差异 | 位置 | 能否解释"卡住" |
|---|---|---|---|
| D1 | phandle cache：5.4 循环外 `devtree_lock` 保护预填充（kcalloc 动态）vs 6.12 循环内联写 static 128 项数组（无锁） | of_core_init | 写是平凡 store，**直接排除**（父任务已排除） |
| D2 | `of_platform_register_reconfig_notifier` 提前到 of_core_init 开头（5.4 在 platform_bus_init 末尾） | 6.12 base.c:176 | 已执行成功（x=8 亮），排除 |
| D3 | `__of_attach_node_sysfs` 结尾 of_node_get：6.12 有 / stock 二进制无（vendor 删）→ 6.12 对每个 attach 节点做一次 kref refcount_inc | kobj.c | 前 64 节点同样执行且成功；单独看无害 |
| D4 | kernfs 锁：全局 mutex（5.4）→ per-root rwsem（6.12） | fs/kernfs | 本阶段单 CPU，排除 |
| D5 | **分配器**：6.12 SLUB `try_cmpxchg128`（128 位 freelist+counters ABA，当前 LSE 强制 LL/SC → `ldxp/stxp` 对）vs 5.4 `cmpxchg_double` | mm/slub.c | **最可疑**：循环是 driver_init 链最重分配阶段（~950 节点 × ~12 属性 × 2~3 次 kmalloc ≈ 3 万+ 次分配、数千 slab 页），若更早阶段（8 核 bringup / alternatives patch / 页表）在某次分配上确定性损坏了 freelist/计数器，卡点会**确定性地**出现在"某次分配"——探针改变分配量 → 卡点漂移（与历史"加探针能过"吻合） |
| D6 | IRQ/tick：8 核在线、PREEMPT=y，timer tick 每秒 1000 次打断 CPU0；6.12 使用**移植的 EL2 virtual timer（PPI 12）** | arch/arm64 | 次可疑：若某次 tick 中断路径 hang（中断期间访问坏内存/错误处理），表现为"paint 后不返回" |
| D7 | errata alternatives：当前已恢复 SM#2 全量 patch（repeat-TLBI / speculative-AT / 若干 errata） | arch/arm64/kernel/alternative.c | 低-中：patch 后指令序列在 T770 上的行为差异（A/B 可测） |
| D8 | **CONFIG_SOFTLOCKUP/HARDLOCKUP 关闭（6.12）**（原厂开） | .config | 诊断盲区：若循环实际是"超慢"而非"死"，表现完全相同且无打印 |
| D9 | config：NR_CPUS=512（原厂 32）、NUMA（已关）、DEVTMPFS 开（kdevtmpfsd 睡眠中，不干扰） | .config | 低 |

**综合判定**：of_core_init / __of_attach_node_sysfs 代码路径上**不存在 5.4→6.12 的功能差异**能解释"卡住"。
"原厂能过、6.12 卡"必须在**这段代码之外**找：最可能是 D5（分配器/内存状态，确定性发作在重分配循环）、
其次是 D6（循环期间中断路径）、D8（慢而非死）与 D7（errata 序列）。

---

## 5. 修复建议（按可判别性排序，一次一个变量）

1. **细粒度探针定位子步骤**（必做）：在 `__of_attach_node_sysfs` 内部分段打点：
   - 进入函数 / `kobject_add` 返回后 / 属性循环每 4 个属性 / `of_node_get` 后。
   - 判定卡在 `kobject_add`（→ kernfs/锁/分配）还是属性循环（→ 纯分配）还是收尾。
2. **确认实机 DTB**：在卡住节点处把 `kbasename(np->full_name)` 首个字符编码成颜色/坐标块
   （或 paint 不同颜色区分节点），确认实机 DTB == `work/vendor_boot.dtb`（948 节点）且卡点确为 `i2c@2260000`。
3. **`maxcpus=1`**：隔离 8 核/跨核因素。单核过 → 跨核路径（D5/D6 的并发面）；单核仍卡 → 纯分配/代码。
4. **临时关 IRQ 包住循环**：`local_irq_disable()` … 循环 … `local_irq_enable()`（先存 flags）。
   不再卡 → tick/中断路径（D6）是元凶；仍卡 → 与中断无关。
5. **分配器 A/B**：bootargs `slub_debug=U`（或 `init_on_alloc=1`、`slub_min_objects`、`slub_nomerge`）
   改变分配模式/暴露损坏。若卡点移动/消失 → 指向 D5，进一步用 `slub_debug=FZPU` 找损坏源。
6. **打开诊断**：`CONFIG_SOFTLOCKUP_DETECTOR=y`（6.12 当前关，原厂开）——若真"慢"，会打印栈。
7. **还原 5.4 式 phandle cache 预填充（代码改动，可直接做）**：把 6.12 `of_core_init` 循环内的
   `if (np->phandle && !phandle_cache[hash]) phandle_cache[hash] = np;` 移到 kset 创建之前
   （循环外先 `for_each_of_allnodes` 填 static cache），循环内只保留 `__of_attach_node_sysfs`——
   排除循环内新增操作（即便概率低，成本也低）。
8. **errata A/B**：临时把 SM#2 的 `__apply_alternatives(&kernel_alternatives,...)` 换回只应用
   `boot_cpucaps`（即跳过 speculative-AT / repeat-TLBI 等 system-scope patch）重测（D7）。
9. **终极兜底（若锁定分配器/原语）**：对照 5.4 SLUB `cmpxchg_double` 语义调整 6.12 slub（6.12 无 SLAB，
   可试 `slub_cpu_partial=0` 等参数缩小跨 CPU 面），或对照原厂在 driver_init 前的执行序做差异。
10. **通用**：所有改动记录在 `linux-6.12.103` git diff；维持"一次刷机一个变量"。

---

## 附 A：反汇编用符号（stock_kallsyms.map）

| 符号 | 地址 |
|---|---|
| of_core_init | ffffffc0121a00a8 |
| of_populate_phandle_cache | ffffffc010feacb8 |
| __of_free_phandle_cache_entry | ffffffc010feac80 |
| __of_attach_node_sysfs | ffffffc010ff1e8c |
| __of_add_property_sysfs | ffffffc010ff1b30 |
| safe_name | ffffffc010ff1c28 |
| kobject_add | ffffffc011683da4 |
| kobject_add_internal | ffffffc011682444 |
| kset_create_and_add | ffffffc011682f40 |
| sysfs_create_dir_ns | ffffffc010639e58 |
| proc_symlink | ffffffc01061c354 |
| mutex_lock / mutex_unlock | ffffffc0116a336c / ffffffc0116a2548 |

## 附 B：关键源码位置

- 6.12：`drivers/of/base.c:167`（of_core_init）、`drivers/of/kobj.c:115`（__of_attach_node_sysfs）、
  `drivers/of/platform.c:783`（of_platform_register_reconfig_notifier）、`lib/kobject.c`、`fs/sysfs/dir.c:40`、`fs/kernfs/dir.c`、`fs/sysfs/file.c:321`
- 5.4：`work/linux-5.4.210/drivers/of/base.c:230`、`drivers/of/kobj.c:115`、`drivers/base/platform.c:1305`（notifier 调用点）
- 实机 DTB：`work/vendor_boot.dtb`（948 节点）；精简版：`vendor_boot_linux.img` 内嵌 5658 B（57 节点）
- 探针：`drivers/of/base.c`（of_core_init 内，y=210 行）、`arch/arm64/kernel/t508n_dbg.c`
