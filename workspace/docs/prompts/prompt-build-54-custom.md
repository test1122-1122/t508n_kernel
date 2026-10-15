# 任务：干净环境编译复刻原厂 5.4 内核（5.4.210 + 紫光魔改 + 原厂 config）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210 fork）mainline 6.12.103 bring-up。
为对照"原厂式内核"在 T770 上的行为，需要**编译一个复刻原厂的 5.4 内核**：
完整 5.4.210 源码 + 已知紫光魔改（从反汇编重建）+ 原厂 config。

**先读**（获取魔改细节）：
- /home/test/t508n/work/UNISOC-DETAILED.md（第 1 节紫光魔改详表、第 2 节 config）
- /home/test/t508n/work/DISASM-B-sched-调度器.md（scheduler_ipi/stopper 魔改反汇编）
- /home/test/t508n/work/DISASM-C-irq-中断GIC/Timer.md（死循环门/softirq 魔改）
- /home/test/t508n/work/DISASM-D-mm-内存管理.md（pfn_valid NOMAP/__rmqueue_cma）
- /home/test/t508n/work/DISASM-E-devm-驱动模型sysfs.md（mem_sysfs_mutex/fw_devlink）
- /home/test/t508n/work/DISASM-F-fs-文件系统VFS.md（shmem mode）

## 材料
- /home/test/t508n/work/linux-5.4.210/   ← 5.4.210 git 仓库（shallow，有 v5.4.210 tag）——需完整 checkout
- /home/test/t508n/work/kernel.config    ← 原厂 .config（直接使用）
- /home/test/t508n/work/stock_Image_full.bin + stock_kallsyms.map（对比用）

## 步骤

### 1. 干净源码树
- 在 /home/test/t508n/work/ 下新建 linux-5.4-custom/（或完整 checkout 现有仓库到新目录）
- 用 git archive 或 git checkout 提取完整 v5.4.210 树（`git -C linux-5.4.210 archive v5.4.210 | tar -x -C linux-5.4-custom`）

### 2. 原厂 config
- 复制 work/kernel.config 到 .config
- `make ARCH=arm64 O=... olddefconfig`（或就地）处理缺失符号
- 记录 olddefconfig 改了哪些（diff）

### 3. 应用紫光魔改（从反汇编重建，按优先级）
从 DISASM 报告重建以下魔改（每个给出 diff）：
- **pfn_valid NOMAP 拒绝**（D1：memblock 二分 + NOMAP bit2 拒绝）——最优先
- **memmap_init_zone 逐页跳过 no-map**（D2）
- **scheduler_ipi 扩展**（B1：fold + wake_list 处理 + 自触发 nohz idle balance + 唤醒 ksoftirqd）
- **cpu_stopper_thread 队空显式 preempt_schedule**（B2）
- **mem_sysfs_mutex**（E4：memory_dev_init 全程持有）
- **shmem mode=0777**（F1）
- （可选）**死循环门**（C1：ipi_handler case0）——注意：实现为**检测自旋**有风险，可先注释/降级为计数
- （可选）sugov_set_freq_margin 25%

**注意**：魔改实现基于反汇编推断，与真原厂可能有出入；以"行为接近"为目标，不必逐字节。

### 4. 编译
```bash
export PATH=/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
```
（5.4 的 certs 也需要 openssl——用 CPATH/LIBRARY_PATH 环境变量，勿用命令行 HOST_EXTRACFLAGS）
- 编译错误逐个修复（5.4.210 用 gcc13 工具链可能有老代码警告/错误——必要时加
  `KCFLAGS=-Wno-...` 或 `scripts/config` 关相关项）

### 5. 对比验证
- 产出 arch/arm64/boot/Image，记录大小
- 与 stock_Image_full.bin 对比：尺寸差异、`nm` 对比关键符号（scheduler_ipi/pfn_valid 等）是否一致

## 输出
- 编译产物：/home/test/t508n/work/linux-5.4-custom/arch/arm64/boot/Image（或同目录镜像）
- 报告：/home/test/t508n/work/REPORT-54-custom-build.md
  - 魔改实现清单（每个魔改的 diff 要点）
  - olddefconfig 差异
  - 编译日志摘要（错误/警告）
  - 镜像对比（尺寸/符号）
  - 与原厂的差异评估 + 建议

## 注意
- 只写 REPORT-54-custom-build.md + 在 linux-5.4-custom/ 内操作（不碰 6.12 树）
- 5.4 源码魔改位置与 6.12 可能不同（5.4 的 memory.c/core.c 行号），按 5.4 源码实现
