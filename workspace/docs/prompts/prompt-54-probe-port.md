# 任务：为复刻 5.4 内核移植 framebuffer 探针（清屏 + 启动全程进度）

## 背景
T508N（Unisoc T770/UMS9620）。复刻原厂 5.4 内核已编译在
/home/test/t508n/work/linux-5.4-custom/（5.4.210 + 紫光魔改 + 原厂 config）。
**已刷入设备但无探针**（屏幕无法观察启动状态）。需要移植 framebuffer 探针系统，
实现：①启动清屏；②启动全程进度标记（顶部进度条 + 关键阶段行），与 6.12 树的探针风格一致。

## 参考实现（6.12 树，已工作）
- /home/test/t508n/linux-6.12.103/arch/arm64/kernel/t508n_dbg.c（探针 API 实现）
- /home/test/t508n/linux-6.12.103/arch/arm64/include/asm/t508n_dbg.h（头文件）
- 6.12 树的探针调用点（init/main.c、drivers/base/init.c 等）可参考挂点风格

## 硬事实（已确认）
- framebuffer：32bpp BGRA，320×480，stride 1280，base 0xb0000000，白色=0xFFFFFFFF
- 画屏用 phys_to_virt(0xb0000000) + 写内存 + dcache_clean_inval_poc（cacheable 映射需要刷）
- 5.4 的 phys_to_virt / dcache_clean_inval_poc / dcache_clean_inval_pou 均可用
- 5.4 树：/home/test/t508n/work/linux-5.4-custom/（git 3 commits）

## 任务

### 1. 移植探针 API
- 复制 t508n_dbg.c → linux-5.4-custom/arch/arm64/kernel/，t508n_dbg.h → arch/arm64/include/asm/
- 适配 5.4：include（5.4 的 <asm/cacheflush.h> API 名）、phys_to_virt、无 t508n_sm2_done 等 6.12 特有
- arch/arm64/kernel/Makefile 加 t508n_dbg.o

### 2. 清屏 + 进度条
- 在 MMU 开、线性映射可用后的最早点调用清屏 + 进度条 stage 0
- 参考 6.12：t508n_fbready()（设置 fb 基址 + 清屏 + stage 0）；5.4 的对应调用点
  （start_kernel 早期？5.4 的 setup_arch 后？——确认 5.4 中 phys_to_virt 可用时机）
- 进度条 stage 1-9：对应 5.4 启动阶段（start_kernel 到达 / mm_init / rest_init /
  kernel_init / smp_prepare_cpus / workqueue / smp_init / sched_init_smp /
  do_basic_setup / init 执行）

### 3. 启动全程阶段探针（5.4 挂点）
在 5.4 的启动路径加关键阶段标记（画格/画行）：
- kernel_init_freeable：smp_prepare_cpus / workqueue_init / rcu_init / smp_init /
  sched_init_smp / do_basic_setup 每个函数后
- do_basic_setup → driver_init：devtmpfs/devices/of_core/memory_dev_init 等（5.4 的
  drivers/base/init.c 顺序：devices→buses→classes→firmware→hypervisor→of_core→platform→
  cpu→memory→container）
- memory_dev_init 的 block 循环计数
- 注意：5.4 的 driver_init 无 devtmpfs（原厂 config 关）——按 5.4 实际顺序挂

### 4. 编译
```bash
export PATH=/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
```
- 修复编译错误（5.4 API 差异）
- 产出 arch/arm64/boot/Image

### 5. 打包
```bash
cd /home/test/t508n
cp work/linux-5.4-custom/arch/arm64/boot/Image work/stock54-custom_Image
python3 tools/sprd_mkbootimg.py --kernel work/stock54-custom_Image --ramdisk initramfs.cpio \
  --output boot_54custom_probe.img --key tools/ot5_boot_key.pem
```

## 输出
- 修改后的 linux-5.4-custom 树（git commit 记录改动）
- 镜像：/home/test/t508n/boot_54custom_probe.img
- 报告：/home/test/t508n/work/REPORT-54-probe-port.md（移植清单 + 探针布局 + 编译结果）

## 注意
- 5.4 的启动路径/API 与 6.12 有差异（行号、函数名），按 5.4 源码实际挂
- 探针布局保持简洁（进度条 + 每阶段一格 + memory 循环计数）
- 只改 linux-5.4-custom 树（不碰 6.12 树）
