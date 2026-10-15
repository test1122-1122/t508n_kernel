# v2 工具（对比样本，勿用于 BROM exploit）

本目录只保留对照样本，用于说明 v2 与 v3 的差异：

| 文件 | 大小 | 说明 |
|---|---:|---|
| `spd_dump.exe` | 36352 B | v2 老版 spd_dump |
| `unlock_emmc.bat` | 1587 B | **第 2 行 `spd_dump fdl fdl1-dl.bin ...` 缺少 `exec_addr 0x65012f48`** |
| `misc-fastbootd.bin` | 84 B | 解锁后写 misc 用 |

对照 v3：`spd_dump.exe` 40448 B，`unlock_emmc.bat` 第一条命令带 `exec_addr`。

结论（已由文件字节核实）：BROM exploit 请使用 v3 / magic64 包（`01-TOOLS/unlock-v3`、`01-TOOLS/magic64`）
或最新 `srag-v6` 的 `spd_dump.exe`（见 `01-TOOLS/spd-dump-versions/`）。
