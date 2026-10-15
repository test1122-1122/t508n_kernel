# Flashing

Tested image:

```text
mainline/images/boot_mainline_6.18.55_t508n_test40.img
```

Fastboot:

```sh
fastboot flash boot_b boot_mainline_6.18.55_t508n_test40.img
fastboot flash misc  ../misc/misc_slot_b.img
fastboot reboot
```

SPD (BROM): use `19-刷B槽-test40.bat` with the `spd_dump.exe` toolchain.
