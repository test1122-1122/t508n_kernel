// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal legacy usb_phy for SPRD's "sprd,usb-dummy-phy" node.
 *
 * The stock T508N DWC3 node has:
 *     usb-phy = <&dummyphy>, <&ssphy>;
 * DWC3 index 0 (USB2) resolves to dummyphy.  The vendor kernel has
 * phy-sprd-dummy.ko for it; mainline has no driver, so DWC3's
 * devm_usb_get_phy_by_phandle() defers forever and dwc3_core_init()
 * fails with -EPROBE_DEFER.  Register a no-op USB2 phy so the real
 * SS PHY can be used for index 1.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>

struct sprd_dummy_phy {
struct usb_phy phy;
struct usb_otg otg;
};

static int sprd_dummy_phy_probe(struct platform_device *pdev)
{
struct device *dev = &pdev->dev;
struct sprd_dummy_phy *d;

d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
if (!d)
return -ENOMEM;

d->phy.dev = dev;
d->phy.label = "sprd-dummy-phy";
d->phy.type = USB_PHY_TYPE_USB2;
d->phy.otg = &d->otg;
d->otg.usb_phy = &d->phy;

platform_set_drvdata(pdev, d);

if (usb_add_phy_dev(&d->phy))
return -EINVAL;

dev_info(dev, "SPRD dummy USB2 PHY registered\n");
return 0;
}

static void sprd_dummy_phy_remove(struct platform_device *pdev)
{
struct sprd_dummy_phy *d = platform_get_drvdata(pdev);

usb_remove_phy(&d->phy);
}

static const struct of_device_id sprd_dummy_phy_match[] = {
{ .compatible = "sprd,usb-dummy-phy" },
{ }
};
MODULE_DEVICE_TABLE(of, sprd_dummy_phy_match);

static struct platform_driver sprd_dummy_phy_driver = {
.probe = sprd_dummy_phy_probe,
.remove = sprd_dummy_phy_remove,
.driver = {
.name = "sprd-dummy-phy",
.of_match_table = sprd_dummy_phy_match,
},
};
module_platform_driver(sprd_dummy_phy_driver);

MODULE_DESCRIPTION("Minimal SPRD USB dummy PHY for T508N DWC3");
MODULE_LICENSE("GPL v2");
