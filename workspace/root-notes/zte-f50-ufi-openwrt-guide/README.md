# ZTE F50 — UFI Tools + OpenWrt Dual-Boot Guide

[![Platform](https://img.shields.io/badge/Platform-ZTE%20F50%20(MU300)-blue)]()
[![Chipset](https://img.shields.io/badge/Chipset-Unisoc%20UMS9620-orange)]()
[![OS](https://img.shields.io/badge/OS-Android%2013%20%2B%20OpenWrt%2025.12-green)]()
[![Region](https://img.shields.io/badge/Tested-China%20(no%20VPN%20required)-red)]()

A minimal, verified pathway from a **stock ZTE F50 5G modem** to a **dual-boot UFI Tools + OpenWrt** router.

This guide focuses on **what actually works behind the Great Firewall (China)** without requiring a VPN during installation. It deliberately avoids the PC-side source-code installer, which fails when GitHub is blocked.

> **Who this is for:** F50 owners who want OpenWrt as the primary router OS, with Android + UFI Tools available as a fallback.
>
> **What this is NOT:** A post-install OpenWrt configuration guide. Once OpenWrt is running, network design (travel router / home router / gateway / mesh) is your responsibility.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Step 1 — Prepare the Device](#step-1--prepare-the-device)
- [Step 2 — Backup, Downgrade B09, Root, Unlock BL](#step-2--backup-downgrade-b09-root-unlock-bl)
- [Step 3 — Flash UFI Tools](#step-3--flash-ufi-tools)
- [Step 4 — Magisk App, SU Setup Using scrcpy](#step-4--magisk-app-su-setup-using-scrcpy)
- [Step 5 — Install OpenWrt via Magisk Module](#step-5--install-openwrt-via-magisk-module)
- [Step 6 — Switching Between Android and OpenWrt](#step-6--switching-between-android-and-openwrt)
- [Known Limitations](#known-limitations)
- [Source Links](#source-links)
- [What This Guide Deliberately Excludes](#what-this-guide-deliberately-excludes)
- [License](#license)

---

## Prerequisites

| Item | Requirement | Notes |
|---|---|---|
| Device | ZTE F50 5G modem (MU300) | Unmodified, stock firmware |
| Chipset | Unisoc UMS9620 | 8-core ARM64 |
| Storage | 32 GB microSD card | **exFAT**, inserted **before first boot** |
| PC | Windows 10/11 | With ADB + scrcpy installed |
| Network | Local Wi-Fi or USB | USB preferred for stability during flashing |

### Required Tools

- **ADB (Platform Tools)** — [developer.android.com/tools/releases/platform-tools](https://developer.android.com/tools/releases/platform-tools)
- **scrcpy** — [github.com/Genymobile/scrcpy](https://github.com/Genymobile/scrcpy)
- **ZTE F50 Toolkit** — [github.com/dikeckaan/zte-f50-toolkit](https://github.com/dikeckaan/zte-f50-toolkit)
- **UFI Tools** — [github.com/kanoqwq/UFI-TOOLS](https://github.com/kanoqwq/UFI-TOOLS)
- **mu300-linux** — [github.com/dikeckaan/mu300-linux](https://github.com/dikeckaan/mu300-linux)

---

## Step 1 — Prepare the Device

### 1.1 Insert the SD Card

Insert the **32 GB exFAT SD card** into the F50 **before** first boot. This ensures the F50 initializes the card correctly for later use.

### 1.2 Enable USB Debugging

1. Connect the F50 to your PC via USB.
2. Log in to the web admin panel: `http://192.168.0.1/`
3. Open this URL in a new tab: `http://192.168.0.1/index.html#usb_port`
4. Enable **USB debugging** from that page.

### 1.3 Verify ADB Connection

```cmd
adb devices
```

You should see your device listed. If not, check the USB cable and the driver (see Step 2.1).

---

## Step 2 — Backup, Downgrade B09, Root, Unlock BL

**All steps in this section are performed with the [dikeckaan/zte-f50-toolkit](https://github.com/dikeckaan/zte-f50-toolkit).**

> [!WARNING]
> **The first thing you should do with this toolkit is run a full backup.**
> If anything goes wrong later, the backup is your only recovery path. Do not skip it.

### 2.1 Install Drivers

From the toolkit folder, run the driver installer inside `drivers\`:

- Windows 10 → `drivers\Win10\`
- Windows 8 → `drivers\Win8\`
- Windows 7 → `drivers\Win7\`

### 2.2 Toolkit Workflow

The toolkit ships a documented, ordered sequence of scripts. **Run them in this order:**

| # | Script | Purpose | Required? |
|---|---|---|---|
| 1 | `backup_direct.bat` | Dump all partitions | ✅ **Required** |
| 2 | `flash-rom_direct.bat` | Write B09 stock ROM | ✅ **Required** |
| 3 | `flash-root_direct.bat` | Flash Magisk-patched boot | ✅ **Required** for the OpenWrt installation path in this guide |
| 4 | `unlock-bl.bat` | Patch trustos to disable verified boot | ✅ **Required** before step 5 |
| 5 | `patch-modem.bat` | Enable modem write via AT commands | ⬜ Optional (legal risk) |

> [!CAUTION]
> **Step 4 (`unlock-bl.bat`) MUST come before step 5 (`patch-modem.bat`).**
> Flashing a patched modem onto a **locked** bootloader will brick the device.

### 2.3 Connection Modes

Each script comes in **two flavours**:

**Direct USB** — `*_direct.bat`

1. Power the device **fully off**.
2. Start the script.
3. When it prints `Waiting for device...`, plug in the USB cable.

**Short-circuit** — `*_shortcircuit.bat`

Use when direct mode keeps failing.

1. Open the device and locate the debug / test points on the board.
2. Short the points.
3. Plug in USB **while still shorting**.
4. Release once the progress bar appears.

> Direct mode on F50 can be flaky — the bootrom does not always accept the kick on cold boot.
> The script's built-in retry loop lets you unplug, press a key, and retry as many times as needed.

### 2.4 What `flash-root_direct.bat` Does

Writes two files:

- `bin\f50b09_t.bin` → `trustos` partition
- `bin\magiskf50b09.img` → `boot` partition (pre-patched with Magisk)

> This only makes sense **after** the device is already running stock **B09**.
> The Magisk daemon and app are made available by this step. Open Magisk in Android and complete its setup (see Step 4).

### 2.5 What `unlock-bl.bat` Does

Flashes a patched `trustos` image that disables the verified-boot check. After the device reboots, the bootloader is unlocked.

> [!WARNING]
> **Do not attempt to re-lock the bootloader.**
> The toolkit deliberately ships no re-lock script. If you restore the original `trustos` while the modem is patched, the verified-boot chain will reject the running image and the device will hard-brick or bootloop.

### 2.6 Legal Notice for `patch-modem.bat`

> *"Modem modification changing without explicit manufacturer authorisation is a criminal offence in most jurisdictions (including the EU, UK, US, and Turkey)."* — ZTE F50 Toolkit README

The legitimate use of `patch-modem.bat` is restoring **your own original modem** after a wipe. **Skip step 5 if you don't need modem modification.**

---

## Step 3 — Flash UFI Tools

**Sources:**

- Main repo — [github.com/kanoqwq/UFI-TOOLS](https://github.com/kanoqwq/UFI-TOOLS)
- User guide — [User_Doc.md](https://github.com/kanoqwq/UFI-TOOLS/blob/http-server-version/User_Doc.md)
- Downloads — [pan.kanokano.cn/UFI-TOOLS-UPDATE](https://pan.kanokano.cn/UFI-TOOLS-UPDATE)

### 3.1 Choose the Edition

UFI Tools has two forms:

| Edition | Where installed | Best for |
|---|---|---|
| **Pocket Edition (PE)** | Phone only | Light remote management |
| **Full version** | On the F50 device itself | Full system control, plugin store, advanced features |

For a travel router, install the **Full version on the device**.

### 3.2 Install UFI Tools with the One-Click Installer

This guide's tested workflow uses **wired ADB** and the **UFI-TOOLS One-Click Installer** to deploy the Full version to the F50 itself. It does not use the PC-side source-code `install.cmd` method.

1. Connect the F50 to your Windows PC with a USB cable and open the ZTE administration page: `http://192.168.0.1/index.html#usb_port`.
2. Enable **USB debugging** in the ZTE UI.
3. Confirm the PC can see the device over wired ADB. For example, run `adb devices` from Command Prompt and approve the debugging prompt on the F50 if one appears.
4. Download the [UFI-TOOLS One-Click Installer](https://pan.kanokano.cn/UFI-TOOLS-UPDATE/UFI-TOOLS%E4%B8%80%E9%94%AE%E5%AE%89%E8%A3%85%E5%99%A8) and the corresponding installer package if it is provided separately. Run the installer and follow its prompts to complete deployment to the F50.
5. Keep the USB connection and power stable until deployment finishes. Afterward, connect a phone or PC to the F50's Wi-Fi and open the [UFI Tools dashboard](http://192.168.0.1:2333) in a browser.

For the official F50 / U30 Air installation walkthrough, see the [Bilibili video](https://www.bilibili.com/video/BV1qUHpzeEDd). For other supported connection methods and troubleshooting, see the project's [User_Doc](https://github.com/kanoqwq/UFI-TOOLS/blob/http-server-version/User_Doc.md).

### 3.3 Open the UFI Tools Dashboard

1. Connect your phone or PC to the F50's Wi-Fi network.
2. Open a browser and go to `http://192.168.0.1:2333`.
3. Sign in when prompted. UFI Tools runs on the F50; scrcpy is not needed to access its web dashboard.

The project's User_Doc recommends installing on the **device itself** for access to its backend services, automation tasks, plugin system, and system-level controls.

---

## Step 4 — Magisk App, SU Setup Using scrcpy

### 4.1 Set Up Magisk from Android

After `flash-root_direct.bat` finishes, open the F50's Android interface using scrcpy. Find and launch **Magisk** from the app list, then follow the prompts in the Magisk app to complete its installation and setup. If Magisk offers to install or update the app from its Android interface, complete that step there.

### 4.2 Launch scrcpy

```cmd
scrcpy --no-audio
```

> The `--no-audio` flag is required because the F50 has no microphone — audio capture fails without it.

### 4.3 Grant SU and Verify

1. Use the scrcpy window to interact with the Android UI.
2. Open **Magisk** and grant root permissions when prompted.
3. Verify root from the PC:

```cmd
adb shell su -c id
```

Expected output:

```
uid=0(root) gid=0(root) groups=0(root) context=u:r:magisk:s0
```

---

## Step 5 — Install OpenWrt via Magisk Module

**Sources:**

- Main repo — [github.com/dikeckaan/mu300-linux](https://github.com/dikeckaan/mu300-linux)
- Releases — [github.com/dikeckaan/mu300-linux/releases](https://github.com/dikeckaan/mu300-linux/releases)

### 5.1 Download the Correct File

From the releases page, download the **Magisk module** matching your kernel:

```
mu300-magisk-v2026.10.18-openwrt-k7.2.zip
```

> [!IMPORTANT]
> **Do NOT use the PC-side `install.cmd` source-code method.**
> It downloads from GitHub during installation, which fails behind the Great Firewall.
> The Magisk module ZIP contains everything needed.

### 5.2 Establish Network ADB

```cmd
adb tcpip 5555
adb shell ip route
```

Note the IP address (typically `192.168.0.1`). Then connect over TCP:

```cmd
adb connect 192.168.0.1:5555
adb devices
```

> If both USB and TCP show as connected, target the TCP device explicitly with `adb -s 192.168.0.1:5555 shell ...`.

### 5.3 Push the Magisk ZIP to SD Card

```cmd
adb push "C:\path\to\mu300-magisk-v2026.10.18-openwrt-k7.2.zip" /sdcard/
```

### 5.4 Install via Magisk

```cmd
adb shell su -c magisk --install-module /sdcard/mu300-magisk-v2026.10.18-openwrt-k7.2.zip
```

The installer will prompt for:

| Prompt | Recommended answer |
|---|---|
| Device detection | *(auto: F50 / MU300)* |
| Android slot / Linux slot | *(auto: slot A / slot B)* |
| Storage location | **sd** (your 32 GB exFAT card) |
| System to install | **OpenWrt** |
| OpenWrt variant | Standard LuCI **or** MU300 control panel |
| Boot Linux by default? | **yes** |
| Failed boots before Android | **3–5** (5 is the default) |
| Copy Android hotspot name/password? | **yes** (optional) |
| Mali GPU userspace | **no** (unless you need OpenCL) |
| VPN module | **no** (separate install) |
| Kernel | **7.2** (or 5.4 / 6.18) |
| update or wipe | **update** (keeps settings) |
| Root password | *(choose and remember this)* |

### 5.5 Reboot into OpenWrt

```cmd
adb reboot
```

Or from Android:

```cmd
adb shell su -c mu300-linux
```

Once OpenWrt boots, SSH into `192.168.77.1` with the root password you set.

---

## Step 6 — Switching Between Android and OpenWrt

The mu300-linux project ships `mu300-next-boot` for boot management. The official README states:

> *"Start Linux again with the Linux button of the Magisk module or `su -c mu300-linux`, then update as above. If Linux does not start any more, the boot image did not survive: run the installer again from a computer and choose `update`"*

### Commands (from OpenWrt SSH)

```sh
# Set next boot to Android (one-time)
mu300-next-boot android && reboot

# Set Linux as permanent default and lock it
mu300-next-boot linux
mu300-next-boot lock
reboot

# Check current boot state
mu300-next-boot status
```

### Recovery: Failed-Boot Fallback

If OpenWrt fails to boot, the device automatically returns to Android after **5 consecutive failed boots** (configurable via `mu300-next-boot attempts N`).

To trigger manually:

> *"Unplug the device about ten seconds after it powers on, then plug it in again, as many times in a row as the failed boots you chose."*

---

## Known Limitations

Documented in the [mu300-linux README](https://github.com/dikeckaan/mu300-linux):

- **Wi-Fi is single-band.** 5 GHz (802.11ac) **or** 2.4 GHz, one at a time — not both simultaneously. The ZTE official UI confirms this: only a toggle between bands is available.
- **Wi-Fi security/channel behavior needs verification.** An AP issue was observed with channel 36 and WPA2-PSK (`psk2`), but its cause has not been confirmed. It may relate to the security mode, changing the channel from 36 to 44, or the OpenWrt kernel. Do not treat this as evidence that WPA3 is unsupported; this point is pending further testing.
- **Kernel 6.18** hangs at boot about once in 20 boots. Falls back to Android automatically.
- **Never use OpenWrt's `sysupgrade`** or flash OpenWrt firmware images — they overwrite the device's storage. A normal `apk upgrade` is fine, **except** `kernel` and `kmod-*` packages.
- **Vendor log noise.** The WCN driver floods the kernel log with `sc2355_dump_survey` and `sprd_cfg80211_change_beacon`. Cosmetic only — cannot be fixed without recompiling the module.

---

## Source Links

| Step | Source |
|---|---|
| Toolkit (downgrade / root / unlock) | [github.com/dikeckaan/zte-f50-toolkit](https://github.com/dikeckaan/zte-f50-toolkit) |
| UFI Tools | [github.com/kanoqwq/UFI-TOOLS](https://github.com/kanoqwq/UFI-TOOLS) |
| UFI Tools — User Doc | [User_Doc.md](https://github.com/kanoqwq/UFI-TOOLS/blob/http-server-version/User_Doc.md) |
| UFI Tools — Downloads | [pan.kanokano.cn/UFI-TOOLS-UPDATE](https://pan.kanokano.cn/UFI-TOOLS-UPDATE) |
| UFI Tools — Video | [bilibili.com/video/BV1qUHpzeEDd](https://www.bilibili.com/video/BV1qUHpzeEDd) |
| mu300-linux | [github.com/dikeckaan/mu300-linux](https://github.com/dikeckaan/mu300-linux) |
| mu300-linux — Releases | [github.com/dikeckaan/mu300-linux/releases](https://github.com/dikeckaan/mu300-linux/releases) |
| mu300-linux — Video | [bilibili.com/video/BV1B4pN6zEio](https://www.bilibili.com/video/BV1B4pN6zEio) |

---

## What This Guide Deliberately Excludes

Per the scope of this repository:

- ❌ **Ubuntu installation** — adds complexity; not needed for travel router use.
- ❌ **IMEI / modem modification** — legal risk; the toolkit includes it but this guide skips it.
- ❌ **PC-side `install.cmd` source-code method** — fails behind the Great Firewall without a VPN.
- ❌ **OpenWrt post-install configuration** — travel router / home router / gateway / mesh setups are separate.

---

## Contributing

If you find an error, please open an issue with:

1. The exact step
2. The exact error message
3. Your firmware version and kernel

Pull requests are welcome, especially for:

- Translations (Chinese / Turkish)
- Screenshots
- Corrections to official links (they move)

---

## License

This guide is released under **[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)**.

All linked tools retain their own licenses:

- `zte-f50-toolkit` — see its repository
- `UFI-TOOLS` — see its repository
- `mu300-linux` — GPL where applicable

---

## Disclaimer

> [!CAUTION]
> Flashing third-party firmware, unlocking bootloaders, and modifying system partitions **can permanently brick your device** and **void your warranty**.
>
> The authors of this guide and the linked tools are **not responsible** for any damage, data loss, or legal consequences resulting from following these instructions.
>
> Proceed only if you understand the risks and can afford to lose the device.
