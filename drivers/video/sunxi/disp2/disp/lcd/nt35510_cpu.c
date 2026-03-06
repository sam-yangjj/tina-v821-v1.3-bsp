/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
	lcd_used            = <1>;

	lcd_driver_name     = "nt35510_cpu";
	lcd_if              = <1>;

	lcd_x               = <480>;
	lcd_y               = <800>;
	lcd_width           = <43>;
	lcd_height          = <63>;

	lcd_dclk_freq       = <58>;

	lcd_hbp             = <35>;
	lcd_ht              = <650>;
	lcd_hspw            = <10>;
	lcd_vbp             = <55>;
	lcd_vt              = <1240>;
	lcd_vspw            = <20>;

	lcd_backlight       = <50>;

	lcd_pwm_used        = <0>;
	lcd_pwm_ch          = <4>;
	lcd_pwm_freq        = <50000>;
	lcd_pwm_pol         = <1>;
	lcd_pwm_max_limit   = <255>;
	lcd_bright_curve_en = <1>;

	lcd_frm             = <2>;
	lcd_gamma_en        = <0>;
	lcd_bright_curve_en = <0>;
	lcd_cmap_en         = <0>;

	lcdgamma4iep        = <22>;
	lcd_cpu_mode        = <1>;
	lcd_cpu_te          = <2>;
	lcd_cpu_if          = <12>;
*/

#include "nt35510_cpu.h"

#define CPU_TRI_MODE

#define DBG_INFO(format, args...)                                              \
	(printk("[NT35510 LCD INFO] LINE:%04d-->%s:" format, __LINE__,         \
		__func__, ##args))
#define DBG_ERR(format, args...)                                               \
	(printk("[NT35510 LCD ERR] LINE:%04d-->%s:" format, __LINE__,          \
		__func__, ##args))
#define panel_reset(val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define lcd_cs(val) sunxi_lcd_gpio_set_value(sel, 1, val)

static void lcd_panel_nt35510_init(u32 sel, struct disp_panel_para *info);
static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
#if defined(__DISP_TEMP_CODE__)
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
#endif
}

static s32 LCD_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, LCD_power_on, 120);
#ifdef CPU_TRI_MODE
	LCD_OPEN_FUNC(sel, LCD_panel_init, 100);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
#else
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
#endif
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 20);
#ifdef CPU_TRI_MODE
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 50);
#else
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
#endif
	LCD_CLOSE_FUNC(sel, LCD_power_off, 0);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/* config lcd_power pin to open lcd power0 */
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	/* lcd_cs, active low */
	lcd_cs(1);
	sunxi_lcd_delay_ms(10);
	/* lcd_rst, active hight */
	panel_reset(1);
	sunxi_lcd_delay_ms(10);

	sunxi_lcd_pin_cfg(sel, 0);
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

/* static int bootup_flag = 0; */
static void LCD_panel_init(u32 sel)
{
	struct disp_panel_para *info =
		kmalloc(sizeof(struct disp_panel_para), GFP_KERNEL);

	DBG_INFO("\n");
	bsp_disp_get_panel_info(sel, info);
	lcd_panel_nt35510_init(sel, info);

	kfree(info);
	return;
}

static void LCD_panel_exit(u32 sel)
{
}

static void sunxi_lcd_cpu_write_index_16b_8b(u32 sel, u32 i)
{
	sunxi_lcd_cpu_write_index(sel, (i >> 8) & 0xFF);
	sunxi_lcd_cpu_write_index(sel, i & 0xFF);
}

static void sunxi_lcd_cpu_write_data_8b(u32 sel, u32 i)
{
	sunxi_lcd_cpu_write_data(sel, i & 0xFF);
}

static void lcd_panel_nt35510_init(u32 sel, struct disp_panel_para *info)
{
	DBG_INFO("\n");
	/* lcd_cs, active low */
	lcd_cs(0);
	sunxi_lcd_delay_ms(5);
	panel_reset(1);
	sunxi_lcd_delay_ms(20);
	panel_reset(0);
	sunxi_lcd_delay_ms(20);
	panel_reset(1);
	sunxi_lcd_delay_ms(120);
	/* NT35510SH_BOE3.97IPS_24BIT_20141224 */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF00);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0055);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF02);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A5);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF03);
	sunxi_lcd_cpu_write_data_8b(0, 0x0080);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF200);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF201);
	sunxi_lcd_cpu_write_data_8b(0, 0x0084);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF202);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0x6F00);
	sunxi_lcd_cpu_write_data_8b(0, 0x000A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF400);
	sunxi_lcd_cpu_write_data_8b(0, 0x0013);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF00);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0055);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF02);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A5);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xFF03);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* Enable Page 1 */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF000);
	sunxi_lcd_cpu_write_data_8b(0, 0x0055);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF001);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF002);
	sunxi_lcd_cpu_write_data_8b(0, 0x0052);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF003);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF004);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);

	/* AVDD Set AVDD 5.2V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB000);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB001);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB002);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);

	/* AVDD ratio */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB600);
	sunxi_lcd_cpu_write_data_8b(0, 0x0046);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB601);
	sunxi_lcd_cpu_write_data_8b(0, 0x0046);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB602);
	sunxi_lcd_cpu_write_data_8b(0, 0x0046);

	/* AVEE  -5.2V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB100);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB101);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB102);
	sunxi_lcd_cpu_write_data_8b(0, 0x000C);

	/* AVEE ratio */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB700);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB701);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB702);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);

	/* VCL  -2.5V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB200);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB201);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB202);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* VCL ratio */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB800);
	sunxi_lcd_cpu_write_data_8b(0, 0x0034);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB801);
	sunxi_lcd_cpu_write_data_8b(0, 0x0034);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB802);
	sunxi_lcd_cpu_write_data_8b(0, 0x0034);

	/* VGH 15V  (Free pump) */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB300);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB301);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB302);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);

	/* VGH ratio */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB900);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB901);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB902);
	sunxi_lcd_cpu_write_data_8b(0, 0x0026);

	/* VGL_REG -10V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB500);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB501);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB502);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);

	/* sunxi_lcd_cpu_write_index_16b_8b(0, 0xC200);sunxi_lcd_cpu_write_data_8b(0, 0x0003); */

	/* VGLX ratio */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBA00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0036);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBA01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0036);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBA02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0036);

	/* VGMP/VGSP 4.5V/0V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0080);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* VGMN/VGSN -4.5V/0V */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0080);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* VCOM */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0055); /*  6A */

	/* Gamma Setting */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD100);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD101);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD102);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD103);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD104);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD105);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD106);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD107);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD108);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD109);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD10F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD110);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD111);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD112);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD113);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD114);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD115);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD116);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD117);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD118);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD119);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD11F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD120);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD121);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD122);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD123);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD124);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD125);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD126);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD127);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD128);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD129);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD12F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD130);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD131);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD132);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD133);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD200);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD201);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD202);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD203);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD204);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD205);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD206);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD207);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD208);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD209);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD20F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD210);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD211);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD212);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD213);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD214);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD215);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD216);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD217);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD218);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD219);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD21F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD220);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD221);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD222);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD223);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD224);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD225);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD226);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD227);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD228);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD229);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD22F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD230);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD231);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD232);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD233);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD300);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD301);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD302);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD303);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD304);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD305);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD306);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD307);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD308);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD309);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD30F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD310);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD311);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD312);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD313);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD314);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD315);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD316);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD317);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD318);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD319);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD31F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD320);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD321);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD322);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD323);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD324);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD325);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD326);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD327);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD328);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD329);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD32F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD330);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD331);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD332);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD333);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD400);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD401);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD402);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD403);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD404);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD405);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD406);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD407);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD408);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD409);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD40F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD410);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD411);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD412);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD413);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD414);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD415);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD416);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD417);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD418);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD419);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD41F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD420);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD421);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD422);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD423);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD424);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD425);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD426);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD427);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD428);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD429);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD42F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD430);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD431);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD432);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD433);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD500);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD501);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD502);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD503);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD504);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD505);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD506);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD507);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD508);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD509);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD50F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD510);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD511);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD512);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD513);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD514);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD515);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD516);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD517);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD518);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD519);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD51F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD520);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD521);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD522);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD523);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD524);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD525);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD526);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD527);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD528);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD529);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD52F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD530);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD531);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD532);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD533);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD600);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD601);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD602);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD603);
	sunxi_lcd_cpu_write_data_8b(0, 0x001C);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD604);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD605);
	sunxi_lcd_cpu_write_data_8b(0, 0x004E);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD606);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD607);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD608);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD609);
	sunxi_lcd_cpu_write_data_8b(0, 0x0085);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60B);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00C4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD60F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD610);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD611);
	sunxi_lcd_cpu_write_data_8b(0, 0x0023);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD612);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD613);
	sunxi_lcd_cpu_write_data_8b(0, 0x0061);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD614);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD615);
	sunxi_lcd_cpu_write_data_8b(0, 0x0094);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD616);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD617);
	sunxi_lcd_cpu_write_data_8b(0, 0x00E4);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD618);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD619);
	sunxi_lcd_cpu_write_data_8b(0, 0x0027);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0029);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61D);
	sunxi_lcd_cpu_write_data_8b(0, 0x0065);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD61F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00A6);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD620);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD621);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD622);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD623);
	sunxi_lcd_cpu_write_data_8b(0, 0x00FD);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD624);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD625);
	sunxi_lcd_cpu_write_data_8b(0, 0x001D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD626);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD627);
	sunxi_lcd_cpu_write_data_8b(0, 0x004D);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD628);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD629);
	sunxi_lcd_cpu_write_data_8b(0, 0x006A);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62A);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62B);
	sunxi_lcd_cpu_write_data_8b(0, 0x0095);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62C);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62D);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62E);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD62F);
	sunxi_lcd_cpu_write_data_8b(0, 0x00CB);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD630);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD631);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD632);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xD633);
	sunxi_lcd_cpu_write_data_8b(0, 0x00EF);

	/* Enable Page 0 */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF000);
	sunxi_lcd_cpu_write_data_8b(0, 0x0055);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF001);
	sunxi_lcd_cpu_write_data_8b(0, 0x00AA);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF002);
	sunxi_lcd_cpu_write_data_8b(0, 0x0052);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF003);
	sunxi_lcd_cpu_write_data_8b(0, 0x0008);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xF004);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* Display control */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB100);
	/*  MCU I/F & mipi cmd mode?CC, RGB I/F  & mipi video mode ?FC */
	sunxi_lcd_cpu_write_data_8b(0, 0x00CC);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB101);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB400);
	sunxi_lcd_cpu_write_data_8b(0, 0x0010);

	/* Source hold time */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB600);
	sunxi_lcd_cpu_write_data_8b(0, 0x0005);

	/* Gate EQ control */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB700);
	sunxi_lcd_cpu_write_data_8b(0, 0x0070);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB701);
	sunxi_lcd_cpu_write_data_8b(0, 0x0070);

	/* Source EQ control (Mode 2) */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB800);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB801);
	sunxi_lcd_cpu_write_data_8b(0, 0x0005);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB802);
	sunxi_lcd_cpu_write_data_8b(0, 0x0005);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xB803);
	sunxi_lcd_cpu_write_data_8b(0, 0x0005);

	/* Inversion mode  (C) */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0002); /* 02:2DOT */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBC02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	/* BOE's Setting (default) */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xCC00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0003);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xCC01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0050);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xCC02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0050);

	/* Porch Adjust */
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0084);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0007);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD03);
	sunxi_lcd_cpu_write_data_8b(0, 0x0031);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBD04);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0084);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0007);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE03);
	sunxi_lcd_cpu_write_data_8b(0, 0x0031);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBE04);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);

	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0001);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF01);
	sunxi_lcd_cpu_write_data_8b(0, 0x0084);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF02);
	sunxi_lcd_cpu_write_data_8b(0, 0x0007);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF03);
	sunxi_lcd_cpu_write_data_8b(0, 0x0031);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0xBF04);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
#if defined(CPU_TRI_MODE)
	sunxi_lcd_cpu_write_index_16b_8b(0, 0x3500);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
#endif
	sunxi_lcd_cpu_write_index_16b_8b(0, 0x3600);
	sunxi_lcd_cpu_write_data_8b(0, 0x0000);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0x3A00);
	sunxi_lcd_cpu_write_data_8b(0, 0x0066); /* 0x77:24BIT */

	sunxi_lcd_cpu_write_index_16b_8b(0, 0x1100);
	sunxi_lcd_delay_ms(120);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0x2900);
	sunxi_lcd_delay_ms(120);
	sunxi_lcd_cpu_write_index_16b_8b(0, 0x2c00);
}

/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
struct __lcd_panel nt35510_cpu_panel = {
	.name = "nt35510_cpu",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
	},
};
