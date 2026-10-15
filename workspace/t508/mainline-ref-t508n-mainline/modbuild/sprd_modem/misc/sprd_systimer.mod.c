#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
	.arch = MODULE_ARCH_INIT,
};

KSYMTAB_FUNC(get_convert_para, "", "");
KSYMTAB_FUNC(sprd_systimer_to_boottime, "", "");
KSYMTAB_FUNC(sprd_systimer_read, "", "");
KSYMTAB_FUNC(sprd_sysfrt_read, "", "");

MODULE_INFO(depends, "");

