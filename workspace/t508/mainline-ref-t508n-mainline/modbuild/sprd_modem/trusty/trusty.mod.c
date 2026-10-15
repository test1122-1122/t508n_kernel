#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

KSYMTAB_FUNC(trusty_fast_call32_power, "", "");
KSYMTAB_FUNC(trusty_fast_call32, "", "");
KSYMTAB_FUNC(trusty_std_call32, "", "");
KSYMTAB_FUNC(trusty_call_notifier_register, "", "");
KSYMTAB_FUNC(trusty_call_notifier_unregister, "", "");
KSYMTAB_FUNC(trusty_version_str_get, "", "");
KSYMTAB_FUNC(trusty_get_api_version, "", "");
KSYMTAB_FUNC(trusty_enqueue_nop, "", "");
KSYMTAB_FUNC(trusty_dequeue_nop, "", "");
KSYMTAB_FUNC(trusty_call32_mem_buf, "", "");

MODULE_INFO(depends, "");

