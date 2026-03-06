/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2026 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 *
 * Copyright (c) 2021-2028 Allwinnertech Co., Ltd.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* #define DEBUG */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "isp_param.h"
#include "../flash_app.h"

#define DRV_NAME "isp_param"
#define DRV_VERSION "1.0.0"

#define SECTOR_SIZE (512)

static struct sunxi_isp_param_dev *isp_param_dev;

static int _sunxi_isp_param_raw_part_write(loff_t to, size_t len,
					   const u_char *buf)
{
	u_char *check_data = NULL;
	int ret = 0;

	check_data = kzalloc(len, GFP_KERNEL);
	if (!check_data) {
		pr_err("isp-param: failed to allocate memory for check_data\n");
		return -ENOMEM;
	}

	pr_debug("isp-param: write data size %zu, to %08llx\n", len,
		 to * SECTOR_SIZE);

	ret = sunxi_rawpart_write(isp_param_dev->rawpart_name, to * SECTOR_SIZE,
				  len, buf);
	if (ret) {
		pr_err("isp-param: failed to write data to raw partition\n");
		kfree(check_data);
		return ret;
	}

	ret = sunxi_rawpart_read(isp_param_dev->rawpart_name, to * SECTOR_SIZE,
				 len, check_data);
	if (ret) {
		pr_err("isp-param: failed to read data from raw partition\n");
		kfree(check_data);
		return ret;
	}

	if (memcmp(buf, check_data, len) != 0) {
		pr_err("isp-param: data mismatch: write and read data do not match\n");
		print_hex_dump(KERN_INFO, "orig dump  : ", DUMP_PREFIX_ADDRESS,
			       16, 1, buf, len, false);
		print_hex_dump(KERN_INFO, "check_data : ", DUMP_PREFIX_ADDRESS,
			       16, 1, check_data, len, false);
		kfree(check_data);
		return -EIO;
	}

	pr_debug("isp-param: data write and read verified successfully\n");

	kfree(check_data);

	return 0;
}

static int sunxi_isp_param_raw_part_write(size_t len, const u_char *buf,
					  int sensor_id, u8 type)
{
	pr_debug("isp-param: write data size %d, sensor id %d, type %d\n", len,
		 sensor_id, type);

	if (AW_ISP_PARAM_ISP == type) {
		if (sensor_id == 0)
			_sunxi_isp_param_raw_part_write(
				isp_param_dev->sensor0_isp_sector[SECTOR_OFFSET],
				len, buf);
		else if (sensor_id == 1)
			_sunxi_isp_param_raw_part_write(
				isp_param_dev->sensor1_isp_sector[SECTOR_OFFSET],
				len, buf);
		else
			pr_err("isp-param: unsupported sensor id %d\n",
			       sensor_id);
	} else if (AW_ISP_PARAM_SENSOR == type) {
		if (sensor_id == 0)
			_sunxi_isp_param_raw_part_write(
				isp_param_dev
					->sensor0_config_sector[SECTOR_OFFSET],
				len, buf);
		else if (sensor_id == 1)
			_sunxi_isp_param_raw_part_write(
				isp_param_dev
					->sensor1_config_sector[SECTOR_OFFSET],
				len, buf);
		else
			pr_err("isp-param: unsupported sensor id %d\n",
			       sensor_id);
	}

	pr_debug("isp-param: write isp param done!\n");

	return 0;
}

int sunxi_isp_param_save_isp_info(loff_t to, size_t len, const u_char *buf)
{
	if (to == (ISP0_THRESHOLD_PARAM_OFFSET * SECTOR_SIZE))
		sunxi_isp_param_raw_part_write(len, buf, 0, AW_ISP_PARAM_ISP);
	else if (to == (ISP1_THRESHOLD_PARAM_OFFSET * SECTOR_SIZE))
		sunxi_isp_param_raw_part_write(len, buf, 1, AW_ISP_PARAM_ISP);
	return 0;
}
EXPORT_SYMBOL_GPL(sunxi_isp_param_save_isp_info);

static int sunxi_isp_param_save_sensor_info(uint32_t sensor_id, size_t len,
					    const u_char *buf)
{
	pr_debug("isp-param: want to write %d data, sensor_id %d\n", len,
		 sensor_id);
	if (len <= isp_param_dev->sensor0_config_sector[SECTOR_SIZE_INDEX] *
			   SECTOR_SIZE) {
		sunxi_isp_param_raw_part_write(len, buf, sensor_id,
					       AW_ISP_PARAM_SENSOR);
	} else {
		pr_err("isp-param: data size %d is more then given block %d\n",
		       len,
		       isp_param_dev->sensor0_config_sector[SECTOR_SIZE_INDEX] *
			       SECTOR_SIZE);
		return -1;
	}
	return 0;
}

static int _sunxi_isp_param_raw_part_read(loff_t from, size_t len, u_char *buf)
{
	int ret;

	pr_debug("isp-param: read data size %zu, from %08llx\n", len,
		from * SECTOR_SIZE);

	ret = sunxi_rawpart_read(isp_param_dev->rawpart_name, from * SECTOR_SIZE, len, buf);
	if (ret) {
		pr_err("isp-param: failed to read data from raw partition\n");
		return ret;
	}
	pr_debug("isp-param: data read successfully\n");

	return 0;
}

static int sunxi_isp_param_raw_part_read(size_t len, u_char *buf, int sensor_id, u8 type)
{
	pr_debug("isp-param: read data size %d, sensor id %d, type %d\n", len,
		sensor_id, type);

	if (AW_ISP_PARAM_ISP == type) {
		if (sensor_id == 0)
			return _sunxi_isp_param_raw_part_read(isp_param_dev->sensor0_isp_sector[SECTOR_OFFSET], len, buf);
		else if (sensor_id == 1)
			return _sunxi_isp_param_raw_part_read(isp_param_dev->sensor1_isp_sector[SECTOR_OFFSET], len, buf);
		else
			pr_err("isp-param: unsupported sensor id %d\n", sensor_id);
	} else if (AW_ISP_PARAM_SENSOR == type) {
		if (sensor_id == 0)
			return _sunxi_isp_param_raw_part_read(isp_param_dev->sensor0_config_sector[SECTOR_OFFSET], len, buf);
		else if (sensor_id == 1)
			return _sunxi_isp_param_raw_part_read(isp_param_dev->sensor1_config_sector[SECTOR_OFFSET], len, buf);
		else
			pr_err("isp-param: unsupported sensor id %d\n", sensor_id);
	}

	return -EINVAL;
}

int sunxi_isp_param_load_isp_info(loff_t from, size_t len, u_char *buf)
{
	if (from == (ISP0_THRESHOLD_PARAM_OFFSET * SECTOR_SIZE))
		return sunxi_isp_param_raw_part_read(len, buf, 0, AW_ISP_PARAM_ISP);
	else if (from == (ISP1_THRESHOLD_PARAM_OFFSET * SECTOR_SIZE))
		return sunxi_isp_param_raw_part_read(len, buf, 1, AW_ISP_PARAM_ISP);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(sunxi_isp_param_load_isp_info);

static int sunxi_isp_param_load_sensor_info(uint32_t sensor_id, size_t len, u_char *buf)
{
	pr_debug("isp-param: want to read %zu data, sensor_id %d\n", len, sensor_id);

	if (len <= isp_param_dev->sensor0_config_sector[SECTOR_SIZE_INDEX] * SECTOR_SIZE) {
		return sunxi_isp_param_raw_part_read(len, buf, sensor_id, AW_ISP_PARAM_SENSOR);
	} else {
		pr_err("isp-param: data size %zu is more than given block %d\n",
			len, isp_param_dev->sensor0_config_sector[SECTOR_SIZE_INDEX] * SECTOR_SIZE);
		return -EINVAL;
	}
}

static int isp_param_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int isp_param_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long isp_param_drv_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int err = 0;

	switch (cmd) {
	case SENSOR_INFO_PARAM_WRITE: {
		struct isp_param_write_info op_param;
		u_char *isp_config = NULL;

		if (copy_from_user(&op_param,
				   (struct isp_param_write_info *)argp,
				   sizeof(op_param)))
			return -EFAULT;

		if (!(void __user *)(uintptr_t)op_param.isp_config)
			return -EINVAL;

		isp_config = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(isp_config))
			return PTR_ERR(isp_config);

		if (copy_from_user(isp_config,
				   (void __user *)(uintptr_t)op_param.isp_config,
				   op_param.len)) {
			kfree(isp_config);
			return -EFAULT;
		}

		sunxi_isp_param_save_sensor_info(op_param.sensor_id,
						 op_param.len, isp_config);

		kfree(isp_config);
		break;
	}
	case SENSOR_INFO_PARAM_READ: {
		struct isp_param_read_info op_param;
		u_char *isp_config = NULL;

		if (copy_from_user(&op_param, (struct isp_param_read_info *)argp, sizeof(op_param)))
			return -EFAULT;

		if (!(void __user *)(uintptr_t)op_param.isp_config)
			return -EINVAL;

		isp_config = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(isp_config))
			return PTR_ERR(isp_config);

		err = sunxi_isp_param_load_sensor_info(op_param.sensor_id, op_param.len, isp_config);

		if (err) {
			kfree(isp_config);
			return err;
		}

		if (copy_to_user((void __user *)(uintptr_t)op_param.isp_config, isp_config, op_param.len)) {
			kfree(isp_config);
			return -EFAULT;
		}

		kfree(isp_config);
		break;
	}
	default:
		err = -ENOTTY; /* Command not supported */
		break;
	}
	return err;
}

static struct file_operations isp_param_fops = {
	.owner = THIS_MODULE,
	.open = isp_param_open,
	.release = isp_param_close,
	.unlocked_ioctl = isp_param_drv_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = isp_param_drv_ioctl,
#endif
};

static struct miscdevice isp_param_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "isp_param",
	.fops = &isp_param_fops,
};

static int isp_param_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	ret = of_property_read_u32(np, "sensor_count",
				   &isp_param_dev->sensor_count);
	if (ret < 0) {
		pr_warn("Failed to read sensor_count property, using default 1\n");
		isp_param_dev->sensor_count = 1;
	}

	ret = of_property_read_u32(np, "preload_addr",
				   &isp_param_dev->preload_addr);
	if (ret < 0) {
		pr_err("Failed to read preload_addr property\n");
		return ret;
	}

	if (isp_param_dev->sensor_count >= 1) {
		ret = of_property_read_u32_array(
			np, "sensor0_isp_sector",
			isp_param_dev->sensor0_isp_sector, 2);
		if (ret < 0) {
			pr_err("Failed to read sensor0_isp_sector property\n");
			return ret;
		}

		ret = of_property_read_u32_array(
			np, "sensor0_config_sector",
			isp_param_dev->sensor0_config_sector, 2);
		if (ret < 0) {
			pr_err("Failed to read sensor0_config_sector property\n");
			return ret;
		}
	}

	if (isp_param_dev->sensor_count >= 2) {
		ret = of_property_read_u32_array(
			np, "sensor1_isp_sector",
			isp_param_dev->sensor1_isp_sector, 2);
		if (ret < 0) {
			pr_err("Failed to read sensor1_isp_sector property\n");
			return ret;
		}

		ret = of_property_read_u32_array(
			np, "sensor1_config_sector",
			isp_param_dev->sensor1_config_sector, 2);
		if (ret < 0) {
			pr_err("Failed to read sensor1_config_sector property\n");
			return ret;
		}
	}

	ret = of_property_read_string(np, "partition_name",
				      &isp_param_dev->rawpart_name);
	if (ret < 0) {
		pr_err("Failed to read partition_name property\n");
		return ret;
	}

	/* register misc device */
	ret = misc_register(&isp_param_miscdev);
	if (0 != ret) {
		pr_err("isp_param: register motor device failed!\n");
		return -ENOMEM;
	}

	return 0;
}

static int isp_param_remove(struct platform_device *pdev)
{
	misc_deregister(&isp_param_miscdev);
	return 0;
}

static const struct of_device_id isp_param[] = {
	{ .compatible = "allwinner,isp-param" },
	{ /* Sentinel */ },
};

static struct platform_driver isp_param_driver = {
	.probe = isp_param_probe,
	.remove = isp_param_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = isp_param,
	},
};

static int __init isp_param_init(void)
{
	int ret = 0;

	if (isp_param_dev)
		kfree(isp_param_dev);

	/* init sunxi gpio motor */
	isp_param_dev = kzalloc(sizeof(struct sunxi_isp_param_dev), GFP_KERNEL);
	memset(isp_param_dev, 0, sizeof(struct sunxi_isp_param_dev));
	if (!isp_param_dev)
		return -ENOMEM;

	ret = platform_driver_register(&isp_param_driver);

	pr_info("isp_param: init success.\n");
	return ret;
}

static void __exit isp_param_exit(void)
{
	platform_driver_unregister(&isp_param_driver);
	kfree(isp_param_dev);
	return;
}

module_init(isp_param_init); /* Initialize the module */
module_exit(isp_param_exit); /* Cleanup the module */

MODULE_AUTHOR("zhangyuanjing<zhangyuanjing@allwinnertech.com>");
MODULE_DESCRIPTION("allwinner isp param driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
