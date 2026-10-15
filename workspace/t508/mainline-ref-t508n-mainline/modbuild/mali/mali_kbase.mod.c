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

KSYMTAB_FUNC(kbasep_debug_assert_call_hook, "", "");
KSYMTAB_FUNC(kbase_find_device, "", "");
KSYMTAB_FUNC(kbase_release_device, "", "");
KSYMTAB_FUNC(kbase_context_debugfs_init, "", "");
KSYMTAB_FUNC(kbase_context_debugfs_term, "", "");
KSYMTAB_FUNC(kbase_create_context, "", "");
KSYMTAB_FUNC(kbase_destroy_context, "", "");
KSYMTAB_FUNC(kbase_instr_hwcnt_request_dump, "", "");
KSYMTAB_FUNC(kbase_instr_hwcnt_dump_complete, "", "");
KSYMTAB_FUNC(kbase_instr_hwcnt_clear, "", "");
KSYMTAB_FUNC(kbase_instr_hwcnt_on_unrecoverable_error, "", "");
KSYMTAB_FUNC(kbase_instr_hwcnt_on_before_reset, "", "");

MODULE_INFO(depends, "trusty");

MODULE_ALIAS("of:N*T*Csprd,mali-natt");
MODULE_ALIAS("of:N*T*Csprd,mali-nattC*");

MODULE_INFO(srcversion, "BF711C0C743121473C7B96A");
