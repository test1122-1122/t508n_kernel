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

KSYMTAB_FUNC(sprd_wlan_power_status_sync, "_gpl", "");
KSYMTAB_FUNC(get_wcn_bus_ops, "_gpl", "");
KSYMTAB_FUNC(module_bus_init, "", "");
KSYMTAB_FUNC(module_bus_deinit, "", "");
KSYMTAB_FUNC(marlin_reset_register_notify, "_gpl", "");
KSYMTAB_FUNC(marlin_reset_unregister_notify, "_gpl", "");
KSYMTAB_FUNC(mdbg_device_lock_notify, "_gpl", "");
KSYMTAB_FUNC(mdbg_device_unlock_notify, "_gpl", "");
KSYMTAB_FUNC(wcn_is_assert, "_gpl", "");
KSYMTAB_FUNC(wcn_set_powerdown_flag, "_gpl", "");
KSYMTAB_FUNC(wcn_assert_interface, "_gpl", "");
KSYMTAB_FUNC(wcn_assert_interface_async, "_gpl", "");
KSYMTAB_FUNC(mdbg_assert_interface, "_gpl", "");
KSYMTAB_FUNC(mdbg_assert_read, "_gpl", "");
KSYMTAB_FUNC(mdbg_loopcheck_read, "_gpl", "");
KSYMTAB_FUNC(mdbg_at_cmd_read, "_gpl", "");
KSYMTAB_FUNC(mdbg_send, "_gpl", "");
KSYMTAB_FUNC(wcn_write_data_to_phy_addr, "_gpl", "");
KSYMTAB_FUNC(wcn_read_data_from_phy_addr, "_gpl", "");
KSYMTAB_FUNC(wcn_set_armlog, "_gpl", "");
KSYMTAB_FUNC(__wcn_get_sw_ver, "_gpl", "");
KSYMTAB_DATA(wcn_reset_notifier_list, "_gpl", "");
KSYMTAB_FUNC(wcn_reset_cp2, "_gpl", "");
KSYMTAB_FUNC(mdbg_dump_gnss_unregister, "_gpl", "");
KSYMTAB_FUNC(wcn_get_chip_type, "_gpl", "");
KSYMTAB_FUNC(wcn_get_chip_name, "_gpl", "");
KSYMTAB_FUNC(wcn_gnss_ops_register, "_gpl", "");
KSYMTAB_FUNC(marlin_get_wcn_xpe_efuse_data, "_gpl", "");
KSYMTAB_FUNC(wcn_get_xtal_26m_clk_type, "_gpl", "");
KSYMTAB_FUNC(wcn_get_xtal_26m_clk_mode, "_gpl", "");
KSYMTAB_FUNC(marlin_chip_en, "_gpl", "");
KSYMTAB_FUNC(gnss_firmware_path_get, "_gpl", "");
KSYMTAB_FUNC(wcn_reset_pcie, "_gpl", "");
KSYMTAB_FUNC(wcn_chip_power_on, "_gpl", "");
KSYMTAB_FUNC(wcn_chip_power_off, "_gpl", "");
KSYMTAB_FUNC(open_power_ctl, "_gpl", "");
KSYMTAB_FUNC(marlin_get_power, "_gpl", "");
KSYMTAB_FUNC(marlin_get_download_status, "_gpl", "");
KSYMTAB_FUNC(marlin_set_download_status, "_gpl", "");
KSYMTAB_FUNC(wcn_get_module_status_changed, "_gpl", "");
KSYMTAB_FUNC(wcn_set_module_status_changed, "_gpl", "");
KSYMTAB_FUNC(marlin_get_module_status, "_gpl", "");
KSYMTAB_FUNC(is_first_power_on, "_gpl", "");
KSYMTAB_FUNC(cali_ini_need_download, "_gpl", "");
KSYMTAB_FUNC(marlin_set_wakeup, "_gpl", "");
KSYMTAB_FUNC(marlin_set_sleep, "_gpl", "");
KSYMTAB_FUNC(marlin_reset_reg, "_gpl", "");
KSYMTAB_FUNC(start_marlin, "_gpl", "");
KSYMTAB_FUNC(stop_marlin, "_gpl", "");
KSYMTAB_FUNC(gnss_dump_mem, "_gpl", "");
KSYMTAB_FUNC(wcn_pm_qos_enable, "_gpl", "");
KSYMTAB_FUNC(wcn_pm_qos_disable, "_gpl", "");
KSYMTAB_FUNC(wcn_pm_qos_reset, "_gpl", "");
KSYMTAB_FUNC(sdio_wait_pub_int_done, "", "");
KSYMTAB_FUNC(sdio_ap_int_cp0, "", "");
KSYMTAB_FUNC(sdio_pub_int_regcb, "", "");
KSYMTAB_FUNC(sdio_pub_int_btwf_en0, "", "");
KSYMTAB_FUNC(sdio_pub_int_gnss_en0, "", "");
KSYMTAB_FUNC(sdio_pub_int_poweron, "", "");
KSYMTAB_FUNC(sdio_pub_int_init, "", "");
KSYMTAB_FUNC(sdio_pub_int_deinit, "", "");
KSYMTAB_FUNC(slp_mgr_init, "", "");
KSYMTAB_FUNC(slp_mgr_deinit, "", "");
KSYMTAB_FUNC(slp_mgr_death, "", "");
KSYMTAB_FUNC(wcn_get_aon_chip_id, "_gpl", "");
KSYMTAB_FUNC(wcn_platform_chip_type, "_gpl", "");
KSYMTAB_FUNC(wcn_get_gnss_regmap, "_gpl", "");
KSYMTAB_FUNC(wcn_get_gnss_base_addr, "_gpl", "");
KSYMTAB_FUNC(integ_marlin_get_module_status, "_gpl", "");
KSYMTAB_FUNC(wcn_regmap_raw_write_bit, "_gpl", "");
KSYMTAB_FUNC(wcn_regmap_read, "_gpl", "");
KSYMTAB_FUNC(wcn_is_power_busy, "_gpl", "");
KSYMTAB_FUNC(wcn_debug_bus_show, "_gpl", "");
KSYMTAB_FUNC(debug_bus_show, "_gpl", "");
KSYMTAB_FUNC(module_bus_sipc_init, "", "");
KSYMTAB_FUNC(module_bus_sipc_deinit, "", "");
KSYMTAB_FUNC(sdiohal_dump_aon_reg, "_gpl", "");
KSYMTAB_FUNC(wcn_thread_setattr, "_gpl", "");
KSYMTAB_FUNC(module_bus_sdio_init, "_gpl", "");
KSYMTAB_FUNC(module_bus_sdio_deinit, "_gpl", "");

MODULE_INFO(depends, "");

