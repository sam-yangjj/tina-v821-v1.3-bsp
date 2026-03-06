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
#ifndef _DEV_LCD_FB_H
#define _DEV_LCD_FB_H

#include "include.h"
#include "lcd_fb_ion_mem.h"

struct sunxi_lcd_sysfs_config {
	u32 screen_id;
};

struct sunxi_lcd_fb_dev_lcd_fb_t {
	struct device *device;
	struct work_struct start_work;
	struct sunxi_lcd_sysfs_config sysfs_config;
	u32 lcd_fb_num;
	u32 lcd_panel_count;
#if defined(USE_LCDFB_MEM_FOR_FB)
	struct lcdfb_ion_mgr ion_mgr;
#endif /* USE_LCDFB_MEM_FOR_FB */
};

extern struct sunxi_lcd_fb_dev_lcd_fb_t g_drv_info;

#endif /* End of file */
