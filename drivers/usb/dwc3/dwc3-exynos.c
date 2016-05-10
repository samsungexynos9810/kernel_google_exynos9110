/**
 * dwc3-exynos.c - Samsung EXYNOS DWC3 Specific Glue layer
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/usb/otg.h>
#include <linux/usb/usb_phy_generic.h>
#include <linux/usb/samsung_usb.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include <linux/io.h>
#include <linux/usb/otg-fsm.h>

/* -------------------------------------------------------------------------- */

struct dwc3_exynos_rsw {
	struct otg_fsm		*fsm;
	struct work_struct	work;
};

struct dwc3_exynos_drvdata {
	int cpu_type;
	int ip_type;
};

struct dwc3_exynos {
	struct platform_device	*usb2_phy;
	struct platform_device	*usb3_phy;
	struct device		*dev;

	struct clk		*clk;
	struct clk		*susp_clk;
	struct clk		*axius_clk;

	struct regulator	*vdd33;
	struct regulator	*vdd10;

	struct dwc3_exynos_rsw	rsw;
	const struct dwc3_exynos_drvdata *drv_data;
};

void dwc3_otg_run_sm(struct otg_fsm *fsm);

/* -------------------------------------------------------------------------- */

static struct dwc3_exynos_drvdata dwc3_exynos5250 = {
	.cpu_type	= TYPE_EXYNOS5250,
};

static struct dwc3_exynos_drvdata dwc3_exynos7420 = {
	.cpu_type	= TYPE_EXYNOS7420,
};

static struct dwc3_exynos_drvdata dwc3_exynos8890 = {
	.cpu_type	= TYPE_EXYNOS8890,
	.ip_type	= TYPE_USB3DRD,
};

static struct dwc3_exynos_drvdata dwc2_exynos8890 = {
	.cpu_type	= TYPE_EXYNOS8890,
	.ip_type	= TYPE_USB2HOST,
};

static const struct of_device_id exynos_dwc3_match[] = {
	{
		.compatible = "samsung,exynos5250-dwusb3",
		.data = &dwc3_exynos5250,
	},
	{
		.compatible = "samsung,exynos7420-dwusb3",
		.data = &dwc3_exynos7420,
	},
	{
		.compatible = "samsung,exynos8890-dwusb3",
		.data = &dwc3_exynos8890,
	},
	{
		.compatible = "samsung,exynos8890-dwusb2",
		.data = &dwc2_exynos8890,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dwc3_match);

/* -------------------------------------------------------------------------- */

static struct dwc3_exynos *dwc3_exynos_match(struct device *dev)
{
	const struct of_device_id *matches = NULL;
	struct dwc3_exynos *exynos = NULL;

	if (!dev)
		return NULL;

	matches = exynos_dwc3_match;

	if (of_match_device(matches, dev))
		exynos = dev_get_drvdata(dev);

	return exynos;
}

static inline const struct dwc3_exynos_drvdata
*dwc3_exynos_get_driver_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(exynos_dwc3_match, pdev->dev.of_node);
		return match->data;
	}

	return NULL;
}

bool dwc3_exynos_rsw_available(struct device *dev)
{
	struct dwc3_exynos *exynos;

	exynos = dwc3_exynos_match(dev);
	if (!exynos)
		return false;

	return true;
}

int dwc3_exynos_rsw_start(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;

	dev_dbg(dev, "%s\n", __func__);

	/* B-device by default */
	rsw->fsm->id = 1;
	rsw->fsm->b_sess_vld = 0;

	return 0;
}

void dwc3_exynos_rsw_stop(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
}

static void dwc3_exynos_rsw_work(struct work_struct *w)
{
	struct dwc3_exynos_rsw	*rsw = container_of(w,
					struct dwc3_exynos_rsw, work);
	struct dwc3_exynos	*exynos = container_of(rsw,
					struct dwc3_exynos, rsw);

	dev_vdbg(exynos->dev, "%s\n", __func__);

	dwc3_otg_run_sm(rsw->fsm);
}

int dwc3_exynos_rsw_setup(struct device *dev, struct otg_fsm *fsm)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;

	dev_dbg(dev, "%s\n", __func__);

	INIT_WORK(&rsw->work, dwc3_exynos_rsw_work);

	rsw->fsm = fsm;

	return 0;
}

void dwc3_exynos_rsw_exit(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;

	dev_dbg(dev, "%s\n", __func__);

	cancel_work_sync(&rsw->work);

	rsw->fsm = NULL;
}

/**
 * dwc3_exynos_id_event - receive ID pin state change event.
 * @state : New ID pin state.
 */
int dwc3_exynos_id_event(struct device *dev, int state)
{
	struct dwc3_exynos	*exynos;
	struct dwc3_exynos_rsw	*rsw;
	struct otg_fsm		*fsm;

	dev_dbg(dev, "EVENT: ID: %d\n", state);

	exynos = dev_get_drvdata(dev);
	if (!exynos)
		return -ENOENT;

	rsw = &exynos->rsw;

	fsm = rsw->fsm;
	if (!fsm)
		return -ENOENT;

	if (fsm->id != state) {
		fsm->id = state;
		schedule_work(&rsw->work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dwc3_exynos_id_event);

/**
 * dwc3_exynos_vbus_event - receive VBus change event.
 * vbus_active : New VBus state, true if active, false otherwise.
 */
int dwc3_exynos_vbus_event(struct device *dev, bool vbus_active)
{
	struct dwc3_exynos	*exynos;
	struct dwc3_exynos_rsw	*rsw;
	struct otg_fsm		*fsm;

	dev_dbg(dev, "EVENT: VBUS: %sactive\n", vbus_active ? "" : "in");

	exynos = dev_get_drvdata(dev);
	if (!exynos)
		return -ENOENT;

	rsw = &exynos->rsw;

	fsm = rsw->fsm;
	if (!fsm)
		return -ENOENT;

	if (fsm->b_sess_vld != vbus_active) {
		fsm->b_sess_vld = vbus_active;
		schedule_work(&rsw->work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dwc3_exynos_vbus_event);

static int dwc3_exynos_register_phys(struct dwc3_exynos *exynos)
{
	struct usb_phy_generic_platform_data pdata;
	struct platform_device	*pdev;
	int			ret;

	memset(&pdata, 0x00, sizeof(pdata));

	pdev = platform_device_alloc("usb_phy_generic", PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	exynos->usb2_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB2;
	pdata.gpio_reset = -1;

	ret = platform_device_add_data(exynos->usb2_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err1;

	pdev = platform_device_alloc("usb_phy_generic", PLATFORM_DEVID_AUTO);
	if (!pdev) {
		ret = -ENOMEM;
		goto err1;
	}

	exynos->usb3_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB3;

	ret = platform_device_add_data(exynos->usb3_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err2;

	ret = platform_device_add(exynos->usb2_phy);
	if (ret)
		goto err2;

	ret = platform_device_add(exynos->usb3_phy);
	if (ret)
		goto err3;

	return 0;

err3:
	platform_device_del(exynos->usb2_phy);

err2:
	platform_device_put(exynos->usb3_phy);

err1:
	platform_device_put(exynos->usb2_phy);

	return ret;
}

static int dwc3_exynos_remove_child(struct device *dev, void *unused)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int dwc3_exynos_probe(struct platform_device *pdev)
{
	struct dwc3_exynos	*exynos;
	struct device		*dev = &pdev->dev;
	struct device_node	*node = dev->of_node;

	int			ret;

	exynos = devm_kzalloc(dev, sizeof(*exynos), GFP_KERNEL);
	if (!exynos)
		return -ENOMEM;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	platform_set_drvdata(pdev, exynos);

	ret = dwc3_exynos_register_phys(exynos);
	if (ret) {
		dev_err(dev, "couldn't register PHYs\n");
		return ret;
	}

	exynos->dev = dev;

	exynos->drv_data = dwc3_exynos_get_driver_data(pdev);
	if (!exynos->drv_data) {
		dev_info(exynos->dev,
			"%s fail: drv_data is not available\n", __func__);
		return -EINVAL;
	}

	exynos->clk = devm_clk_get(dev, "usbdrd30");
	if (IS_ERR(exynos->clk)) {
		dev_err(dev, "couldn't get clock\n");
		return -EINVAL;
	}
	clk_prepare_enable(exynos->clk);

	exynos->susp_clk = devm_clk_get(dev, "usbdrd30_susp_clk");
	if (IS_ERR(exynos->susp_clk)) {
		dev_info(dev, "no suspend clk specified\n");
		exynos->susp_clk = NULL;
	}
	clk_prepare_enable(exynos->susp_clk);

	if (of_device_is_compatible(node, "samsung,exynos7-dwusb3")) {
		exynos->axius_clk = devm_clk_get(dev, "usbdrd30_axius_clk");
		if (IS_ERR(exynos->axius_clk)) {
			dev_err(dev, "no AXI UpScaler clk specified\n");
			ret = -ENODEV;
			goto axius_clk_err;
		}
		clk_prepare_enable(exynos->axius_clk);
	} else {
		exynos->axius_clk = NULL;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	exynos->vdd33 = devm_regulator_get(dev, "vdd33");
	if (IS_ERR(exynos->vdd33)) {
		dev_dbg(dev, "couldn't get regulator vdd33\n");
		exynos->vdd33 = NULL;
	}
	if (exynos->vdd33) {
		ret = regulator_enable(exynos->vdd33);
		if (ret) {
			dev_err(dev, "Failed to enable VDD33 supply\n");
			goto err2;
		}
	}

	exynos->vdd10 = devm_regulator_get(dev, "vdd10");
	if (IS_ERR(exynos->vdd10)) {
		dev_dbg(dev, "couldn't get regulator vdd10\n");
		exynos->vdd10 = NULL;
	}
	if (exynos->vdd10) {
		ret = regulator_enable(exynos->vdd10);
		if (ret) {
			dev_err(dev, "Failed to enable VDD10 supply\n");
			goto err3;
		}
	}

	ret = dwc3_exynos_register_phys(exynos);
	if (ret) {
		dev_err(dev, "couldn't register PHYs\n");
		goto err4;
	}

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret) {
			dev_err(dev, "failed to add dwc3 core\n");
			goto err5;
		}
	} else {
		dev_err(dev, "no device node, failed to add dwc3 core\n");
		ret = -ENODEV;
		goto err5;
	}

	return 0;

err5:
	platform_device_unregister(exynos->usb2_phy);
	platform_device_unregister(exynos->usb3_phy);
err4:
	if (exynos->vdd10)
		regulator_disable(exynos->vdd10);
err3:
	if (exynos->vdd33)
		regulator_disable(exynos->vdd33);
err2:
	pm_runtime_disable(&pdev->dev);
	clk_disable_unprepare(exynos->axius_clk);
axius_clk_err:
	clk_disable_unprepare(exynos->susp_clk);
	clk_disable_unprepare(exynos->clk);
	pm_runtime_set_suspended(&pdev->dev);
	return ret;
}

static int dwc3_exynos_remove(struct platform_device *pdev)
{
	struct dwc3_exynos	*exynos = platform_get_drvdata(pdev);

	device_for_each_child(&pdev->dev, NULL, dwc3_exynos_remove_child);
	platform_device_unregister(exynos->usb2_phy);
	platform_device_unregister(exynos->usb3_phy);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev)) {
		clk_disable(exynos->axius_clk);
		clk_disable(exynos->susp_clk);
		clk_disable(exynos->clk);
		pm_runtime_set_suspended(&pdev->dev);
	}

	clk_unprepare(exynos->axius_clk);
	clk_unprepare(exynos->susp_clk);
	clk_unprepare(exynos->clk);

	if (exynos->vdd33)
		regulator_disable(exynos->vdd33);
	if (exynos->vdd10)
		regulator_disable(exynos->vdd10);

	return 0;
}

#ifdef CONFIG_PM
static int dwc3_exynos_runtime_suspend(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	clk_disable(exynos->axius_clk);
	clk_disable(exynos->susp_clk);
	clk_disable(exynos->clk);

	return 0;
}

static int dwc3_exynos_runtime_resume(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	clk_enable(exynos->clk);
	clk_enable(exynos->susp_clk);
	clk_enable(exynos->axius_clk);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int dwc3_exynos_suspend(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	clk_disable(exynos->axius_clk);
	clk_disable(exynos->susp_clk);
	clk_disable(exynos->clk);

	if (exynos->vdd33)
		regulator_disable(exynos->vdd33);
	if (exynos->vdd10)
		regulator_disable(exynos->vdd10);

	return 0;
}

static int dwc3_exynos_resume(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	if (exynos->vdd33) {
		ret = regulator_enable(exynos->vdd33);
		if (ret) {
			dev_err(dev, "Failed to enable VDD33 supply\n");
			return ret;
		}
	}
	if (exynos->vdd10) {
		ret = regulator_enable(exynos->vdd10);
		if (ret) {
			if (exynos->vdd33)
				regulator_disable(exynos->vdd33);
			dev_err(dev, "Failed to enable VDD10 supply\n");
			return ret;
		}
	}

	clk_enable(exynos->clk);
	clk_disable(exynos->susp_clk);
	clk_enable(exynos->axius_clk);

	/* runtime set active to reflect active state. */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}

static const struct dev_pm_ops dwc3_exynos_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_exynos_suspend, dwc3_exynos_resume)
	SET_RUNTIME_PM_OPS(dwc3_exynos_runtime_suspend,
			dwc3_exynos_runtime_resume, NULL)
};

#define DEV_PM_OPS	(&dwc3_exynos_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver dwc3_exynos_driver = {
	.probe		= dwc3_exynos_probe,
	.remove		= dwc3_exynos_remove,
	.driver		= {
		.name	= "exynos-dwc3",
		.of_match_table = exynos_dwc3_match,
		.pm	= DEV_PM_OPS,
	},
};

module_platform_driver(dwc3_exynos_driver);

MODULE_ALIAS("platform:exynos-dwc3");
MODULE_AUTHOR("Anton Tikhomirov <av.tikhomirov@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 EXYNOS Glue Layer");
