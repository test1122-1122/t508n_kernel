# BlueZ 5.83 offline userspace

These files are OpenWrt 25.12.5 `aarch64_generic` packages and the OpenWrt
`apk` binary.  They are bundled so the T508N mainline initramfs can install
BlueZ into the persistent p75 filesystem without internet access.

Install:

```sh
/persist/userspace/bluez/install-bluez.sh
```

The packages are third-party works under their own licenses; keep their
license notices and source offers.
