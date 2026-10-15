// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal SPRD QogirN6Pro PAMU3 driver for T508N DWC3 bring-up.
 *
 * The stock 5.4 pamu3 driver gates the USB3/PAMU3 clock and releases
 * MASK_IPA_APB_PAM_U3_SOFT_RST before DWC3 core initialization.  Mainline
 * has no pamu3 driver; without it the IPA power domain sync_state() stays
 * pending and dwc3_core_init() fails.
 */

#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct sprd_pamu3 {
struct clk *clk;
struct regmap *reset_regmap;
u32 reset_offset;
u32 reset_mask;
};

static int sprd_pamu3_probe(struct platform_device *pdev)
{
struct device *dev = &pdev->dev;
struct sprd_pamu3 *pamu;
u32 args[2];
int ret;

pamu = devm_kzalloc(dev, sizeof(*pamu), GFP_KERNEL);
if (!pamu)
return -ENOMEM;

pamu->clk = devm_clk_get(dev, "pamu3_clk");
if (IS_ERR(pamu->clk))
return dev_err_probe(dev, PTR_ERR(pamu->clk),
     "failed to get pamu3_clk\n");

ret = clk_prepare_enable(pamu->clk);
if (ret)
return dev_err_probe(dev, ret, "failed to enable pamu3_clk\n");

pamu->reset_regmap = syscon_regmap_lookup_by_phandle_args(dev->of_node,
  "syscons", 2, args);
if (IS_ERR(pamu->reset_regmap)) {
dev_warn(dev, "no syscons reset regmap: %ld\n",
 PTR_ERR(pamu->reset_regmap));
pamu->reset_regmap = NULL;
} else {
pamu->reset_offset = args[0];
pamu->reset_mask = args[1];
/* Deassert PAMU3 soft reset (vendor writes 0). */
regmap_update_bits(pamu->reset_regmap, pamu->reset_offset,
   pamu->reset_mask, 0);
dev_info(dev, "PAMU3 reset released offset=0x%x mask=0x%x\n",
 pamu->reset_offset, pamu->reset_mask);
}

platform_set_drvdata(pdev, pamu);
dev_info(dev, "SPRD PAMU3 minimal init done\n");
return 0;
}

static void sprd_pamu3_remove(struct platform_device *pdev)
{
struct sprd_pamu3 *pamu = platform_get_drvdata(pdev);

if (pamu->reset_regmap)
regmap_update_bits(pamu->reset_regmap, pamu->reset_offset,
   pamu->reset_mask, pamu->reset_mask);
if (pamu->clk)
clk_disable_unprepare(pamu->clk);
}

static const struct of_device_id sprd_pamu3_match[] = {
{ .compatible = "sprd,qogirn6pro-pamu3" },
{ }
};
MODULE_DEVICE_TABLE(of, sprd_pamu3_match);

static struct platform_driver sprd_pamu3_driver = {
.probe = sprd_pamu3_probe,
.remove = sprd_pamu3_remove,
.driver = {
.name = "sprd-pamu3-min",
.of_match_table = sprd_pamu3_match,
},
};
module_platform_driver(sprd_pamu3_driver);

MODULE_DESCRIPTION("Minimal SPRD QogirN6Pro PAMU3 for T508N DWC3");
MODULE_LICENSE("GPL v2");
