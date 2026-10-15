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

KSYMTAB_FUNC(sprd_mpm_create, "_gpl", "");
KSYMTAB_FUNC(sprd_mpm_disable_later_idle_for_sleep, "_gpl", "");
KSYMTAB_FUNC(sprd_mpm_init_resource_ops, "_gpl", "");
KSYMTAB_FUNC(sprd_mpm_destroy, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_create, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_destroy, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_request_resource, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_release_resource, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_request_wakelock, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_release_wakelock, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_request_wakelock_period, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_release_wakelock_later, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_power_up, "_gpl", "");
KSYMTAB_FUNC(sprd_pms_power_down, "_gpl", "");

MODULE_INFO(depends, "");

