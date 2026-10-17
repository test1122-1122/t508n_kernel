# Binary manifest

Self-built mainline artifacts. Vendor WCN firmware is not included;
firmware is loaded from the persistent p75 filesystem at runtime.

```text
9bfb7a0e92e8cbe65ce4753bf3e800b338ed7f276e338a063a9d78b4c1728bac  boot_mainline_6.18.55_t508n_test40.img
f1c5bbefd89facfb52ec6b01728ebe73b27c04fe088563d35951bde448aed53d  Image.lk
```

boot_mainline_6.18.55_t508n_test40.img:
- mainline 6.18.55
- initramfs: DWC3 ACM first, embedded regulatory.db, WiFi/BT helper callouts
- p75 holds modules/firmware/userspace

Image.lk:
- arm64 Image wrapped for Unisoc LK
