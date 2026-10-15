# Binary manifest

These are self-built mainline artifacts.  They do not contain vendor WCN firmware;
firmware is loaded at runtime from the persistent p75 filesystem.

```text
234d57947e9a606cc03faa73fc7c3fdd  boot_mainline_6.18.55_t508n_test40.img
a178351e4be3a986daf9aeb327c01356  Image.lk
```

boot_mainline_6.18.55_t508n_test40.img:
- kernel: mainline 6.18.55
- initramfs: DWC3 ACM first, embedded regulatory.db, wifi-up/bluetooth scripts
- rootfs: p75 ext4 (modules/firmware/userspace live there)

Image.lk:
- mainline arm64 Image wrapped for Unisoc LK (text_offset workaround)
