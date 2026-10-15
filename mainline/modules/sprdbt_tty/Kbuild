obj-m += sprdbt_tty.o
sprdbt_tty-y := tty.o lpm.o rfkill.o dump.o alignment/sitm.o
ccflags-y += -I$(src) -I$(src)/alignment
ccflags-y += -I$(src)/../wcn_bsp -I$(src)/../wcn_bsp/include -I$(src)/../wcn_bsp/kinclude
ccflags-y += -I$(src)/../wcn_bsp/sdio
ccflags-y += -DDEBUG
