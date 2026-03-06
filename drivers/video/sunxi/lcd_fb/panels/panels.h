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

#ifndef __PANEL_H__
#define __PANEL_H__

#include "../include.h"
#include "lcd_source.h"
#include "../disp_display.h"
#include "../dev_fb.h"

struct __lcd_panel {
	char name[32];
	struct sunxi_lcd_fb_disp_lcd_panel_fun func;
};

extern struct __lcd_panel *g_lcd_fb_panel_array[];

extern int
sunxi_lcd_fb_disp_get_source_ops(struct sunxi_lcd_fb_disp_source_ops *src_ops);

int sunxi_lcd_fb_lcd_panel_init(void);

#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD35512)
extern struct __lcd_panel kld35512_panel;
#endif

#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD39501)
extern struct __lcd_panel kld39501_panel;
#endif

#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD2844B)
extern struct __lcd_panel kld2844b_panel;
#endif

#if IS_ENABLED(CONFIG_LCD_SUPPORT_NV3029S)
extern struct __lcd_panel nv3029s_panel;
#endif

#if IS_ENABLED(CONFIG_LCD_SUPPORT_ST77916_QSPI)
extern struct __lcd_panel st77916_qspi_panel;
#endif

#if IS_ENABLED(CONFIG_LCD_SUPPORT_CH13613_QSPI)
extern struct __lcd_panel ch13613_qspi_panel;
#endif

#endif
