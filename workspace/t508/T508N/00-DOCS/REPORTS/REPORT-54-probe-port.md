# REPORT: 5.4 复刻内核 framebuffer 探针移植（清屏 + 启动全程进度条 + 阶段标记）

日期：2025-08-18
任务：`prompt-54-probe-port.md` — 把 6.12 树的 framebuffer 探针系统移植到复刻 5.4 树
（`/home/test/t508n/work/linux-5.4-custom/`，5.4.210 + 紫光魔改 + 原厂 config）。

---

## 1. 移植清单（文件级）

| 文件 | 动作 | 说明 |
|---|---|---|
| `arch/arm64/include/asm/t508n_dbg.h` | 新增 | 6.12 头文件裁剪版：仅保留 fb 探针 API（fbready / paint_bar / mark_row / clear / stage / fb_is_ready / fb_va），去掉 6.12 特有（idmap 验证全局量、wdt、probe_timer、paint_initcall、sm2_done 等） |
| `arch/arm64/kernel/t508n_dbg.c` | 新增 | 6.12 实现移植，见 §2 API 适配 |
| `arch/arm64/kernel/Makefile` | 修改 | `obj-y` 追加 `t508n_dbg.o` |
| `init/main.c` | 修改 | start_kernel / kernel_init / kernel_init_freeable / do_basic_setup / run_init_process 挂探针（§3） |
| `drivers/base/init.c` | 修改 | `driver_init()` 按 5.4 实际顺序挂探针（§3） |
| `drivers/base/memory.c` | 修改 | `memory_dev_init()` 探针 + block 循环计数（§3） |

git commit：`c4bd9f7ae`（`T508N framebuffer probe port (5.4): clear screen + boot progress bar + stage marks`，6 files changed, 178 insertions(+), 1 deletion(-)）。

> 注：工作区另有 `.config` 未提交改动（上次构建用 arm gcc 13.2.1 重新生成配置的编译器版本/栈保护项），属构建副产物，不属本移植。

## 2. 5.4 API 适配（与 6.12 的差异处理）

1. **`dcache_clean_inval_poc(start, end)` 不存在于 5.4**。
   5.4 `arch/arm64/include/asm/cacheflush.h` 只有 `__clean_dcache_area_poc` /
   `__inval_dcache_area` / `__flush_dcache_area`。
   → 移植实现里用 **`__flush_dcache_area((void *)start, end - start)`**
   （clean + invalidate to PoC，与 6.12 `dcache_clean_inval_poc` 语义等价），
   封装为 `t508n_flush_poc()`。任务描述称"5.4 的 dcache_clean_inval_poc 可用"，
   实际核对源码后确认 5.4 无此符号，已按 5.4 实际 API 替换。
2. **`phys_to_virt`**：5.4 可用，`arch/arm64/include/asm/memory.h` 中为
   `static inline void *phys_to_virt(phys_addr_t x)`，
   `__phys_to_virt(x) = ((x) - PHYS_OFFSET) | PAGE_OFFSET`（VA_BITS=39 → PAGE_OFFSET
   = 0xffffff8000000000，fb VA = 0xffffff80b0000000，与 6.12 一致）。
3. **清屏时机**：5.4 `setup_arch()` 内部 `arm64_memblock_init()`(line 321) +
   `paging_init()`(line 323) 之后线性映射可用 → 在 `start_kernel` 中
   `setup_arch(&command_line)` 返回后立即 `t508n_fbready()`（清屏 + stage 0），
   与 6.12 挂点相同。
4. **driver_init 顺序（5.4 实际）**：`devtmpfs_init → devices_init → buses_init →
   classes_init → firmware_init → hypervisor_init → of_core_init → platform_bus_init
   → cpu_dev_init → memory_dev_init → container_dev_init`。
   `.config` 确认 `CONFIG_DEVTMPFS` 未开（`# CONFIG_DEVTMPFS is not set`），
   `devtmpfs_init()` 为 no-op stub → 探针按实际生效顺序挂（devices→…→container），
   不给 devtmpfs 画格。
5. 6.12 特有、**不移植**的内容：`t508n_sm2_done`、idmap 链验证全局量
   （`t508n_idmap_*`）、WDT（`t508n_wdt_start`，5.4 无对应启动点需求）、
   `t508n_probe_timer` / `t508n_mark` / `t508n_pass` / `t508n_paint_initcall`。

## 3. 探针布局（屏幕 320×480, 32bpp BGRA, stride 1280, base 0xb0000000）

```
y=20   进度条 10 格 (28x20, 间距30, x=8+n*30) —— t508n_stage(n), n=0..9
y=60   driver_init 前段: devices_init(8) classes_init(44) hypervisor_init(80) of_core_init(116)
       + mark_row(60,1)
y=100  driver_init 后段: platform_bus_init(8) cpu_dev_init(44) memory_dev_init(80) container_dev_init(116)
       + mark_row(100,2)
y=140  memory_dev_init 内部: block_sz 计算后(8) subsys 注册后(44) block 循环完成(80)
y=155  memory block 循环计数: x=44+n*10 每 4 个 block 一格, 最多 8 格 (8x8)
y=180  do_basic_setup: cpuset_init_smp(8) driver_init 后(44) init_irq_proc(80) do_ctors(116) usermodehelper_enable(152)
y=400  do_initcalls 完成(8) + mark_row(400,6)
y=460  exec init 前(8) + mark_row(460,9)   [run_init_process]
```

进度条 stage 语义（与任务 §2 清单对应）：

| stage | 挂点（5.4 源码位置） | 含义 |
|---|---|---|
| 0 | `start_kernel` 内 `t508n_fbready()` | fb 就绪 + 清屏 |
| 1 | `start_kernel` setup_arch 后 | start_kernel 到达（线性映射可用） |
| 2 | `start_kernel` mm_init() 后 | mm_init 完成 |
| 3 | `start_kernel` arch_call_rest_init() 前 | rest_init 调用 |
| 4 | `kernel_init()` 入口 | kernel_init 到达 |
| 5 | `kernel_init_freeable` smp_prepare_cpus() 后 | smp_prepare_cpus 完成 |
| 6 | `kernel_init_freeable` workqueue_init() 后 | workqueue_init 完成 |
| 7 | `kernel_init_freeable` smp_init() 后 | smp_init 完成（二级核上线） |
| 8 | `kernel_init_freeable` sched_init_smp() 后 | sched_init_smp 完成 |
| 9 | `kernel_init_freeable` do_basic_setup() 后 | do_basic_setup 完成 |

（5.4 与 6.12 路径差异：5.4 的 `kernel_init` 直接调 `kernel_init_freeable`，
无独立 `rcu_init_tasks_generic` 阶段；`do_basic_setup` 内含 `usermodehelper_enable`，
5.4 的 `kernel_init_freeable` 末尾用 `ksys_open/ksys_dup` 开 /dev/console 而非
`console_on_rootfs()`。）

## 4. 编译结果

命令（gcc13 工具链 + openssl-local）：
```bash
export PATH=/home/test/t508n/arm-gnu-toolchain-13.2.Rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CPATH=/home/test/t508n/openssl-local/usr/include:/home/test/t508n/openssl-local/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/test/t508n/openssl-local/usr/lib/x86_64-linux-gnu
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- Image -j8
```

- **编译通过**（exit 0），无错误；仅原有 `LOAD segment with RWX permissions` 链接告警（与本次改动无关）。
- `t508n_dbg.o` 一次通过；`init/main.o`、`drivers/base/init.o`、`drivers/base/memory.o` 增量重编。
- 产出：`arch/arm64/boot/Image`（31,621,632 字节）。
- 符号验证（System.map）：`t508n_paint_bar / t508n_clear / t508n_fbready /
  t508n_stage / t508n_mark_row / t508n_fb_is_ready / t508n_fb_va` 均在。

## 5. 打包结果

```bash
cp work/linux-5.4-custom/arch/arm64/boot/Image work/stock54-custom_Image
python3 tools/sprd_mkbootimg.py --kernel work/stock54-custom_Image --ramdisk initramfs.cpio \
  --output boot_54custom_probe.img --key tools/ot5_boot_key.pem
```

- 产出：**`/home/test/t508n/boot_54custom_probe.img`**（67,108,864 字节，
  kernel 31621632(+3584 pad) @0x1000, container lz4 882722, ramdisk_size 0xd782e）。
- 内核 md5 `39de0ca8…`，与既有 `boot_54custom.img` 内核不同，确认含探针改动。

## 6. 验证建议（上机）

1. 刷入 `boot_54custom_probe.img`，观察：
   - 启动即清屏（无残留白条）；
   - y=20 进度条随启动推进逐格点亮（0→9）；
   - y=155 memory block 计数格数 ≈ 内存 block 数/4；
   - 若某阶段卡死，进度条停在对应格，即定位挂起点。
2. 预期正常满格路径：stage 0-9 全亮 + y=400 格亮 + y=460 格亮（init 已 exec）。
3. 若 stage 0/1 未亮且屏幕无变化：怀疑 fb 物理地址 0xb0000000 未落入 5.4 线性映射
   （原厂 config 的 memblock 布局差异），需在 `t508n_fbready()` 前加
   `early_ioremap`/idmap 映射（参考 6.12 `mmu.c create_idmap` 的 fb 1MB 映射）。

## 7. 产物汇总

- 修改树：`/home/test/t508n/work/linux-5.4-custom/`（commit `c4bd9f7ae`）
- 镜像：`/home/test/t508n/boot_54custom_probe.img`
- 内核副本：`/home/test/t508n/work/stock54-custom_Image`
- 本报告：`/home/test/t508n/work/REPORT-54-probe-port.md`
