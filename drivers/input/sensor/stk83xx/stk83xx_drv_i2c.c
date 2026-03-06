/*
 *  stk83xx_drv_i2c.c - Linux kernel modules for sensortek stk83xx
 *  accelerometer sensor (I2C Interface)
 *
 *  Copyright (C) 2019 STK, sensortek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/pm.h>
//#include <linux/sensors.h>
#include <stk83xx.h>
#include "stk83xx_qualcomm.h"

#ifdef CONFIG_OF
static struct of_device_id stk83xx_match_table[] = {
	{
		.compatible = "stk,stk83xx",
	},
	{}
};
#endif /* CONFIG_OF */

/*
 * @brief: Proble function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] id: struct i2c_device_id *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk83xx_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct common_function common_fn = {
		.bops = &stk_i2c_bops,
		.tops = &stk_t_ops,
		.gops = &stk_g_ops,
		.sops = &stk_s_ops,
	};
	return stk_i2c_probe(client, &common_fn);
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
static int stk83xx_i2c_remove(struct i2c_client *client)
{
	return stk_i2c_remove(client);
}

/**
 * @brief:
 */
static int stk83xx_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info)
{
	strcpy(info->type, STK83XX_NAME);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk83xx_i2c_suspend(struct device *dev)
{
	return stk83xx_suspend(dev);
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk83xx_i2c_resume(struct device *dev)
{
	return stk83xx_resume(dev);
}

static const struct dev_pm_ops stk83xx_pm_ops = {
	.suspend = stk83xx_i2c_suspend,
	.resume = stk83xx_i2c_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_ACPI
static const struct acpi_device_id stk83xx_acpi_id[] = { { "STK83XX", 0 }, {} };
MODULE_DEVICE_TABLE(acpi, stk83xx_acpi_id);
#endif /* CONFIG_ACPI */

static const struct i2c_device_id stk83xx_i2c_id[] = { { STK83XX_NAME, 0 },
						       {} };

MODULE_DEVICE_TABLE(i2c, stk83xx_i2c_id);

static struct i2c_driver
	stk83xx_i2c_driver = { .probe = stk83xx_i2c_probe,
			       .remove = stk83xx_i2c_remove,
			       .detect = stk83xx_i2c_detect,
			       .id_table = stk83xx_i2c_id,
			       .class = I2C_CLASS_HWMON,
			       .driver = {
				       .owner = THIS_MODULE,
				       .name = STK83XX_NAME,
#ifdef CONFIG_PM_SLEEP
				       .pm = &stk83xx_pm_ops,
#endif
#ifdef CONFIG_ACPI
				       .acpi_match_table =
					       ACPI_PTR(stk83xx_acpi_id),
#endif /* CONFIG_ACPI */
#ifdef CONFIG_OF
				       .of_match_table = stk83xx_match_table,
#endif /* CONFIG_OF */
			       } };

module_i2c_driver(stk83xx_i2c_driver);

MODULE_AUTHOR("Sensortek");
MODULE_DESCRIPTION("stk83xx 3-Axis accelerometer driver");
MODULE_LICENSE("GPL");
//MODULE_VERSION(STK_QUALCOMM_VERSION);
