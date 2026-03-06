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

#include "panels.h"

struct sunxi_lcd_drv g_lcd_fb_drv;

struct __lcd_panel *g_lcd_fb_panel_array[] = {
#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD35512)
	&kld35512_panel,
#endif
#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD39501)
	&kld39501_panel,
#endif
#if IS_ENABLED(CONFIG_LCD_SUPPORT_KLD2844B)
	&kld2844b_panel,
#endif
#if IS_ENABLED(CONFIG_LCD_SUPPORT_NV3029S)
	&nv3029s_panel,
#endif
#if IS_ENABLED(CONFIG_LCD_SUPPORT_ST77916_QSPI)
	&st77916_qspi_panel,
#endif
#if IS_ENABLED(CONFIG_LCD_SUPPORT_CH13613_QSPI)
	&ch13613_qspi_panel,
#endif
	NULL,
};

int sunxi_lcd_fb_lcd_panel_init(void)
{
	int i;
	sunxi_lcd_fb_disp_get_source_ops(&g_lcd_fb_drv.src_ops);
	for (i = 0; g_lcd_fb_panel_array[i] != NULL; i++) {
		sunxi_lcd_set_panel_funs(g_lcd_fb_panel_array[i]->name,
					 &g_lcd_fb_panel_array[i]->func);
	}
	return 0;
}
