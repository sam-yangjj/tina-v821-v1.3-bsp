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

#ifndef __FLASH_APP_H__
#define __FLASH_APP_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>
#include <linux/mtd/aw-spinand.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/genhd.h>
#include <linux/buffer_head.h>
#include <linux/mmc/card.h>
#include <linux/device.h>

#include <uapi/flash_app/uapi_flash_app.h>

#define SUNXI_FLASH_APP_NAME		"sunxi_flash_app"
#define SECURE_STORAGE_ITEMNUM		(8)
#define GPT_SIZE			(16 * 1024)
#define SECTOR_SIZE			(512)

struct mtd_info *__mtd_next_device(int i);

int sunxi_rawpart_write(const void *part_name, loff_t to,
		size_t len, const u_char *buf);

int sunxi_rawpart_read(const void *part_name, loff_t from,
		size_t len, u_char *buf);

#endif /* __FLASH_APP_H__ */