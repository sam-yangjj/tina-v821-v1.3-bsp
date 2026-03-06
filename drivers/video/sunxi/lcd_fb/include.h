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
#ifndef _INCLUDE_H
#define _INCLUDE_H

#define __LINUX_PLAT__
#include "asm-generic/int-ll64.h"
#include "linux/semaphore.h"
#include <asm/barrier.h>
#include <asm/div64.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/err.h> /* IS_ERR()??PTR_ERR() */
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h> /* kthread_create()??kthread_run() */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_iommu.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/version.h>
#include <linux/sched.h> /* wake_up_process() */
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <sunxi-clk.h>
#include <sunxi-gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/clk-provider.h>
#include <linux/spi/spidev.h>
#include <linux/spi/spi.h>
#include <linux/spi/sunxi-spi.h>

#include "lcd_fb_feature.h"

#define LCD_GAMMA_TABLE_SIZE (256 * sizeof(u32))
#define LCD_FB_DEBUG_LEVEL 0

#if LCD_FB_DEBUG_LEVEL == 1
#define LCDFB_INF(fmt, arg...)                                                                     \
	printk("sunxi:lcd_fb:[INFO]: %s,line:%d - " fmt, __func__, __LINE__, ##arg)
#define LCDFB_MSG(fmt, arg...)                                                                     \
	printk("sunxi:lcd_fb:[MSG ]: %s,line:%d - " fmt, __func__, __LINE__, ##arg)
#define LCDFB_HERE printk("[LCD_FB] %s,line:%d", __func__, __LINE__)
#define LCDFB_DBG(fmt, arg...)                                                                     \
	printk("sunxi:lcd_fb:[DBG ]: %s,line:%d - " fmt, __func__, __LINE__, ##arg)
#else
#define LCDFB_INF(fmt, arg...)
#define LCDFB_MSG(fmt, arg...)
#define LCDFB_HERE
#define LCDFB_DBG(fmt, arg...)
#endif

#define LCDFB_WRN(fmt, arg...) printk("sunxi:lcd_fb:[WARN]: " fmt, ##arg)
#define LCDFB_NOTE(fmt, arg...) printk("sunxi:lcd_fb:[NOTE]: " fmt, ##arg)

#if defined(SUPPORT_DBI_IF)
#define DBI_READ(dbi_mode) (dbi_mode |= (SUNXI_DBI_COMMAND_READ))
#define DBI_WRITE(dbi_mode) (dbi_mode &= ~(SUNXI_DBI_COMMAND_READ))
#define DBI_LSB_FIRST(dbi_mode) (dbi_mode |= SUNXI_DBI_LSB_FIRST)
#define DBI_MSB_FIRST(dbi_mode) (dbi_mode &= ~SUNXI_DBI_LSB_FIRST)
#define DBI_TR_VIDEO(dbi_mode) (dbi_mode |= SUNXI_DBI_TRANSMIT_VIDEO)
#define DBI_TR_COMMAND(dbi_mode) (dbi_mode &= ~(SUNXI_DBI_TRANSMIT_VIDEO))
#define DBI_DCX_DATA(dbi_mode) (dbi_mode |= SUNXI_DBI_DCX_DATA)
#define DBI_DCX_COMMAND(dbi_mode) (dbi_mode &= ~(SUNXI_DBI_DCX_DATA))
#endif

typedef void (*LCD_FUNC)(u32 sel);
struct sunxi_lcd_fb_disp_lcd_function {
	LCD_FUNC func;
	u32 delay;
};

#define LCD_MAX_SEQUENCES 7
struct sunxi_lcd_fb_disp_lcd_flow {
	struct sunxi_lcd_fb_disp_lcd_function func[LCD_MAX_SEQUENCES];
	u32 func_num;
	u32 cur_step;
};

struct sunxi_lcd_fb_panel_extend_para {
	u32 lcd_gamma_en;
	u32 lcd_gamma_tbl[256];
	u32 lcd_cmap_en;
	u32 lcd_cmap_tbl[2][3][4];
	u32 lcd_bright_curve_tbl[256];
};

struct sunxi_lcd_fb_disp_lcd_panel_fun {
	void (*cfg_panel_info)(struct sunxi_lcd_fb_panel_extend_para *info);
	int (*cfg_open_flow)(u32 sel);
	int (*cfg_close_flow)(u32 sel);
	int (*lcd_user_defined_func)(u32 sel, u32 para1, u32 para2, u32 para3);
	int (*set_bright)(u32 sel, u32 bright);
	int (*blank)(u32 sel, u32 en);
	int (*set_var)(u32 sel, struct fb_info *info);
	int (*set_addr_win)(u32 sel, int x, int y, int width, int height);
};

enum sunxi_lcd_fb_disp_lcd_if {
	LCD_FB_IF_SPI = 0,
	LCD_FB_IF_DBI = 1,
	LCD_FB_IF_QSPI = 2,
};

enum sunxi_lcd_fb_dbi_if {
	LCD_FB_L3I1 = 0x0,
	LCD_FB_L3I2 = 0x1,
	LCD_FB_L4I1 = 0x2,
	LCD_FB_L4I2 = 0x3,
	LCD_FB_D2LI = 0x4,
};

enum sunxi_lcd_fb_qspi_if {
	LCD_FB_QSPI_LANE1 = 0x0,
	LCD_FB_QSPI_LANE2 = 0x1,
	LCD_FB_QSPI_LANE4 = 0x2,
};

enum sunxi_lcd_fb_pixel_format {
	LCDFB_FORMAT_ARGB_8888 = 0x00, /* MSB  A-R-G-B  LSB */
	LCDFB_FORMAT_ABGR_8888 = 0x01,
	LCDFB_FORMAT_RGBA_8888 = 0x02,
	LCDFB_FORMAT_BGRA_8888 = 0x03,
	LCDFB_FORMAT_XRGB_8888 = 0x04,
	LCDFB_FORMAT_XBGR_8888 = 0x05,
	LCDFB_FORMAT_RGBX_8888 = 0x06,
	LCDFB_FORMAT_BGRX_8888 = 0x07,
	LCDFB_FORMAT_RGB_888 = 0x08,
	LCDFB_FORMAT_BGR_888 = 0x09,
	LCDFB_FORMAT_RGB_565 = 0x0a,
	LCDFB_FORMAT_BGR_565 = 0x0b,
	LCDFB_FORMAT_ARGB_4444 = 0x0c,
	LCDFB_FORMAT_ABGR_4444 = 0x0d,
	LCDFB_FORMAT_RGBA_4444 = 0x0e,
	LCDFB_FORMAT_BGRA_4444 = 0x0f,
	LCDFB_FORMAT_ARGB_1555 = 0x10,
	LCDFB_FORMAT_ABGR_1555 = 0x11,
	LCDFB_FORMAT_RGBA_5551 = 0x12,
	LCDFB_FORMAT_BGRA_5551 = 0x13,
};

enum sunxi_lcd_fb_dbi_fmt {
	LCDFB_DBI_RGB111 = 0x0,
	LCDFB_DBI_RGB444 = 0x1,
	LCDFB_DBI_RGB565 = 0x2,
	LCDFB_DBI_RGB666 = 0x3,
	LCDFB_DBI_RGB888 = 0x4,
};

struct sunxi_lcd_fb_disp_panel_para {
	enum sunxi_lcd_fb_disp_lcd_if lcd_if;
	enum sunxi_lcd_fb_dbi_if dbi_if;
	enum sunxi_lcd_fb_qspi_if qspi_if;

	u32 lcd_data_speed;
	u64 lcd_data_speed_hz;
	u32 lcd_x; /* horizontal resolution */
	u32 lcd_y; /* vertical resolution */
	u32 lcd_width; /* width of lcd in mm */
	u32 lcd_height; /* height of lcd in mm */

	u32 lcd_pwm_used;
	u32 lcd_pwm_ch;
	u32 lcd_pwm_freq;
	u32 lcd_pwm_pol;
	enum sunxi_lcd_fb_pixel_format lcd_pixel_fmt;
	enum sunxi_lcd_fb_dbi_fmt lcd_dbi_fmt;
	u32 lcd_dbi_clk_mode;
	u32 lcd_dbi_te;
	u32 fb_buffer_num;

	enum sunxi_dbi_src_seq lcd_rgb_order;
	u32 lcd_fps;

	u32 lcd_frm;
	u32 lcd_gamma_en;
	u32 lcd_bright_curve_en;
	u32 lines_per_transfer;

	u8 lcd_vsync_send_frame;

	u8 bpp;

	char lcd_size[8]; /* e.g. 7.9, 9.7 */
	char lcd_model_name[32];
};

struct sunxi_lcd_fb_device {
	struct list_head list;
	struct device *dev;
	struct device_node *panel_node;
	char name[32];
	u32 disp;

	void *priv_data;
	struct spi_device *spi_device;

	s32 (*init)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*exit)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*enable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*fake_enable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*disable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*is_enabled)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*get_resolution)(struct sunxi_lcd_fb_device *p_lcd, u32 *xres, u32 *yres);
	s32 (*get_dimensions)(struct sunxi_lcd_fb_device *dispdev, u32 *width, u32 *height);
	s32 (*set_color_temperature)(struct sunxi_lcd_fb_device *dispdev, s32 color_temperature);
	s32 (*get_color_temperature)(struct sunxi_lcd_fb_device *dispdev);
	s32 (*suspend)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*resume)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*set_bright)(struct sunxi_lcd_fb_device *p_lcd, u32 bright);
	s32 (*get_bright)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*backlight_enable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*backlight_disable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*pwm_enable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*pwm_disable)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*power_enable)(struct sunxi_lcd_fb_device *p_lcd, u32 power_id);
	s32 (*power_disable)(struct sunxi_lcd_fb_device *p_lcd, u32 power_id);
	s32 (*pin_cfg)(struct sunxi_lcd_fb_device *p_lcd, u32 bon);
	s32 (*set_gamma_tbl)(struct sunxi_lcd_fb_device *p_lcd, u32 *tbl, u32 size);
	s32 (*set_bright_dimming)(struct sunxi_lcd_fb_device *dispdev, u32 dimming);
	s32 (*enable_gamma)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*disable_gamma)(struct sunxi_lcd_fb_device *p_lcd);
	s32 (*set_panel_func)(struct sunxi_lcd_fb_device *lcd, char *name,
			      struct sunxi_lcd_fb_disp_lcd_panel_fun *lcd_cfg);
	s32 (*set_open_func)(struct sunxi_lcd_fb_device *lcd, LCD_FUNC func, u32 delay);
	s32 (*set_close_func)(struct sunxi_lcd_fb_device *lcd, LCD_FUNC func, u32 delay);
	int (*gpio_set_value)(struct sunxi_lcd_fb_device *p_lcd, u32 io_index, u32 value);
	int (*gpio_set_direction)(struct sunxi_lcd_fb_device *p_lcd, u32 io_index, u32 direction);
	int (*get_panel_info)(struct sunxi_lcd_fb_device *p_lcd,
			      struct sunxi_lcd_fb_disp_panel_para *info);
	int (*set_layer)(struct sunxi_lcd_fb_device *p_lcd, struct fb_info *p_info);
	int (*set_var)(struct sunxi_lcd_fb_device *p_lcd, struct fb_info *p_info);
	int (*blank)(struct sunxi_lcd_fb_device *p_lcd, u32 en);
	int (*cmd_write)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd);
	int (*para_write)(struct sunxi_lcd_fb_device *p_lcd, u8 para);
	int (*cmd_read)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd, u8 *rx_buf, u8 len);
	int (*qspi_cmd_write)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd);
	int (*qspi_para_write)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd, u8 para);
	int (*qspi_multi_para_write)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd, u8 *tx_buf,
				     u32 len);
	int (*qspi_cmd_read)(struct sunxi_lcd_fb_device *p_lcd, u8 cmd, u8 *rx_buf, u8 len);
	int (*wait_for_vsync)(struct sunxi_lcd_fb_device *p_lcd);
};

struct sunxi_lcd_fb_disp_source_ops {
	int (*sunxi_lcd_fb_delay_ms_ops)(u32 ms);
	int (*sunxi_lcd_fb_delay_us_ops)(u32 us);
	int (*sunxi_lcd_fb_backlight_enable_ops)(u32 screen_id);
	int (*sunxi_lcd_fb_backlight_disable_ops)(u32 screen_id);
	int (*sunxi_lcd_fb_pwm_enable_ops)(u32 screen_id);
	int (*sunxi_lcd_fb_pwm_disable_ops)(u32 screen_id);
	int (*sunxi_lcd_fb_power_enable_ops)(u32 screen_id, u32 pwr_id);
	int (*sunxi_lcd_fb_power_disable_ops)(u32 screen_id, u32 pwr_id);
	int (*sunxi_lcd_fb_set_panel_funs_ops)(char *drv_name,
					       struct sunxi_lcd_fb_disp_lcd_panel_fun *lcd_cfg);
	int (*sunxi_lcd_fb_pin_cfg_ops)(u32 screen_id, u32 bon);
	int (*sunxi_lcd_fb_gpio_set_value_ops)(u32 screen_id, u32 io_index, u32 value);
	int (*sunxi_lcd_fb_gpio_set_direction_ops)(u32 screen_id, u32 io_index, u32 direction);
	int (*sunxi_lcd_fb_cmd_write_ops)(u32 screen_id, u8 cmd);
	int (*sunxi_lcd_fb_para_write_ops)(u32 screen_id, u8 para);
	int (*sunxi_lcd_fb_cmd_read_ops)(u32 screen_id, u8 cmd, u8 *rx_buf, u8 len);
	int (*sunxi_lcd_fb_qspi_cmd_write_ops)(u32 screen_id, u8 cmd);
	int (*sunxi_lcd_fb_qspi_para_write_ops)(u32 screen_id, u8 cmd, u8 para);
	int (*sunxi_lcd_fb_qspi_multi_para_write_ops)(u32 screen_id, u8 cmd, u8 *tx_buf, u32 len);
	int (*sunxi_lcd_fb_qspi_cmd_read_ops)(u32 screen_id, u8 cmd, u8 *rx_buf, u8 len);
};

enum sunxi_lcd_fb_disp_return_value {
	DIS_SUCCESS = 0,
	DIS_FAIL = -1,
	DIS_PARA_FAILED = -2,
	DIS_PRIO_ERROR = -3,
	DIS_OBJ_NOT_INITED = -4,
	DIS_NOT_SUPPORT = -5,
	DIS_NO_RES = -6,
	DIS_OBJ_COLLISION = -7,
	DIS_DEV_NOT_INITED = -8,
	DIS_DEV_SRAM_COLLISION = -9,
	DIS_TASK_ERROR = -10,
	DIS_PRIO_COLLSION = -11
};

#endif /* End of file */
