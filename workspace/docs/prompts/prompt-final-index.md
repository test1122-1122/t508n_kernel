# 任务：整理 T508N bring-up 全部成果与文档（最终综合索引）

## 背景
T508N（Unisoc T770/UMS9620，原厂 5.4.210 fork）mainline 6.12.103 bring-up 已告一段落
（用户决定暂停继续修改）。需要**整理全部成果和文档**，生成一份**自包含的综合索引**，
供未来会话/交接/继续调试使用。

## 材料（全部读取，提取要点）
### 主目录 /home/test/t508n/
- AGENTS-GUIDE.md、KERNEL-适配方案.md、REPORT-T508N-启动流程分析.md、
  REPORT-UMS9620-mainline适配.md、REPORT-T508N-mainline-bringup.md、
  SKILL-T508N-framebuffer-debug.md、WORK-适配材料清单.md、无串口验证指南.md、
  UNISOC-DETAILED.md（新建的紫光详记）
- prompt-*.md（各任务书，可略读标题即可）

### work/ 目录（分析/反汇编/补丁）
- SUMMARY-ALL-ANALYSIS.md（汇总，先读）
- REPORT-*.md（约 20 份：stock 分析、锁、同步、completion、唤醒链、LSE、alt 表、
  跨簇可见性、page-alloc、zone-memory、driver-init、wakeup-nohz、of-core-init、
  memory-dev-init、AGENTS-GUIDE-analysis、54-custom-build、UNISOC-ONLY-CONFIGS、
  UNISOC-MODS-ON-612、kick-patch-clean-verify）
- DISASM-A~F-*.md（6 路反编译）
- PATCH-*.md（nohz-kick、mem-sysfs-order、diag-sgi-kfence）
- UNISOC-MODS-AND-CONFIG.md、UNISOC-DETAILED.md（紫光魔改/config 详记）
- stock_kallsyms.map（符号表）、kernel.config、stock_ums9620.dts、stock-5.4.210/、
  linux-5.4.210/、linux-5.4-custom/（复刻树）、linux-6.12-clean/（干净移植树）、
  diff-612-unisoc-mods.patch、build-*.log

### 代码/产物
- /home/test/t508n/linux-6.12.103（魔改树，git HEAD cb8e725 + 未提交改动）
- /home/test/t508n/boot_linux.img（当前 6.12 镜像）、boot_54custom*.img（复刻 5.4 镜像）
- /home/test/t508n/tools/（刷机/打包工具）、initramfs_root/（解包的 initramfs）

## 任务：生成最终综合索引

输出**单文件**：/home/test/t508n/FINAL-BRINGUP-INDEX.md（自包含，未来会话只读这一份+按需查原文）

结构：
1. **项目一句话 + 目标**（mainline 6.12 boot 到 initramfs /init；设备 T508N/T770/3GB）
2. **里程碑时间线**（从最初到现在的关键节点：卡点→修复→进展）
3. **已修复问题清单**（LSE static_key/NOMAP/phandle_cache/DEVTMPFS/config 等 + 效果）
4. **复刻原厂 5.4 的成功**（linux-5.4-custom 编译 + boot 到 initramfs（exec /init y=460 亮）——
   **证明原厂式路径在 T770 可行**）
5. **当前状态**（6.12 魔改树配置、卡点：SM#2 稳定 9 格 + y=130 不亮 = 8 核 bringup 后卡、
   尝试过且回退的方案）
6. **文档索引表**（路径 → 内容概要 → 关键结论，全部文档）
7. **关键结论汇总**（已确认事实/已排除假设/未解决问题——从 SUMMARY 提取）
8. **紫光魔改/config 速查**（指向 UNISOC-DETAILED.md，简述）
9. **材料/工具速查**（镜像/符号表/工具链/刷机命令）
10. **未来方向建议**（未验证项：SGI 计数诊断、KFENCE、死循环门、5.4 稳定性多次验证、
    kick_ilb 单变量重试等）

## 注意
- 只写 FINAL-BRINGUP-INDEX.md 一个文件
- 内容要准确（基于文档实际内容，不新增分析）
- 文档索引表要覆盖所有 .md（主目录 + work/）
