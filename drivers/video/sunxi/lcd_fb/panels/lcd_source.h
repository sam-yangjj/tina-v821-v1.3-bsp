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
#ifndef _LCD_SOURCE_H_
#define _LCD_SOURCE_H_

#include "panels.h"

struct sunxi_lcd_drv {
	struct sunxi_lcd_fb_disp_source_ops src_ops;
};

extern struct sunxi_lcd_drv g_lcd_fb_drv;

static inline s32 sunxi_lcd_panel_delay_ms(u32 ms)
{
	return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_delay_ms_ops(ms);

	return -1;
}

static inline s32 sunxi_lcd_panel_delay_us(u32 us)
{
	return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_delay_us_ops(us);

	return -1;
}

static inline void sunxi_lcd_panel_backlight_enable(u32 screen_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_backlight_enable_ops)
		g_lcd_fb_drv.src_ops.sunxi_lcd_fb_backlight_enable_ops(
			screen_id);
}

static inline void sunxi_lcd_panel_backlight_disable(u32 screen_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_backlight_disable_ops)
		g_lcd_fb_drv.src_ops.sunxi_lcd_fb_backlight_disable_ops(
			screen_id);
}

static inline void sunxi_lcd_panel_power_enable(u32 screen_id, u32 pwr_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_power_enable_ops)
		g_lcd_fb_drv.src_ops.sunxi_lcd_fb_power_enable_ops(screen_id,
								   pwr_id);
}

static inline void sunxi_lcd_panel_power_disable(u32 screen_id, u32 pwr_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_power_disable_ops)
		g_lcd_fb_drv.src_ops.sunxi_lcd_fb_power_disable_ops(screen_id,
								    pwr_id);
}

static inline s32 sunxi_lcd_panel_pwm_enable(u32 screen_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pwm_enable_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pwm_enable_ops(
			screen_id);

	return -1;
}

static inline s32 sunxi_lcd_panel_pwm_disable(u32 screen_id)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pwm_disable_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pwm_disable_ops(
			screen_id);

	return -1;
}

static inline s32
sunxi_lcd_panel_set_panel_funs(char *name,
			       struct sunxi_lcd_fb_disp_lcd_panel_fun *lcd_cfg)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_set_panel_funs_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_set_panel_funs_ops(
			name, lcd_cfg);

	return -1;
}


static inline s32 sunxi_lcd_panel_pin_cfg(u32 screen_id, u32 bon)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pin_cfg_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_pin_cfg_ops(screen_id,
								     bon);

	return -1;
}

static inline s32 sunxi_lcd_panel_gpio_set_value(u32 screen_id, u32 io_index,
						 u32 value)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_gpio_set_value_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_gpio_set_value_ops(
			screen_id, io_index, value);

	return -1;
}

static inline s32 sunxi_lcd_panel_gpio_set_direction(u32 screen_id,
						     u32 io_index, u32 direct)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_gpio_set_direction_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_gpio_set_direction_ops(
			screen_id, io_index, direct);

	return -1;
}

static inline s32 sunxi_lcd_panel_cmd_write(u32 screen_id, u8 cmd)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_write_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_write_ops(
			screen_id, cmd);

	return -1;
}

static inline s32 sunxi_lcd_panel_para_write(u32 screen_id, u8 para)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_para_write_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_para_write_ops(
			screen_id, para);

	return -1;
}

static inline s32 sunxi_lcd_panel_cmd_read(u32 screen_id, u8 cmd, u8 *rx_buf,
					   u8 len)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_read_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_read_ops(
			screen_id, cmd, rx_buf, len);

	return -1;
}

static inline s32 sunxi_lcd_panel_qspi_cmd_write(u32 screen_id, u8 cmd)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_write_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_qspi_cmd_write_ops(
			screen_id, cmd);

	return -1;
}


static inline s32 sunxi_lcd_panel_qspi_para_write(u32 screen_id, u8 cmd, u8 para)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_para_write_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_qspi_para_write_ops(
			screen_id, cmd, para);

	return -1;
}

static inline s32 sunxi_lcd_panel_qspi_multi_para_write(u32 screen_id, u8 cmd, u8 *tx_buf, u32 len)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_para_write_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_qspi_multi_para_write_ops(
			screen_id, cmd, tx_buf, len);

	return -1;
}

static inline s32 sunxi_lcd_panel_qspi_cmd_read(u32 screen_id, u8 cmd, u8 *rx_buf,
					   u8 len)
{
	if (g_lcd_fb_drv.src_ops.sunxi_lcd_fb_cmd_read_ops)
		return g_lcd_fb_drv.src_ops.sunxi_lcd_fb_qspi_cmd_read_ops(
			screen_id, cmd, rx_buf, len);

	return -1;
}

/* For back port to old Panel Driver */
#ifndef sunxi_lcd_delay_ms
#define sunxi_lcd_delay_ms sunxi_lcd_panel_delay_ms
#endif

#ifndef sunxi_lcd_delay_us
#define sunxi_lcd_delay_us sunxi_lcd_panel_delay_us
#endif

#ifndef sunxi_lcd_backlight_enable
#define sunxi_lcd_backlight_enable sunxi_lcd_panel_backlight_enable
#endif

#ifndef sunxi_lcd_backlight_disable
#define sunxi_lcd_backlight_disable sunxi_lcd_panel_backlight_disable
#endif

#ifndef sunxi_lcd_power_enable
#define sunxi_lcd_power_enable sunxi_lcd_panel_power_enable
#endif

#ifndef sunxi_lcd_power_disable
#define sunxi_lcd_power_disable sunxi_lcd_panel_power_disable
#endif

#ifndef sunxi_lcd_pwm_enable
#define sunxi_lcd_pwm_enable sunxi_lcd_panel_pwm_enable
#endif

#ifndef sunxi_lcd_pwm_disable
#define sunxi_lcd_pwm_disable sunxi_lcd_panel_pwm_disable
#endif

#ifndef sunxi_lcd_set_panel_funs
#define sunxi_lcd_set_panel_funs sunxi_lcd_panel_set_panel_funs
#endif

#ifndef sunxi_lcd_pin_cfg
#define sunxi_lcd_pin_cfg sunxi_lcd_panel_pin_cfg
#endif

#ifndef sunxi_lcd_gpio_set_value
#define sunxi_lcd_gpio_set_value sunxi_lcd_panel_gpio_set_value
#endif

#ifndef sunxi_lcd_gpio_set_direction
#define sunxi_lcd_gpio_set_direction sunxi_lcd_panel_gpio_set_direction
#endif

#ifndef sunxi_lcd_cmd_write
#define sunxi_lcd_cmd_write sunxi_lcd_panel_cmd_write
#endif

#ifndef sunxi_lcd_para_write
#define sunxi_lcd_para_write sunxi_lcd_panel_para_write
#endif

#ifndef sunxi_lcd_cmd_read
#define sunxi_lcd_cmd_read sunxi_lcd_panel_cmd_read
#endif

#ifndef sunxi_lcd_qspi_cmd_write
#define sunxi_lcd_qspi_cmd_write sunxi_lcd_panel_qspi_cmd_write
#endif

#ifndef sunxi_lcd_qspi_para_write
#define sunxi_lcd_qspi_para_write sunxi_lcd_panel_qspi_para_write
#endif

#ifndef sunxi_lcd_qspi_multi_para_write
#define sunxi_lcd_qspi_multi_para_write sunxi_lcd_panel_qspi_multi_para_write
#endif

#ifndef sunxi_lcd_qspi_cmd_read
#define sunxi_lcd_qspi_cmd_read sunxi_lcd_panel_qspi_cmd_read
#endif

#ifndef LCD_OPEN_FUNC
#define LCD_OPEN_FUNC LCD_FB_OPEN_FUNC
#endif

#ifndef LCD_CLOSE_FUNC
#define LCD_CLOSE_FUNC LCD_FB_CLOSE_FUNC
#endif

#endif /* _LCD_SOURCE_H_ */
