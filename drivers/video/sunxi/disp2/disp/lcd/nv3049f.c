/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#include "nv3049f.h"

static void lcd_panel_nv3049f_init(void);
static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		/* {input value, corrected value} */
		{ 0, 0 },     { 15, 15 },   { 30, 30 },	  { 45, 45 },
		{ 60, 60 },   { 75, 75 },   { 90, 90 },	  { 105, 105 },
		{ 120, 120 }, { 135, 135 }, { 150, 150 }, { 165, 165 },
		{ 180, 180 }, { 195, 195 }, { 210, 210 }, { 225, 225 },
		{ 240, 240 }, { 255, 255 },
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
			{ LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3 },
			{ LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3 },
			{ LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3 },
		},
		{
			{ LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0 },
			{ LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0 },
			{ LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0 },
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] -
				  lcd_gamma_tbl[i][1]) *
				 j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8) +
				   lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}

static s32 LCD_open_flow(u32 sel)
{
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 0);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 10);
	/* open lcd controller, and delay 100ms */
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);
	/* close lcd controller, and delay 0ms */
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
	/* open lcd power, than delay 10ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 500);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/* config lcd_power pin to open lcd power0 */
	sunxi_lcd_power_enable(sel, 0);
	/* pwr_en, active low */
	sunxi_lcd_gpio_set_value(0, 0, 0);
	sunxi_lcd_delay_ms(100);
	sunxi_lcd_gpio_set_value(0, 0, 1);
	/* sunxi_lcd_gpio_set_value(sel, 3, 0); */
	sunxi_lcd_pin_cfg(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	/* pwr_en, active low */
	sunxi_lcd_gpio_set_value(0, 0, 0);
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	/* config lcd_bl_en pin to open lcd backlight */
	sunxi_lcd_backlight_enable(sel);
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

static void LCD_panel_init(u32 sel)
{
	struct disp_panel_para *info = kmalloc(sizeof(struct disp_panel_para),
					       GFP_KERNEL | __GFP_ZERO);

	bsp_disp_get_panel_info(sel, info);
	lcd_panel_nv3049f_init();
	kfree(info);
}

static void LCD_panel_exit(u32 sel)
{
	struct disp_panel_para *info = kmalloc(sizeof(struct disp_panel_para),
					       GFP_KERNEL | __GFP_ZERO);
	bsp_disp_get_panel_info(sel, info);
	kfree(info);
}

static void nv3049f_spi_write_cmd(u8 value)
{
	int i;
	sunxi_lcd_gpio_set_value(0, 2, 0);
	sunxi_lcd_gpio_set_value(0, 1, 0);
	sunxi_lcd_gpio_set_value(0, 2, 1);
	sunxi_lcd_gpio_set_value(0, 2, 0);
	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			sunxi_lcd_gpio_set_value(0, 1, 1);
		else
			sunxi_lcd_gpio_set_value(0, 1, 0);
		value <<= 1;
		sunxi_lcd_gpio_set_value(0, 2, 1);
		sunxi_lcd_gpio_set_value(0, 2, 0);
	}
	sunxi_lcd_gpio_set_value(0, 1, 0);
}

static void nv3049f_spi_write_data(u8 value)
{
	int i;
	sunxi_lcd_gpio_set_value(0, 2, 0);
	sunxi_lcd_gpio_set_value(0, 1, 1);
	sunxi_lcd_gpio_set_value(0, 2, 1);
	sunxi_lcd_gpio_set_value(0, 2, 0);
	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			sunxi_lcd_gpio_set_value(0, 1, 1);
		else
			sunxi_lcd_gpio_set_value(0, 1, 0);
		value <<= 1;
		sunxi_lcd_gpio_set_value(0, 2, 1);
		sunxi_lcd_gpio_set_value(0, 2, 0);
	}
	sunxi_lcd_gpio_set_value(0, 1, 0);
}

static void lcd_panel_nv3049f_init(void)
{
	nv3049f_spi_write_cmd(0x01);
	sunxi_lcd_delay_ms(6);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x49);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x01);
	nv3049f_spi_write_cmd(0xe2);
	nv3049f_spi_write_data(0x00);
	nv3049f_spi_write_cmd(0xe2);
	nv3049f_spi_write_data(0x00);
	// nv3049f_spi_write_cmd(0xF1);
	// nv3049f_spi_write_data(0x0e);//bist
	nv3049f_spi_write_cmd(0x14);
	nv3049f_spi_write_data(0x08); //vbp_para[7:0]
	nv3049f_spi_write_cmd(0x11);
	nv3049f_spi_write_data(0x08); //vfp_para[7:0]
	nv3049f_spi_write_cmd(0x16);
	nv3049f_spi_write_data(0x14); //hfp
	nv3049f_spi_write_cmd(0x19);
	nv3049f_spi_write_data(0x19); //hbp
	nv3049f_spi_write_cmd(0x15); //vbp_seri[7:0]
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0x12); //vfp_seri[7:0]
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0x17); //hfp_seri
	nv3049f_spi_write_data(0x14);
	nv3049f_spi_write_cmd(0x1A); //hbp_seri
	nv3049f_spi_write_data(0x19);
	nv3049f_spi_write_cmd(0x20);
	nv3049f_spi_write_data(0x5c);
	nv3049f_spi_write_cmd(0x21);
	nv3049f_spi_write_data(0x02); //20241227
	nv3049f_spi_write_cmd(0x3b);
	nv3049f_spi_write_data(0x04); //LVD
	nv3049f_spi_write_cmd(0x41);
	nv3049f_spi_write_data(0x32);
	nv3049f_spi_write_cmd(0x45);
	nv3049f_spi_write_data(0x03);
	nv3049f_spi_write_cmd(0x46);
	nv3049f_spi_write_data(0x55);
	//////////////SOURCE/////////////////
	nv3049f_spi_write_cmd(0x51);
	nv3049f_spi_write_data(0x36);
	nv3049f_spi_write_cmd(0x52);
	nv3049f_spi_write_data(0x01);
	nv3049f_spi_write_cmd(0x53);
	nv3049f_spi_write_data(0x20);
	nv3049f_spi_write_cmd(0x54);
	nv3049f_spi_write_data(0x12);
	nv3049f_spi_write_cmd(0x55);
	nv3049f_spi_write_data(0x01);
	nv3049f_spi_write_cmd(0x56);
	nv3049f_spi_write_data(0x03);
	nv3049f_spi_write_cmd(0x57);
	nv3049f_spi_write_data(0x37);
	//Write_LCD_REG(0x0000,0x0059);
	//Write_LCD_REG(0x0000,0x0100);
	//Write_LCD_REG(0x0000,0x005A);
	//Write_LCD_REG(0x0000,0x0100);
	nv3049f_spi_write_cmd(0x5B);
	nv3049f_spi_write_data(0x82);
	nv3049f_spi_write_cmd(0x5C);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0x5D);
	nv3049f_spi_write_data(0x80); //
	nv3049f_spi_write_cmd(0x79);
	nv3049f_spi_write_data(0xFE);
	nv3049f_spi_write_cmd(0x7D);
	nv3049f_spi_write_data(0x08);
	//////////////SOURCE/////////////////
	nv3049f_spi_write_cmd(0x90);
	nv3049f_spi_write_data(0x00); //VSP
	nv3049f_spi_write_cmd(0x91);
	nv3049f_spi_write_data(0x00); //VSN
	nv3049f_spi_write_cmd(0x95);
	nv3049f_spi_write_data(0x0c);
	nv3049f_spi_write_cmd(0x94);
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0x96);
	nv3049f_spi_write_data(0x16);
	nv3049f_spi_write_cmd(0x98);
	nv3049f_spi_write_data(0x33);
	nv3049f_spi_write_cmd(0xca);
	nv3049f_spi_write_data(0x14);
	nv3049f_spi_write_cmd(0xc8);
	nv3049f_spi_write_data(0x6e);
	nv3049f_spi_write_cmd(0xcc);
	nv3049f_spi_write_data(0x12);
	nv3049f_spi_write_cmd(0xe7);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xA0);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xB4);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xA1);
	nv3049f_spi_write_data(0x22);
	nv3049f_spi_write_cmd(0xB5);
	nv3049f_spi_write_data(0x21);
	nv3049f_spi_write_cmd(0xA2);
	nv3049f_spi_write_data(0x20);
	nv3049f_spi_write_cmd(0xB6);
	nv3049f_spi_write_data(0x1E);
	nv3049f_spi_write_cmd(0xA5);
	nv3049f_spi_write_data(0x0F);
	nv3049f_spi_write_cmd(0xB9);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0xA6);
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0xBA);
	nv3049f_spi_write_data(0x07);
	nv3049f_spi_write_cmd(0xA7);
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0xBB);
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0xA3);
	nv3049f_spi_write_data(0x40);
	nv3049f_spi_write_cmd(0xB7);
	nv3049f_spi_write_data(0x42);
	nv3049f_spi_write_cmd(0xA4);
	nv3049f_spi_write_data(0x40);
	nv3049f_spi_write_cmd(0xB8);
	nv3049f_spi_write_data(0x42);
	nv3049f_spi_write_cmd(0xA8);
	nv3049f_spi_write_data(0x0F);
	nv3049f_spi_write_cmd(0xBC);
	nv3049f_spi_write_data(0x0E);
	nv3049f_spi_write_cmd(0xA9);
	nv3049f_spi_write_data(0x15);
	nv3049f_spi_write_cmd(0xBD);
	nv3049f_spi_write_data(0x14);
	nv3049f_spi_write_cmd(0xAA);
	nv3049f_spi_write_data(0x15);
	nv3049f_spi_write_cmd(0xBE);
	nv3049f_spi_write_data(0x15);
	nv3049f_spi_write_cmd(0xAB);
	nv3049f_spi_write_data(0x15);
	nv3049f_spi_write_cmd(0xBF);
	nv3049f_spi_write_data(0x14);
	nv3049f_spi_write_cmd(0xAC);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0xC0);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0xAD);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0xC1);
	nv3049f_spi_write_data(0x12);
	nv3049f_spi_write_cmd(0xAE);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0xC2);
	nv3049f_spi_write_data(0x13);
	nv3049f_spi_write_cmd(0xAF);
	nv3049f_spi_write_data(0x0f);
	nv3049f_spi_write_cmd(0xC3);
	nv3049f_spi_write_data(0x12);
	nv3049f_spi_write_cmd(0xB0);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0xC4);
	nv3049f_spi_write_data(0x10);
	nv3049f_spi_write_cmd(0xB1);
	nv3049f_spi_write_data(0x08);
	nv3049f_spi_write_cmd(0xC5);
	nv3049f_spi_write_data(0x0A);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x49);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x03);
	///////////////////////////GOA//////////////////////////////
	nv3049f_spi_write_cmd(0x27);
	nv3049f_spi_write_data(0x88);
	nv3049f_spi_write_cmd(0x28);
	nv3049f_spi_write_data(0x06);
	nv3049f_spi_write_cmd(0x29);
	nv3049f_spi_write_data(0x04);
	nv3049f_spi_write_cmd(0x36);
	nv3049f_spi_write_data(0x88);
	nv3049f_spi_write_cmd(0x37);
	nv3049f_spi_write_data(0x05);
	nv3049f_spi_write_cmd(0x38);
	nv3049f_spi_write_data(0x03);
	nv3049f_spi_write_cmd(0x2a);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0x2b);
	nv3049f_spi_write_data(0x60);
	nv3049f_spi_write_cmd(0x2c);
	nv3049f_spi_write_data(0x80);
	nv3049f_spi_write_cmd(0x79);
	nv3049f_spi_write_data(0x3f);
	nv3049f_spi_write_cmd(0x7a);
	nv3049f_spi_write_data(0x3f);
	nv3049f_spi_write_cmd(0x7B);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0x80);
	nv3049f_spi_write_data(0x85);
	nv3049f_spi_write_cmd(0x81);
	nv3049f_spi_write_data(0x13);
	nv3049f_spi_write_cmd(0x82);
	nv3049f_spi_write_data(0x5a);
	nv3049f_spi_write_cmd(0x83);
	nv3049f_spi_write_data(0x05);
	nv3049f_spi_write_cmd(0x84);
	nv3049f_spi_write_data(0x60);
	nv3049f_spi_write_cmd(0x85);
	nv3049f_spi_write_data(0x80);
	nv3049f_spi_write_cmd(0x88);
	nv3049f_spi_write_data(0x84);
	nv3049f_spi_write_cmd(0x89);
	nv3049f_spi_write_data(0x13);
	nv3049f_spi_write_cmd(0x8A);
	nv3049f_spi_write_data(0x5b);
	nv3049f_spi_write_cmd(0x8B);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0x8C);
	nv3049f_spi_write_data(0x60);
	nv3049f_spi_write_cmd(0x8D);
	nv3049f_spi_write_data(0x80);
	nv3049f_spi_write_cmd(0x8E);
	nv3049f_spi_write_data(0x83);
	nv3049f_spi_write_cmd(0x8F);
	nv3049f_spi_write_data(0x13);
	nv3049f_spi_write_cmd(0x90);
	nv3049f_spi_write_data(0x5c);
	nv3049f_spi_write_cmd(0x91);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0x92);
	nv3049f_spi_write_data(0x60);
	nv3049f_spi_write_cmd(0x93);
	nv3049f_spi_write_data(0x80);
	nv3049f_spi_write_cmd(0x94);
	nv3049f_spi_write_data(0x82);
	nv3049f_spi_write_cmd(0x95);
	nv3049f_spi_write_data(0x13);
	nv3049f_spi_write_cmd(0x96);
	nv3049f_spi_write_data(0x5d);
	nv3049f_spi_write_cmd(0x97);
	nv3049f_spi_write_data(0x11);
	nv3049f_spi_write_cmd(0x98);
	nv3049f_spi_write_data(0x60);
	nv3049f_spi_write_cmd(0x99);
	nv3049f_spi_write_data(0x80);
	///////////////////////////GOA//////////////////////////////
	/////////////////////////GOA MAP//////////////////////
	nv3049f_spi_write_cmd(0xD0);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xD1);
	nv3049f_spi_write_data(0x1A);
	nv3049f_spi_write_cmd(0xD2);
	nv3049f_spi_write_data(0x0A);
	nv3049f_spi_write_cmd(0xD3);
	nv3049f_spi_write_data(0x0C);
	nv3049f_spi_write_cmd(0xD4);
	nv3049f_spi_write_data(0x04);
	nv3049f_spi_write_cmd(0xD5);
	nv3049f_spi_write_data(0x1B);
	nv3049f_spi_write_cmd(0xD6);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xD7);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xD8);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xD9);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xDA);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xDB);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xDC);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xDD);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xDE);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xDF);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE0);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE1);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE2);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xE3);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xE4);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xE5);
	nv3049f_spi_write_data(0x1f);
	nv3049f_spi_write_cmd(0xE6);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE7);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE8);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xE9);
	nv3049f_spi_write_data(0x1F);
	nv3049f_spi_write_cmd(0xEA);
	nv3049f_spi_write_data(0x1B);
	nv3049f_spi_write_cmd(0xEB);
	nv3049f_spi_write_data(0x05);
	nv3049f_spi_write_cmd(0xEC);
	nv3049f_spi_write_data(0x0D);
	nv3049f_spi_write_cmd(0xED);
	nv3049f_spi_write_data(0x0B);
	nv3049f_spi_write_cmd(0xEE);
	nv3049f_spi_write_data(0x1A);
	nv3049f_spi_write_cmd(0xEF);
	nv3049f_spi_write_data(0x1F);
	/////////////////////////GOA MAP//////////////////////
	//Write_LCD_REG(0x0000,0x00f7);
	//Write_LCD_REG(0x0000,0x0120);
	//Write_LCD_REG(0x0000,0x00f8);
	//Write_LCD_REG(0x0000,0x0120);
	//HL Temp test
	nv3049f_spi_write_cmd(0xf7);
	nv3049f_spi_write_data(0x50);
	nv3049f_spi_write_cmd(0xf8);
	nv3049f_spi_write_data(0x50);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x30);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x49);
	nv3049f_spi_write_cmd(0xFF);
	nv3049f_spi_write_data(0x00);
	nv3049f_spi_write_cmd(0xF1);
	nv3049f_spi_write_data(0xC4); //
	//Write_LCD_REG(0x0000,0x0035);
	//Write_LCD_REG(0x0000,0x0100);
	//Write_LCD_REG(0x0000,0x003A);
	//Write_LCD_REG(0x0000,0x0150);
	//nv3049f_spi_write_cmd(0x21);
	//nv3049f_spi_write_data(0x00);
	nv3049f_spi_write_cmd(0x11);
	nv3049f_spi_write_data(0x00);
	sunxi_lcd_delay_ms(200);
	nv3049f_spi_write_cmd(0x29);
	nv3049f_spi_write_data(0x00);
	sunxi_lcd_delay_ms(200);
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel nv3049f_panel = {
	/* panel driver name, must mach the lcd_drv_name in sys_config.fex */
	.name = "nv3049f",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
	},
};
