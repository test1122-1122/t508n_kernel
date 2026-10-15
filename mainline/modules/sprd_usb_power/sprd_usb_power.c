// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal SPRD USB power helper for T508N mainline DWC3 bring-up.
 *
 * The UMP9620 PMIC's vddusb33 LDO and the DPDM->USB PHY switch are not
 * represented by a mainline regulator/PMIC driver.  The stock bootloader
 * may turn vddusb33 off when it jumps to Linux, which makes the DWC3 /
 * SS PHY probe fail or defer.  Enable both here before retrying DWC3.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define UMP9620_REGULATOR_BASE 0x2000
#define UMP9620_LDO_VDDUSB33_PD (UMP9620_REGULATOR_BASE + 0x1e8)
#define UMP9620_LDO_VDDUSB33_PD_MASK BIT(0)
#define UMP9620_LDO_VDDUSB33_VOL (UMP9620_REGULATOR_BASE + 0x1ec)
#define UMP9620_LDO_VDDUSB33_VOL_MASK GENMASK(7, 0)
#define UMP9620_VDDUSB33_3300MV_SEL 0xD2
#define CHGR_DET_FGU_CTRL 0x23a0
#define DP_DM_FS_ENB BIT(14)
#define DP_DM_BC_ENB BIT(0)

static struct regmap *sprd_usb_pmic(void)
{
struct device_node *np;
struct platform_device *pdev;
struct regmap *rm = NULL;

np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");
if (!np)
return NULL;
pdev = of_find_device_by_node(np);
if (pdev) {
rm = dev_get_regmap(pdev->dev.parent, NULL);
if (!rm)
rm = dev_get_regmap(&pdev->dev, NULL);
put_device(&pdev->dev);
}
of_node_put(np);
return rm;
}

static int __init sprd_usb_power_init(void)
{
struct regmap *pmic = sprd_usb_pmic();
u32 pd = 0xffffffff, vol = 0xffffffff, dpdm = 0xffffffff;

if (!pmic) {
pr_warn("SPRD_USB_POWER: PMIC regmap not found\n");
return -ENODEV;
}

regmap_update_bits(pmic, UMP9620_LDO_VDDUSB33_PD,
   UMP9620_LDO_VDDUSB33_PD_MASK, 0);
regmap_update_bits(pmic, UMP9620_LDO_VDDUSB33_VOL,
   UMP9620_LDO_VDDUSB33_VOL_MASK,
   UMP9620_VDDUSB33_3300MV_SEL);
regmap_update_bits(pmic, CHGR_DET_FGU_CTRL,
   DP_DM_FS_ENB | DP_DM_BC_ENB,
   DP_DM_FS_ENB | DP_DM_BC_ENB);

regmap_read(pmic, UMP9620_LDO_VDDUSB33_PD, &pd);
regmap_read(pmic, UMP9620_LDO_VDDUSB33_VOL, &vol);
regmap_read(pmic, CHGR_DET_FGU_CTRL, &dpdm);
pr_info("SPRD_USB_POWER: PD=%08x VOL=%08x DPDM=%08x\n", pd, vol, dpdm);
return 0;
}

static void __exit sprd_usb_power_exit(void)
{
pr_info("SPRD_USB_POWER: module exit\n");
}

module_init(sprd_usb_power_init);
module_exit(sprd_usb_power_exit);
MODULE_DESCRIPTION("Minimal UMP9620 vddusb33/DPDM helper for T508N DWC3");
MODULE_LICENSE("GPL v2");
