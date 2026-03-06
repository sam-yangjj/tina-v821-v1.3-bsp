// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
* Allwinner SoCs display driver.
*
* Copyright (C) 2016 Allwinner.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#include "../include.h"

#define SPIDEV_MODULE_NAME "lcd_fb_panel_device"

static int spi_dev_probe(struct spi_device *spi)
{
	LCDFB_HERE;
	return 0;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0))
static void spi_dev_remove(struct spi_device *spi)
#else
static int spi_dev_remove(struct spi_device *spi)
#endif
{
	LCDFB_HERE;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0))
	return;
#else
	return 0;
#endif
}

static const struct of_device_id spidev_match[] = {
	{
		.compatible = "allwinner,spi-panel",
	},
	{},
};

static struct spi_driver spidev_driver = {
	.probe = spi_dev_probe,
	.remove = spi_dev_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SPIDEV_MODULE_NAME,
		.of_match_table = spidev_match,
	},
};

static int __init spi_panel_init(void)
{
	int ret;

	ret = spi_register_driver(&spidev_driver);
	if (ret) {
		LCDFB_WRN("spi panel register driver failed!\n");
		return ret;
	}

	return ret;
}

static void __exit spi_panel_exit(void)
{
	spi_unregister_driver(&spidev_driver);
}

module_init(spi_panel_init);
module_exit(spi_panel_exit);

MODULE_AUTHOR("zhangyuanjing <zhangyuanjing@allwinnertech.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("Register SPI Panel device for Allwinner LCD FB");
