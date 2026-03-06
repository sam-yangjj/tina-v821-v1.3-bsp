/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for sp1405 Raw cameras.
 *
 * Copyright (c) 2024 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zou ZhiYu <zouzhiyu@allwinnertech.com>
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

MODULE_AUTHOR("zzy");
MODULE_DESCRIPTION("A low-level driver for sp1405 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x9732
/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 15

/*
 * The SP1405 i2c address
 */
#define I2C_ADDR 0x6c

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sp1405_mipi"
#define SENSOR_NAME_2 "sp1405_mipi_2"

/*
 * The default register settings
 */
 static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_1280x720p15_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3007, 0x1f},
	{0x3008, 0xff},
	{0x3009, 0x02},
	{0x3010, 0x00},
	{0x3011, 0x08},
	{0x3014, 0x22},
	{0x301e, 0x15},
	{0x3030, 0x19},
	{0x3080, 0x02},
	{0x3081, 0x3c},
	{0x3082, 0x04},
	{0x3083, 0x00},
	{0x3084, 0x02},
	{0x3085, 0x01},
	{0x3086, 0x01},
	{0x3089, 0x01},
	{0x308a, 0x00},
	{0x3103, 0x01},
	{0x3600, 0xf6},
	{0x3601, 0x72},
	{0x3605, 0x66},
	{0x3610, 0x0c},
	{0x3611, 0x60},
	{0x3612, 0x35},
	{0x3654, 0x10},
	{0x3655, 0x77},
	{0x3656, 0x77},
	{0x3657, 0x07},
	{0x3658, 0x22},
	{0x3659, 0x22},
	{0x365a, 0x02},
	{0x3700, 0x1f},
	{0x3701, 0x10},
	{0x3702, 0x0c},
	{0x3703, 0x0b},
	{0x3704, 0x3c},
	{0x3705, 0x51},
	{0x370d, 0x20},
	{0x3710, 0x0d},
	{0x3782, 0x58},
	{0x3783, 0x60},
	{0x3784, 0x05},
	{0x3785, 0x55},
	{0x37c0, 0x07},
	{0x3800, 0x00},
	{0x3801, 0x04},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x05},
	{0x3805, 0x0b},
	{0x3806, 0x02},
	{0x3807, 0xdb},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380c, 0x05},
	{0x380d, 0xc6},
	{0x380e, 0x06},//vts:1622
	{0x380f, 0x56},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x04},
	{0x3820, 0x10},
	{0x3821, 0x00},
	{0x382c, 0x06},
	{0x3500, 0x00},
	{0x3501, 0x31},
	{0x3502, 0x00},
	{0x3503, 0x03},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3509, 0x10},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3d00, 0x00},
	{0x3d01, 0x00},
	{0x3d02, 0x00},
	{0x3d03, 0x00},
	{0x3d04, 0x00},
	{0x3d05, 0x00},
	{0x3d06, 0x00},
	{0x3d07, 0x00},
	{0x3d08, 0x00},
	{0x3d09, 0x00},
	{0x3d0a, 0x00},
	{0x3d0b, 0x00},
	{0x3d0c, 0x00},
	{0x3d0d, 0x00},
	{0x3d0e, 0x00},
	{0x3d0f, 0x00},
	{0x3d80, 0x00},
	{0x3d81, 0x00},
	{0x3d82, 0x38},
	{0x3d83, 0xa4},
	{0x3d84, 0x00},
	{0x3d85, 0x00},
	{0x3d86, 0x1f},
	{0x3d87, 0x03},
	{0x3d8b, 0x00},
	{0x3d8f, 0x00},
	{0x4001, 0xe0},
	{0x4004, 0x00},
	{0x4005, 0x02},
	{0x4006, 0x01},
	{0x4007, 0x40},
	{0x4009, 0x0b},
	{0x4300, 0x03},
	{0x4301, 0xff},
	{0x4304, 0x00},
	{0x4305, 0x00},
	{0x4309, 0x00},
	{0x4600, 0x00},
	{0x4601, 0x04},
	{0x4800, 0x20},
	{0x4802, 0x00},
	{0x4805, 0x00},
	{0x4821, 0x50},
	{0x4823, 0x50},
	{0x4837, 0x2d},
	{0x4a00, 0x00},
	{0x4f00, 0x80},
	{0x4f01, 0x10},
	{0x4f02, 0x00},
	{0x4f03, 0x00},
	{0x4f04, 0x00},
	{0x4f05, 0x00},
	{0x4f06, 0x00},
	{0x4f07, 0x00},
	{0x4f08, 0x00},
	{0x4f09, 0x00},
	{0x5000, 0x07},
	{0x500c, 0x00},
	{0x500d, 0x00},
	{0x500e, 0x00},
	{0x500f, 0x00},
	{0x5010, 0x00},
	{0x5011, 0x00},
	{0x5012, 0x00},
	{0x5013, 0x00},
	{0x5014, 0x00},
	{0x5015, 0x00},
	{0x5016, 0x00},
	{0x5017, 0x00},
	{0x5080, 0x00},
	{0x5180, 0x01},
	{0x5181, 0x00},
	{0x5182, 0x01},
	{0x5183, 0x00},
	{0x5184, 0x01},
	{0x5185, 0x00},
	{0x5708, 0x06},

	{0x5780, 0x03},
	{0x5781, 0x00},
	{0x5782, 0x77},
	{0x5783, 0x0f},

	{0x3603, 0x70},
	{0x3620, 0x1e},
	{0x400a, 0x01},
	{0x400b, 0xc0},
	{0x0100, 0x01},
};
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {
};

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

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	unsigned char explow, exphigh;
	unsigned int all_exp;
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;

	all_exp = exp_val / 16;
	if (all_exp > (wsize->vts - 4)) {
		all_exp = wsize->vts - 4;
	} else if (all_exp < 4) {
		all_exp = 4;
	}

	all_exp = all_exp << 4;

	exphigh  = (unsigned char) ((all_exp >> 8) & 0xff);
	explow  = (unsigned char) (all_exp & 0xff);
	sensor_write(sd, 0x3501, exphigh);
	sensor_write(sd, 0x3502, explow);

	sensor_dbg("sensor_s_exp = %d!\n", exp_val);
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

static int sensor_s_gain(struct v4l2_subdev *sd, unsigned int gain_val)
{
	struct sensor_info *info = to_state(sd);
	unsigned char  All_gain;\

	All_gain = gain_val;
	if (All_gain < 16) /* 64 */
		All_gain = 16;

	sensor_write(sd, 0x350b, All_gain);
	sensor_dbg("sensor_s_gain = %d\n", All_gain);
	info->gain = gain_val;
	return 0;
}

static int glb_exp_val = 1622 * 16;
static int glb_gain_val = 32 * 16 - 1;
static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 16)
		gain_val = 16;
	if (gain_val > 15*16-1)
		gain_val = 15*16-1;
	glb_exp_val = exp_val;
	glb_gain_val = gain_val;

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	sensor_dbg("sensor_set_gain exp = %d, gain = %d Done!\n", exp_val, gain_val);
	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
    struct sensor_info *info = to_state(sd);
    struct sensor_win_size *wsize = info->current_wins;
    data_type frame_length = 0, act_vts = 0;

    sensor_read(sd, 0x380e, &frame_length);
    act_vts = frame_length << 8;
    sensor_read(sd, 0x380f, &frame_length);
    act_vts |= frame_length;
    fps->fps = wsize->pclk / (wsize->hts * act_vts);
    sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	int times_out = 5;
	int eRet;

	if (!(enable == 0 || enable == 1)) {
		sensor_err("Invalid parameter!!!\n");
		return -1;
	}

	eRet = sensor_read(sd, 0x3820, &get_value);
	sensor_print("[H] read regs : get_value = 0x%x\n", get_value);

	if (enable)
		set_value = get_value | 0x08;
	else
		set_value = get_value & 0x07;

	do {
		/* write repeatly */
		sensor_write(sd, 0x3820, set_value);
		eRet = sensor_read(sd, 0x3820, &get_value);
		sensor_print("[H] eRet:%d, get_value = 0x%x, times_out:%d\n", eRet, get_value, times_out);
		usleep_range(5000, 10000);
		times_out--;
	} while ((get_value != set_value) && (times_out >= 0));

	if ((times_out < 0) && ((get_value != set_value))) {
		sensor_err("set hflip failed, please set more times!!!\n");
		return -1;
	} else {
		sensor_print("hflip finish, set_value : 0x%x, get_value = 0x%x\n",
			set_value, get_value);
	}

	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	int times_out = 5;
	int eRet;

	if (!(enable == 0 || enable == 1)) {
		sensor_err("Invalid parameter!!!\n");
		return -1;
	}

	eRet = sensor_read(sd, 0x3820, &get_value);
	sensor_print("[V] read regs : value_0015 = 0x%x\n", get_value);

	if (enable)
		set_value = get_value | 0x04;
	else
		set_value = get_value & 0x0B;

	do {
		/* write repeatly */
		sensor_write(sd, 0x3820, set_value);
		eRet = sensor_read(sd, 0x3820, &get_value);
		sensor_print("[V] eRet:%d, get_value = 0x%x, times_out:%d\n", eRet, get_value, times_out);
		usleep_range(5000, 10000);
		times_out--;
	} while ((get_value != set_value) && (times_out >= 0));

	if ((times_out < 0) && ((get_value != set_value))) {
		sensor_err("set vflip failed, please set more times!!!\n");
		return -1;
	} else {
		sensor_print("vflip finish, set_value : 0x%x, get_value = 0x%x\n",
			set_value, get_value);
	}

	return 0;
}

static void sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{

}

/*
 *set && get sensor flip
 */
static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);

	*code = info->fmt->mbus_code;

	return 0;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_print("STBY_ON!\n");
		cci_lock(sd);
		sensor_s_sw_stby(sd, STBY_ON);
		usleep_range(1000, 1200);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_print("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(1000, 1200);
		sensor_s_sw_stby(sd, STBY_OFF);
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(100, 120);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(5000, 7000);
		vin_set_mclk(sd, ON);
		usleep_range(5000, 7000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(5000, 7000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
		usleep_range(100, 120);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	sensor_err("%s:%d:%d\r\n", __func__, __LINE__, val);
	switch (val) {
	case 0:
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(100, 120);
		break;
	case 1:
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(100, 120);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	unsigned int SENSOR_ID = 0;
	data_type val;
	int cnt = 0;

	sensor_read(sd, 0x300A, &val);
	SENSOR_ID |= (val << 8);
	sensor_read(sd, 0x300B, &val);
	SENSOR_ID |= (val);
	sensor_print("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);
	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 3)) {
		sensor_read(sd, 0x300A, &val);
		SENSOR_ID |= (val << 8);
		sensor_read(sd, 0x300B, &val);
		SENSOR_ID |= (val);
		sensor_dbg("retry = %d, V4L2_IDENT_SENSO = %x\n", cnt, SENSOR_ID);
		cnt++;
	}

	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

	return 0;

}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/* Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1280;
	info->height = 720;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;
	info->deskew = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 15; /* 15fps */

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
				sizeof(*info->current_wins));
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
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_ACT_INIT:
		ret = actuator_init(sd, (struct actuator_para *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_CODE:
		ret = actuator_set_code(sd, (struct actuator_ctrl *)arg);
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
	{
	.width      = 1280,
	.height     = 720,
	.hoffset    = 0,
	.voffset    = 0,
	.hts        = 1478,
	.vts        = 1622,
	.pclk       = 35959740,
	.mipi_bps   = 360*1000*1000,
	.fps_fixed  = 15,
	.bin_factor = 1,
	.intg_min   = 1<<4,
	.intg_max   = 1622<<4,
	.gain_min   = 1<<4,
	.gain_max   = 15<<4,
	.regs       = sensor_1280x720p15_regs,
	.regs_size  = ARRAY_SIZE(sensor_1280x720p15_regs),
	.set_size   = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cfg->bus.mipi_csi2.num_data_lanes = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif

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
	struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(sd, sensor_default_regs,
				ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}
	sensor_print("sensor_reg_init\n");
	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);
	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	if (wsize->set_size)
		wsize->set_size(sd);
	info->width = wsize->width;
	info->height = wsize->height;
	exp_gain.exp_val = glb_exp_val;
	exp_gain.gain_val = glb_gain_val;

	sensor_s_exp_gain(sd, &exp_gain);

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
	//.get_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

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

	v4l2_ctrl_handler_init(handler, 2);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
				256 * 1600, 1, 1 * 1600);
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
				65536 * 16, 1, 0);
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

	printk("sensor_probe-start.\n");
	sensor_print("sensor_probe--start.\n");

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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
#if IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	info->preview_first_flag = 1;
#else
	info->preview_first_flag = 0;
#endif
	info->exp = 10000;
	info->gain = 1024;
	info->first_power_flag = 1;
	printk("sensor_probe success.\n");
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
