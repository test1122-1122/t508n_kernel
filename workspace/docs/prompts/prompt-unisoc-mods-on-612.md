# 任务：干净 6.12 树上移植全部紫光魔改并编译验证

## 背景
T508N（Unisoc T770/UMS9620）mainline 6.12.103 bring-up。
**复刻原厂 5.4 内核（5.4.210 + 紫光魔改 + 原厂 config）已 boot 到 initramfs（exec /init）**——
证明紫光魔改路径在 T770 可行。现在需要**把紫光魔改全部移植到干净 6.12 树**（适配 6.12 API），
编译验证，并重点分析**上下文安全性**（尤其 scheduler_ipi 相关，魔改树曾因 kick_ilb 补丁回归 9 格）。

## 参考源（5.4 复刻的紫光魔改实现——已验证可 boot）
- /home/test/t508n/work/linux-5.4-custom/  ← 5.4 复刻树（git commits 含魔改实现）
  - `git -C /home/test/t508n/work/linux-5.4-custom log --oneline` 看 commits
  - `git show c4bd9f7ae`（探针移植）、魔改 commit（2ca901fb0 等）看魔改 diff
  - 魔改位置：mmzone.h（pfn_valid NOMAP）、kernel/sched/core.c（scheduler_ipi）、
    kernel/stop_machine.c（stopper preempt_schedule）、kernel/sched/cpufreq_schedutil.c
    （sugov 25%）、drivers/base/memory.c（mem_sysfs_mutex）、mm/shmem.c（mode 0777）、
    arch/arm64/kernel/smp.c（死循环门计数）、mm/memmap（memmap 跳 no-map）
- /home/test/t508n/work/DISASM-*.md（原厂反汇编细节）
- /home/test/t508n/work/PATCH-nohz-kick.md（kick_ilb → RESCHEDULE 补丁，6.12 版）
- /home/test/t508n/work/UNISOC-DETAILED.md（紫光魔改清单）

## 材料
- /home/test/t508n/dl/linux-6.12.103.tar.xz  ← 干净 6.12.103 源码
- /home/test/t508n/linux-6.12.103/.config    ← 我们的 config 基线（可复制）
- 工具链/openssl（同前）

## 任务

### 1. 干净树
- 解包 dl/linux-6.12.103.tar.xz → /home/test/t508n/work/linux-6.12-clean/
- 复制 .config（从 linux-6.12.103/.config）→ olddefconfig

### 2. 移植全部紫光魔改（适配 6.12 API，每个魔改给出 diff）
从 5.4-custom 树参考实现，移植到 6.12：
1. **scheduler_ipi 扩展**（6.12 版）：RESCHEDULE 就地处理 + nohz kick（参考
   PATCH-nohz-kick.md 的设计 + 5.4 复刻 B1）——**注意上下文安全**
2. **cpu_stopper_thread 队空显式 preempt_schedule**（B2）
3. **sugov_set_freq_margin 25%**（B3）
4. **memmap_init 跳 no-map**（D2：6.12 的 memmap_init_range 逐页检查——6.12 的
   pfn_valid 已有我们的 NOMAP 修复（__pageblock_pfn_to_page），memmap 部分参照 5.4）
5. **shmem mode=0777**（F1）
6. **死循环门降级计数**（C1：smp.c per-CPU 计数，不自旋）
7. **irq_exit softirq 分派定制**（C3：pending&0x41 → 立即 __do_softirq）
8. **kick_ilb → smp_send_reschedule**（PATCH-nohz-kick.md，含 scheduler_ipi 整合）
9. （可选）CONFIG_KFENCE=y（6.12 原生）
10. **pfn_valid NOMAP / mem_sysfs_mutex**：6.12 魔改树已有修复——可参照移植（或在干净树也应用）
11. **LSE**：6.12 魔改树用 static_key 替代原厂经典 ALTERNATIVE——干净树**保持 6.12 原版**
    （暂不移植 LSE 改动，除非分析需要）

**注意**：6.12 与 5.4 的 API/结构差异（函数名、行号、锁），按 6.12 源码适配。
**重点**：scheduler_ipi 的 nohz_csd_func 调用在 stop_machine 上下文的**安全性分析**
（spinlock 在关中断上下文？softirq raise 时机？）——对比原厂/5.4 做法（原厂 scheduler_ipi
用 idle_balance + SCHED_SOFTIRQ + 唤醒 ksoftirqd，**不直接调 nohz_csd_func**？）——
如果 PATCH-nohz-kick 的设计有风险，给出修正版。

### 3. 编译
```bash
export PATH=.../bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
```
逐个修复编译错误

### 4. 输出
- 干净树 + 全部紫光魔改（6.12 版）编译结果
- 报告：/home/test/t508n/work/REPORT-UNISOC-MODS-ON-612.md
  - 每个魔改的 6.12 移植 diff 要点
  - 编译结果
  - **上下文安全性分析**（重点：scheduler_ipi → nohz 处理在 stop_machine 期间）
  - 与 5.4 复刻的对应关系
  - 建议（哪些移植到魔改树、哪些修正）

## 注意
- 只写 REPORT-UNISOC-MODS-ON-612.md + 在 linux-6.12-clean/ 操作（不碰魔改树）
- 魔改按 6.12 源码适配；scheduler_ipi 安全性是核心分析点
