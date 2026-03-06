/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */

#include "st77916.h"

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
	{ 0xF0, { 0x28 }, 1, 0 },
	{ 0xF2, { 0x28 }, 1, 0 },
	{ 0x73, { 0xF0 }, 1, 0 },
	{ 0x7C, { 0xD1 }, 1, 0 },
	{ 0x83, { 0xE0 }, 1, 0 },
	{ 0x84, { 0x61 }, 1, 0 },
	{ 0xF2, { 0x82 }, 1, 0 },
	{ 0xF0, { 0x00 }, 1, 0 },
	{ 0xF0, { 0x01 }, 1, 0 },
	{ 0xF1, { 0x01 }, 1, 0 },
	{ 0xB0, { 0x5E }, 1, 0 },
	{ 0xB1, { 0x55 }, 1, 0 },
	{ 0xB2, { 0x24 }, 1, 0 },
	{ 0xB3, { 0x01 }, 1, 0 },
	{ 0xB4, { 0x87 }, 1, 0 },
	{ 0xB5, { 0x44 }, 1, 0 },
	{ 0xB6, { 0x8B }, 1, 0 },
	{ 0xB7, { 0x40 }, 1, 0 },
	{ 0xB8, { 0x86 }, 1, 0 },
	{ 0xB9, { 0x15 }, 1, 0 },
	{ 0xBA, { 0x00 }, 1, 0 },
	{ 0xBB, { 0x08 }, 1, 0 },
	{ 0xBC, { 0x08 }, 1, 0 },
	{ 0xBD, { 0x00 }, 1, 0 },
	{ 0xBE, { 0x00 }, 1, 0 },
	{ 0xBF, { 0x07 }, 1, 0 },
	{ 0xC0, { 0x80 }, 1, 0 },
	{ 0xC1, { 0x10 }, 1, 0 },
	{ 0xC2, { 0x37 }, 1, 0 },
	{ 0xC3, { 0x80 }, 1, 0 },
	{ 0xC4, { 0x10 }, 1, 0 },
	{ 0xC5, { 0x37 }, 1, 0 },
	{ 0xC6, { 0xA9 }, 1, 0 },
	{ 0xC7, { 0x41 }, 1, 0 },
	{ 0xC8, { 0x01 }, 1, 0 },
	{ 0xC9, { 0xA9 }, 1, 0 },
	{ 0xCA, { 0x41 }, 1, 0 },
	{ 0xCB, { 0x01 }, 1, 0 },
	{ 0xCC, { 0x7F }, 1, 0 },
	{ 0xCD, { 0x7F }, 1, 0 },
	{ 0xCE, { 0xFF }, 1, 0 },
	{ 0xD0, { 0x91 }, 1, 0 },
	{ 0xD1, { 0x68 }, 1, 0 },
	{ 0xD2, { 0x68 }, 1, 0 },
	{ 0xF5, { 0x00, 0xA5 }, 2, 0 },
	{ 0xDD, { 0x40 }, 1, 0 },
	{ 0xDE, { 0x40 }, 1, 0 },
	{ 0xF1, { 0x10 }, 1, 0 },
	{ 0xF0, { 0x00 }, 1, 0 },
	{ 0xF0, { 0x02 }, 1, 0 },
	{ 0xE0, { 0xF0, 0x10, 0x18, 0x0D, 0x0C, 0x38, 0x3E, 0x44, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34 }, 14, 0 },
	{ 0xE1, { 0xF0, 0x0F, 0x17, 0x0D, 0x0B, 0x07, 0x3E, 0x33, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34 }, 14, 0 },
	{ 0xF0, { 0x10 }, 1, 0 },
	{ 0xF3, { 0x10 }, 1, 0 },
	{ 0xE0, { 0x08 }, 1, 0 },
	{ 0xE1, { 0x00 }, 1, 0 },
	{ 0xE2, { 0x00 }, 1, 0 },
	{ 0xE3, { 0x00 }, 1, 0 },
	{ 0xE4, { 0xE0 }, 1, 0 },
	{ 0xE5, { 0x06 }, 1, 0 },
	{ 0xE6, { 0x21 }, 1, 0 },
	{ 0xE7, { 0x03 }, 1, 0 },
	{ 0xE8, { 0x05 }, 1, 0 },
	{ 0xE9, { 0x02 }, 1, 0 },
	{ 0xEA, { 0xE9 }, 1, 0 },
	{ 0xEB, { 0x00 }, 1, 0 },
	{ 0xEC, { 0x00 }, 1, 0 },
	{ 0xED, { 0x14 }, 1, 0 },
	{ 0xEE, { 0xFF }, 1, 0 },
	{ 0xEF, { 0x00 }, 1, 0 },
	{ 0xF8, { 0xFF }, 1, 0 },
	{ 0xF9, { 0x00 }, 1, 0 },
	{ 0xFA, { 0x00 }, 1, 0 },
	{ 0xFB, { 0x30 }, 1, 0 },
	{ 0xFC, { 0x00 }, 1, 0 },
	{ 0xFD, { 0x00 }, 1, 0 },
	{ 0xFE, { 0x00 }, 1, 0 },
	{ 0xFF, { 0x00 }, 1, 0 },
	{ 0x60, { 0x40 }, 1, 0 },
	{ 0x61, { 0x05 }, 1, 0 },
	{ 0x62, { 0x00 }, 1, 0 },
	{ 0x63, { 0x42 }, 1, 0 },
	{ 0x64, { 0xDA }, 1, 0 },
	{ 0x65, { 0x00 }, 1, 0 },
	{ 0x66, { 0x00 }, 1, 0 },
	{ 0x67, { 0x00 }, 1, 0 },
	{ 0x68, { 0x00 }, 1, 0 },
	{ 0x69, { 0x00 }, 1, 0 },
	{ 0x6A, { 0x00 }, 1, 0 },
	{ 0x6B, { 0x00 }, 1, 0 },
	{ 0x70, { 0x40 }, 1, 0 },
	{ 0x71, { 0x04 }, 1, 0 },
	{ 0x72, { 0x00 }, 1, 0 },
	{ 0x73, { 0x42 }, 1, 0 },
	{ 0x74, { 0xD9 }, 1, 0 },
	{ 0x75, { 0x00 }, 1, 0 },
	{ 0x76, { 0x00 }, 1, 0 },
	{ 0x77, { 0x00 }, 1, 0 },
	{ 0x78, { 0x00 }, 1, 0 },
	{ 0x79, { 0x00 }, 1, 0 },
	{ 0x7A, { 0x00 }, 1, 0 },
	{ 0x7B, { 0x00 }, 1, 0 },
	{ 0x80, { 0x48 }, 1, 0 },
	{ 0x81, { 0x00 }, 1, 0 },
	{ 0x82, { 0x07 }, 1, 0 },
	{ 0x83, { 0x02 }, 1, 0 },
	{ 0x84, { 0xD7 }, 1, 0 },
	{ 0x85, { 0x04 }, 1, 0 },
	{ 0x86, { 0x00 }, 1, 0 },
	{ 0x87, { 0x00 }, 1, 0 },
	{ 0x88, { 0x48 }, 1, 0 },
	{ 0x89, { 0x00 }, 1, 0 },
	{ 0x8A, { 0x09 }, 1, 0 },
	{ 0x8B, { 0x02 }, 1, 0 },
	{ 0x8C, { 0xD9 }, 1, 0 },
	{ 0x8D, { 0x04 }, 1, 0 },
	{ 0x8E, { 0x00 }, 1, 0 },
	{ 0x8F, { 0x00 }, 1, 0 },
	{ 0x90, { 0x48 }, 1, 0 },
	{ 0x91, { 0x00 }, 1, 0 },
	{ 0x92, { 0x0B }, 1, 0 },
	{ 0x93, { 0x02 }, 1, 0 },
	{ 0x94, { 0xDB }, 1, 0 },
	{ 0x95, { 0x04 }, 1, 0 },
	{ 0x96, { 0x00 }, 1, 0 },
	{ 0x97, { 0x00 }, 1, 0 },
	{ 0x98, { 0x48 }, 1, 0 },
	{ 0x99, { 0x00 }, 1, 0 },
	{ 0x9A, { 0x0D }, 1, 0 },
	{ 0x9B, { 0x02 }, 1, 0 },
	{ 0x9C, { 0xDD }, 1, 0 },
	{ 0x9D, { 0x04 }, 1, 0 },
	{ 0x9E, { 0x00 }, 1, 0 },
	{ 0x9F, { 0x00 }, 1, 0 },
	{ 0xA0, { 0x48 }, 1, 0 },
	{ 0xA1, { 0x00 }, 1, 0 },
	{ 0xA2, { 0x06 }, 1, 0 },
	{ 0xA3, { 0x02 }, 1, 0 },
	{ 0xA4, { 0xD6 }, 1, 0 },
	{ 0xA5, { 0x04 }, 1, 0 },
	{ 0xA6, { 0x00 }, 1, 0 },
	{ 0xA7, { 0x00 }, 1, 0 },
	{ 0xA8, { 0x48 }, 1, 0 },
	{ 0xA9, { 0x00 }, 1, 0 },
	{ 0xAA, { 0x08 }, 1, 0 },
	{ 0xAB, { 0x02 }, 1, 0 },
	{ 0xAC, { 0xD8 }, 1, 0 },
	{ 0xAD, { 0x04 }, 1, 0 },
	{ 0xAE, { 0x00 }, 1, 0 },
	{ 0xAF, { 0x00 }, 1, 0 },
	{ 0xB0, { 0x48 }, 1, 0 },
	{ 0xB1, { 0x00 }, 1, 0 },
	{ 0xB2, { 0x0A }, 1, 0 },
	{ 0xB3, { 0x02 }, 1, 0 },
	{ 0xB4, { 0xDA }, 1, 0 },
	{ 0xB5, { 0x04 }, 1, 0 },
	{ 0xB6, { 0x00 }, 1, 0 },
	{ 0xB7, { 0x00 }, 1, 0 },
	{ 0xB8, { 0x48 }, 1, 0 },
	{ 0xB9, { 0x00 }, 1, 0 },
	{ 0xBA, { 0x0C }, 1, 0 },
	{ 0xBB, { 0x02 }, 1, 0 },
	{ 0xBC, { 0xDC }, 1, 0 },
	{ 0xBD, { 0x04 }, 1, 0 },
	{ 0xBE, { 0x00 }, 1, 0 },
	{ 0xBF, { 0x00 }, 1, 0 },
	{ 0xC0, { 0x10 }, 1, 0 },
	{ 0xC1, { 0x47 }, 1, 0 },
	{ 0xC2, { 0x56 }, 1, 0 },
	{ 0xC3, { 0x65 }, 1, 0 },
	{ 0xC4, { 0x74 }, 1, 0 },
	{ 0xC5, { 0x88 }, 1, 0 },
	{ 0xC6, { 0x99 }, 1, 0 },
	{ 0xC7, { 0x01 }, 1, 0 },
	{ 0xC8, { 0xBB }, 1, 0 },
	{ 0xC9, { 0xAA }, 1, 0 },
	{ 0xD0, { 0x10 }, 1, 0 },
	{ 0xD1, { 0x47 }, 1, 0 },
	{ 0xD2, { 0x56 }, 1, 0 },
	{ 0xD3, { 0x65 }, 1, 0 },
	{ 0xD4, { 0x74 }, 1, 0 },
	{ 0xD5, { 0x88 }, 1, 0 },
	{ 0xD6, { 0x99 }, 1, 0 },
	{ 0xD7, { 0x01 }, 1, 0 },
	{ 0xD8, { 0xBB }, 1, 0 },
	{ 0xD9, { 0xAA }, 1, 0 },
	{ 0xF3, { 0x01 }, 1, 0 },
	{ 0xF0, { 0x00 }, 1, 0 },
	{ REGFLAG_END_OF_TABLE, {}, 0, 0 },
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

	/* display on */
	sunxi_lcd_panel_qspi_cmd_write(sel, 0x21);
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

struct __lcd_panel st77916_qspi_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex
	*/
	.name = "st77916_qspi",
	.func = {
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
		.blank = lcd_blank,
		.set_var = lcd_set_var,
		.set_addr_win = lcd_set_addr_win,
	},
};