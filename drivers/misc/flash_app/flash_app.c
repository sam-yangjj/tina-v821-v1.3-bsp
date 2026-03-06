/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
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

* SUNXI Flash App Driver
*
* 2024.12.1  lujianliang <lujianliang@allwinnertech.com>
*/
/* #define DEBUG */

#include "flash_app.h"

#define SUNXI_FLASH_APP_MODULE_VERSION	"1.1.1"

static int major_number;
static struct class *flash_class;
struct device *flash_device;

static int cmdline_boot_type;

static int __init get_boot_type_from_cmdline(char *p)
{

	pr_debug("%s(%s)\n", __func__, p);
	if (p == NULL) {
		cmdline_boot_type = 0;
	} else {
		cmdline_boot_type = memparse(p, &p);

		if (cmdline_boot_type < 0)
			cmdline_boot_type = 0;
	}
	return 0;
}

early_param("boot_type", get_boot_type_from_cmdline);

static uint32_t get_erasesize(struct mtd_info *mtd)
{
	uint32_t erasesize = 0;

	if (cmdline_boot_type == STORAGE_NOR)
		erasesize = 4096;
	else
		erasesize = mtd->erasesize;

	return erasesize;
}

static int sunxi_mtd_read(struct mtd_info *mtd, loff_t from,
		size_t len, u_char *buf)
{
	size_t retlen;

	pr_debug("from 0x%08x, len %zd\n", (u32)from, len);

	return mtd_read(mtd, from, len, &retlen, buf);
}

static int sunxi_mtd_write(struct mtd_info *mtd, loff_t to,
		size_t len, const u_char *buf)
{
	size_t retlen;
	struct erase_info instr;
	u_char *op_buf = NULL;
	size_t op_len = 0, op_to = 0, op_offset = 0;
	uint32_t erasesize = get_erasesize(mtd);
	int err = 0;

	pr_debug("to 0x%08x, len %zd\n", (u32)to, len);

	op_buf = kzalloc(erasesize, GFP_KERNEL);
	if (IS_ERR(op_buf))
		return PTR_ERR(op_buf);

	while (len) {
		if (((uint32_t)to % erasesize) || (len < erasesize)) {
			if ((uint32_t)to % erasesize) {
				op_len = (uint32_t)to % erasesize > len ? len : ((uint32_t)to % erasesize);
				op_offset = (uint32_t)to % erasesize;
			} else {
				op_len = len;
				op_offset = 0;
			}
			op_to = (uint32_t)to & ~(erasesize - 1);

			err = mtd_read(mtd, op_to, erasesize, &retlen, op_buf);
			if (unlikely(err || retlen != erasesize)) {
				if (mtd_is_bitflip(err) && retlen == erasesize) {
					pr_info("ECC correction at %#llx\n",
						(long long)op_to);
					err = 0;
				} else {
					pr_err("error: read failed at %#llx\n",
						(long long)op_to);
					err = err ? err : -1;
					goto done;
				}
			}

			memcpy(op_buf + op_offset, buf, op_len);

			instr.addr = op_to;
			instr.len = erasesize;
			err = mtd_erase(mtd, &instr);
			if (err) {
				pr_err("error %d while erasing EB %#llx\n", err, (long long)op_to);
				goto done;
			}

			err = mtd_write(mtd, op_to, erasesize, &retlen, op_buf);
			if (!err && retlen != erasesize)
				err = -EIO;
			if (err)
				pr_err("error: write failed at %#llx\n", (long long)to);

			len -= op_len;
			to += op_len;
			buf += op_len;
		} else {
			instr.addr = to;
			instr.len = erasesize;
			err = mtd_erase(mtd, &instr);
			if (err) {
				pr_info("error %d while erasing EB %#llx\n", err, (long long)to);
				goto done;
			}

			err = mtd_write(mtd, to, erasesize, &retlen, buf);
			if (!err && retlen != erasesize)
				err = -EIO;
			if (err)
				pr_err("error: write failed at %#llx\n", (long long)to);

			len -= erasesize;
			to += erasesize;
			buf += erasesize;
		}
	}

done:
	kfree(op_buf);
	return 0;
}

static int sunxi_mmc_part_read(const void *part_name, loff_t from,
							size_t len, u_char *buf)
{
	struct file *file = NULL;
	mm_segment_t old_fs;
	ssize_t bytes_read;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file = filp_open((char *)part_name, O_RDONLY, 0);
	if (IS_ERR(file)) {
		pr_err("failed to open partition %s for reading\n", (char *)part_name);
		return PTR_ERR(file);
	}

	bytes_read = vfs_read(file, buf, len, &from);
	if (bytes_read != len) {
		pr_err("failed to read the expected number of bytes\n");
		ret = -EIO;
	}

	filp_close(file, NULL);

	set_fs(old_fs);

	return ret;
}

static int sunxi_mmc_part_write(const void *part_name, loff_t to,
								size_t len, const u_char *buf)
{
	struct file *file = NULL;
	mm_segment_t old_fs;
	ssize_t bytes_written;
	int ret = 0;
	unsigned int retry_num = 10;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

retry:
	file = filp_open((char *)part_name, O_WRONLY, 0);
	if (IS_ERR(file)) {
		pr_err("failed to open partition %s for writing\n", (char *)part_name);
		if (retry_num) {
			retry_num--;
			msleep(500);
			goto retry;
		}
		return PTR_ERR(file);
	}

	bytes_written = vfs_write(file, buf, len, &to);
	if (bytes_written != len) {
		pr_err("failed to write the expected number of bytes\n");
		ret = -EIO;
	}

	ret = vfs_fsync(file, 0);
	if (ret)
		pr_err("vfs_fsync failed\n");

	filp_close(file, NULL);

	set_fs(old_fs);

	return ret;
}

int sunxi_rawpart_read(const void *part_name, loff_t from,
		size_t len, u_char *buf)
{
	if (!strncmp(part_name, "/dev/mtd", 8)) {
		char *endptr;
		int offset = strncmp(part_name, "/dev/mtdblock", 13) ? 8 : 13;
		int i = simple_strtol((char *)(part_name + offset), &endptr, 10);
		struct mtd_info *mtd = __mtd_next_device(i);
		if (!mtd) {
			pr_err("The %s partion could not found\n", (char *)part_name);
			return -EINVAL;
		}

		return sunxi_mtd_read(mtd, from, len, buf);
	} else if (!strncmp(part_name, "/dev/mmc", 8))
		return sunxi_mmc_part_read(part_name, from, len, buf);
	else
		pr_err("Tyoe of the device node that is not support\n");

	return -1;
}
EXPORT_SYMBOL_GPL(sunxi_rawpart_read);

int sunxi_rawpart_write(const void *part_name, loff_t to,
		size_t len, const u_char *buf)
{
	pr_debug("to 0x%08x, len %zd\n", (u32)to, len);

	if (!strncmp(part_name, "/dev/mtd", 8)) {
		char *endptr;
		int offset = strncmp(part_name, "/dev/mtdblock", 13) ? 8 : 13;
		int i = simple_strtol((char *)(part_name + offset), &endptr, 10);
		struct mtd_info *mtd = __mtd_next_device(i);
		if (!mtd) {
			pr_err("The %s partion could not found\n", (char *)part_name);
			return -EINVAL;
		}

		return sunxi_mtd_write(mtd, to, len, buf);
	} else if (!strncmp(part_name, "/dev/mmc", 8))
		return sunxi_mmc_part_write(part_name, to, len, buf);
	else
		pr_err("Tyoe of the device node that is not support\n");

	return -1;
}
EXPORT_SYMBOL_GPL(sunxi_rawpart_write);

static int sunxi_secstorage_read(int item, unsigned char *buf, unsigned int len)
{
	loff_t offset = 0;
	struct mtd_info *mtd = NULL;

	mtd = get_mtd_device_nm("secure_storage"); //spinad
	if (!IS_ERR(mtd)) {
#if IS_ENABLED(CONFIG_AW_SPINAND_SECURE_STORAGE)
		return aw_spinand_mtd_read_secure_storage(mtd, item, buf, len);
#else
		pr_err("Configuration CONFIG_AW_SPINAND_SECURE_STORAGE is not enabled\n");
		return -1;
#endif
	}

	offset += item * get_erasesize(mtd);
	if (offset + len > (get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM)) {
		pr_err("secstorage size :0x%x, over read secstorage offset:0x%llx len:0x%x\n",
				(get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM), offset, len);
		pr_err("stop secstorage read\n");
		return -EINVAL;
	}

	mtd = get_mtd_device_nm("uboot"); //spinor
	if (IS_ERR(mtd)) {
		pr_err("The secure_storage partion could not found\n");
		return -EINVAL;
	}
	offset += (mtd->size - (get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM) - GPT_SIZE);

	return sunxi_mtd_read(mtd, offset, len, buf);
}

static int sunxi_secstorage_write(int item, unsigned char *buf, unsigned int len)
{
	loff_t offset = 0;
	struct mtd_info *mtd = NULL;

	mtd = get_mtd_device_nm("secure_storage"); //spinad
	if (!IS_ERR(mtd)) {
#if IS_ENABLED(CONFIG_AW_SPINAND_SECURE_STORAGE)
		return aw_spinand_mtd_write_secure_storage(mtd, item, buf, len);
#else
		pr_err("Configuration CONFIG_AW_SPINAND_SECURE_STORAGE is not enabled\n");
		return -1;
#endif
	}

	offset += item * get_erasesize(mtd);
	if (offset + len > (get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM)) {
		pr_err("secstorage size :0x%x, over write secstorage offset:0x%llx len:0x%x\n",
				(get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM), offset, len);
		pr_err("stop secstorage write\n");
		return -EINVAL;
	}

	mtd = get_mtd_device_nm("uboot"); //spinor
	if (IS_ERR(mtd)) {
		pr_err("The secure_storage partion could not found\n");
		return -EINVAL;
	}
	offset += (mtd->size - (get_erasesize(mtd) * SECURE_STORAGE_ITEMNUM) - GPT_SIZE);

	return sunxi_mtd_write(mtd, offset, len, buf);
}

static int sunxi_flashapp_open(struct inode *inode, struct file *file)
{
	pr_info("%s: open\n", SUNXI_FLASH_APP_NAME);
	return 0;
}

static ssize_t sunxi_flashapp_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	pr_debug("%s:Function awaiting replenishment \n", __func__);
	return 0;
}

static ssize_t sunxi_flashapp_write(struct file *file, const char __user *buf,
		size_t len, loff_t *offset)
{
	pr_debug("%s:Function awaiting replenishment \n", __func__);
	return 0;
}

static long sunxi_flashapp_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	pr_debug("%s: ioctl\n", SUNXI_FLASH_APP_NAME);

	switch (cmd) {
	case RAWPART_READ:
	{
		struct rawpart_op_param op_param;
		u_char *user_data = NULL;

		if (copy_from_user(&op_param, (struct rawpart_op_param *)argp, sizeof(op_param)))
			return -EFAULT;

		if (!(const void __user *)(uintptr_t)op_param.user_data)
			return -EINVAL;

		user_data = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(user_data))
			return PTR_ERR(user_data);

		ret = sunxi_rawpart_read(op_param.name, op_param.offset,
				op_param.len, user_data);

		if (copy_to_user((void __user *)(uintptr_t)op_param.user_data, user_data, op_param.len)) {
			kfree(user_data);
			return -EFAULT;
		}

		kfree(user_data);
		break;
	}
	case RAWPART_WRITE:
	{
		struct rawpart_op_param op_param;
		u_char *user_data = NULL;

		if (copy_from_user(&op_param, (struct rawpart_op_param *)argp, sizeof(op_param)))
			return -EFAULT;

		if (!(void __user *)(uintptr_t)op_param.user_data)
			return -EINVAL;

		user_data = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(user_data))
			return PTR_ERR(user_data);

		if (copy_from_user(user_data, (void __user *)(uintptr_t)op_param.user_data, op_param.len)) {
			kfree(user_data);
			return -EFAULT;
		}

		ret = sunxi_rawpart_write(op_param.name, op_param.offset,
				op_param.len, user_data);

		kfree(user_data);
		break;
	}
	case SECURE_STORAGE_READ:
	{
		struct secstorage_op_param op_param;
		u_char *user_data = NULL;

		if (copy_from_user(&op_param, (struct secstorage_op_param *)argp, sizeof(op_param)))
			return -EFAULT;

		if (!(void __user *)(uintptr_t)op_param.buf)
			return -EINVAL;

		user_data = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(user_data))
			return PTR_ERR(user_data);

		ret = sunxi_secstorage_read(op_param.item, user_data, op_param.len);

		if (copy_to_user((void __user *)(uintptr_t)op_param.buf, user_data, op_param.len)) {
			kfree(user_data);
			return -EFAULT;
		}

		kfree(user_data);
		break;
	}
	case SECURE_STORAGE_WRITE:
	{
		struct secstorage_op_param op_param;
		u_char *user_data = NULL;

		if (copy_from_user(&op_param, (struct secstorage_op_param *)argp, sizeof(op_param)))
			return -EFAULT;

		if (!(void __user *)(uintptr_t)op_param.buf)
			return -EINVAL;

		user_data = kzalloc(op_param.len, GFP_KERNEL);
		if (IS_ERR(user_data))
			return PTR_ERR(user_data);

		if (copy_from_user(user_data, (void __user *)(uintptr_t)op_param.buf, op_param.len)) {
			kfree(user_data);
			return -EFAULT;
		}

		ret = sunxi_secstorage_write(op_param.item, user_data, op_param.len);

		kfree(user_data);
		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}

static int sunxi_flashapp_close(struct inode *inode, struct file *file)
{
	pr_info("%s: closed\n", SUNXI_FLASH_APP_NAME);
	return 0;
}

static struct file_operations flash_app_fops = {
	.owner	    	= THIS_MODULE,
	.open	    	= sunxi_flashapp_open,
	.read	    	= sunxi_flashapp_read,
	.write		    = sunxi_flashapp_write,
	.unlocked_ioctl = sunxi_flashapp_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl	= sunxi_flashapp_ioctl,
#endif
	.release	    = sunxi_flashapp_close,
};

static int __init sunxi_flashapp_device_init(void)
{
	int ret = 0;

	major_number = register_chrdev(major_number, SUNXI_FLASH_APP_NAME, &flash_app_fops);
	if (major_number < 0) {
		pr_err("%s: failed to register a major number\n", SUNXI_FLASH_APP_NAME);
		return major_number;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	flash_class = class_create(SUNXI_FLASH_APP_NAME);
#else
	flash_class = class_create(THIS_MODULE, SUNXI_FLASH_APP_NAME);
#endif
	if (IS_ERR(flash_class)) {
		pr_err("%s: failed to create device class\n", SUNXI_FLASH_APP_NAME);
		ret = PTR_ERR(flash_class);
		goto unregister_chrdev;
	}

	flash_device = device_create(flash_class, NULL,
			MKDEV(major_number, 0), NULL, SUNXI_FLASH_APP_NAME);
	if (IS_ERR(flash_device)) {
		pr_err("%s: failed to create device file\n", SUNXI_FLASH_APP_NAME);
		ret = PTR_ERR(flash_device);
		goto class_destroy;
	}

	pr_info("%s: device registered with major number %d\n",
			SUNXI_FLASH_APP_NAME, major_number);
	return 0;

class_destroy:
	class_destroy(flash_class);
unregister_chrdev:
	unregister_chrdev(major_number, SUNXI_FLASH_APP_NAME);

	return -1;
}

static void __exit sunxi_flashapp_device_exit(void)
{
	device_destroy(flash_class, MKDEV(major_number, 0));
	class_destroy(flash_class);
	unregister_chrdev(major_number, SUNXI_FLASH_APP_NAME);

	pr_info("%s: device unregistered\n", SUNXI_FLASH_APP_NAME);
}

module_init(sunxi_flashapp_device_init);
module_exit(sunxi_flashapp_device_exit);

MODULE_AUTHOR("lujianliang@allwinnertech.com");
MODULE_DESCRIPTION("sunxi flash app driver");
MODULE_ALIAS("platform:"SUNXI_FLASH_APP_NAME);
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SUNXI_FLASH_APP_MODULE_VERSION);
