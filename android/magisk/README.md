# Magisk module: switch to Linux from Android

A one-tap way to start the Linux system from Android. It replaces having a computer plugged in just to change the
boot slot. There are two ways to get it:

* **The release's installer zips** (`mu300-magisk-<tag>-<system>-k<kernel>.zip`, see "Installing from Android with a
  Magisk zip" in the [main README](../../README.md)) install Linux and then stay on the device as this module. After
  the install it is called "MU300 Linux" instead of "MU300 Linux switch"; the module id is the same
  (`mu300_linux_switch`), so the two replace each other. The Action button, `mu300-linux` and everything below work
  the same. Their files come from this directory (`installer/` is the installation part, `mu300-linux-switch/` is
  this module), assembled by `tools/make-magisk-zips.sh`.
* **The switch-only module**, for a device where Linux was installed from a computer (`./install.sh`). It installs
  nothing and only changes the boot slot:

```sh
android/magisk/build.sh          # builds mu300-linux-switch.zip
```

Install the zip in the Magisk app (Modules → Install from storage), or from a computer:

```sh
adb push android/magisk/mu300-linux-switch.zip /data/local/tmp/
adb shell "su -c 'magisk --install-module /data/local/tmp/mu300-linux-switch.zip'"
adb reboot                      # Magisk applies a new module on the next boot
```

## Using it

* **Magisk app → Modules → MU300 Linux switch → Action.** The device arms the slot Android is not on and reboots
  into Linux.
* **From a terminal or adb:**

```sh
su -c mu300-linux               # switch to Linux now
su -c 'mu300-linux status'      # what is on each slot; changes nothing
su -c 'mu300-linux --dry-run'   # print the block it would write, write nothing
```

Going back needs nothing: Linux restores Android's block during its own boot, so the next reboot returns to
Android. From inside Linux, `mu300-next-boot linux` arms another Linux boot and `mu300-os <name>` picks which
system starts.

## What it writes

Exactly 32 bytes: the AOSP `bootloader_control` block at offset `0x800` of `misc`, with the slot Android is not on
at the highest priority and `tries_remaining = 2` (LK treats 1 as an already failed boot), Android's slot still
bootable and marked successful. `boot_a`, `boot_b`, the GPT, `userdata` and every other partition are untouched, so
the worst case of a Linux that does not boot is that LK falls back to Android by itself.

The block is not a canned blob: the module reads the live one, changes the slot suffix and the two metadata bytes,
and recomputes the CRC32 (zlib, over the first 28 bytes) in `awk`, so it stays correct on a device whose block
differs. It refuses to do anything when the magic is not `BCAB`, when Android's slot is unknown, or when the other
boot partition is missing, is not a boot image, or is just a copy of Android's (no Linux installed).

## Checked on hardware

ZTE F50 / MU300, Magisk 30.7, Android 13: `status` and `--dry-run` produce exactly the trial block that
`boot/flash-trial.sh` computes on a computer, and the Action/`mu300-linux` path rebooted the device into Linux.
