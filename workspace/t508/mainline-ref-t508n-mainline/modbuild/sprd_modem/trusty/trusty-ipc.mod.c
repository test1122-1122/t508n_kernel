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

KSYMTAB_FUNC(tipc_create_channel, "", "");
KSYMTAB_FUNC(tipc_chan_get_rxbuf, "", "");
KSYMTAB_FUNC(tipc_chan_put_rxbuf, "", "");
KSYMTAB_FUNC(tipc_chan_get_txbuf_timeout, "", "");
KSYMTAB_FUNC(tipc_chan_put_txbuf, "", "");
KSYMTAB_FUNC(tipc_chan_queue_msg, "", "");
KSYMTAB_FUNC(tipc_chan_queue_msg_list, "", "");
KSYMTAB_FUNC(tipc_chan_connect, "", "");
KSYMTAB_FUNC(tipc_chan_shutdown, "", "");
KSYMTAB_FUNC(tipc_chan_destroy, "", "");

MODULE_INFO(depends, "");

