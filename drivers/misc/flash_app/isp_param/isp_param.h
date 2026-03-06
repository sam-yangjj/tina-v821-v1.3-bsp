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

#ifndef _ISP_PARAM_H_
#define _ISP_PARAM_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#include <uapi/flash_app/uapi_isp_param.h>

#define ISP0_THRESHOLD_PARAM_OFFSET CONFIG_ISP0_THRESHOLD_PARAM_OFFSET
#define ISP1_THRESHOLD_PARAM_OFFSET CONFIG_ISP1_THRESHOLD_PARAM_OFFSET

enum {
	AW_ISP_PARAM_ISP = 0x0,
	AW_ISP_PARAM_SENSOR = 0x1,
	AW_ISP_PARAM_MAX = 0x2,
};

#define DAY_LIGNT_SENSOR_SIZE 96
#define NIGHT_LIGNT_SENSOR_SIZE 40
#define SECTOR_OFFSET (0)
#define SECTOR_SIZE_INDEX (1)

struct sunxi_isp_param_dev {
	int major;
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	uint32_t sensor_count;
	uint32_t preload_addr;
	uint32_t sensor0_config_sector[2];
	uint32_t sensor0_isp_sector[2];
	uint32_t sensor1_config_sector[2];
	uint32_t sensor1_isp_sector[2];
	const char *rawpart_name;
};

int sunxi_isp_param_save_isp_info(loff_t to, size_t len, const u_char *buf);

#endif