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

KSYMTAB_FUNC(sprd_dump_frame_prot_info, "", "");
KSYMTAB_FUNC(sprd_cfg80211_change_iface, "", "");
KSYMTAB_FUNC(get_max_fw_tx_dscr, "", "");
KSYMTAB_FUNC(get_tdls_threshold, "", "");
KSYMTAB_FUNC(get_vo_ratio, "", "");
KSYMTAB_FUNC(get_vi_ratio, "", "");
KSYMTAB_FUNC(get_be_ratio, "", "");
KSYMTAB_FUNC(get_wmmac_ratio, "", "");
KSYMTAB_FUNC(is_tcp_ack_enabled, "", "");
KSYMTAB_FUNC(adjust_tcp_ack, "", "");
KSYMTAB_FUNC(adjust_max_fw_tx_dscr, "", "");
KSYMTAB_FUNC(sprd_alloc_work, "", "");
KSYMTAB_FUNC(sprd_queue_work, "", "");
KSYMTAB_FUNC(sprd_cancel_work, "", "");
KSYMTAB_FUNC(sprd_clean_work, "", "");
KSYMTAB_FUNC(sprd_put_vif, "", "");
KSYMTAB_FUNC(sprd_mode_to_vif, "", "");
KSYMTAB_FUNC(sprd_net_flowcontrl, "", "");
KSYMTAB_FUNC(sprd_netif_rx, "", "");
KSYMTAB_FUNC(sprd_rx_monitor_process, "", "");
KSYMTAB_FUNC(sprd_iface_probe, "", "");
KSYMTAB_FUNC(sprd_iface_remove, "", "");
KSYMTAB_FUNC(sprd_init_msg, "", "");
KSYMTAB_FUNC(sprd_deinit_msg, "", "");
KSYMTAB_FUNC(sprd_alloc_msg, "", "");
KSYMTAB_FUNC(sprd_free_msg, "", "");
KSYMTAB_FUNC(sprd_queue_msg, "", "");
KSYMTAB_FUNC(sprd_peek_msg, "", "");
KSYMTAB_FUNC(sprd_dequeue_msg, "", "");
KSYMTAB_FUNC(sprd_report_scan_done, "", "");
KSYMTAB_FUNC(sprd_report_sched_scan_done, "", "");
KSYMTAB_FUNC(sprd_report_softap, "", "");
KSYMTAB_FUNC(sprd_report_connection, "", "");
KSYMTAB_FUNC(sprd_report_disconnection, "", "");
KSYMTAB_FUNC(sprd_report_mic_failure, "", "");
KSYMTAB_FUNC(sprd_report_remain_on_channel_expired, "", "");
KSYMTAB_FUNC(sprd_report_mgmt_tx_status, "", "");
KSYMTAB_FUNC(sprd_report_mgmt, "", "");
KSYMTAB_FUNC(sprd_report_mgmt_deauth, "", "");
KSYMTAB_FUNC(sprd_report_mgmt_disassoc, "", "");
KSYMTAB_FUNC(sprd_report_cqm, "", "");
KSYMTAB_FUNC(sprd_report_tdls, "", "");

MODULE_INFO(depends, "wcn_bsp");

MODULE_ALIAS("of:N*T*Csprd,sc2332-sipc-wifi");
MODULE_ALIAS("of:N*T*Csprd,sc2332-sipc-wifiC*");
MODULE_ALIAS("of:N*T*Csprd,sc2355-pcie-wifi");
MODULE_ALIAS("of:N*T*Csprd,sc2355-pcie-wifiC*");
MODULE_ALIAS("of:N*T*Csprd,sc2355-sdio-wifi");
MODULE_ALIAS("of:N*T*Csprd,sc2355-sdio-wifiC*");
MODULE_ALIAS("of:N*T*Csprd,sc2355-sipc-wifi");
MODULE_ALIAS("of:N*T*Csprd,sc2355-sipc-wifiC*");
