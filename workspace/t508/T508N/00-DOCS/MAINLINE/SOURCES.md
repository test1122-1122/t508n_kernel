# Sources cloned / fetched

| Project | Path | Commit / ref | Size | Purpose |
|---|---|---|---|---|
| `dikeckaan/mu300-linux` | `../mu300-linux` | `aff105f` (main, shallow) | 31 MB | UMS9620 mainline 6.18.55 / 7.2 reference |
| `AgentFan0315/f50-openwrt` | `../f50-openwrt` | main tarball | 25 MB | clean OpenWrt squashfs/overlay + firewall fix |
| `yjy116/OpenWrt-CI-ZTE-F50` | `../OpenWrt-CI-ZTE-F50` | main tarball | 340 KB | OpenWrt + 7.2 BTF/eBPF assembly |
| `TheGammaSqueeze/UnisocBypass` | `t508n-srag-refs/UnisocBypass` | main tarball | 38 MB | UMS9620 magic64 packer, AVB resign references |
| `zjh812/VN300E-UMS9620-root` | `t508n-srag-refs/VN300E-UMS9620-root` | main tarball | 252 KB | UMS9620/T9100 magic64 offset derivation and docs |
| `dikeckaan/zte-f50-toolkit` | `t508n-srag-refs/zte-f50-toolkit` | main tarball | 2.1 MB | UMS9620 F50 spd_dump source, bootloader_control/set_active reference |
| `TomKing062/CVE-2022-38694_unlock_bootloader` | upstream release | UMS9620 universal package | - | standard BROM exploit, FDLs, unlock payloads |

## T508N delta artifacts

| Artifact | Purpose |
|---|---|
| `dts/ums9620-ot5.dts` | reference DTS with T508N live memory/reserved-memory |
| `DELTA.md` | T508N vs MU300 board differences |
| `DRIVER-MAP.md` | driver / module reuse and port map |
| `NEXT.md` | staged bring-up plan |
| `SOURCES.md` | this file |
| `BOOT-CHAIN.md` | UMS9620/T508N boot verification chain |
| `UBOOT-MAGIC64.md` | T508N-specific U-Boot patch offsets and packaging |
| `SLOT-CONTROL.md` | misc bootloader_control / A-B slot switching |
| `RECOVERY.md` | rescue procedures and known-good state |

## Live T508N facts

```text
model      = Spreadtrum UMS9620 SoC
compatible = sprd,ums9620
sprd,sc-id = ums9620 1000 1000
memory     = 0x80000000 + 0xbffff000
MemTotal   = 2520900 kB
WLAN       = /sys/devices/platform/sprd-marlin3/sprd-marlin3:wlan (SDIO)
USB        = /sys/devices/platform/soc/soc:aon/64a00000.usb/musb-hdrc.1.auto/gadget
GNSS       = gnss_common_ctl_all / /dev/data0_gnss
```


## Local reference copies

```text
/home/test/t508n/t508/work/srag-refs/UnisocBypass/
/home/test/t508n/t508/work/srag-refs/VN300E-UMS9620-root/
/home/test/t508n/t508/work/srag-refs/zte-f50-toolkit/
```

Windows packages and dumps:

```text
E:\Download\ums9620_tcl_t508n_magic64\        magic64 + slot switch package
E:\Download\ums9620_tcl_t508n_v3 (1)\          v3 unlock tool
E:\rootphone\t508n\savebrock\flash_all_spd_dump\   full stock Android dump
E:\rootphone\t508n\savebrock\Image\               stock Android image folder
E:\rootphone\t508n\2unlock\                       original SPL/U-Boot/boot backups
