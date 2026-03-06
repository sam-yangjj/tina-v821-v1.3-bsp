/*
 * aglink/debug.c
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * Debug info implementation for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/debugfs.h>

#include "ag_version.h"
#include "os_dep/os_intf.h"
#include "aglink.h"
#include "debug.h"

//#define AGLINK_DBG_DEFAULT (AGLINK_DBG_ALWY | AGLINK_DBG_ERROR | AGLINK_DBG_WARN)
#define AGLINK_DBG_DEFAULT (AGLINK_DBG_ALWY | AGLINK_DBG_ERROR | AGLINK_DBG_WARN | AGLINK_DBG_NIY | AGLINK_DBG_MSG)

u8 dbg_common = AGLINK_DBG_DEFAULT;
u8 dbg_cfg = AGLINK_DBG_DEFAULT;
u8 dbg_iface = AGLINK_DBG_DEFAULT;
u8 dbg_plat = AGLINK_DBG_DEFAULT;
u8 dbg_queue = AGLINK_DBG_DEFAULT;
u8 dbg_io = AGLINK_DBG_DEFAULT;//AGLINK_DBG_DEFAULT;
u8 dbg_txrx = AGLINK_DBG_DEFAULT;
u8 dbg_cmd = AGLINK_DBG_DEFAULT;

#define aglink_dbg aglink_printk

void aglink_parse_frame(u8 *eth_data, u8 tx, u32 flags, u16 len)
{

}

static int aglink_version_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "Driver Label:%s\n", AGLINK_VERSION);
	return 0;
}

static int aglink_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, &aglink_version_show, inode->i_private);
}

static int aglink_generic_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t aglink_generic_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{

	char buf[15];
	size_t size = 0;

	sprintf(buf, "Not supported\n");
	size = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static const struct file_operations fops_version = {
	.open = aglink_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

extern u16 txparse_flags;
extern u16 rxparse_flags;

static ssize_t aglink_parse_flags_set(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[30] = { 0 };
	char *start = &buf[0];
	char *endptr = NULL;

	count = (count > 29 ? 29 : count);
	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	txparse_flags = simple_strtoul(buf, &endptr, 16);
	start = endptr + 1;
	if (start < buf + count)
		rxparse_flags = simple_strtoul(start, &endptr, 16);

	txparse_flags &= 0x7fff;
	rxparse_flags &= 0x7fff;

	aglink_dbg(AGLINK_DBG_ALWY, "txparse=0x%04x, rxparse=0x%04x\n", txparse_flags, rxparse_flags);
	return count;
}

static ssize_t aglink_parse_flags_get(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[100];
	size_t size = 0;

	sprintf(buf, "txparse=0x%04x, rxparse=0x%04x\n", txparse_flags, rxparse_flags);
	size = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static const struct file_operations fops_parse_flags = {
	.open = aglink_generic_open,
	.write = aglink_parse_flags_set,
	.read = aglink_parse_flags_get,
	.llseek = default_llseek,
};

static ssize_t aglink_dbg_level_set(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[30] = { 0 };
	char *start = &buf[0];
	char *endptr = NULL;

	count = (count > 29 ? 29 : count);
	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	dbg_txrx = simple_strtoul(buf, &endptr, 16);
	start = endptr + 1;
	if (start < buf + count)
		dbg_cmd = simple_strtoul(start, &endptr, 16);

	aglink_dbg(AGLINK_DBG_ALWY, "dbg_txrx=0x%02x, dbg_cmd=0x%02x\n", dbg_txrx, dbg_cmd);
	return count;
}

static ssize_t aglink_dbg_level_get(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[100];
	size_t size = 0;

	sprintf(buf, "dbg_txrx=0x%02x, dbg_cmd=0x%02x\n", dbg_txrx, dbg_cmd);
	size = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static const struct file_operations fops_dbg_level = {
	.open = aglink_generic_open,
	.write = aglink_dbg_level_set,
	.read = aglink_dbg_level_get,
	.llseek = default_llseek,
};


extern int aglink_tx_data(int dev_id, u8 type, u8 *buff, u16 len);

static ssize_t aglink_write_down(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	int device_id = 0;

	buff = aglink_k_zmalloc(count);
	if (!buff) {
		aglink_dbg(AGLINK_DBG_ERROR, "no memory.\n");
		return -ENOMEM;
	}

	if (copy_from_user(buff, user_buf, count)) {
		aglink_k_free(buff);
		return -EFAULT;
	}

	aglink_tx_data(device_id, 0, buff, count);

	aglink_k_free(buff);

	return count;
}

static const struct file_operations fops_write_down = {
	.open = aglink_generic_open,
	.write = aglink_write_down,
	.read = aglink_generic_read,
	.llseek = default_llseek,
};

extern int aglink_bus_rx_raw_data(int dev_id, u8 *buff, u16 len);

static ssize_t aglink_write_up(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{

	char *buff;
	int device_id = 0;

	buff = aglink_k_zmalloc(count);
	if (!buff) {
		aglink_dbg(AGLINK_DBG_ERROR, "no memory.\n");
		return -ENOMEM;
	}

	if (copy_from_user(buff, user_buf, count)) {
		aglink_k_free(buff);
		return -EFAULT;
	}

	aglink_bus_rx_raw_data(device_id, buff, count);

	aglink_k_free(buff);

	return count;
}

static const struct file_operations fops_write_up = {
	.open = aglink_generic_open,
	.write = aglink_write_up,
	.read = aglink_generic_read,
	.llseek = default_llseek,
};

int aglink_debug_init_common(struct aglink_hw *aglink_hw, int dev_id)
{
	int ret = -ENOMEM;
	struct aglink_debug_common *d = NULL;
	char dir_name[16] = {0};

#define ERR_LINE  do { goto err; } while (0)

	d = aglink_k_zmalloc(sizeof(struct aglink_debug_common));
	if (!d) {
		aglink_dbg(AGLINK_DBG_ERROR, "malloc failed.\n");
		return ret;
	}
	aglink_hw->debug = d;

	snprintf(dir_name, sizeof(dir_name), "aglink%d", dev_id);

	d->debugfs_phy = debugfs_create_dir(dir_name, NULL);

	if (!d->debugfs_phy)
		ERR_LINE;

	if (!debugfs_create_file("version", S_IRUSR, d->debugfs_phy, aglink_hw, &fops_version))
		ERR_LINE;

	if (!debugfs_create_file("parse_flags", S_IRUSR | S_IWUSR, d->debugfs_phy, aglink_hw, &fops_parse_flags))
		ERR_LINE;

	if (!debugfs_create_file("dbg_level", S_IRUSR | S_IWUSR, d->debugfs_phy, aglink_hw, &fops_dbg_level))
		ERR_LINE;

	if (!debugfs_create_file("w_down", S_IRUSR | S_IWUSR, d->debugfs_phy, aglink_hw, &fops_write_down))
		ERR_LINE;

	if (!debugfs_create_file("w_up", S_IRUSR | S_IWUSR, d->debugfs_phy, aglink_hw, &fops_write_up))
		ERR_LINE;
	return 0;
err:
	debugfs_remove_recursive(d->debugfs_phy);
	aglink_k_free(d);
	aglink_hw->debug = NULL;
	return ret;
}

void aglink_debug_deinit_common(struct aglink_hw *aglink_hw, int dev_id)
{
	struct aglink_debug_common *d = aglink_hw->debug;

	if (d) {
		debugfs_remove_recursive(d->debugfs_phy);
		aglink_k_free(d);
		aglink_hw->debug = NULL;
	}
}
