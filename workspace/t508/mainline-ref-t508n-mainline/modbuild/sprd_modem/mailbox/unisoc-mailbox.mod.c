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

KSYMTAB_FUNC(sprd_mbox_wakeup_flag_callback_register_r2, "_gpl", "");
KSYMTAB_FUNC(sprd_mbox_wakeup_flag_callback_unregister_r2, "_gpl", "");
KSYMTAB_FUNC(sprd_mbox_change_wakeup_flag_r2, "_gpl", "");

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cunisoc,mailbox");
MODULE_ALIAS("of:N*T*Cunisoc,mailboxC*");
