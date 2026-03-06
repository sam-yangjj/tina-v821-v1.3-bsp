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
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vfs.h>

#include <uapi/flash_app/uapi_isp_iq_file.h>

#include "../flash_app.h"

#define DRV_NAME "isp_iq_file"
#define DRV_VERSION "1.0.0"

#define SECTOR_SIZE (512)
#define SECTOR_OFFSET (0)
#define SECTOR_SIZE_INDEX (1)

struct sunxi_isp_iq_file_dev {
	int major;
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	const char *partition_name;
	uint32_t partition_size;
};

static struct sunxi_isp_iq_file_dev *isp_iq_file_dev;

static int _sunxi_isp_iq_file_raw_part_write(loff_t to, size_t len, const u_char *buf)
{
	u_char *check_data = NULL;
	int ret = 0;

	check_data = kzalloc(len, GFP_KERNEL);
	if (!check_data) {
		pr_err("iq-file: failed to allocate memory for check_data\n");
		return -ENOMEM;
	}

	pr_debug("iq-file: write data size %zu, to %08llx\n", len, to * SECTOR_SIZE);

	ret = sunxi_rawpart_write(isp_iq_file_dev->partition_name, to * SECTOR_SIZE, len, buf);
	if (ret) {
		pr_err("iq-file: failed to write data to raw partition\n");
		kfree(check_data);
		return ret;
	}

	ret = sunxi_rawpart_read(isp_iq_file_dev->partition_name, to * SECTOR_SIZE, len, check_data);
	if (ret) {
		pr_err("iq-file: failed to read data from raw partition\n");
		kfree(check_data);
		return ret;
	}

	if (memcmp(buf, check_data, len) != 0) {
		pr_err("iq-file: data mismatch: write and read data do not match\n");
		print_hex_dump(KERN_INFO, "orig dump  : ", DUMP_PREFIX_ADDRESS, 16, 1, buf, len, false);
		print_hex_dump(KERN_INFO, "check_data : ", DUMP_PREFIX_ADDRESS, 16, 1, check_data, len, false);
		kfree(check_data);
		return -EIO;
	}

	pr_debug("iq-file: data write and read verified successfully\n");

	kfree(check_data);

	return 0;
}

static int _sunxi_isp_iq_file_write(char *filename, loff_t *offset)
{
	struct file *source_file;
	mm_segment_t old_fs;
	ssize_t bytes_read;
	ssize_t file_size;
	char *kernel_buffer;

	pr_debug("iq-file: Opening file: %s\n", filename);
	source_file = filp_open(filename, O_RDONLY, 0);

	if (IS_ERR(source_file)) {
		pr_err("iq-file: Failed to open file: %ld\n", PTR_ERR(source_file));
		kfree(filename);
		return PTR_ERR(source_file);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file_size = vfs_llseek(source_file, 0, SEEK_END);
	if (file_size < 0) {
		pr_err("iq-file: Failed to get file size: %d\n", file_size);
		filp_close(source_file, NULL);
		kfree(filename);
		return file_size;
	}

	pr_debug("iq-file: File size: %d bytes\n", file_size);

	kernel_buffer = kmalloc(file_size, GFP_KERNEL);
	if (!kernel_buffer) {
		pr_err("iq-file: Failed to allocate memory for kernel buffer\n");
		filp_close(source_file, NULL);
		kfree(filename);
		return -ENOMEM;
	}

	pr_debug("iq-file: Reading the file content\n");

	bytes_read = vfs_read(source_file, kernel_buffer, file_size, offset);
	if (bytes_read < 0) {
		pr_err("iq-file: Failed to read file content: %d\n", bytes_read);
		kfree(kernel_buffer);
		filp_close(source_file, NULL);
		kfree(filename);
		set_fs(old_fs);
		return bytes_read;
	}

	set_fs(old_fs);

	pr_debug("iq-file: save file content\n");

	_sunxi_isp_iq_file_raw_part_write(0x0, file_size, kernel_buffer);

	kfree(kernel_buffer);
	filp_close(source_file, NULL);

	return 0;
}

static int isp_iq_file_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int isp_iq_file_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t isp_iq_file_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
	char *filename;

	filename = kmalloc(size + 1, GFP_KERNEL);
	if (!filename) {
		pr_err("iq-file: Failed to allocate memory for filename\n");
		return -ENOMEM;
	}

	if (copy_from_user(filename, buf, size)) {
		kfree(filename);
		return -EFAULT;
	}
	filename[size] = '\0';
	if (filename[size - 1] == '\n') {
		filename[size - 1] = '\0';
	}

	_sunxi_isp_iq_file_write(filename, offset);

	kfree(filename);

	return size;
}

static long isp_iq_file_drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int err = 0;

	switch (cmd) {
	case ISP_PARAM_WRITE_IQ_FILE: {
		struct isp_param_iq_file_write_info info;
		if (copy_from_user(&info, argp, sizeof(info))) {
			pr_err("Failed to copy data from user space\n");
			return -EFAULT;
		}
		if (!info.file_path) {
			pr_debug("file_path is NULL\n");
			return -EINVAL;
		}
		pr_debug("Received file path: %s\n", info.file_path);
		_sunxi_isp_iq_file_write(info.file_path, &info.offset);
		break;
	}
	default:
		err = -ENOTTY;
		break;
	}
	return err;
}

static struct file_operations isp_iq_file_fops = {
	.owner = THIS_MODULE,
	.open = isp_iq_file_open,
	.release = isp_iq_file_close,
	.write = isp_iq_file_write,
	.unlocked_ioctl = isp_iq_file_drv_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = isp_iq_file_drv_ioctl,
#endif
};

static struct miscdevice isp_iq_file_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "isp_iq_file",
	.fops = &isp_iq_file_fops,
};

static int isp_iq_file_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	ret = of_property_read_u32(np, "partition_size", &isp_iq_file_dev->partition_size);
	if (ret < 0) {
		pr_warn("isp_iq_file: Failed to read partition_size property, using default 1\n");
		return ret;
	}

	ret = of_property_read_string(np, "partition_name", &isp_iq_file_dev->partition_name);
	if (ret < 0) {
		pr_err("isp_iq_file: Failed to read partition_name property\n");
		return ret;
	}

	/* register misc device */
	ret = misc_register(&isp_iq_file_miscdev);
	if (0 != ret) {
		pr_err("isp_iq_file: register motor device failed!\n");
		return -ENOMEM;
	}

	return 0;
}

static int isp_iq_file_remove(struct platform_device *pdev)
{
	misc_deregister(&isp_iq_file_miscdev);
	return 0;
}

static const struct of_device_id isp_iq_file[] = {
	{ .compatible = "allwinner,isp-iq-file" },
	{ /* Sentinel */ },
};

static struct platform_driver isp_iq_file_driver = {
	.probe = isp_iq_file_probe,
	.remove = isp_iq_file_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = isp_iq_file,
	},
};

static int __init isp_iq_file_init(void)
{
	int ret = 0;

	if (isp_iq_file_dev)
		kfree(isp_iq_file_dev);

	isp_iq_file_dev = kzalloc(sizeof(struct sunxi_isp_iq_file_dev), GFP_KERNEL);
	memset(isp_iq_file_dev, 0, sizeof(struct sunxi_isp_iq_file_dev));
	if (!isp_iq_file_dev)
		return -ENOMEM;

	ret = platform_driver_register(&isp_iq_file_driver);

	pr_info("isp_iq_file: init success.\n");
	return ret;
}

static void __exit isp_iq_file_exit(void)
{
	platform_driver_unregister(&isp_iq_file_driver);
	kfree(isp_iq_file_dev);
	return;
}

module_init(isp_iq_file_init); /* Initialize the module */
module_exit(isp_iq_file_exit); /* Cleanup the module */

MODULE_AUTHOR("zhangyuanjing<zhangyuanjing@allwinnertech.com>");
MODULE_DESCRIPTION("allwinner isp param IQ file driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
