/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "../../utility/vin_log.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("hzh");
MODULE_DESCRIPTION("A low-level driver for RN6752V1 sensors");
MODULE_LICENSE("GPL");

#define MCLK            (27000000)
#define V4L2_IDENT_SENSOR  0x2601
#define ID_VAL_HIGH		(0x26)
#define ID_VAL_LOW		(0x01)

// The MIS2009 i2c address
#define I2C_ADDR 		(0x58)
#define SENSOR_NAME 	"rn6752v1_mipi"

#define CVBS_W 			(720)
//#define USE_BLUE_SCREEN 	0
//#define USE_CVBS_ONE_FIELD	1
//#define USE_CVBS_FRAME_MODE 1

/*
 * The default register settings
 */
static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_cvbs_ntsc_regs[] = {
    {0xff, 0x08},
    {0x6c, 0x00},
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04},
	{0xDF, 0x0F}, // enable CVBS format
	{0x88, 0x40}, // disable SCLK1 out
	{0xFF, 0x00}, // ch0
	{0x00, 0x00}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x63}, // HD format
	{0x2A, 0x81}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x00}, // sync control
	{0x50, 0x00}, // D1 resolution
	{0x56, 0x01}, // 72M mode and BT656 mode
	{0x5F, 0x00}, // blank level
	{0x63, 0x75}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x00}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x02}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x5B, 0x00}, // H-scaling control
	{0x28, 0xB2}, // cropping
	{0x51, 0xE8},
	{0x52, 0x08},
	{0x53, 0x11},
	{0x5E, 0x08},
	{0x6A, 0x89},
#ifdef USE_CVBS_ONE_FIELD
	{0x20, 0x24},
	{0x23, 0x11},
	{0x24, 0x05},
	{0x25, 0x11},
	{0x26, 0x00},
	{0x42, 0x00},
#elif (defined USE_CVBS_FRAME_MODE)
	{0x3F, 0x90},
	{0x20, 0xA4},
	{0x23, 0x11},
	{0x24, 0xFF},
	{0x25, 0x00},
	{0x26, 0xFF},
	{0x42, 0x00},
#else
	{0x28, 0x92},
#endif
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x03}, // sharpness
	{0x57, 0x20}, // black/white stretch
	{0x68, 0x32}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#if USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10},	// video in detection
	{0x4A, 0xA8},   // video in detection
	{0x2E, 0x30},	// force no video
	{0x2E, 0x00},	// return to normal
#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x09},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x05},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif
#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28, 0x02},
	{0x29, 0x02},
	{0x2A, 0x01},
	{0x2B, 0x04},
	{0x32, 0x02},
	{0x33, 0x04},
	{0x34, 0x01},
	{0x35, 0x05},
	{0x36, 0x01},
	{0xAE, 0x09},
	{0x89, 0x0A},
	{0x88, 0x41},
#else
	{0x2B, 0x08},
	{0x34, 0x06},
	{0x35, 0x0B},
#endif
	{0x78, 0x68},
	{0x79, 0x01},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif
};

static struct regval_list sensor_cvbs_pal_regs[] = {
	{0xff, 0x08},
	{0x6c, 0x00},
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04},
	{0xDF, 0x0F}, // enable CVBS format
	{0x88, 0x40}, // disable {SCLK1 out
	{0xFF, 0x00}, // ch0
	{0x00, 0x00}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x62}, // HD format
	{0x2A, 0x81}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x00}, // sync control
	{0x50, 0x00}, // D1 resolution
	{0x56, 0x01}, // 72M mode and BT656 mode
	{0x5F, 0x00}, // blank level
	{0x63, 0x75}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x00}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x02}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x5B, 0x00}, // H-scaling control
	{0x28, 0xB2}, // cropping
	{0x53, 0x11},
	{0x54, 0x44},
	{0x55, 0x44},
	{0x5E, 0x08},
	{0x6A, 0x8C},
#ifdef USE_CVBS_ONE_FIELD
	{0x20, 0x24},
	{0x23, 0x17},
	{0x24, 0x37},
	{0x25, 0x17},
	{0x26, 0x00},
	{0x42, 0x00},
#elif (defined USE_CVBS_FRAME_MODE)
	{0x3F, 0x90},
	{0x20, 0xA4},
	{0x23, 0x17},
	{0x24, 0xFF},
	{0x25, 0x00},
	{0x26, 0xFF},
	{0x42, 0x00},
#else
	{0x28, 0x92},
#endif
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x03}, // sharpness
	{0x57, 0x20}, // black/white stretch
	{0x68, 0x32}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#if USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10}, // video in detection
	{0x4A, 0xA8}, // video in detection
	{0x2E, 0x30}, // force no video
	{0x2E, 0x00}, // return to normal
#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x09},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x05},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif
#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28, 0x02},
	{0x29, 0x02},
	{0x2A, 0x01},
	{0x2B, 0x04},
	{0x32, 0x02},
	{0x33, 0x04},
	{0x34, 0x01},
	{0x35, 0x05},
	{0x36, 0x01},
	{0xAE, 0x09},
	{0x89, 0x0A},
	{0x88, 0x41},
#else
	{0x2B, 0x08},
	{0x34, 0x06},
	{0x35, 0x0B},
#endif
	{0x78, 0x68},
	{0x79, 0x01},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
	#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif
};

static struct regval_list sensor_ahd_720P25_regs[] = {
	{0xff, 0x08},
	{0x6c, 0x00},
	{0x81, 0x01},
	{0xa3, 0x04},
	{0xdf, 0xfe},
	{0x88, 0x40},
	{0xff, 0x00},
	{0x00, 0x20},//20->80彩条模式
	{0x06, 0x08},
	{0x07, 0x63},
	{0x2a, 0x01},
	{0x3a, 0x24},
	{0x3f, 0x10},
	{0x4c, 0x37},
	{0x4f, 0x03},
	{0x50, 0x02},
	{0x56, 0x01},
	{0x5f, 0x40},
	{0x63, 0xf5},
	{0x59, 0x00},
	{0x5a, 0x42},
	{0x58, 0x01},
	{0x59, 0x33},
	{0x5a, 0x02},
	{0x58, 0x01},
	{0x51, 0xe1},
	{0x52, 0x88},
	{0x53, 0x12},
	{0x5b, 0x07},
	{0x5e, 0x08},
	{0x6a, 0x82},
	{0x28, 0x92},
	{0x03, 0x80},
	{0x04, 0x80},
	{0x05, 0x00},
	{0x57, 0x23},
	{0x68, 0x32},
	{0x37, 0x33},
	{0x61, 0x6c},
#if USE_BLUE_SCREEN
	{0x3a, 0x24},
#else
	{0x3a, 0x2c},
	{0x3b, 0x00},
	{0x3c, 0x80},
	{0x3d, 0x80},
#endif
	{0x33, 0x10},
	{0x4a, 0xa8},
	{0x2e, 0x30},
	{0x2e, 0x00},
#ifdef USE _DVP
	{0x8e, 0x00},
	{0x8f, 0x80},
	{0x8d, 0x31},
	{0x89, 0x09},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xff, 0x00},
	{0x56, 0x05},
	{0x3a, 0x24},
	{0x3e, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0b},
	{0x98, 0x00},
	{0x9a, 0x40},
	{0x9b, 0xe1},
	{0x9c, 0x00},
#endif
#else
	{0xff, 0x09},
	{0x00, 0x03},
	{0xff, 0x08},
	{0x04, 0x03},
	{0x6c, 0x11},
	{0x2b, 0x08},
	{0x34, 0x06},
	{0x35, 0x0b},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7c},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28, 0x02},
	{0x29, 0x02},
	{0x2a, 0x01},
	{0x2b, 0x04},
	{0x32, 0x02},
	{0x33, 0x04},
	{0x34, 0x01},
	{0x35, 0x05},
	{0x36, 0x01},
	{0xae, 0x09},
	{0x89, 0x0a},
	{0x88, 0x41},
#endif
#else
	{0x06, 0x4c},
#endif
	{0x21, 0x01},
	{0x78, 0x80},
	{0x79, 0x02},
	{0x6c, 0x01},
	{0x04, 0x00},
	{0x20, 0xaa},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
#else
	{0x07, 0x04},
#endif
	{0xff, 0x0a},
	{0x6c, 0x10},
#endif
};

static struct regval_list sensor_fhd_1080P25_regs[] = {
	{0xff, 0x08},
	{0x6c, 0x00},
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04}, //
	{0xDF, 0xFE}, // enable HD format
	{0xF0, 0xC0},
	{0x88, 0x40}, // disable SCLK1 out
	{0xFF, 0x00},
	{0x00, 0x20}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x63}, // HD format
	{0x2A, 0x01}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x03}, // sync control
	{0x50, 0x03}, // 1080p resolution
	{0x56, 0x02}, // 144M and BT656 mode
	{0x5F, 0x44}, // blank level
	{0x63, 0xF8}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x48}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x23}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x51, 0xF4}, // scale factor1
	{0x52, 0x29}, // scale factor2
	{0x53, 0x15}, // scale factor3
	{0x5B, 0x01}, // H-scaling control
	{0x5E, 0x08}, // enable H-scaling control
	{0x6A, 0x87}, // H-scaling control
	{0x28, 0x92}, // cropping
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x04}, // sharpness
	{0x57, 0x23}, // black/white stretch
	{0x68, 0x00}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#if USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10},	// video in detection
	{0x4A, 0xA8},   // video in detection
	{0x2E, 0x30},	// force no video
	{0x2E, 0x00},	// return to normal
#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x0A},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x06},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif
#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
	{0x34, 0x06},
	{0x35, 0x0B},
	{0x78, 0xC0},
	{0x79, 0x03},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
	#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif
};

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function , retrun -EINVAL
 */
static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	data_type frame_length = 0, act_vts = 0;

//	sensor_read(sd, 0x0d41, &frame_length);
//	act_vts = frame_length << 8;
//	sensor_read(sd, 0x0d42, &frame_length);
//	act_vts |= frame_length;
	fps->fps = 25;//wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				 struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
	break;

	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		usleep_range(10000, 12000);
	break;

	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);

		vin_gpio_set_status(sd, PWDN, 1);		// set PWDN output
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);

		vin_set_mclk(sd, ON);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);

		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);

		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);	// out high
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(50000, 51000);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(50000, 51000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(50000, 51000);
		cci_unlock(sd);
	break;

	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		cci_unlock(sd);
	break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(30000, 32000);
		break;

	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(60000, 62000);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	int i = 0;
	data_type rdval = 0;

	sensor_read(sd, 0xfe, &rdval);
	sensor_print("reg 0x%x = 0x%x\n", 0xfe, rdval);
#if 0
	while ((rdval != V4L2_IDENT_SENSOR) && (i < 2)) {
		sensor_read(sd, 0xfe, &rdval);
		sensor_print("reg 0x%x = 0x%x\n", 0xfe, rdval);
		i++;
	}
	if (rdval != V4L2_IDENT_SENSOR) {
		sensor_print("reg 0x%x = 0x%x\n", 0xfe, rdval);
		return -EINVAL;
	}
#endif
	return 0;
}
static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_print("sensor_init\n");

	/* Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed    = 0;
	info->width        = 1280;
	info->height       = 720;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = 25;

	return 0;
}

//static int sensor_initial(struct v4l2_subdev *sd)
//{
//	data_type rom_byte1, rom_byte2, rom_byte3, rom_byte4, rom_byte5, rom_byte6;
//	printk("sensor_initial\n");
//	sensor_write(sd, 0xE1, 0x80);
//	sensor_write(sd, 0xFA, 0x81);
//	sensor_read (sd, 0xFB, &rom_byte1);
//	sensor_read (sd, 0xFB, &rom_byte2);
//	sensor_read (sd, 0xFB, &rom_byte3);
//	sensor_read (sd, 0xFB, &rom_byte4);
//	sensor_read (sd, 0xFB, &rom_byte5);
//	sensor_read (sd, 0xFB, &rom_byte6);
//
//	// config. decoder accroding to rom_byte5 and rom_byte6
//	if ((rom_byte6 == 0x00) && (rom_byte5 == 0x00)) {
//		sensor_write(sd, 0xEF, 0xAA);
//		sensor_write(sd, 0xE7, 0xFF);
//		sensor_write(sd, 0xFF, 0x09);
//		sensor_write(sd, 0x03, 0x0C);
//		sensor_write(sd, 0xFF, 0x0B);
//		sensor_write(sd, 0x03, 0x0C);
//	} else if (((rom_byte6 == 0x34) && (rom_byte5 == 0xA9)) ||
//		 ((rom_byte6 == 0x2C) && (rom_byte5 == 0xA8))) {
//		sensor_write(sd, 0xEF, 0xAA);
//		sensor_write(sd, 0xE7, 0xFF);
//		sensor_write(sd, 0xFC, 0x60);
//		sensor_write(sd, 0xFF, 0x09);
//		sensor_write(sd, 0x03, 0x18);
//		sensor_write(sd, 0xFF, 0x0B);
//		sensor_write(sd, 0x03, 0x18);
//	} else {
//		sensor_write(sd, 0xEF, 0xAA);
//		sensor_write(sd, 0xFC, 0x60);
//		sensor_write(sd, 0xFF, 0x09);
//		sensor_write(sd, 0x03, 0x18);
//		sensor_write(sd, 0xFF, 0x0B);
//		sensor_write(sd, 0x03, 0x18);
//	}
//
//	return 0;
//
//}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(*info->current_wins));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc      = "BT656 4CH",
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		.regs      = sensor_default_regs,
		.regs_size = ARRAY_SIZE(sensor_default_regs),
		.bpp       = 2
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if 1
	{
		.width      = CVBS_W,
		.height     = 240,
		.hoffset    = 0,
		.voffset    = 0,
		.pclk		= 567 * 1000 * 1000,
		.mipi_bps	= 567 * 1000 * 1000,
		.fps_fixed  = 25,
		.regs       = sensor_cvbs_ntsc_regs,
		.regs_size  = ARRAY_SIZE(sensor_cvbs_ntsc_regs),
		.set_size   = NULL,
	},
	{
		.width		= CVBS_W,
		.height 	= 576,
		.hoffset	= 0,
		.voffset	= 0,
		.pclk		= 567 * 1000 * 1000,
		.mipi_bps	= 567 * 1000 * 1000,
		.fps_fixed	= 25,
		.regs		= sensor_cvbs_pal_regs,
		.regs_size	= ARRAY_SIZE(sensor_cvbs_pal_regs),
		.set_size	= NULL,
	},
#endif
	{
		.width		= 1280,
		.height 	= 720,
		.hoffset	= 0,
		.voffset	= 0,
		.pclk		= 567 * 1000 * 1000,
		.mipi_bps	= 567 * 1000 * 1000,
		.fps_fixed	= 25,
		.regs		= sensor_ahd_720P25_regs,
		.regs_size	= ARRAY_SIZE(sensor_ahd_720P25_regs),
		.set_size	= NULL,
	},
	{
		.width		= 1920,
		.height 	= 1080,
		.hoffset	= 0,
		.voffset	= 0,
		.pclk		= 567 * 1000 * 1000,
		.mipi_bps	= 567 * 1000 * 1000,
		.fps_fixed	= 25,
		.regs		= sensor_fhd_1080P25_regs,
		.regs_size	= ARRAY_SIZE(sensor_fhd_1080P25_regs),
		.set_size	= NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type  = V4L2_MBUS_CSI2_DPHY;
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
		container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	}

	return 0;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("sensor_reg_init\n");
	//sensor_initial(sd);
	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}
	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->fmt = sensor_fmt;
	info->width = wsize->width;
	info->height = wsize->height;
	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
		      wsize->height);
	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
			 info->current_wins->width, info->current_wins->height,
			 info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable)
		return 0;

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */
static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
	/* .queryctrl = sensor_queryctrl, */
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	.g_mbus_config = sensor_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.enum_frame_interval = sensor_enum_frame_interval,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/*  ----------------------------------------------------------------------- */
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
				  256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
				  65536 * 16, 1, 1);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int sensor_probe(struct i2c_client *client)
#else
static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
#endif
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);
	sensor_init_controls(sd, &sensor_ctrl_ops);
	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;//V4L2_FIELD_INTERLACED;//V4L2_FIELD_NONE;
	/*  info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE; */
	info->combo_mode = CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int sensor_remove(struct i2c_client *client)
#else
static void sensor_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd;
	sd = cci_dev_remove_helper(client, &cci_drv);
	kfree(to_state(sd));
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = SENSOR_NAME,
		   },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	return cci_dev_init_helper(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	cci_dev_exit_helper(&sensor_driver);
}

VIN_INIT_DRIVERS(init_sensor);
module_exit(exit_sensor);
