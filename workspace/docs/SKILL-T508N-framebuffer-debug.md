# Skill：T508N 无串口 framebuffer 调试法

> 适用范围：T508N（Unisoc T770/UMS9620）上 mainline 内核 bring-up，
> 无 UART、无 USB，只有 LCD 屏幕 + fastboot。

---

## 1. 硬事实（已确认，直接复用）

- 帧缓冲：**32bpp BGRA，320×480，行宽 stride=1280 字节，基址 0xb0000000，白色=0xFFFFFFFF**
- boot 链：BootROM → SPL → LK（`uboot` 分区）→ 内核；LK 加载 raw `Image` @0x80080000，dtb @0xbbf00000
- 刷机：`tools/winfastboot/fastboot.exe flash boot_a boot_linux.img`（slot a 活动）
- 恢复出厂：`fastboot flash boot_a/vendor_boot_a/dtbo_a <stock> + set_active a + reboot`，之后 dtbo 需用 adb+dd 刷 noop（fastboot 写 dtbo 报 "Too many links"）

## 2. 探针 API（`arch/arm64/kernel/t508n_dbg.c` + `include/asm/t508n_dbg.h`）

| 函数/宏 | 用途 |
|---|---|
| `t508n_paint_bar(x,y,w,h)` | C 侧画白块（phys_to_virt + dcache_clean_inval_poc 刷 PoC） |
| `t508n_stage(n)` | 顶部进度条第 n 格（0..9） |
| `t508n_mark_row(y,n)` | 行末画 n 个小方块 = 第 n 行标注（分辨密集行） |
| `t508n_sec_paint_xy x,y`（head.S 宏） | 汇编侧，物理地址 0xb0000000 直接写（MMU 关时） |
| `t508n_sec_paint_va_xy x,y`（head.S 宏） | 汇编侧，线性映射 0xffffff80b0000000 + dc civac（MMU 开后） |

## 3. 关键坑（务必记住）

1. **`__enable_mmu` 是 boot CPU 和二级核共用**，里面放线性映射写探针会搞挂 boot CPU
   （boot CPU 的 TTBR1 那时还是 init_pg_dir，无线性映射）。二级核专属探针要
   判断 `x2 == idmap_pg_dir`，或放到 `secondary_startup`（`bl __enable_mmu` 之后）。
2. **`.idmap.text` 每次编译地址会漂移**，验证 idmap 页表时不能硬编码 PTE index，
   要从 `__pa_symbol(__idmap_text_start)` 动态算（`>>30/>>21/>>12 & 0x1ff`）。
3. **二级核 idmap 不映射 framebuffer**（只有 .idmap.text）。要二级核开 MMU 后写屏，
   需在 `create_idmap()` 里给 idmap 额外加 fb 映射（PROT_NORMAL_NC），或走 TTBR1 线性映射。
4. **`ic ialluis`（广播全部失效）在 T770 上 hang**，`ic iallu`（非广播全部失效）会清掉
   boot CPU 自身 I-cache 导致取指异常。原厂 5.4 只用 `ic ivau`（按 VA）。→ 跳过 icache 失效。
5. **`stop_machine` 跨 cluster 竞态**（加探针能过、不加就卡）。→ 绕过。
6. **探针别太多**，否则 `.idmap.text` 超 4KB（链接报 "ID map text too big"），且屏幕难读。

## 4. 标准二分流程

```
怀疑某函数卡 → 在函数内分步加 t508n_paint_bar(8+i*36, y, 24, 10)
→ make Image → cp 到 work/mainline_Image
→ python3 tools/sprd_mkbootimg.py --kernel work/mainline_Image \
    --ramdisk initramfs.cpio --output boot_linux.img --key tools/ot5_boot_key.pem
→ fastboot flash boot_a boot_linux.img && fastboot reboot
→ 读屏：亮到第几个 = 卡在哪一步
```

## 5. 读屏约定（和用户对齐）

- 报数格式："进度条几格 + 每行亮几个 + 末尾小方块几个（行号）"。
- 每行末尾的小方块数 = 行号（`t508n_mark_row`），用于区分密集行。

## 6. 已定位的启动链（复用结论）

```
二级核: secondary_entry(EL2) → init_kernel_el(降EL1) → __cpu_setup → __enable_mmu
        → __secondary_switched → secondary_start_kernel(全通过, 进idle)   ✅
boot CPU: ... → smp_init(8核上线, 完成) → do_basic_setup → driver_init
        → memory_dev_init → device_register  ← 当前卡点 ⚠️
```

## 7. 下一步方向

- 深挖 `device_register` 卡点（kobject_add / bus_add_device / sysfs 属性）。
- 若确认为跨 cluster 同步问题 → 查 CCI/CMN 互联初始化或 errata workaround。
- 打通 USB（SPRD USB PHY + DWC3 glue + DTS）后可转 adb 观察。
