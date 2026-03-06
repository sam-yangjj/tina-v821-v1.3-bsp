// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "include.h"

#include "disp_lcd.h"
#include "disp_display.h"
#include "dev_fb.h"
#include "dev_lcd_fb.h"
#include "lcd_fb_ion_mem.h"

#include "./panels/panels.h"

struct sunxi_lcd_fb_dev_lcd_fb_t g_drv_info;
static struct class *lcd_fb_class; /* LCD FB class for sysfs */
static struct device *lcd_fb_class_dev; /* LCD FB class device */

static ssize_t sunxi_lcdfb_colorbar_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int err;
	unsigned int val;

	err = kstrtou32(buf, 10, &val);
	if (err) {
		LCDFB_WRN("Invalid size\n");
		return err;
	}

	if (val == 1) {
		sunxi_lcd_fb_lcd_draw_colorbar(g_drv_info.sysfs_config.screen_id);
	} else if (val == 2) {
		sunxi_lcd_fb_draw_test_screen(g_drv_info.sysfs_config.screen_id);
	} else {
		sunxi_lcd_fb_draw_black_screen(g_drv_info.sysfs_config.screen_id);
	}

	return count;
}

static ssize_t sunxi_lcdfb_screenid_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int err;
	u32 val;

	err = kstrtou32(buf, 10, &val);
	if (err) {
		LCDFB_WRN("Invalid screen id\n");
		return err;
	}

	g_drv_info.sysfs_config.screen_id = val;

	return count;
}

static DEVICE_ATTR(colorbar, 0660, NULL, sunxi_lcdfb_colorbar_store);
static DEVICE_ATTR(screenid, 0660, NULL, sunxi_lcdfb_screenid_store);

static struct attribute *lcd_fb_attributes[] = {
	&dev_attr_colorbar.attr,
	&dev_attr_screenid.attr,
	NULL,
};

static struct attribute_group lcd_fb_attribute_group = { .name = "attr",
							 .attrs = lcd_fb_attributes };

static void sunxi_lcd_fb_module_start_work(struct work_struct *work)
{
	int i = 0;
	struct sunxi_lcd_fb_device *dispdev = NULL;

	for (i = 0; i < g_drv_info.lcd_panel_count; ++i) {
		dispdev = sunxi_sunxi_lcd_fb_device_get(i);
		if (dispdev)
			dispdev->enable(dispdev);
	}
}

static s32 sunxi_lcd_fb_module_start_process(void)
{
	flush_work(&g_drv_info.start_work);
	schedule_work(&g_drv_info.start_work);
	return 0;
}

static int sunxi_lcd_fb_module_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	LCDFB_DBG("running sunxi_lcd_fb_module_probe\n");

	g_drv_info.device = &pdev->dev;

	/* Create sysfs attributes under /sys/class/lcd_fb/lcd_fb/attr/ */
	if (lcd_fb_class && !lcd_fb_class_dev) {
		lcd_fb_class_dev = device_create(lcd_fb_class, &pdev->dev, 0, NULL, "lcd_fb");
		if (IS_ERR(lcd_fb_class_dev)) {
			LCDFB_WRN("Failed to create class device\n");
			return PTR_ERR(lcd_fb_class_dev);
		}

		ret = sysfs_create_group(&lcd_fb_class_dev->kobj, &lcd_fb_attribute_group);
		if (ret) {
			LCDFB_WRN("Failed to create sysfs group under class\n");
			device_destroy(lcd_fb_class, 0);
			lcd_fb_class_dev = NULL;
			return ret;
		}
	} else {
		/* Fallback to platform device sysfs if class is not available */
		ret = sysfs_create_group(&pdev->dev.kobj, &lcd_fb_attribute_group);
		if (ret) {
			LCDFB_WRN("Failed to create sysfs group\n");
			return ret;
		}
	}

	INIT_WORK(&g_drv_info.start_work, sunxi_lcd_fb_module_start_work);

	g_drv_info.lcd_panel_count = of_graph_get_endpoint_count(np);
	if (g_drv_info.lcd_panel_count <= 0) {
		LCDFB_WRN("no endpoints found for node\n");
		return -ENODEV;
	}

	LCDFB_DBG("get endpoint count %d\n", g_drv_info.lcd_panel_count);

#if defined(USE_LCDFB_MEM_FOR_FB)
	sunxi_lcd_fb_init_ion_mgr(&g_drv_info.ion_mgr);
#endif

	sunxi_lcd_fb_disp_init_lcd(&g_drv_info);

	if (!g_drv_info.lcd_fb_num) {
		LCDFB_WRN("no panel found for lcd fb\n");
		return -ENODEV;
	}

	sunxi_lcd_fb_lcd_panel_init();

	ret = sunxi_lcd_fb_init(&g_drv_info);
	if (ret) {
		LCDFB_WRN("Failed to initialize lcd fb\n");
		return ret;
	}

	sunxi_lcd_fb_module_start_process();

	return ret;
}

static void sunxi_lcd_fb_module_shutdown(struct platform_device *pdev)
{
	int i = 0;
	struct sunxi_lcd_fb_device *dispdev = NULL;

	for (i = 0; i < g_drv_info.lcd_panel_count; ++i) {
		dispdev = sunxi_sunxi_lcd_fb_device_get(i);
		if (dispdev)
			dispdev->disable(dispdev);
	}

	LCDFB_DBG("running sunxi_lcd_fb_module_shutdown\n");
}

static int sunxi_lcd_fb_module_remove(struct platform_device *pdev)
{
#if defined(USE_LCDFB_MEM_FOR_FB)
	sunxi_lcd_fb_deinit_ion_mgr(&g_drv_info.ion_mgr);
#endif /* USE_LCDFB_MEM_FOR_FB */

	sunxi_lcd_fb_module_shutdown(pdev);

	sunxi_lcd_fb_exit();

	sunxi_lcd_fb_disp_exit_lcd();

	/* Remove sysfs attributes from class device if available */
	if (lcd_fb_class_dev) {
		sysfs_remove_group(&lcd_fb_class_dev->kobj, &lcd_fb_attribute_group);
		device_destroy(lcd_fb_class, 0);
		lcd_fb_class_dev = NULL;
	} else {
		/* Fallback to remove from platform device */
		sysfs_remove_group(&pdev->dev.kobj, &lcd_fb_attribute_group);
	}

	platform_set_drvdata(pdev, NULL);

	LCDFB_DBG("running sunxi_lcd_fb_module_remove\n");

	return 0;
}

static const struct of_device_id sunxi_lcd_fb_module_match[] = {
	{
		.compatible = "allwinner,lcd-fb",
	},
	{},
};

int sunxi_lcd_fb_module_suspend(struct device *dev)
{
	int i = 0;
	int ret = 0;
	struct sunxi_lcd_fb_device *dispdev = NULL;

	for (i = 0; i < g_drv_info.lcd_panel_count; ++i) {
		dispdev = sunxi_sunxi_lcd_fb_device_get(i);
		if (dispdev)
			ret = dispdev->disable(dispdev);
	}
	return ret;
}

int sunxi_lcd_fb_module_resume(struct device *dev)
{
	int i = 0;
	int ret = 0;
	struct sunxi_lcd_fb_device *dispdev = NULL;

	for (i = 0; i < g_drv_info.lcd_panel_count; ++i) {
		dispdev = sunxi_sunxi_lcd_fb_device_get(i);
		if (dispdev)
			ret = dispdev->enable(dispdev);
	}
	return ret;
}

static const struct dev_pm_ops sunxi_lcd_fb_module_runtime_pm_ops = {
	.suspend = sunxi_lcd_fb_module_suspend,
	.resume = sunxi_lcd_fb_module_resume,
};

struct platform_driver sunxi_lcd_fb_driver = {
	.probe = sunxi_lcd_fb_module_probe,
	.remove = sunxi_lcd_fb_module_remove,
	.shutdown = sunxi_lcd_fb_module_shutdown,
	.driver = {
		.name = "lcd_fb",
		.owner = THIS_MODULE,
		.pm = &sunxi_lcd_fb_module_runtime_pm_ops,
		.of_match_table = sunxi_lcd_fb_module_match,
	},
};

static int __init sunxi_lcd_fb_module_init(void)
{
	s32 ret = -1;
	LCDFB_DBG("running sunxi_lcd_fb_module_init\n");

	/* Create lcd_fb class for sysfs */
	lcd_fb_class = class_create(THIS_MODULE, "lcd_fb");
	if (IS_ERR(lcd_fb_class)) {
		LCDFB_WRN("Failed to create lcd_fb class\n");
		ret = PTR_ERR(lcd_fb_class);
		lcd_fb_class = NULL;
		return ret;
	}

	ret = platform_driver_register(&sunxi_lcd_fb_driver);
	if (ret) {
		LCDFB_WRN("lcd fb register driver failed!\n");
		class_destroy(lcd_fb_class);
		lcd_fb_class = NULL;
		return ret;
	}

	return ret;
}

static void __exit sunxi_lcd_fb_module_exit(void)
{
	platform_driver_unregister(&sunxi_lcd_fb_driver);

	/* Clean up class if it was created */
	if (lcd_fb_class) {
		class_destroy(lcd_fb_class);
		lcd_fb_class = NULL;
	}

	LCDFB_HERE;
}

int sunxi_lcd_fb_disp_get_source_ops(struct sunxi_lcd_fb_disp_source_ops *src_ops)
{
	memset((void *)src_ops, 0, sizeof(*src_ops));

	src_ops->sunxi_lcd_fb_set_panel_funs_ops = sunxi_lcd_fb_bsp_disp_lcd_set_panel_funs;
	src_ops->sunxi_lcd_fb_delay_ms_ops = sunxi_lcd_fb_disp_delay_ms;
	src_ops->sunxi_lcd_fb_delay_us_ops = sunxi_lcd_fb_disp_delay_us;
	src_ops->sunxi_lcd_fb_backlight_enable_ops = sunxi_lcd_fb_bsp_disp_lcd_backlight_enable;
	src_ops->sunxi_lcd_fb_backlight_disable_ops = sunxi_lcd_fb_bsp_disp_lcd_backlight_disable;
	src_ops->sunxi_lcd_fb_pwm_enable_ops = sunxi_lcd_fb_bsp_disp_lcd_pwm_enable;
	src_ops->sunxi_lcd_fb_pwm_disable_ops = sunxi_lcd_fb_bsp_disp_lcd_pwm_disable;
	src_ops->sunxi_lcd_fb_power_enable_ops = sunxi_lcd_fb_bsp_disp_lcd_power_enable;
	src_ops->sunxi_lcd_fb_power_disable_ops = sunxi_lcd_fb_bsp_disp_lcd_power_disable;
	src_ops->sunxi_lcd_fb_pin_cfg_ops = sunxi_lcd_fb_bsp_disp_lcd_pin_cfg;
	src_ops->sunxi_lcd_fb_gpio_set_value_ops = sunxi_lcd_fb_bsp_disp_lcd_gpio_set_value;
	src_ops->sunxi_lcd_fb_gpio_set_direction_ops = sunxi_lcd_fb_bsp_disp_lcd_gpio_set_direction;
	src_ops->sunxi_lcd_fb_cmd_write_ops = sunxi_lcd_fb_bsp_disp_lcd_cmd_write;
	src_ops->sunxi_lcd_fb_para_write_ops = sunxi_lcd_fb_bsp_disp_lcd_para_write;
	src_ops->sunxi_lcd_fb_cmd_read_ops = sunxi_lcd_fb_bsp_disp_lcd_cmd_read;
	src_ops->sunxi_lcd_fb_qspi_cmd_write_ops = sunxi_lcd_fb_bsp_disp_qspi_lcd_cmd_write;
	src_ops->sunxi_lcd_fb_qspi_para_write_ops = sunxi_lcd_fb_bsp_disp_qspi_lcd_para_write;
	src_ops->sunxi_lcd_fb_qspi_multi_para_write_ops =
		sunxi_lcd_fb_bsp_disp_qspi_lcd_multi_para_write;
	src_ops->sunxi_lcd_fb_qspi_cmd_read_ops = sunxi_lcd_fb_bsp_disp_qspi_lcd_cmd_read;
	return 0;
}

module_init(sunxi_lcd_fb_module_init);
module_exit(sunxi_lcd_fb_module_exit);

MODULE_AUTHOR("zhengxiaobin <zhengxiaobin@allwinnertech.com>");
MODULE_AUTHOR("zhangyuanjing <zhangyuanjing@allwinnertech.com>");
MODULE_DESCRIPTION("sunxi lcd fb module");
MODULE_VERSION("1.0.7");
MODULE_LICENSE("GPL");
