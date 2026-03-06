/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for sc1346 Raw cameras.
 *
 * Copyright (c) 2023 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
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

MODULE_AUTHOR("cwm");
MODULE_DESCRIPTION("A low-level driver for SC1346_MIPI sensors");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
#define MCLK              (24*1000*1000)
#else
#define MCLK              (27*1000*1000)
#endif
#define V4L2_IDENT_SENSOR 0xda4d

#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
#define TARGET_I2C_ADDR 0x64
#define SC1346_I2C_ADDR_UPDATE 0x3004
#endif

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

#define HDR_RATIO 16

/*
 * The SC1346_MIPI i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc1346_mipi"
#define SENSOR_NAME_2 "sc1346_mipi_2"

static int sc1346_fps_change_flag;

#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
/*
	two sc1346 multiplex IIC:

	one should control RESET for itself(by hardware),
		it is must to do sensor_detect and change it's IIC addr firstly,
		it is not need to control RESET in sensor_power.

		another one control RESET by SOC.
*/
#define RESET_CONTROL_BY_SOC SENSOR_NAME_2
#define RESET_CONTROL_BY_ITSELF SENSOR_NAME
#endif

#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
static int sc1346_synchr_max_exp_line;
typedef enum sensor_output_mode {
	MASTER = 0,
	SLAVE = 1
} sensor_output_mode;
struct sc1346_master_slave_register {
	sensor_output_mode sensor_mode;
	char sensor_name[16];
};
static struct sc1346_master_slave_register sc1346_output_register[2] = {
	{MASTER, SENSOR_NAME_2},
	{SLAVE,  SENSOR_NAME},
};

static struct regval_list sensor_master_mode_regs[] = {
#if !IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
	{0x0103, 0x01},
#endif
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x300a, 0x24},
	{0x301f, 0x32},
	{0x3032, 0xa0},
	{0x3106, 0x05},
	{0x320c, 0x06},
	{0x320d, 0x40},
	{0x320e, 0x0b},
	{0x320f, 0xb8},
	{0x322e, 0x0b},
	{0x322f, 0xb4},
	{0x3301, 0x0b},
	{0x3303, 0x10},
	{0x3306, 0x50},
	{0x3308, 0x0a},
	{0x330a, 0x00},
	{0x330b, 0xda},
	{0x330e, 0x0a},
	{0x331e, 0x61},
	{0x331f, 0xa1},
	{0x3320, 0x04},
	{0x3327, 0x08},
	{0x3329, 0x09},
	{0x3364, 0x1f},
	{0x3390, 0x09},
	{0x3391, 0x0f},
	{0x3392, 0x1f},
	{0x3393, 0x30},
	{0x3394, 0xff},
	{0x3395, 0xff},
	{0x33ad, 0x10},
	{0x33b3, 0x40},
	{0x33f9, 0x50},
	{0x33fb, 0x80},
	{0x33fc, 0x09},
	{0x33fd, 0x0f},
	{0x349f, 0x03},
	{0x34a6, 0x09},
	{0x34a7, 0x0f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x0c},
	{0x3630, 0xe2},
	{0x3632, 0x76},
	{0x3633, 0x33},
	{0x3639, 0xf4},
	{0x3641, 0x28},
	{0x3670, 0x09},
	{0x3674, 0xe2},
	{0x3675, 0xea},
	{0x3676, 0xea},
	{0x367c, 0x09},
	{0x367d, 0x0f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x32},
	{0x3698, 0x88},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xd1},
	{0x369c, 0x09},
	{0x369d, 0x0f},
	{0x36a2, 0x09},
	{0x36a3, 0x0b},
	{0x36a4, 0x0f},
	{0x36d0, 0x01},
	{0x36ea, 0x06},
	{0x36eb, 0x05},
	{0x36ec, 0x05},
	{0x36ed, 0x28},
	{0x370f, 0x01},
	{0x3722, 0x41},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3727, 0x14},
	{0x3728, 0x00},
	{0x37b0, 0x21},
	{0x37b1, 0x21},
	{0x37b2, 0x37},
	{0x37b3, 0x09},
	{0x37b4, 0x0f},
	{0x37fa, 0x0c},
	{0x37fb, 0x31},
	{0x37fc, 0x01},
	{0x37fd, 0x17},
	{0x3903, 0x40},
	{0x3904, 0x04},
	{0x3905, 0x8d},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391f, 0x41},
	{0x3933, 0x80},
	{0x3934, 0x0a},
	{0x3935, 0x00},
	{0x3936, 0xd6},
	{0x3937, 0x73},
	{0x3938, 0x75},
	{0x3939, 0x0f},
	{0x393a, 0xef},
	{0x393b, 0x0f},
	{0x393c, 0xcd},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x20},
	{0x4509, 0x25},
	{0x450d, 0x28},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x13},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5780, 0x66},
	{0x578d, 0x40},
	{0x36e9, 0x24},
	{0x37f9, 0x20},
	{0x0100, 0x01},
};

static struct regval_list sensor_slave_mode_regs[] = {
#if !IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
	{0x0103, 0x01},
#endif
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x300a, 0x22},
	{0x301f, 0x31},
	{0x3106, 0x05},
	{0x320c, 0x06},
	{0x320d, 0x40},
	{0x320e, 0x0b},
	{0x320f, 0xb8},
	{0x3222, 0x02},
	{0x3224, 0x96},
	{0x3225, 0x10},
	{0x322e, 0x0b},
	{0x322f, 0xb4},
	{0x3230, 0x00},
	{0x3231, 0x04},
	{0x3301, 0x0b},
	{0x3303, 0x10},
	{0x3306, 0x50},
	{0x3308, 0x0a},
	{0x330a, 0x00},
	{0x330b, 0xda},
	{0x330e, 0x0a},
	{0x331e, 0x61},
	{0x331f, 0xa1},
	{0x3320, 0x04},
	{0x3327, 0x08},
	{0x3329, 0x09},
	{0x3364, 0x1f},
	{0x3390, 0x09},
	{0x3391, 0x0f},
	{0x3392, 0x1f},
	{0x3393, 0x30},
	{0x3394, 0xff},
	{0x3395, 0xff},
	{0x33ad, 0x10},
	{0x33b3, 0x40},
	{0x33f9, 0x50},
	{0x33fb, 0x80},
	{0x33fc, 0x09},
	{0x33fd, 0x0f},
	{0x349f, 0x03},
	{0x34a6, 0x09},
	{0x34a7, 0x0f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34aa, 0x00},
	{0x34ab, 0xe8},
	{0x34ac, 0x01},
	{0x34ad, 0x0c},
	{0x3630, 0xe2},
	{0x3632, 0x76},
	{0x3633, 0x33},
	{0x3639, 0xf4},
	{0x3641, 0x28},
	{0x3670, 0x09},
	{0x3674, 0xe2},
	{0x3675, 0xea},
	{0x3676, 0xea},
	{0x367c, 0x09},
	{0x367d, 0x0f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x32},
	{0x3698, 0x88},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xd1},
	{0x369c, 0x09},
	{0x369d, 0x0f},
	{0x36a2, 0x09},
	{0x36a3, 0x0b},
	{0x36a4, 0x0f},
	{0x36d0, 0x01},
	{0x36ea, 0x06},
	{0x36eb, 0x05},
	{0x36ec, 0x05},
	{0x36ed, 0x28},
	{0x370f, 0x01},
	{0x3722, 0x41},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3727, 0x14},
	{0x3728, 0x00},
	{0x37b0, 0x21},
	{0x37b1, 0x21},
	{0x37b2, 0x37},
	{0x37b3, 0x09},
	{0x37b4, 0x0f},
	{0x37fa, 0x0c},
	{0x37fb, 0x31},
	{0x37fc, 0x01},
	{0x37fd, 0x17},
	{0x3903, 0x40},
	{0x3904, 0x04},
	{0x3905, 0x8d},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391f, 0x41},
	{0x3933, 0x80},
	{0x3934, 0x0a},
	{0x3935, 0x00},
	{0x3936, 0xd6},
	{0x3937, 0x73},
	{0x3938, 0x75},
	{0x3939, 0x0f},
	{0x393a, 0xef},
	{0x393b, 0x0f},
	{0x393c, 0xcd},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x20},
	{0x4509, 0x25},
	{0x450d, 0x28},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x13},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5780, 0x66},
	{0x578d, 0x40},
	{0x36e9, 0x24},
	{0x37f9, 0x20},
	{0x0100, 0x01},
};
#endif

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

#if !IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
static struct regval_list sensor_720p10b15_regs[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x36e9, 0x80},
    {0x37f9, 0x80},
    {0x301f, 0x01},
    {0x3106, 0x05},
	{0x320c, 0x06},//HTS=1650
	{0x320d, 0x72},
	{0x320e, 0x05},//VTS=1500
	{0x320f, 0xDC},
    {0x3301, 0x06},
    {0x3306, 0x50},
    {0x3308, 0x0a},
    {0x330a, 0x00},
    {0x330b, 0xda},
    {0x330e, 0x0a},
    {0x331e, 0x61},
    {0x331f, 0xa1},
    {0x3364, 0x1f},
    {0x3390, 0x09},
    {0x3391, 0x0f},
    {0x3392, 0x1f},
    {0x3393, 0x30},
    {0x3394, 0x30},
    {0x3395, 0x30},
    {0x33ad, 0x10},
    {0x33b3, 0x40},
    {0x33f9, 0x50},
    {0x33fb, 0x80},
    {0x33fc, 0x09},
    {0x33fd, 0x0f},
    {0x349f, 0x03},
    {0x34a6, 0x09},
    {0x34a7, 0x0f},
    {0x34a8, 0x40},
    {0x34a9, 0x30},
    {0x34aa, 0x00},
    {0x34ab, 0xe8},
    {0x34ac, 0x01},
    {0x34ad, 0x0c},
    {0x3630, 0xe2},
    {0x3632, 0x76},
    {0x3633, 0x33},
    {0x3639, 0xf4},
    {0x3641, 0x00},
    {0x3670, 0x09},
    {0x3674, 0xe2},
    {0x3675, 0xea},
    {0x3651, 0x7f},
    {0x3676, 0xea},
    {0x367c, 0x09},
    {0x367d, 0x0f},
    {0x3690, 0x22},
    {0x3691, 0x22},
    {0x3692, 0x22},
    {0x3698, 0x88},
    {0x3699, 0x90},
    {0x369a, 0xa1},
    {0x369b, 0xc3},
    {0x369c, 0x09},
    {0x369d, 0x0f},
    {0x36a2, 0x09},
    {0x36a3, 0x0b},
    {0x36a4, 0x0f},
    {0x36d0, 0x01},
    {0x370f, 0x01},
    {0x3722, 0x41},
    {0x3724, 0x41},
    {0x3725, 0xc1},
    {0x3728, 0x00},
    {0x37b0, 0x41},
    {0x37b1, 0x41},
    {0x37b2, 0x47},
    {0x37b3, 0x09},
    {0x37b4, 0x0f},
    {0x3903, 0x40},
    {0x3904, 0x04},
    {0x3905, 0x8d},
    {0x3907, 0x00},
    {0x3908, 0x41},
    {0x391f, 0x41},
    {0x3933, 0x80},
    {0x3934, 0x02},
    {0x3937, 0x74},
    {0x3939, 0x0f},
    {0x393a, 0xd4},
    {0x3e01, 0x2e},
    {0x3e02, 0xa0},
	{0x4800, 0x44},//continue
	{0x3250, 0x40},
    {0x440e, 0x02},
    {0x4509, 0x20},
    {0x450d, 0x28},
    {0x5780, 0x66},
    {0x578d, 0x40},
	{0x4800, 0x44},//continue
	{0x3250, 0x40},
    {0x36e9, 0x20},
    {0x37f9, 0x20},
    {0x0100, 0x01},
};

static struct regval_list sensor_720p10b30_regs[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x36e9, 0x80},
    {0x37f9, 0x80},
    {0x301f, 0x01},
    {0x3106, 0x05},
	{0x320c, 0x06},//HTS=1650
	{0x320d, 0x72},
	{0x320e, 0x02},//VTS=750
	{0x320f, 0xee},
    {0x3301, 0x06},
    {0x3306, 0x50},
    {0x3308, 0x0a},
    {0x330a, 0x00},
    {0x330b, 0xda},
    {0x330e, 0x0a},
    {0x331e, 0x61},
    {0x331f, 0xa1},
    {0x3364, 0x1f},
    {0x3390, 0x09},
    {0x3391, 0x0f},
    {0x3392, 0x1f},
    {0x3393, 0x30},
    {0x3394, 0x30},
    {0x3395, 0x30},
    {0x33ad, 0x10},
    {0x33b3, 0x40},
    {0x33f9, 0x50},
    {0x33fb, 0x80},
    {0x33fc, 0x09},
    {0x33fd, 0x0f},
    {0x349f, 0x03},
    {0x34a6, 0x09},
    {0x34a7, 0x0f},
    {0x34a8, 0x40},
    {0x34a9, 0x30},
    {0x34aa, 0x00},
    {0x34ab, 0xe8},
    {0x34ac, 0x01},
    {0x34ad, 0x0c},
    {0x3630, 0xe2},
    {0x3632, 0x76},
    {0x3633, 0x33},
    {0x3639, 0xf4},
    {0x3641, 0x00},
    {0x3670, 0x09},
    {0x3674, 0xe2},
    {0x3675, 0xea},
    {0x3651, 0x7f},
    {0x3676, 0xea},
    {0x367c, 0x09},
    {0x367d, 0x0f},
    {0x3690, 0x22},
    {0x3691, 0x22},
    {0x3692, 0x22},
    {0x3698, 0x88},
    {0x3699, 0x90},
    {0x369a, 0xa1},
    {0x369b, 0xc3},
    {0x369c, 0x09},
    {0x369d, 0x0f},
    {0x36a2, 0x09},
    {0x36a3, 0x0b},
    {0x36a4, 0x0f},
    {0x36d0, 0x01},
    {0x370f, 0x01},
    {0x3722, 0x41},
    {0x3724, 0x41},
    {0x3725, 0xc1},
    {0x3728, 0x00},
    {0x37b0, 0x41},
    {0x37b1, 0x41},
    {0x37b2, 0x47},
    {0x37b3, 0x09},
    {0x37b4, 0x0f},
    {0x3903, 0x40},
    {0x3904, 0x04},
    {0x3905, 0x8d},
    {0x3907, 0x00},
    {0x3908, 0x41},
    {0x391f, 0x41},
    {0x3933, 0x80},
    {0x3934, 0x02},
    {0x3937, 0x74},
    {0x3939, 0x0f},
    {0x393a, 0xd4},
    {0x3e01, 0x2e},
    {0x3e02, 0xa0},
	{0x4800, 0x44},//continue
	{0x3250, 0x40},
    {0x440e, 0x02},
    {0x4509, 0x20},
    {0x450d, 0x28},
    {0x5780, 0x66},
    {0x578d, 0x40},
	{0x4800, 0x44},//continue
	{0x3250, 0x40},
    {0x36e9, 0x20},
    {0x37f9, 0x20},
    {0x0100, 0x01},
};
#endif

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
static int sensor_i2c_addr_get(struct v4l2_subdev *sd, unsigned int *iic_addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (client == NULL) {
		sensor_err("client is NULL!!!\n");
		return -1;
	}
	/* i2c addr useful for 7bit */
	*iic_addr = client->addr << 1;

	return 0;
}

static int sensor_i2c_addr_set(struct v4l2_subdev *sd, unsigned int iic_addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (client == NULL) {
		sensor_err("client is NULL!!!\n");
		return -1;
	}
	/* i2c addr useful for 7bit */
	client->addr = iic_addr >> 1;

	return 0;
}
#endif

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sc1346_sensor_vts;
//static int sc1346_sensor_svr;
//static int shutter_delay = 1;
//static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

    exphigh = (unsigned char) (0xf & (exp_val>>16));
    expmid = (unsigned char) (0xff & (exp_val>>8));
    explow = (unsigned char) (0xf0 & exp_val);

    sensor_write(sd, 0x3e02, explow);
    sensor_write(sd, 0x3e01, expmid);
    sensor_write(sd, 0x3e00, exphigh);
    sensor_dbg("sensor_set_exp = %d line Done! exphigh %d expmid %d explow %d\n", exp_val, exphigh, expmid, explow);
    info->exp = exp_val;
    return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	data_type anagain = 0x00;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;
	int gain_tmp;

	gain_tmp = gain_val << 3;
	if (gain_val < 32) {// 16 * 2
		anagain = 0x00;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp;
	} else if (gain_val < 64) {//16 * 4
		anagain = 0x08;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200/ 1;
	} else if (gain_val < 128) {//16 * 8
		anagain = 0x09;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 2;
	} else if (gain_val < 256) {//16 * 16
		anagain = 0x0b;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 4;
	} else if (gain_val < 512) {//16 * 32
		anagain = 0x0f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 8;
	} else if (gain_val < 1024) {//16 * 32 * 2
		anagain = 0x1f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 16;
	} else if (gain_val < 2048) {//16 * 32 * 4
		anagain = 0x1f;
		gaindighigh = 0x01;
		gaindiglow = gain_tmp * 100 / 200 / 32;
	} else if (gain_val < 4096) {//16 * 32 * 8
		anagain = 0x1f;
		gaindighigh = 0x03;
		gaindiglow = gain_tmp * 100 / 200 / 64;
	} else if (gain_val < 8192) {//16 * 32 * 16
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = gain_tmp * 100 / 200 / 128;
	} else {
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = 0xfc;
	}

	sensor_write(sd, 0x3e09, (unsigned char)anagain);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x, 0x%x Done!\n", gain_val, anagain, gaindighigh, gaindiglow);
	//sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
#if !IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	int shutter, frame_length;
#endif
	int exp_val, gain_val;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16)
		gain_val = 16;

#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	if (exp_val >= sc1346_synchr_max_exp_line)
		exp_val = sc1346_synchr_max_exp_line;
#else
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;
#endif

#if !IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	if (!sc1346_fps_change_flag) {
		shutter = exp_val >> 4;
		if (shutter > sc1346_sensor_vts - 6) {//12
			frame_length = shutter + 6;
		} else
			frame_length = sc1346_sensor_vts;
		sensor_write(sd, 0x320f, (frame_length & 0xff));
		sensor_write(sd, 0x320e, (frame_length >> 8));

//		sensor_read(sd, 0x320e, &read_high);
//		sensor_read(sd, 0x320f, &read_low);
//		sensor_print("0x41 = 0x%x, 0x42 = 0x%x\n", read_high, read_low);
	}
#endif
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);
	info->exp = exp_val;
	info->gain = gain_val;

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
    data_type get_value;
    data_type set_value;

    sensor_print("into set sensor hfilp the value:%d \n", enable);
    if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	if (enable)
		set_value = get_value | 0x06;
	else
		set_value = get_value & 0xf9;

	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	sensor_print("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	if (enable)
		set_value = get_value | 0x60;
	else
		set_value = get_value & 0x9f;

	sensor_write(sd, 0x3221, set_value);
	return 0;

}

 static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("-- read value:0x%X --\n", get_value);
	switch (get_value & 0x66) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x06:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x60:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x66:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	data_type frame_length = 0, act_vts = 0;

	sensor_read(sd, 0x320e, &frame_length);
	act_vts = frame_length << 8;
	sensor_read(sd, 0x320f, &frame_length);
	act_vts |= frame_length;
	fps->fps = wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

// #define PRINT_VTS
static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	int sc1346_sensor_target_vts = 0;
#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	unsigned int row_total = 0;
#endif

	sc1346_fps_change_flag = 1;
#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	if (fps->fps == 15) {
			sc1346_sensor_target_vts = 3000;
	} else if (fps->fps == 14) {
			sc1346_sensor_target_vts = 3151;
	}
	/* row_total = active_row - blank_row = vts - rb_row*/
	/* rb_row = {0x3230, 0x03231} */
	row_total = sc1346_sensor_target_vts - 4;
#else
	sc1346_sensor_target_vts = wsize->pclk / fps->fps / wsize->hts;
	if (sc1346_sensor_target_vts <= wsize->vts) {// the max fps = 20fps
		sc1346_sensor_target_vts = wsize->vts;
	} else if (sc1346_sensor_target_vts >= (wsize->pclk/wsize->hts)) {// the min fps = 1fps
		sc1346_sensor_target_vts = (wsize->pclk / wsize->hts) - 6;
	}
#endif

	sc1346_sensor_vts = sc1346_sensor_target_vts;
	sensor_dbg("target_fps = %d, sc1346_sensor_target_vts = %d, 0x320e = 0x%x, 0x320f = 0x%x\n", fps->fps,
		sc1346_sensor_target_vts, sc1346_sensor_target_vts >> 8, sc1346_sensor_target_vts & 0xff);
	sensor_write(sd, 0x320f, (sc1346_sensor_target_vts & 0xff));
	sensor_write(sd, 0x320e, (sc1346_sensor_target_vts >> 8));
#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	sensor_write(sd, 0x322f, (row_total & 0xff));
	sensor_write(sd, 0x322e, (row_total >> 8));
#endif
	sensor_print("%s --> %dfps\n", __func__, fps->fps);

#ifdef PRINT_VTS
	data_type read_high, read_low;
	sensor_read(sd, 0x320e, &read_high);
	sensor_read(sd, 0x320f, &read_low);
	sensor_print("[get write_times] 0x320e = 0x%x, 0x320f = 0x%x\n", read_high, read_low);
	sensor_read(sd, 0x322e, &read_high);
	sensor_read(sd, 0x322f, &read_low);
	sensor_print("[get write_times] 0x322e = 0x%x, 0x322f = 0x%x\n", read_high, read_low);
	sc1346_fps_change_flag = 0;
#endif

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
    int ret = 0;
    data_type rdval;

//  ret = sensor_read(sd, 0x0100, &rdval);
//  if (ret != 0)
//      return ret;
//
//  if (on_off == STBY_ON)
//      ret = sensor_write(sd, 0x0100, rdval&0xfe);
//  else
//      ret = sensor_write(sd, 0x0100, rdval|0x01);
    return ret;
}

static int sensor_get_temp(struct v4l2_subdev *sd,
				struct sensor_temp *temp)
{
	data_type rdval_high = 0, rdval_low = 0;
	int rdval_total = 0;
	int ret = 0;

	ret = sensor_read(sd, 0x3911, &rdval_high);
	ret = sensor_read(sd, 0x3912, &rdval_low);
	rdval_total |= rdval_high << 8;
	rdval_total |= rdval_low;
	sensor_dbg("rdval_total = 0x%x\n", rdval_total);
	temp->temp = ((rdval_total - 240) / 15) + 55;

	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
static bool sensor_power_flag;
static bool sensor_update_2ic_addr;
#endif
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
		if (!strcmp(sd->name, RESET_CONTROL_BY_SOC)) {
#endif
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
		} else {
			if (!sensor_power_flag) {
				sensor_power_flag = true;
				sensor_print("%s will not control RESET, but enable MCLK\n", sd->name);
			}
		}
#endif
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(30000, 32000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
		if (!strcmp(sd->name, RESET_CONTROL_BY_SOC)) {
#endif
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
#if !IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
		vin_set_mclk(sd, OFF);
#else
		sensor_print("It will not disable MCLK\n");
#endif
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
		} else {
			sensor_print("%s will not do poweroff\n", sd->name);
		}
#endif
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
		usleep_range(1000, 1200);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	int cnt = 0;

	sensor_read(sd, 0x3107, &rdval);
	SENSOR_ID |= (rdval << 8);
	sensor_read(sd, 0x3108, &rdval);
	SENSOR_ID |= (rdval);
	sensor_print("== sc1346 V4L2_IDENT_SENSOR = 0x%x ==\n", SENSOR_ID);

	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		sensor_read(sd, 0x3107, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, 0x3108, &rdval);
		SENSOR_ID |= (rdval);
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
	}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

	sensor_read(sd, 0x3004, &rdval);
	sensor_print("0x3004 = 0x%x\n", rdval);

	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int iic_addr;
	data_type rdval;
#endif

	sensor_print("sensor_init: %s\n", sd->name);
	/*Make sure it is a target sensor */
#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
	if (!strcmp(sd->name, RESET_CONTROL_BY_ITSELF)) {
		if (sensor_power_flag && !sensor_update_2ic_addr) {
			sensor_update_2ic_addr = true;
			sensor_i2c_addr_get(sd, &iic_addr);
			sensor_print("[INFO] i2c addr client name = [%s] iic_addr = 0x%x\n", client->name, iic_addr);
			sensor_i2c_addr_set(sd, 0x60);
			usleep_range(20000, 22000);
			/* update IIC addr(0x60 -> 0x64) for self */
			sensor_write(sd, SC1346_I2C_ADDR_UPDATE, TARGET_I2C_ADDR);
			sensor_print("SC1346_I2C_ADDR_UPDATE done\n");
			sensor_i2c_addr_set(sd, TARGET_I2C_ADDR);
			sensor_read(sd, SC1346_I2C_ADDR_UPDATE, &rdval);
			sensor_print("[INFO] i2c addr client name = [%s] set TARGET_I2C_ADDR = 0x%x, read: 0x%x\n", client->name, TARGET_I2C_ADDR, rdval);
		} else {
			sensor_print("%s is poweron, not to do it repeatedly\n", client->name);
		}
	}
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}
#else
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}
#endif

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1280;
	info->height = 720;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 15;	/* 15fps */

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
				sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		ret = sensor_get_temp(sd, (struct sensor_temp *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
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
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	{
		.width = HD720_WIDTH,
		.height = HD720_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1600,
		.vts = 3000,
		.pclk = 72 * 1000 * 1000,
		.mipi_bps = 720 * 1000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 2 << 4,
		.intg_max = (3000 - 6)<< 4,
		.gain_min = 1 << 4,
		.gain_max = 125 << 4,
		.regs = sensor_master_mode_regs,
		.regs_size = ARRAY_SIZE(sensor_master_mode_regs),
		.set_size = NULL,
	},
#else
	{
		.width = HD720_WIDTH,
		.height = HD720_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1650,
		.vts = 1500,
		.pclk = 37125 * 1000,
		.mipi_bps = 37125 * 10 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 2 << 4,
		.intg_max = (1500 - 6)<< 4,
		.gain_min = 1 << 4,
		.gain_max = 125 << 4,
		.regs = sensor_720p10b15_regs,
		.regs_size = ARRAY_SIZE(sensor_720p10b15_regs),
		.set_size = NULL,
	},

	{
		.width = HD720_WIDTH,
		.height = HD720_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1650,
		.vts = 750,
		.pclk = 37125 * 1000,
		.mipi_bps = 37125 * 10 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 2 << 4,
		.intg_max = (750 - 6)<< 4,
		.gain_min = 1 << 4,
		.gain_max = 125 << 4,
		.regs = sensor_720p10b30_regs,
		.regs_size = ARRAY_SIZE(sensor_720p10b30_regs),
		.set_size = NULL,
	   },
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2_DPHY;
    cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

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
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->val);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	data_type rdval;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");
	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	if (!strcmp(sd->name, sc1346_output_register[MASTER].sensor_name)) {
		sensor_print("%s master mode reg init. \n", sd->name);
		sensor_write_array(sd, sensor_master_mode_regs,  ARRAY_SIZE(sensor_master_mode_regs));
	}

	if (!strcmp(sd->name, sc1346_output_register[SLAVE].sensor_name)) {
		sensor_print("%s slave mode reg init. \n", sd->name);
		sensor_write_array(sd, sensor_slave_mode_regs,  ARRAY_SIZE(sensor_slave_mode_regs));
	}
#else
	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
#endif

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc1346_sensor_vts = wsize->vts;
#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	sc1346_synchr_max_exp_line = wsize->intg_max;
	sensor_print("%s  sc1346_synchr_max_exp_line = %d\n", sd->name, sc1346_synchr_max_exp_line);
#endif

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
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
	.try_ctrl = sensor_try_ctrl,
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
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			      65536 * 16, 1, 1);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_dev_id;

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

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

#if IS_ENABLED(CONFIG_SENSOR_SC1346_SYNCHR_OUTPUT)
	sensor_print("%s is working by SYNCHR_OUTPUT\n", sd->name);
	if (!strcmp(sd->name, sc1346_output_register[MASTER].sensor_name)) {
		sensor_print("%s -----> MASTER\n", sd->name);
	} else if (!strcmp(sd->name, sc1346_output_register[SLAVE].sensor_name)) {
		sensor_print("%s -----> SLAVE\n", sd->name);
	}

#if IS_ENABLED(CONFIG_SENSOR_SC1346_MULTIPLEX_IIC)
	if (!strcmp(sd->name, RESET_CONTROL_BY_SOC)) {
		sensor_print("%s is RESET_CONTROL_BY_SOC\n", sd->name);
	} else {
		sensor_print("%s is RESET_CONTROL_BY_ITSELF\n", sd->name);
	}
#endif
#endif

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int sensor_remove(struct i2c_client *client)
#else
static void sensor_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd;
	int i;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

static const struct i2c_device_id sensor_id_2[] = {
	{SENSOR_NAME_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	}, {
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME_2,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id_2,
	},
};
static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		ret = cci_dev_init_helper(&sensor_driver[i]);

	return ret;
}

static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		cci_dev_exit_helper(&sensor_driver[i]);
}

VIN_INIT_DRIVERS(init_sensor);
module_exit(exit_sensor);
