/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */

#include "ch13613.h"

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);
static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

#define RESET(s, v) sunxi_lcd_gpio_set_value(s, 0, v)
#define power_en(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)

struct sunxi_lcd_fb_disp_panel_para info;

static void address(unsigned int sel, int x, int y, int width, int height)
{
	unsigned char reg_para_buf[4];
	reg_para_buf[0] = (y >> 8) & 0xff;
	reg_para_buf[1] = y & 0xff;
	reg_para_buf[2] = (height >> 8) & 0xff;
	reg_para_buf[3] = height & 0xff;
	sunxi_lcd_qspi_multi_para_write(sel, 0x2B, reg_para_buf, 4);

	reg_para_buf[0] = (x >> 8) & 0xff;
	reg_para_buf[1] = x & 0xff;
	reg_para_buf[2] = (width >> 8) & 0xff;
	reg_para_buf[3] = width & 0xff;
	sunxi_lcd_qspi_multi_para_write(sel, 0x2A, reg_para_buf, 4);
	return;
}

/* cmd is u8 type, we using u16 as internal control */
#define REGFLAG_END_OF_TABLE 0xFFFC /* END OF REGISTERS MARKER */

struct lcm_setting_table {
	u16 cmd;
	u8 para_list[32];
	u32 count;
	u32 delay_ms;
};

/* clang-format off */
static struct lcm_setting_table lcm_initialization_setting[] = {
	{ 0xFE, { 0x00 }, 1, 0 },
	{ 0xF0, { 0x50 }, 1, 0 },
	{ 0xB1, { 0x78, 0x70 }, 2, 0 },
	{ 0xC4, { 0x80 }, 1, 0 },
	{ 0x35, { 0x00 }, 1, 0 },
	{ 0x36, { 0x00 }, 1, 0 },
	{ 0x53, { 0x20 }, 1, 0 },
	{ 0x51, { 0xFF }, 1, 0 },
	{ 0x63, { 0xFF }, 1, 0 },
	{ 0x64, { 0x10 }, 1, 0 },
	{ 0x67, { 0x01 }, 1, 0 },
	{ 0x68, { 0x31 }, 1, 0 },
	{ REGFLAG_END_OF_TABLE, {}, 0, 0 }
};
/* clang-format on */

static void LCD_panel_init(unsigned int sel)
{
	int i = 0;

	memset(&info, 0, sizeof(info));
	if (sunxi_lcd_fb_bsp_disp_get_panel_info(sel, &info)) {
		LCDFB_WRN("get panel info fail!\n");
		return;
	}

	// Iterate over the initialization table
	while (lcm_initialization_setting[i].cmd != REGFLAG_END_OF_TABLE) {
		u16 cmd = lcm_initialization_setting[i].cmd;
		u32 para_count = lcm_initialization_setting[i].count;
		u32 delay_ms = lcm_initialization_setting[i].delay_ms;
		u8 *para_list = lcm_initialization_setting[i].para_list;
		if (para_count == 1) {
			sunxi_lcd_panel_qspi_para_write(sel, (cmd & 0xFF),
							para_list[0]);
		} else if (para_count > 1) {
			sunxi_lcd_panel_qspi_multi_para_write(
				sel, (cmd & 0xFF), para_list, para_count);
		} else {
			sunxi_lcd_panel_qspi_cmd_write(sel, (cmd & 0xFF));
		}
		sunxi_lcd_delay_ms(delay_ms);
		i++;
	}

	/* set panel format */
	/* 55----RGB565;66---RGB666 */
	switch (info.lcd_pixel_fmt) {
	case LCDFB_FORMAT_RGB_565:
	case LCDFB_FORMAT_BGR_565:
		sunxi_lcd_panel_qspi_para_write(sel, 0x3a, 0x55);
		break;
	default:
		sunxi_lcd_panel_qspi_para_write(sel, 0x3a, 0x66);
		break;
	}

	address(sel, 0, 0, info.lcd_x - 1, info.lcd_y - 1);

	/* display on */
	sunxi_lcd_panel_qspi_cmd_write(sel, 0x11);
	sunxi_lcd_delay_ms(120);
	sunxi_lcd_panel_qspi_cmd_write(sel, 0x29);
}

static void LCD_panel_exit(unsigned int sel)
{
	sunxi_lcd_panel_qspi_cmd_write(sel, 0x28);
	sunxi_lcd_panel_qspi_cmd_write(sel, 0x10);
}

static s32 LCD_open_flow(u32 sel)
{
	LCDFB_HERE;
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 50);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 200);

	LCD_OPEN_FUNC(sel, sunxi_lcd_fb_black_screen, 50);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	LCDFB_HERE;
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 50);
	/* open lcd power, than delay 200ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 10);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/* config lcd_power pin to open lcd power0 */
	LCDFB_HERE;
	power_en(sel, 1);

	sunxi_lcd_power_enable(sel, 0);

	sunxi_lcd_pin_cfg(sel, 1);
	RESET(sel, 1);
	sunxi_lcd_delay_ms(100);
	RESET(sel, 0);
	sunxi_lcd_delay_ms(100);
	RESET(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	LCDFB_HERE;
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
	power_en(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	/* config lcd_bl_en pin to open lcd backlight */
	sunxi_lcd_backlight_enable(sel);
	LCDFB_HERE;
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
	LCDFB_HERE;
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	LCDFB_HERE;
	return 0;
}

static int lcd_set_var(unsigned int sel, struct fb_info *p_info)
{
	return 0;
}

static int lcd_set_addr_win(unsigned int sel, int x, int y, int width,
			    int height)
{
	address(sel, x, y, width, height);
	return 0;
}

static int lcd_blank(unsigned int sel, unsigned int en)
{
	return 0;
}

struct __lcd_panel ch13613_qspi_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex
	*/
	.name = "ch13613_qspi",
	.func = {
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
		.blank = lcd_blank,
		.set_var = lcd_set_var,
		.set_addr_win = lcd_set_addr_win,
	},
};