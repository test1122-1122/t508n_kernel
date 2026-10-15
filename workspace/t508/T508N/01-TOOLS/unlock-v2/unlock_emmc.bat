adb reboot autodloader
spd_dump fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec read_part splloader 0 1m spl.bin read_part uboot_a 0 3m uboota.bin read_part uboot_b 0 3m ubootb.bin erase_part splloader erase_part splloader_bak reset
@echo "(This is a notice, not an error.) Don't continue if you see find port failed, just close and re-run this batch"
pause
spd_dump exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec write_part uboot_a fdl2-cboot.bin write_part uboot_b fdl2-cboot.bin reset
set StorageType=emmc
set spl_bak=spl.bin
set uboot_a_bak=uboota.bin
set uboot_b_bak=ubootb.bin
chsize spl.bin
if %errorlevel% equ 1 ( set spl_bak=u-boot-spl-16k-%StorageType%-sign.bin )
chsize uboota.bin
if %errorlevel% equ 1 ( set uboot_a_bak=fdl2-sign.bin )
chsize ubootb.bin
if %errorlevel% equ 1 ( set uboot_b_bak=fdl2-sign.bin )
TIMEOUT /T 10 /NOBREAK
::unlock runs here, may need twice
set spl_unlock=spl-%StorageType%-unlock.bin
spd_dump exec_addr 0x65012f48 fdl %spl_unlock% 0x65000800
::check unlock (if get 64 zeros, locked; if 32 string + 16 hash + 16 hash, unlocked)
spd_dump exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec verbose 2 read_part miscdata 8192 64 m.bin reset
pause
::restore spl and uboot
spd_dump exec_addr 0x65012f48 fdl fdl1-dl.bin 0x65000800 fdl fdl2-dl.bin 0xb4fffe00 exec read_part boot_a 0 64m boota.img read_part boot_b 0 64m bootb.img write_part splloader %spl_bak% write_part uboot_a %uboot_a_bak% write_part uboot_b %uboot_b_bak% erase_part misc write_part misc misc-fastbootd.bin reset