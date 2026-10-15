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

KSYMTAB_FUNC(sipa_get_ep_info, "", "");
KSYMTAB_FUNC(sipa_get_ep_status, "", "");
KSYMTAB_FUNC(sipa_check_endpoint_complete, "", "");
KSYMTAB_FUNC(sipa_pam_connect, "", "");
KSYMTAB_FUNC(sipa_ext_open_pcie, "", "");
KSYMTAB_FUNC(sipa_pam_init_free_fifo, "", "");
KSYMTAB_FUNC(sipa_disconnect, "", "");
KSYMTAB_FUNC(sipa_get_ctrl_pointer, "", "");
KSYMTAB_FUNC(sipa_set_enabled, "", "");
KSYMTAB_FUNC(sipa_udp_is_frag, "", "");
KSYMTAB_FUNC(sipa_udp_is_port, "", "");
KSYMTAB_FUNC(sipa_hal_set_enabled, "", "");
KSYMTAB_FUNC(sipa_hal_get_rx_node_wptr, "", "");
KSYMTAB_FUNC(sipa_hal_set_hash_sync_req, "", "");
KSYMTAB_FUNC(sipa_prepare_modem_power_off, "", "");
KSYMTAB_FUNC(sipa_prepare_modem_power_on, "", "");
KSYMTAB_FUNC(sipa_swap_hash_table, "", "");
KSYMTAB_FUNC(sipa_config_ifilter, "", "");
KSYMTAB_FUNC(sipa_config_ofilter, "", "");
KSYMTAB_FUNC(sipa_hal_set_map_flow_ctl_to_wifi, "", "");
KSYMTAB_FUNC(sipa_hal_set_wifi_ul_map0_int_sel, "", "");
KSYMTAB_FUNC(sipa_hal_cmn_fifo_non_stop_on_flowctl, "", "");
KSYMTAB_FUNC(sipa_hal_cmn_fifo_flowctl_recover, "", "");
KSYMTAB_FUNC(sipa_hal_check_cmn_fifo_flowctl, "", "");
KSYMTAB_FUNC(sipa_hal_check_cmn_fifo_enter_flowctl, "", "");
KSYMTAB_FUNC(sipa_hal_check_cmn_fifo_exit_flowctl, "", "");
KSYMTAB_FUNC(sipa_hal_clr_cmn_fifo_flowctl_interrupt, "", "");
KSYMTAB_FUNC(sipa_hal_clr_cfifo_flowctl_enter_inter, "", "");
KSYMTAB_FUNC(sipa_hal_clr_cfifo_flowctl_exit_inter, "", "");
KSYMTAB_FUNC(sipa_nic_notify_evt, "", "");
KSYMTAB_FUNC(sipa_nic_open, "", "");
KSYMTAB_FUNC(sipa_nic_close, "", "");
KSYMTAB_FUNC(sipa_nic_tx, "", "");
KSYMTAB_FUNC(sipa_nic_rx, "", "");
KSYMTAB_FUNC(sipa_nic_check_flow_ctrl, "", "");
KSYMTAB_FUNC(sipa_nic_trigger_flow_ctrl_work, "", "");
KSYMTAB_FUNC(sipa_nic_check_suspend_condition, "", "");
KSYMTAB_FUNC(sipa_nic_check_recv_queue_empty, "", "");
KSYMTAB_FUNC(sipa_nic_restore_cmn_fifo_irq, "", "");
KSYMTAB_FUNC(sipa_nic_sync_recv_pkts, "", "");
KSYMTAB_FUNC(sipa_nic_add_tx_fifo_rptr, "", "");
KSYMTAB_FUNC(sipa_nic_update_res, "", "");
KSYMTAB_FUNC(sipa_reinit_recv_array, "", "");
KSYMTAB_FUNC(sipa_recv_wake_up, "_gpl", "");
KSYMTAB_FUNC(sipa_init_free_fifo, "_gpl", "");
KSYMTAB_FUNC(sipa_create_skb_receiver, "_gpl", "");
KSYMTAB_FUNC(sipa_find_sent_skb, "", "");
KSYMTAB_FUNC(sipa_rm_is_initialized, "", "");
KSYMTAB_FUNC(sipa_rm_create_resource, "", "");
KSYMTAB_FUNC(sipa_rm_delete_resource, "", "");
KSYMTAB_FUNC(sipa_rm_register, "", "");
KSYMTAB_FUNC(sipa_rm_deregister, "", "");
KSYMTAB_FUNC(sipa_rm_add_dependency, "", "");
KSYMTAB_FUNC(sipa_rm_add_dependency_sync, "", "");
KSYMTAB_FUNC(sipa_rm_delete_dependency, "", "");
KSYMTAB_FUNC(sipa_rm_check_dependency, "", "");
KSYMTAB_FUNC(sipa_rm_request_resource, "", "");
KSYMTAB_FUNC(sipa_rm_release_resource, "", "");
KSYMTAB_FUNC(sipa_rm_notify_completion, "", "");
KSYMTAB_FUNC(sipa_rm_inactivity_timer_init, "", "");
KSYMTAB_FUNC(sipa_rm_inactivity_timer_destroy, "", "");
KSYMTAB_FUNC(sipa_rm_inactivity_timer_request_resource, "", "");
KSYMTAB_FUNC(sipa_rm_inactivity_timer_release_resource, "", "");
KSYMTAB_FUNC(sipa_rm_usb_cons_init, "", "");
KSYMTAB_FUNC(sipa_rm_usb_cons_deinit, "", "");
KSYMTAB_FUNC(sipa_rm_set_usb_eth_up, "", "");
KSYMTAB_FUNC(sipa_rm_set_usb_eth_down, "", "");
KSYMTAB_FUNC(sipa_rm_enable_usb_tether, "", "");
KSYMTAB_FUNC(sipa_dummy_register_wifi_recv_handler, "", "");
KSYMTAB_FUNC(sipa_dummy_unregister_wifi_recv_handler, "", "");
KSYMTAB_FUNC(sipa_dummy_set_rps_mode, "", "");
KSYMTAB_FUNC(sipa_dummy_set_rps_cpus, "", "");
KSYMTAB_FUNC(sipa_dummy_recv_trigger, "", "");
KSYMTAB_FUNC(sipa_dummy_init, "_gpl", "");
KSYMTAB_FUNC(sipa_dummy_exit, "_gpl", "");

MODULE_INFO(depends, "");

