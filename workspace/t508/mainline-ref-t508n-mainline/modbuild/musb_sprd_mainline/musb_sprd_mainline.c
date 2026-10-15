// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal mainline SPRD QogirN6Pro MUSB + HS PHY glue for T508N.
 *
 * Modeled on the vendor 5.4 sources:
 *   drivers/usb/musb/musb_sprd.c
 *   drivers/usb/phy/phy-sprd-qogirn6pro.c
 *
 * The vendor MUSB glue relies on the HS PHY driver to power the USB2
 * controller block before MUSB core reads its registers.  Mainline has
 * no SPRD HS PHY driver, so this file performs the minimal HS PHY
 * power-on sequence itself, then registers a PIO-only "musb-hdrc".
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/musb.h>
#include <linux/usb/phy.h>

#include "musb_core.h"

#define SPRD_MUSB_MAX_EP_NUM 16
#define SPRD_MUSB_RAM_BITS 13
#define DEFAULT_DEVICE_EYE_PATTERN 0x067bd1c0

/* AON APB registers (vendor qogirn6pro-regs.h) */
#define REG_AON_APB_APB_EB1 0x0004
#define REG_AON_APB_APB_RST1 0x0010
#define REG_AON_APB_CGM_REG1 0x0138
#define REG_AON_APB_AON_SOC_USB_CTRL 0x0190
#define REG_AON_APB_OTG_PHY_TEST 0x0204
#define REG_AON_APB_MIPI_CSI_POWER_CTRL 0x0350

/* AON APB masks (vendor qogirn6pro-mask.h) */
#define MASK_AON_APB_AON_USB2_TOP_EB 0x0100
#define MASK_AON_APB_OTG_PHY_EB 0x0200
#define MASK_AON_APB_CGM_OTG_REF_EN 0x1000
#define MASK_AON_APB_CGM_DPHY_REF_EN 0x0400
#define MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L 0x0001
#define MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S 0x0002
#define MASK_AON_APB_USB20_ISO_SW_EN 0x0010
#define MASK_AON_APB_USB20_CTRL_MUX_REG 0x0008
#define MASK_AON_APB_OTG_VBUS_VALID_PHYREG 0x1000000
#define MASK_AON_APB_OTG_PHY_SOFT_RST 0x0200
#define MASK_AON_APB_OTG_UTMI_SOFT_RST 0x0100

/* analog PHY G0L registers/bits (vendor phy-sprd-qogirn6pro.c) */
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1 0x0004
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING 0x0010
#define REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY 0x001c
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_ISO_SW_EN 0x00000001
#define BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT 0x00010000

/* UMP9620 PMIC regulator registers (vendor ump9620-regulator.c) */
#define UMP9620_REGULATOR_BASE 0x2000
#define UMP9620_LDO_VDDUSB33_PD (UMP9620_REGULATOR_BASE + 0x1e8)
#define UMP9620_LDO_VDDUSB33_PD_MASK BIT(0)
#define UMP9620_LDO_VDDUSB33_VOL (UMP9620_REGULATOR_BASE + 0x1ec)
#define UMP9620_LDO_VDDUSB33_VOL_MASK GENMASK(7, 0)
#define UMP9620_VDDUSB33_3300MV_SEL 0xD2

/* PMIC charger detection / DPDM switch (vendor phy-sprd-qogirn6pro.c) */
#define CHGR_DET_FGU_CTRL 0x23a0
#define DP_DM_FS_ENB BIT(14)
#define DP_DM_BC_ENB BIT(0)

struct sprd_musb_glue {
struct device*dev;
struct clk*clk;
struct regulator*vdd;
struct regmap*hsphy_glb;
struct regmap*ana_g0;
struct regmap*pmic;
struct platform_device*musb;
bool hsphy_on;
};

/*
 * FIFO layout copied from vendor musb_sprd.c device-mode table.
 * Stock DTS enables multipoint.
 */
static const struct musb_fifo_cfg sprd_musb_device_mode_cfg[] = {
MUSB_EP_FIFO_DOUBLE(1, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(1, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(2, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(2, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(3, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(3, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(4, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(4, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(5, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(5, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(6, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(6, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(7, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(7, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(8, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(8, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(9, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(9, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(10, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(10, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(11, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(11, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(12, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(12, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(13, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(13, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(14, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(14, FIFO_RX, 512),
MUSB_EP_FIFO_DOUBLE(15, FIFO_TX, 512),
MUSB_EP_FIFO_DOUBLE(15, FIFO_RX, 512),
};

static struct musb_hdrc_config sprd_musb_hdrc_config = {
.fifo_cfg= sprd_musb_device_mode_cfg,
.fifo_cfg_size= ARRAY_SIZE(sprd_musb_device_mode_cfg),
.multipoint= true,
.dyn_fifo= true,
.num_eps= SPRD_MUSB_MAX_EP_NUM,
.ram_bits= SPRD_MUSB_RAM_BITS,
.maximum_speed= USB_SPEED_HIGH,
};

static struct regmap *sprd_get_pmic_regmap(void)
{
const char *compat[] = {
"sprd,ump962x-syscon",
"sprd,ump962x-regulator",
};
int i;

for (i = 0; i < ARRAY_SIZE(compat); i++) {
struct device_node *np;
struct platform_device *pdev;
struct regmap *rm;

np = of_find_compatible_node(NULL, NULL, compat[i]);
if (!np)
continue;
pdev = of_find_device_by_node(np);
if (!pdev) {
of_node_put(np);
continue;
}
rm = dev_get_regmap(pdev->dev.parent, NULL);
if (!rm)
rm = dev_get_regmap(&pdev->dev, NULL);
put_device(&pdev->dev);
of_node_put(np);
if (rm)
return rm;
}
return NULL;
}

static int sprd_hsphy_power_on(struct sprd_musb_glue *glue)
{
u32 reg, msk;
int ret = 0;

pr_info("MAINLINE_TEST12_HP_ENTER\n");
if (glue->pmic) {
pr_info("MAINLINE_TEST12_PMIC_FOUND\n");
regmap_update_bits(glue->pmic, UMP9620_LDO_VDDUSB33_PD,
   UMP9620_LDO_VDDUSB33_PD_MASK, 0);
regmap_update_bits(glue->pmic, UMP9620_LDO_VDDUSB33_VOL,
   UMP9620_LDO_VDDUSB33_VOL_MASK,
   UMP9620_VDDUSB33_3300MV_SEL);
regmap_update_bits(glue->pmic, CHGR_DET_FGU_CTRL,
   DP_DM_FS_ENB | DP_DM_BC_ENB,
   DP_DM_FS_ENB | DP_DM_BC_ENB);
{
u32 vpd = 0xffffffff, vvol = 0xffffffff, vdpdm = 0xffffffff;
regmap_read(glue->pmic, UMP9620_LDO_VDDUSB33_PD, &vpd);
regmap_read(glue->pmic, UMP9620_LDO_VDDUSB33_VOL, &vvol);
regmap_read(glue->pmic, CHGR_DET_FGU_CTRL, &vdpdm);
pr_info("MAINLINE_TEST13_PMIC_PD=%08x VOL=%08x DPDM=%08x\n",
vpd, vvol, vdpdm);
}
} else {
pr_warn("MAINLINE_TEST12_PMIC_MISSING\n");
}
if (!glue->hsphy_glb || !glue->ana_g0) {
pr_warn("MAINLINE_TEST9_HP_NO_REGMAP\n");
return -ENODEV;
}

pr_info("MAINLINE_TEST9_HP_VDD\n");
if (glue->vdd) {
ret = regulator_enable(glue->vdd);
if (ret)
pr_warn("MAINLINE_TEST9_HP_VDD_FAIL %d\n", ret);
}

/* select AON-SYS USB20 controller mux */
msk = MASK_AON_APB_USB20_CTRL_MUX_REG;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_AON_SOC_USB_CTRL, msk, 0);

/* AON USB2 top + OTG PHY clocks */
reg = msk = MASK_AON_APB_AON_USB2_TOP_EB | MASK_AON_APB_OTG_PHY_EB;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

reg = msk = MASK_AON_APB_CGM_OTG_REF_EN | MASK_AON_APB_CGM_DPHY_REF_EN;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

/* release USB20 power down */
msk = MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_L |
      MASK_AON_APB_C2G_ANALOG_USB20_USB20_PS_PD_S;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_MIPI_CSI_POWER_CTRL,
   msk, 0);

regmap_update_bits(glue->ana_g0, REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_PHY,
   BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_ISO_SW_EN, 0);

msk = MASK_AON_APB_USB20_ISO_SW_EN;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_AON_SOC_USB_CTRL, msk, 0);

/* force VBUS valid for peripheral mode */
reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);

reg = msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
regmap_update_bits(glue->ana_g0,
   REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,
   msk, reg);

regmap_write(glue->ana_g0, REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING,
     DEFAULT_DEVICE_EYE_PATTERN);

/* reset core: assert, wait 20-30ms, deassert */
msk = MASK_AON_APB_OTG_PHY_SOFT_RST | MASK_AON_APB_OTG_UTMI_SOFT_RST;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_APB_RST1, msk, msk);
usleep_range(20000, 30000);
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_APB_RST1, msk, 0);

glue->hsphy_on = true;
pr_info("MAINLINE_TEST9_HP_DONE\n");
dev_info(glue->dev, "SPRD HS PHY minimal power-on done\n");
return 0;
}

static int sprd_hsphy_power_off(struct sprd_musb_glue *glue)
{
u32 msk;

if (!glue->hsphy_on || !glue->hsphy_glb || !glue->ana_g0)
return 0;

msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
regmap_update_bits(glue->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
msk = BIT_ANLG_PHY_G0L_ANALOG_USB20_USB20_VBUSVLDEXT;
regmap_update_bits(glue->ana_g0,
   REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_UTMI_CTL1,
   msk, 0);

if (glue->vdd)
regulator_disable(glue->vdd);
glue->hsphy_on = false;
return 0;
}

static irqreturn_t sprd_musb_interrupt(int irq, void *hci)
{
return musb_interrupt((struct musb *)hci);
}

static int sprd_musb_init(struct musb *musb)
{
struct sprd_musb_glue *glue = dev_get_drvdata(musb->controller->parent);

musb->phy = NULL;
musb->xceiv = NULL;
musb->isr = sprd_musb_interrupt;
dev_info(glue->dev, "SPRD MUSB minimal glue init, peripheral/PIO\n");
return 0;
}

static int sprd_musb_exit(struct musb *musb)
{
return 0;
}

static void sprd_musb_enable(struct musb *musb) { }
static void sprd_musb_disable(struct musb *musb) { }
static int sprd_musb_set_mode(struct musb *musb, u8 mode) { return 0; }
static void sprd_musb_set_vbus(struct musb *musb, int on) { }
static int sprd_musb_vbus_status(struct musb *musb) { return MUSB_VBUS_VALID; }

static struct dma_controller *sprd_musb_dma_init(struct musb *musb,
 void __iomem *base)
{
return NULL; /* PIO only */
}
static void sprd_musb_dma_exit(struct dma_controller *c) { }

static const struct musb_platform_ops sprd_musb_ops = {
.quirks= 0,
.init= sprd_musb_init,
.exit= sprd_musb_exit,
.enable= sprd_musb_enable,
.disable= sprd_musb_disable,
.dma_init= sprd_musb_dma_init,
.dma_exit= sprd_musb_dma_exit,
.set_mode= sprd_musb_set_mode,
.set_vbus= sprd_musb_set_vbus,
.vbus_status= sprd_musb_vbus_status,
};

static ssize_t t508n_connect_store(struct device *dev,
   struct device_attribute *attr,
   const char *buf, size_t count)
{
struct sprd_musb_glue *glue = dev_get_drvdata(dev);
struct musb *musb;

pr_info("MAINLINE_TEST15_CONNECT_STORE\n");
if (!glue) {
pr_warn("MAINLINE_TEST15_CONNECT_NO_GLUE\n");
return count;
}

/* Re-run the HS PHY mux/power sequence after gadget bind. */
sprd_hsphy_power_on(glue);

musb = glue->musb ? platform_get_drvdata(glue->musb) : NULL;
if (musb && musb->mregs) {
u8 pwr = musb_readb(musb->mregs, MUSB_POWER);
pwr |= MUSB_POWER_SOFTCONN;
musb_writeb(musb->mregs, MUSB_POWER, pwr);
pr_info("MAINLINE_TEST15_SOFTCONN=%02x\n", pwr);
}

return count;
}
static DEVICE_ATTR_WO(t508n_connect);

static struct attribute *sprd_musb_attrs[] = {
&dev_attr_t508n_connect.attr,
NULL,
};
ATTRIBUTE_GROUPS(sprd_musb);

static int sprd_musb_probe(struct platform_device *pdev)
{
struct device_node *np = pdev->dev.of_node;
struct device_node *phy_np;
struct sprd_musb_glue *glue;
struct musb_hdrc_platform_data pdata;
struct platform_device_info pinfo;
int ret;

if (!np)
return -EINVAL;

glue = devm_kzalloc(&pdev->dev, sizeof(*glue), GFP_KERNEL);
if (!glue)
return -ENOMEM;
glue->dev = &pdev->dev;

glue->clk = devm_clk_get(&pdev->dev, "core_clk");
if (IS_ERR(glue->clk)) {
dev_err(&pdev->dev, "no core_clk: %ld\n", PTR_ERR(glue->clk));
return PTR_ERR(glue->clk);
}
ret = clk_prepare_enable(glue->clk);
if (ret) {
dev_err(&pdev->dev, "core_clk enable failed: %d\n", ret);
return ret;
}

/* optional vddusb33: may not exist in mainline regulator tree */
glue->vdd = regulator_get_optional(NULL, "vddusb33");
if (IS_ERR(glue->vdd)) {
dev_warn(&pdev->dev, "vddusb33 not available: %ld\n",
 PTR_ERR(glue->vdd));
glue->vdd = NULL;
}

phy_np = of_parse_phandle(np, "usb-phy", 0);
if (phy_np) {
glue->hsphy_glb = syscon_regmap_lookup_by_phandle(phy_np,
"sprd,syscon-enable");
if (IS_ERR(glue->hsphy_glb)) {
dev_warn(&pdev->dev, "hsphy syscon-enable missing: %ld\n",
 PTR_ERR(glue->hsphy_glb));
glue->hsphy_glb = NULL;
}
glue->ana_g0 = syscon_regmap_lookup_by_phandle(phy_np,
"sprd,syscon-anag0");
if (IS_ERR(glue->ana_g0)) {
dev_warn(&pdev->dev, "hsphy syscon-anag0 missing: %ld\n",
 PTR_ERR(glue->ana_g0));
glue->ana_g0 = NULL;
}
of_node_put(phy_np);
} else {
dev_warn(&pdev->dev, "no usb-phy phandle\n");
}

pr_info("MAINLINE_TEST9_PROBE_BEFORE_HP\n");
glue->pmic = sprd_get_pmic_regmap();
if (glue->pmic)
pr_info("MAINLINE_TEST12_PMIC_REGMAp_OK\n");
else
pr_warn("MAINLINE_TEST12_PMIC_REGMAp_MISSING\n");

ret = sprd_hsphy_power_on(glue);
pr_info("MAINLINE_TEST9_PROBE_AFTER_HP ret=%d\n", ret);

memset(&pdata, 0, sizeof(pdata));
pdata.mode = MUSB_PERIPHERAL;
pdata.config = &sprd_musb_hdrc_config;
pdata.platform_ops = &sprd_musb_ops;
pdata.min_power = 100;

memset(&pinfo, 0, sizeof(pinfo));
pinfo.name = "musb-hdrc";
pinfo.id = PLATFORM_DEVID_AUTO;
pinfo.parent = &pdev->dev;
pinfo.fwnode = of_fwnode_handle(np);
pinfo.of_node_reused = true;
pinfo.res = pdev->resource;
pinfo.num_res = pdev->num_resources;
pinfo.data = &pdata;
pinfo.size_data = sizeof(pdata);

platform_set_drvdata(pdev, glue);

glue->musb = platform_device_register_full(&pinfo);
if (IS_ERR(glue->musb)) {
ret = PTR_ERR(glue->musb);
dev_err(&pdev->dev, "failed to register musb-hdrc: %d\n", ret);
sprd_hsphy_power_off(glue);
clk_disable_unprepare(glue->clk);
return ret;
}

dev_info(&pdev->dev, "SPRD MUSB platform device registered\n");
return 0;
}

static void sprd_musb_remove(struct platform_device *pdev)
{
struct sprd_musb_glue *glue = platform_get_drvdata(pdev);

if (glue->musb)
platform_device_unregister(glue->musb);
sprd_hsphy_power_off(glue);
if (glue->clk)
clk_disable_unprepare(glue->clk);
}

static const struct of_device_id sprd_musb_of_match[] = {
{ .compatible = "sprd,qogirn6pro-musb" },
{ }
};
MODULE_DEVICE_TABLE(of, sprd_musb_of_match);

static struct platform_driver sprd_musb_driver = {
.probe = sprd_musb_probe,
.remove = sprd_musb_remove,
.driver = {
.name = "sprd-musb-mainline",
.of_match_table = sprd_musb_of_match,
.dev_groups = sprd_musb_groups,
},
};
module_platform_driver(sprd_musb_driver);

MODULE_DESCRIPTION("Minimal SPRD QogirN6Pro MUSB + HS PHY glue for T508N");
MODULE_LICENSE("GPL v2");
