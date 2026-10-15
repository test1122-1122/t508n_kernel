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

KSYMTAB_FUNC(sipc_debug_putline, "_gpl", "");
KSYMTAB_DATA(smsg_ipcs, "_gpl", "");
KSYMTAB_FUNC(smsg_callback_register, "_gpl", "");
KSYMTAB_FUNC(smsg_callback_unregister, "_gpl", "");
KSYMTAB_FUNC(smsg_msg_process, "", "");
KSYMTAB_FUNC(sipc_get_wakeup_flag, "_gpl", "");
KSYMTAB_FUNC(sipc_set_wakeup_flag, "_gpl", "");
KSYMTAB_FUNC(sipc_clear_wakeup_flag, "_gpl", "");
KSYMTAB_FUNC(smsg_init_wakeup, "_gpl", "");
KSYMTAB_FUNC(smsg_remove_wakeup, "_gpl", "");
KSYMTAB_FUNC(smsg_ch_wake_unlock, "_gpl", "");
KSYMTAB_FUNC(smsg_ch_open, "_gpl", "");
KSYMTAB_FUNC(smsg_ch_close, "_gpl", "");
KSYMTAB_FUNC(smsg_senddie, "_gpl", "");
KSYMTAB_FUNC(smsg_send, "_gpl", "");
KSYMTAB_FUNC(smsg_recv, "_gpl", "");
KSYMTAB_FUNC(sipc_channel2index, "_gpl", "");
KSYMTAB_FUNC(smsg_init_debug, "_gpl", "");
KSYMTAB_FUNC(smsg_destroy_debug, "_gpl", "");
KSYMTAB_FUNC(smem_alloc_ex, "_gpl", "");
KSYMTAB_FUNC(smem_free_ex, "_gpl", "");
KSYMTAB_FUNC(shmem_ram_vmap_nocache_ex, "_gpl", "");
KSYMTAB_FUNC(shmem_ram_vmap_cache_ex, "_gpl", "");
KSYMTAB_FUNC(shmem_ram_unmap_ex, "_gpl", "");
KSYMTAB_FUNC(modem_ram_vmap_ex, "_gpl", "");
KSYMTAB_FUNC(modem_ram_vmap_nocache, "_gpl", "");
KSYMTAB_FUNC(modem_ram_vmap_cache, "_gpl", "");
KSYMTAB_FUNC(modem_ram_unmap, "_gpl", "");
KSYMTAB_FUNC(smem_init_debug, "_gpl", "");
KSYMTAB_FUNC(smem_destroy_debug, "_gpl", "");
KSYMTAB_FUNC(sbuf_create_ex, "_gpl", "");
KSYMTAB_FUNC(sbuf_set_no_need_wake_lock, "_gpl", "");
KSYMTAB_FUNC(sbuf_destroy, "_gpl", "");
KSYMTAB_FUNC(sbuf_write, "_gpl", "");
KSYMTAB_FUNC(sbuf_read, "_gpl", "");
KSYMTAB_FUNC(sbuf_poll_wait, "_gpl", "");
KSYMTAB_FUNC(sbuf_status, "_gpl", "");
KSYMTAB_FUNC(sbuf_register_notifier, "_gpl", "");
KSYMTAB_FUNC(sbuf_register_notifier_ex, "_gpl", "");
KSYMTAB_FUNC(sbuf_get_status, "_gpl", "");
KSYMTAB_FUNC(sbuf_init_debug, "_gpl", "");
KSYMTAB_FUNC(sbuf_destroy_debug, "_gpl", "");
KSYMTAB_DATA(sb_prepare_list_info, "_gpl", "");
KSYMTAB_DATA(slog_dst2index, "_gpl", "");
KSYMTAB_FUNC(sblock_put, "_gpl", "");
KSYMTAB_FUNC(sblock_register_slog_clean_sendlist, "_gpl", "");
KSYMTAB_FUNC(sblock_unregister_slog_clean_sendlist, "_gpl", "");
KSYMTAB_FUNC(sblock_has_data, "_gpl", "");
KSYMTAB_FUNC(sblock_create_ex, "_gpl", "");
KSYMTAB_FUNC(sblock_create, "_gpl", "");
KSYMTAB_FUNC(sblock_pcfg_create, "_gpl", "");
KSYMTAB_FUNC(sblock_destroy, "_gpl", "");
KSYMTAB_FUNC(sblock_pcfg_open, "_gpl", "");
KSYMTAB_FUNC(sblock_close, "_gpl", "");
KSYMTAB_FUNC(sblock_register_notifier, "_gpl", "");
KSYMTAB_FUNC(sblock_register_notifier_ex, "_gpl", "");
KSYMTAB_FUNC(sblock_get_smem_cp_addr, "_gpl", "");
KSYMTAB_FUNC(sblock_get, "_gpl", "");
KSYMTAB_FUNC(sblock_send, "_gpl", "");
KSYMTAB_FUNC(sblock_send_prepare, "_gpl", "");
KSYMTAB_FUNC(sblock_send_finish, "_gpl", "");
KSYMTAB_FUNC(sblock_receive, "_gpl", "");
KSYMTAB_FUNC(sblock_receive_loop, "_gpl", "");
KSYMTAB_FUNC(sblock_get_arrived_count, "_gpl", "");
KSYMTAB_FUNC(sblock_mgr_get_addr, "_gpl", "");
KSYMTAB_FUNC(sblock_get_free_count, "_gpl", "");
KSYMTAB_FUNC(sblock_release, "_gpl", "");
KSYMTAB_FUNC(sblock_release_loop, "_gpl", "");
KSYMTAB_FUNC(sblock_poll_wait, "_gpl", "");
KSYMTAB_FUNC(sblock_query, "_gpl", "");
KSYMTAB_FUNC(sblock_init_debug, "_gpl", "");
KSYMTAB_FUNC(sblock_destroy_debug, "_gpl", "");
KSYMTAB_FUNC(sprd_add_action_ex, "_gpl", "");
KSYMTAB_FUNC(sprd_create_action_queue, "_gpl", "");
KSYMTAB_FUNC(sprd_destroy_action_queue, "_gpl", "");

MODULE_INFO(depends, "unisoc-mailbox,sprd_power_manager");

