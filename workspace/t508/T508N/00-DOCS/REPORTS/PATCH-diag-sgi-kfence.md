# PATCH-diag-sgi-kfence — T508N 诊断方案：①Unisoc 死循环门移植（SGI 丢失诊断）②KFENCE 内存损坏诊断

> 对应未验证项 4（C1 死循环门）+ 未验证项 5（KFENCE）。
> 目标内核：mainline 6.12.103（`/home/test/t508n/linux-6.12.103`，当前探针构建树，已含 `t508n_dbg` framebuffer 探针）。
> 环境：无 UART/USB console，仅 320×480 32bpp framebuffer（0xb0000000）+ pstore/ramoops + A/B 槽安全网。
> 本文档 = 设计 + 补丁 + config + 判定方法。补丁均针对 6.12.103 原文件逐行核对。

---

## 0. 摘要（TL;DR）

1. **死循环门不能单独"确认 SGI 丢失"**——它是接收侧可控自旋（模拟"收到 IPI 但核表现为死"），不是丢包检测。
   确认"发了没到"必须**发送侧计数 vs 接收侧计数对照**。因此本方案分两层：
   - **层 1 计数层**：每个核收到 RESCHEDULE/CALL_FUNC 的次数 + 全局/按目标核的发送计数 →
     未消化 delta（`sent_to[cpu] − rx[cpu]`）持续 >0 即"发了没到/到了没处理"的直接证据；
   - **层 2 门控层**：原厂门等价移植（boot 参数 / proc 可 arm），用于 **A/B 复现对照**——
     把某个核"可控地弄死"，看竞态症状是否 1:1 复现；同时门本身是丢包试验（armed 后若该核
     从未自旋且系统仍卡 → 支持 SGI 丢失假说）。
   - 观测载体：每核一行 fb 计数（rx 低 8 位 / delta / 门状态 / 心跳），**核死（心跳冻结）与
     核活但收不到 IPI（心跳亮、rx 冻结、delta>0）可区分**。
2. **KFENCE 在 6.12 原生可用**（`CONFIG_KFENCE=y`，arm64 已 `select HAVE_ARCH_KFENCE`；
   `kfence_init()` 在 init/main.c:998，早于 `driver_init` → 覆盖 memory_dev_init 竞态窗口；
   默认 `sample_interval=100` 与 stock 回移版一致）。**KFENCE_STATIC_KEYS 必须保持 n**
   （避免 enable/disable 时 IPI 广播翻转——本平台 IPI 敏感）。
3. **无 console 获取 KFENCE 报告的通道**：
   - **主通道（替代判定）**：pstore console —— DTS 已有 `ramoops@fff80000`（console-size 0x8000），
     补 `CONFIG_PSTORE_RAM/CONFIG_PSTORE_CONSOLE`（6.12 的 ramoops 驱动就在 fs/pstore/ram.c 内，
     无需额外 RAMOOPS config）→ KFENCE 的 `pr_err` 报告经 pstore console 落盘，重启后回官方
     系统从 `/sys/fs/pstore/console-ramoops-0` 拉取完整报告（含栈回溯）。
   - **实时通道（fb 挂钩）**：在 `kfence_handle_page_fault`（mm/kfence/core.c:1223）**先于任何
     printk** 画屏（type/读写/地址/命中数）——报告打印本身可能在 printk-unfriendly 上下文
     死锁（report.c 注释明示此风险），先画屏保证最小可见性；`kfence_report_error`（report.c）
     再细化 type。
4. 交付物：本文件内 P1–P3（SGI/IPI 诊断）+ K1–K3（KFENCE）+ 判定矩阵 + 实施顺序。

---

## 1. 设计一：Unisoc 死循环门移植 + SGI 丢失诊断

### 1.1 原厂门语义与移植评估

stock 5.4 二进制（ipi_handler case 0，0x116d48）：
```
if (ipi_cpu(0x123e91ec) == cpu && state(0x1279e7d8) == 1) while(1);
else scheduler_ipi();
```
属 Unisoc sprd-ipi-thread/hang-detect(wdh) 设施：当某核被判"挂起/需要离线"时置
`ipi_cpu=cpu; state=1`，此后发给该核的 RESCHEDULE IPI 使其原地自旋（park）。
默认 state=0，正常路径零开销。

**评估（关键结论）**：
- 门的作用是"把接收侧核弄死"，**不是检测丢包**。若 T770 真的丢 SGI，armed 核收不到 IPI
  根本不会进 handler，门也不会触发——所以"门触发与否"本身就是一次丢包试验。
- 要确认"竞态时某核是否没收到本应到达的 IPI"，证据链必须是：
  **发送计数（全局/按目标核） vs 接收计数（按核）的差值 delta**。delta 持续 >0 且该核
  心跳正常 → 发了没到（SGI 丢失/延迟）；delta==0 且心跳冻结 → 到了没处理（接收侧死/自旋）。
- 门用于 A/B 对照：把门 arm 在疑似丢失的核上，若竞态以完全相同症状复现 → "该核不可用"
  足以产生该症状（必要条件检验）；若 armed 后症状不出现 → 该核可用性不是根因，指向
  SGI 丢失或发送侧问题。

### 1.2 分层设计

| 层 | 机制 | 回答的问题 |
|---|---|---|
| 计数层 | 每核 rx 计数（RESCHEDULE+CALL_FUNC）；发送侧按目标核计数；未消化 delta | "IPI 发了没到 / 到了没处理" |
| 心跳层 | 每核 timer softirq 内节流画屏（250ms 翻转一块） | "该核还活着吗"（与 rx 冻结组合可区分 死 vs 收不到） |
| 门控层 | 原厂门等价移植（boot 参数 / proc 写 arm） | A/B 复现对照 + 丢包试验 |
| 扫描层（可选） | boot CPU 每秒扫全核 delta，持续 >0 的画 y=340 行标记 | 实时"疑似丢失"标记 |

### 1.3 补丁 P1：arch/arm64/kernel/smp.c（计数 + 门 + 发送钩子）

**P1a** — 数据与 boot 参数（插在 `static bool crash_stop;` 之后，约 line 90）：

```c
/* ===== T508N: SGI/IPI 丢失诊断 + Unisoc 死循环门等价移植 ===== */
static int t508n_gate_cpu __read_mostly = -1;	/* 被 park 的核; -1 = 未 arm */
static int t508n_gate_state __read_mostly;	/* 0=off 1=armed (stock state.31123) */

static int __init t508n_ipi_gate_cpu_setup(char *str)
{
	int cpu;

	if (kstrtoint(str, 0, &cpu) == 0 && cpu >= 0 && cpu < NR_CPUS) {
		t508n_gate_cpu = cpu;
		t508n_gate_state = 1;
		pr_alert("t508n: IPI 死循环门 armed: cpu%d state=1\n", cpu);
	}
	return 1;
}
early_param("t508n_ipi_gate_cpu", t508n_ipi_gate_cpu_setup);

bool t508n_ipi_gate_armed(unsigned int cpu)
{
	return READ_ONCE(t508n_gate_state) == 1 &&
	       READ_ONCE(t508n_gate_cpu) == (int)cpu;
}

static void t508n_ipi_gate_spin(unsigned int cpu)
{
	pr_alert("t508n: IPI gate HIT on CPU%u -- park (stock-style)\n", cpu);
	t508n_gate_hit_paint(cpu);	/* fb 大标记: 门已触发 (先画后自旋) */
	while (1)
		cpu_relax();
}
```

**P1b** — `do_handle_IPI` 两个 case 挂钩（smp.c:960-966）：

```c
	case IPI_RESCHEDULE:
		t508n_ipi_rx(IPI_RESCHEDULE);
		if (t508n_ipi_gate_armed(cpu))
			t508n_ipi_gate_spin(cpu);	/* 原厂死循环门: 进入即自旋 */
		t508n_ipi_row_paint(cpu);		/* 节流在函数内 */
		scheduler_ipi();
		break;

	case IPI_CALL_FUNC:
		t508n_ipi_rx(IPI_CALL_FUNC);
		t508n_ipi_row_paint(cpu);
		generic_smp_call_function_interrupt();
		break;
```

**P1c** — `smp_cross_call` 发送钩子（smp.c:1017-1021，覆盖 RESCHEDULE/CALL_FUNC 全部发送源）：
注意 `arch_smp_send_reschedule`(1108)、`arch_send_call_function_*`(854/859) 都汇聚到这里。

```c
static void smp_cross_call(const struct cpumask *target, unsigned int ipinr)
{
	trace_ipi_raise(target, ipi_types[ipinr]);
	if (ipinr == IPI_RESCHEDULE || ipinr == IPI_CALL_FUNC)
		t508n_ipi_sent(ipinr, target);
	__ipi_send_mask(ipi_desc[ipinr], target);
}
```

**P1d** — proc 运行时控制（smp.c 尾部；默认不 arm，A/B 时 echo "N 1" 开、 "N 0" 关）：

```c
static int t508n_gate_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "gate_cpu=%d state=%d\n",
		   READ_ONCE(t508n_gate_cpu), READ_ONCE(t508n_gate_state));
	return 0;
}

static int t508n_gate_proc_write(struct file *f, const char __user *buf,
				 size_t n, loff_t *o)
{
	char s[32];
	int cpu, state;

	if (n >= sizeof(s))
		return -EINVAL;
	if (copy_from_user(s, buf, n))
		return -EFAULT;
	s[n] = 0;
	if (sscanf(s, "%d %d", &cpu, &state) != 2)
		return -EINVAL;
	if (cpu < 0 || cpu >= NR_CPUS || (state != 0 && state != 1))
		return -EINVAL;
	WRITE_ONCE(t508n_gate_cpu, cpu);
	WRITE_ONCE(t508n_gate_state, state);
	pr_alert("t508n: gate cpu=%d state=%d\n", cpu, state);
	return n;
}

static int t508n_gate_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, t508n_gate_proc_show, NULL);
}

static const struct proc_ops t508n_gate_proc_ops = {
	.proc_open	= t508n_gate_proc_open,
	.proc_read	= seq_read,
	.proc_write	= t508n_gate_proc_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int __init t508n_gate_proc_init(void)
{
	proc_create("t508n_ipi_gate", 0644, NULL, &t508n_gate_proc_ops);
	return 0;
}
late_initcall(t508n_gate_proc_init);
```
新增 include：`<linux/proc_fs.h>`、`<linux/seq_file.h>`、`<linux/uaccess.h>`（smp.c 已含
`<asm/t508n_dbg.h>`，无需再加）。

### 1.4 补丁 P2：kernel/time/timer.c（每核心跳）

`run_local_timers`（timer.c:2462，TIMER_SOFTIRQ 每核每 tick 执行；本树 NO_HZ_IDLE=n，
tick 常开，所有核都会到）函数体开头加：

```c
static void run_local_timers(void)
{
	struct timer_base *base = this_cpu_ptr(&timer_bases[BASE_LOCAL]);

#ifdef CONFIG_ARM64
	extern void t508n_ipi_heartbeat(void);
	t508n_ipi_heartbeat();		/* T508N: 每核心跳 + 未消化 delta 扫描(节流) */
#endif

	hrtimer_run_queues();
	...
```

### 1.5 补丁 P3：arch/arm64/kernel/t508n_dbg.c / include/asm/t508n_dbg.h（fb 绘制扩展）

**P3a** — t508n_dbg.h 追加原型：

```c
/* SGI/IPI 诊断 + KFENCE fb 显示 */
void t508n_ipi_sent(unsigned int ipinr, const struct cpumask *target);
void t508n_ipi_rx(unsigned int ipinr);
void t508n_ipi_row_paint(unsigned int cpu);
void t508n_ipi_heartbeat(void);
void t508n_gate_hit_paint(unsigned int cpu);
bool t508n_ipi_gate_armed(unsigned int cpu);	/* 实现在 smp.c */
void t508n_kfence_fault_paint(unsigned long addr, bool is_write, unsigned int cpu);
void t508n_kfence_report_paint(int type, long obj_index,
			       unsigned long addr, bool is_write);
```

**P3b** — t508n_dbg.c 追加实现（新增 include：`<linux/atomic.h>`、`<linux/jiffies.h>`、
`<linux/smp.h>`、`<linux/cpumask.h>`、`<asm/barrier.h>`）：

```c
/* ===== T508N: SGI/IPI 丢失诊断 + KFENCE fb 显示 ===== */
#define T508N_BLK 6	/* 块间距 px */
#define T508N_BW  4	/* 块宽 */
#define T508N_BH  8	/* 块高 */

static atomic_long_t t508n_sent[2];			/* 0=RESCHEDULE 1=CALL_FUNC */
static atomic_long_t t508n_sent_to[NR_CPUS][2];		/* 发给各核的发送计数 */
static DEFINE_PER_CPU(atomic_long_t[2], t508n_rx);	/* 各核收到计数 */
static atomic_long_t t508n_kfence_hits;
static unsigned long t508n_delta_seen[NR_CPUS];

static void t508n_blk(int x, int y, bool on)
{
	if (on)
		t508n_paint_bar(x, y, T508N_BW, T508N_BH);
}

/* 把 v 的低 n 位画成 n 个块 (bit0 在最左) */
static void t508n_paint_bits(int x0, int y, unsigned long v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		t508n_blk(x0 + i * T508N_BLK, y, v & (1UL << i));
}

void t508n_ipi_sent(unsigned int ipinr, const struct cpumask *target)
{
	int cpu, k = (ipinr == IPI_CALL_FUNC);

	atomic_long_inc(&t508n_sent[k]);
	for_each_cpu(cpu, target)
		atomic_long_inc(&t508n_sent_to[cpu][k]);
}

void t508n_ipi_rx(unsigned int ipinr)
{
	int k = (ipinr == IPI_CALL_FUNC);

	atomic_long_inc(this_cpu_ptr(t508n_rx)[k]);
}

/* 每核一行: 12 块 = rx 低8位 / delta / 门armed / 心跳 / KFENCE命中标记 */
void t508n_ipi_row_paint(unsigned int cpu)
{
	unsigned long rx = atomic_long_read(&per_cpu(t508n_rx, cpu)[0]) +
			   atomic_long_read(&per_cpu(t508n_rx, cpu)[1]);
	unsigned long st = atomic_long_read(&t508n_sent_to[cpu][0]) +
			   atomic_long_read(&t508n_sent_to[cpu][1]);
	unsigned long now = jiffies;
	static DEFINE_PER_CPU(unsigned long, last_paint);
	unsigned long *lp = this_cpu_ptr(&last_paint);
	int y = 170 + (int)cpu * 20;

	if (time_after_eq(now, *lp + 1)) {	/* 每 jiffy 至多刷一次 */
		*lp = now;
		t508n_paint_bits(8, y, rx, 8);			/* 块0-7: 本核收到总数低8位 */
		t508n_blk(8 +  8 * T508N_BLK, y, st > rx);	/* 块8: 未消化 delta>0 */
		t508n_blk(8 +  9 * T508N_BLK, y, t508n_ipi_gate_armed(cpu)); /* 块9: 门armed */
		t508n_blk(8 + 10 * T508N_BLK, y, (now / (HZ / 4)) & 1);	/* 块10: 心跳 250ms 翻转 */
		t508n_blk(8 + 11 * T508N_BLK, y,
			  atomic_long_read(&t508n_kfence_hits) > 0);	/* 块11: KFENCE 全局命中 */

		/* y=140: 全局发送计数 (任何核画同一值, 幂等) */
		t508n_paint_bits(8, 140, atomic_long_read(&t508n_sent[0]), 8);
		t508n_paint_bits(8 + 8 * T508N_BLK, 140,
				 atomic_long_read(&t508n_sent[1]), 8);
	}
}

/* 每核心跳: timer softirq 每 tick 调, 节流 16 jiffies; boot CPU 兼做全核 delta 扫描 */
void t508n_ipi_heartbeat(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long now = jiffies;
	static DEFINE_PER_CPU(unsigned long, last_hb);
	unsigned long *lp = this_cpu_ptr(&last_hb);

	if (!time_after_eq(now, *lp + 16))
		return;
	*lp = now;

	t508n_ipi_row_paint(cpu);

	if (cpu == 0) {		/* boot CPU 每秒扫全核: 连续 2 次扫描 delta>0 => 疑似丢失 */
		static unsigned long last_scan;
		int i;

		if (!time_after_eq(now, last_scan + HZ))
			return;
		last_scan = now;
		for (i = 0; i < nr_cpu_ids; i++) {
			unsigned long st = atomic_long_read(&t508n_sent_to[i][0]) +
					   atomic_long_read(&t508n_sent_to[i][1]);
			unsigned long rx = atomic_long_read(&per_cpu(t508n_rx, i)[0]) +
					   atomic_long_read(&per_cpu(t508n_rx, i)[1]);

			t508n_delta_seen[i] = (st > rx) ? t508n_delta_seen[i] + 1 : 0;
			if (t508n_delta_seen[i] >= 2)
				t508n_blk(8 + i * T508N_BLK, 340, true);	/* 疑似丢失行 */
		}
	}
}

/* 门触发大标记 (先画后自旋, 自旋核再也不会刷屏) */
void t508n_gate_hit_paint(unsigned int cpu)
{
	int y = 360;

	t508n_paint_bar(8, y, 60, 8);
	t508n_paint_bar(8, y + 10, 60, 8);
	t508n_paint_bar(8, y + 20, 60, 8);
	t508n_mark_row(y, cpu + 1);
}

/* KFENCE fault 路径: 先于任何 printk 画屏 (报告打印可能在 printk-unfriendly 上下文死锁) */
void t508n_kfence_fault_paint(unsigned long addr, bool is_write, unsigned int cpu)
{
	unsigned long hits = atomic_long_inc_return(&t508n_kfence_hits);

	/* y=150: 块0-3 type(此时未知, 画 0xF), 块4 write, 块5-12 addr>>16, 块13-16 命中数低4位 */
	t508n_paint_bits(8, 150, 0xF, 4);
	t508n_blk(8 + 4 * T508N_BLK, 150, is_write);
	t508n_paint_bits(8 + 5 * T508N_BLK, 150, (addr >> 16) & 0xFF, 8);
	t508n_paint_bits(8 + 13 * T508N_BLK, 150, hits & 0xF, 4);
	(void)cpu;
}

/* KFENCE report 路径: type 已知后细化 */
void t508n_kfence_report_paint(int type, long obj_index,
			       unsigned long addr, bool is_write)
{
	unsigned long hits = atomic_long_read(&t508n_kfence_hits);

	t508n_paint_bits(8, 150, (unsigned long)type & 0xF, 4);
	t508n_blk(8 + 4 * T508N_BLK, 150, is_write);
	t508n_paint_bits(8 + 5 * T508N_BLK, 150, (addr >> 16) & 0xFF, 8);
	t508n_paint_bits(8 + 13 * T508N_BLK, 150, hits & 0xF, 4);
	t508n_paint_bits(8, 160, (unsigned long)obj_index & 0xFF, 8);	/* 对象号低8位 */
}
```

### 1.6 fb 布局与报数约定（320×480，32bpp，白=1）

```
y=20    阶段进度条 0..9 格                    [现有]
y=60    tick 状态                             [现有]
y=100   异常级别 EL1/2/3                      [现有]
y=140   全局发送: 块0-7 = RESCHEDULE 发送总数&0xFF; 块8-15 = CALL_FUNC 发送总数&0xFF
y=150   KFENCE: 块0-3 type(0=OOB 1=UAF 2=损坏 3=非法访问 4=非法free, 0xF=仅fault未分类),
        块4 write, 块5-12 出错地址>>16, 块13-16 命中次数&0xF
y=160   KFENCE 对象号 obj_index&0xFF (report 后)
y=170+i*20  CPU i (0..7) 每行 12 块:
        块0-7   本核收到 IPI 总数 (RESCHEDULE+CALL_FUNC) & 0xFF, bit0 在左
        块8     未消化 delta (发给本核 - 本核收到) > 0
        块9     死循环门 armed (本核)
        块10    心跳 (每 250ms 翻转)
        块11    KFENCE 全局命中标记
y=340   boot CPU 每秒扫描: 块 i = cpu i 连续 2s 未消化 delta (疑似丢失)
y=360   门触发大标记 (3 连块 + 行号=cpu+1)
```
报数格式（与既有约定一致）："每行报 12 个数位(0/1) 或 rx 值 + 4 标志；y=340 行报 8 位；
y=150 行报 type/write/addr/次数"。

### 1.7 使用与判定矩阵（SGI 丢失）

**A/B 流程**：
1. 先纯观测（不 arm 门）复现 memory_dev_init 竞态 → 拍照/读屏，记录各核行。
2. 回官方系统拉 pstore（见 §2.6）对照 dmesg。
3. 需要对照时：`echo "N 1" > /proc/t508n_ipi_gate`（或 boot 参数 `t508n_ipi_gate_cpu=N`
   在早期阶段 arm）→ 复跑 → 观察门标记（y=360）是否亮、竞态是否 1:1 复现。

| 屏态（CPUy 行） | 心跳块10 | delta 块8 | 判定 |
|---|---|---|---|
| rx 冻结、心跳亮 | 活 | 1 | CPUy 活着但发给它的 IPI 未被处理 → **支持 SGI 丢失/延迟**（发了没到） |
| rx 冻结、心跳灭 | 死 | 1 | CPUy 死（IRQ 掩蔽自旋/持锁）→ 症状由接收侧造成（到了没处理） |
| rx 增长、delta=0 | 活 | 0 | 投递正常 → 竞态与 IPI 通道无关 |
| y=360 门标记亮 | — | — | 门触发、CPUy 自旋 → 复现试验生效 |
| y=360 门标记不亮且系统仍卡 | 视心跳 | — | armed 核从未收到 RESCHEDULE → **SGI 丢失的直接证据** |
| y=340 有块 | 活 | — | boot CPU 实时判"疑似丢失"（连续 2s 未消化） |

**注意**：delta>0 瞬时存在是正常的（在途 IPI）；只有**持续**（冻结屏上可见 / y=340 连续块）
才算证据。门自旋核若为 boot CPU，会连带停喂 WDT（t508n_wdt_start 时）→ 看门狗复位属预期。

---

## 2. 设计二：KFENCE 内存损坏诊断（6.12 原生）

### 2.1 可行性评估（6.12）

| 项 | 结论 |
|---|---|
| 原生支持 | ✅ 6.12.103 完整 KFENCE；arm64 已 `select HAVE_ARCH_KFENCE`（arch/arm64/Kconfig:181） |
| 初始化时序 | ✅ `kfence_init()` 在 init/main.c:998（start_kernel，mm_init 之后）→ **早于** `driver_init`/memory_dev_init，覆盖竞态窗口的全部 kobject/kernfs 分配 |
| 采样参数 | ✅ `CONFIG_KFENCE_SAMPLE_INTERVAL=100`（默认即 100，与 stock 回移版 `sample_interval=100` 一致）；可用 boot 参数 `kfence.sample_interval=` 覆盖 |
| 池 | ✅ 255 对象 ≈ 2MB（memblock 早期分配，自动避开 reserved；与 ramoops@fff80000 无冲突） |
| 平台兼容 | ⚠️ **KFENCE_STATIC_KEYS 必须保持 n**（默认）——该选项 enable/disable 走 jump_label + IPI 广播，本平台 IPI 敏感；动态分支方案无此问题 |
| 开销 | 每 100ms 一个采样定时器 + 单次分配走 guard 路径；可忽略（stock 同参数跑通） |
| 时序影响 | ⚠️ 采样分配会做页表操作 + metadata 锁——有"加探针能过"类时序扰动风险，故**先跑 §1 纯观测计数，再开 KFENCE** |

### 2.2 KFENCE 能检测什么（"内存被踩"的直接证据）

| 类型 | 触发条件 | 对应"内存被踩"场景 |
|---|---|---|
| out-of-bounds (OOB) | 越过对象写入/读出，命中红区或 guard 页（双向） | 踩到相邻对象的越界写 |
| use-after-free (UAF) | 释放后访问（对象页 guard） | 野指针/悬垂引用写 |
| invalid free | 双重释放 / 释放非 KFENCE 指针 | free 路径指针错乱 |
| free-time corruption | 释放时 canary 被改（KFENCE_ERROR_CORRUPTION） | 释放前内存被踩 |
| 不能检测 | 栈/全局越界、未命中 guard 页的段内越界（采样+粒度限制） | — |

**判定"内存被踩"的最直接证据 = pstore 里的 `BUG: KFENCE: out-of-bounds write in %pS` +
栈回溯指向 device_register/kobject/kernfs 分配链**，或屏幕 y=150 行出现 type 块。

### 2.3 无 console 报告通道

1. **主通道（替代判定）：pstore console**
   - DTS 已含 `ramoops@fff80000`（`tools/dts/ums9620-ot5.dts`：record/console/pmsg 各 0x8000），
     6.12 的 ramoops 驱动内嵌于 fs/pstore/ram.c（`postcore_initcall(ramoops_init)`，
     of 匹配 `compatible="ramoops"`），只需配置使能。
   - KFENCE 报告 `pr_err` → printk → pstore console（PSTORE_CONSOLE 注册为 console，与真实
     UART 无关）→ 写入 ramoops 缓冲 → **跨重启保留**。重启回官方系统：
     ```bash
     adb shell su -c "cp -r /sys/fs/pstore /sdcard/pstore"
     adb pull /sdcard/pstore
     grep -l "BUG: KFENCE" /sdcard/pstore/console-ramoops-0
     ```
   - 容量：console-size 0x8000（32KB）环形；KFENCE 报告含 16 层栈 + 对象信息 ≈ 2-4KB，可容纳。
2. **实时通道：fb 挂钩**（§1.5 P3b 的 `t508n_kfence_fault_paint`）——在
   `kfence_handle_page_fault` 检测到 guard 页命中后**先画屏再打印**，任何上下文都可见；
   拍照即得 type/地址/命中数。
3. **可选**：`panic_on_warn=1` 把报告转 panic → 复位循环 + pstore 记录（确定性标记）；
   默认保持 0 以便继续读 IPI 计数行。

### 2.4 补丁 K1：config fragment（scripts/config 或 merge_config）

```sh
cd /home/test/t508n/linux-6.12.103
./scripts/config \
  -e KFENCE -e KFENCE_SAMPLE_INTERVAL -d KFENCE_DEFERRABLE -d KFENCE_STATIC_KEYS \
  -d KFENCE_STRESS_TEST_FAULTS -d KFENCE_KUNIT_TEST \
  -e PSTORE -e PSTORE_CONSOLE -e PSTORE_PMSG -e PSTORE_RAM \
  -e PSTORE_DEFAULT_KMSG_BYTES -e CMDLINE_EXTEND
```
等价 fragment（`work/kfence.fragment`，供 `scripts/kconfig/merge_config.sh` 使用）：
```
CONFIG_KFENCE=y
CONFIG_KFENCE_SAMPLE_INTERVAL=100
# CONFIG_KFENCE_DEFERRABLE is not set
# CONFIG_KFENCE_STATIC_KEYS is not set
# CONFIG_KFENCE_STRESS_TEST_FAULTS is not set
# CONFIG_KFENCE_KUNIT_TEST is not set
CONFIG_PSTORE=y
CONFIG_PSTORE_DEFAULT_KMSG_BYTES=16384
CONFIG_PSTORE_CONSOLE=y
CONFIG_PSTORE_PMSG=y
CONFIG_PSTORE_RAM=y
CONFIG_CMDLINE_EXTEND=y
CONFIG_CMDLINE="kfence.sample_interval=100"
```
说明：
- 6.12 无独立 RAMOOPS config（驱动在 fs/pstore/ram.c，由 PSTORE_RAM 编译）。
- `CONFIG_CMDLINE_EXTEND=y` 让 LK 注入的 bootargs 之后**追加** `CONFIG_CMDLINE` 内容——
  本平台 LK 会覆盖 DTB chosen/bootargs，这是注入 `kfence.sample_interval` /
  `t508n_ipi_gate_cpu` 等内核参数的唯一可靠途径（`CONFIG_CMDLINE_FORCE` 不要开，会丢掉
  console= 等 LK 参数）。
- 如需提高命中率（多次复现无报告时）：把 CMDLINE 改为 `kfence.sample_interval=10` 重编
  （注意采样越密时序扰动越大，是双刃剑）。

### 2.5 补丁 K2：mm/kfence/core.c（fault 路径 fb 挂钩）

文件顶部（include 之后）加：
```c
#ifdef CONFIG_ARM64
extern void t508n_kfence_fault_paint(unsigned long addr, bool is_write,
				     unsigned int cpu);
#endif
```
`kfence_handle_page_fault`（core.c:1223）中
`atomic_long_inc(&counters[KFENCE_COUNTER_BUGS]);`（约 1236 行）之后插入：
```c
#ifdef CONFIG_ARM64
	/* T508N: 先画屏 (报告打印可能在 printk-unfriendly 上下文死锁) */
	t508n_kfence_fault_paint(addr, is_write, smp_processor_id());
#endif
```
（core.c 已包含 `<linux/atomic.h>` 等；`smp_processor_id` 需要 `<linux/smp.h>`，若编译报隐式
声明则在 CONFIG_ARM64 分支前补 `#include <linux/smp.h>`。）

### 2.6 补丁 K3：mm/kfence/report.c（type 已知后细化）

文件顶部加：
```c
#ifdef CONFIG_ARM64
extern void t508n_kfence_report_paint(int type, long obj_index,
				      unsigned long addr, bool is_write);
#endif
```
`kfence_report_error` 中 `lockdep_off();`（report.c:220）之后插入：
```c
#ifdef CONFIG_ARM64
	t508n_kfence_report_paint(type, object_index, address, is_write);
#endif
```
（`object_index` 为 `const ptrdiff_t`，隐式转 long 安全。）

### 2.7 判定方法（KFENCE）

| 证据 | 判定 |
|---|---|
| `pstore/console-ramoops-0` 含 `BUG: KFENCE: out-of-bounds write in %pS` + 栈回溯 | **越界写直接证据**；栈指向 device_register/kobject/kernfs 链 ⇒ "内存被踩"证据闭合 |
| 同上含 `use-after-free` / `invalid free` / `memory corruption` | 对应类型损坏证据 |
| 屏幕 y=150 行块亮（type/addr/次数）+ y=170 行块11 亮 | 实时命中证据（拍照），且可与 IPI 计数行同时判读时间点 |
| 多次复现无报告 | **不能排除**踩踏（KFENCE 是采样）→ 降 `sample_interval` 至 10 重试 10 次 |
| 报告栈与竞态卡点核/时刻对齐（该核 delta>0 或心跳冻结） | KFENCE 命中与 SGI/竞态症状耦合的证据链 |

**联合判读示例**：若竞态卡在 CPU0 的 device_register，pstore 报告
`BUG: KFENCE: out-of-bounds write in kernfs_put_active` + 屏上 CPU0 行 rx 冻结/delta 0、
CPU3 行 delta>0 —— 说明 CPU0 在 kernfs 分配链上发生了越界写（踩坏了某结构），同时 CPU3
有未消化的 IPI —— 两个诊断互相印证。

---

## 3. 实施顺序与构建刷机

```
# 0) 备份
cp .config .config.bak; git diff > work/diag-sgi-kfence.diff   (若为 git 树)
# 1) 先只上 P1+P2+P3 (纯观测, 不改语义) → 构建
make -j$(nproc) Image
# 2) 复现竞态 → 读屏报数 (§1.6 格式) → 判定 (§1.7)
# 3) 需要 A/B 时: proc 写门 / 重编加 CONFIG_CMDLINE 的 t508n_ipi_gate_cpu=N
# 4) 再上 K1 (config) + K2 + K3 → 重编 → 复现 → 屏判 + 回官方系统拉 pstore
cp arch/arm64/boot/Image work/mainline_Image
python3 tools/sprd_mkbootimg.py --kernel work/mainline_Image \
    --ramdisk initramfs.cpio --output boot_linux.img --key tools/ot5_boot_key.pem
fastboot flash boot_a boot_linux.img && fastboot reboot
# 读判: 屏(拍照) ; 回官方: adb pull /sys/fs/pstore + /dev/block/by-name/uboot_log
# 回滚: 恢复 .config.bak + git checkout arch/arm64/kernel/smp.c kernel/time/timer.c \
#       arch/arm64/kernel/t508n_dbg.c arch/arm64/include/asm/t508n_dbg.h \
#       mm/kfence/core.c mm/kfence/report.c
```

---

## 4. 风险与注意

1. **门自旋 = 真死核**：armed 后该核收任何 RESCHEDULE 即永久自旋（IRQ 掩蔽）；只用于 A/B，
   默认关闭。若 armed 的是 boot CPU 且 WDT 在跑，会触发看门狗复位。
2. **KFENCE 改变时序**：采样分配有页表操作 + 锁；"加探针能过"现象在本平台已多次出现——
   因此 KFENCE 命中是"锦上添花"证据，**不命中不能下否定结论**；先跑 §1 计数层拿 IPI 证据。
3. **KFENCE_STATIC_KEYS 保持 n**：该选项的 jump_label 翻转走 IPI 广播，正是本平台脆弱点。
4. **pstore 需在竞态卡死前注册**：ramoops 是 postcore initcall，远早于 driver_init，安全；
   但若竞态发生在更早阶段（无），console 缓冲为空属正常，此时以 fb 通道为准。
5. **delta 判读注意在途 IPI**：瞬时 delta>0 正常；以"冻结屏上的持续状态 / y=340 连续块"为准。
6. **只改本方案文件**：smp.c / timer.c / t508n_dbg.[ch] / kfence core.c / report.c / .config，
   不触碰 LSE、DEVTMPFS、driver_init 顺序等已对齐项。

---

## 附录 A：patch 应用清单

| 补丁 | 文件 | 内容 |
|---|---|---|
| P1a-d | arch/arm64/kernel/smp.c | 门数据+boot参数 / do_handle_IPI 计数+门 / smp_cross_call 发送钩子 / proc 控制 |
| P2 | kernel/time/timer.c | run_local_timers 每核心跳 |
| P3a | arch/arm64/include/asm/t508n_dbg.h | 新原型 |
| P3b | arch/arm64/kernel/t508n_dbg.c | 计数/心跳/门标记/KFENCE 画屏实现 |
| K1 | .config | KFENCE + pstore + CMDLINE_EXTEND fragment |
| K2 | mm/kfence/core.c | fault 路径 fb 挂钩（先画后打印） |
| K3 | mm/kfence/report.c | type 细化挂钩 |

## 附录 B：关键源码位置（6.12.103，已核对）

```
arch/arm64/kernel/smp.c:90      crash_stop 之后插 P1a
arch/arm64/kernel/smp.c:960     do_handle_IPI case IPI_RESCHEDULE
arch/arm64/kernel/smp.c:964     case IPI_CALL_FUNC
arch/arm64/kernel/smp.c:1017    smp_cross_call
kernel/time/timer.c:2462        run_local_timers
mm/kfence/core.c:1223           kfence_handle_page_fault
mm/kfence/core.c:~1236          atomic_long_inc(&counters[KFENCE_COUNTER_BUGS])
mm/kfence/report.c:220          lockdep_off()
init/main.c:998                 kfence_init() (早于 driver_init)
arch/arm64/Kconfig:181          select HAVE_ARCH_KFENCE
fs/pstore/ram.c:896             of_match "ramoops" (PSTORE_RAM 内建)
tools/dts/ums9620-ot5.dts:55    ramoops@fff80000
```
