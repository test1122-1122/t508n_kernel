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

KSYMTAB_FUNC(sipx_get_ack_blk_len, "_gpl", "");
KSYMTAB_FUNC(sipx_get, "_gpl", "");
KSYMTAB_FUNC(sipx_put, "_gpl", "");
KSYMTAB_FUNC(sipx_flush, "_gpl", "");
KSYMTAB_FUNC(sipx_send, "_gpl", "");
KSYMTAB_FUNC(sipx_receive, "_gpl", "");
KSYMTAB_FUNC(sipx_release, "_gpl", "");
KSYMTAB_FUNC(sipx_get_arrived_count, "_gpl", "");
KSYMTAB_FUNC(sipx_get_free_count, "_gpl", "");
KSYMTAB_FUNC(sipx_chan_destroy, "_gpl", "");
KSYMTAB_FUNC(sipx_chan_register_notifier, "_gpl", "");
KSYMTAB_FUNC(sipx_chan_create, "_gpl", "");

MODULE_INFO(depends, "sipc-core");

