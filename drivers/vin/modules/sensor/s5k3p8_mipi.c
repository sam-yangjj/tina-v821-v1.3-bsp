/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for s5k3p8_mipi cameras.
 *
 * Copyright (c) 2018 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Liu Chensheng <liuchensheng@allwinnertech.com>
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
#include <linux/io.h>
#include <media/v4l2-mediabus.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("lcs");
MODULE_DESCRIPTION("A low-level driver for s5k3p8 sensors");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x3108

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The s5k3p8_mipi sits on i2c with ID 0x20/0x5a
 */
#define I2C_ADDR 0x20

#define SENSOR_NUM    0x2
#define SENSOR_NAME   "s5k3p8_mipi"
#define SENSOR_NAME_2 "s5k3p8_mipi_2"

static struct regval_list sensor_default_regs[] = {
	//inital sequence
	{0x6028, 0x4000},
	{0x6010, 0x0001},
	{REG_DLY, 0x0003},
	{0x6214, 0x7971},
	{0x6218, 0x7150},
	//Global
	{0x6028, 0x2000},
	{0x602A, 0x2F38},
	{0x6F12, 0x0088},
	{0x6F12, 0x0D70},
	{0x0202, 0x0200},
	{0x0200, 0x0618},
	//{0x0400, 0x0000},
	//{0x0404, 0x0010},
	{0x3604, 0x0002},
	{0x3606, 0x0103},
	{0xF496, 0x0048},
	{0xF470, 0x0020},
	{0xF43A, 0x0015},
	{0xF484, 0x0006},
	{0xF440, 0x00AF},
	{0xF442, 0x44C6},
	{0xF408, 0xFFF7},
	{0x3664, 0x0019},
	{0xF494, 0x1010},
	{0x367A, 0x0100},
	{0x362A, 0x0104},
	{0x362E, 0x0404},
	{0x32B2, 0x0008},
	{0x3286, 0x0003},
	{0x328A, 0x0005},
	{0xF47C, 0x001F},
	{0xF62E, 0x00C5},
	{0xF630, 0x00CD},
	{0xF632, 0x00DD},
	{0xF634, 0x00E5},
	{0xF636, 0x00F5},
	{0xF638, 0x00FD},
	{0xF63A, 0x010D},
	{0xF63C, 0x0115},
	{0xF63E, 0x0125},
	{0xF640, 0x012D},
	{0x3070, 0x0000},
	{0x0B0E, 0x0000},
	{0x31C0, 0x00C8},
	{0x1006, 0x0004},
	//test pattern
	//{0x0600, 0x0002},
};

static struct regval_list sensor_1080p30_regs[] = {
	{0x6028, 0x2000}, //0x4000
	{0x0114, 0x0100},
	{0x0202, 0x0700},
	{0x0204, 0x0040},
	{0x0136, 0x1800},

	{0x0304, 0x0006},
	{0x0306, 0x0069},
	{0x0302, 0x0001},
	{0x0300, 0x0003},
	{0x030C, 0x0004},
	{0x030E, 0x0050}, //0x0037 0x0050
	{0x030A, 0x0001},
	{0x0308, 0x0008},

	{0x0344, 0x0198},
	{0x0346, 0x02A0},
	{0x0348, 0x1097},
	{0x034A, 0x0B0F},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x0408, 0x0000},
	{0x0900, 0x0122},
	{0x0380, 0x0001},
	{0x0382, 0x0003},
	{0x0384, 0x0003},
	{0x0386, 0x0001},
	{0x0400, 0x0000},
	{0x0404, 0x0010},
	{0x0342, 0x2800},
	{0x0340, 0x071E},

	{0x602A, 0x1704},
	{0x6F12, 0x8011},
	{0x317A, 0x0007},
	{0x31A4, 0x0102},
	{0x36C4, 0xFFCD},
	{0x36C6, 0xFFCD},
	{0x36C8, 0xFFCD},
	{0x36CA, 0xFFCD},
	{0x36CC, 0xFFCD},
	{0x36CE, 0xFFCD},
	{0x36D0, 0xFFCD},
	{0x36D2, 0xFFCD},
	{0x36D4, 0xFFCD},
	{0x36D6, 0xFFCD},
	{0x36D8, 0xFFCD},
	{0x36DA, 0xFFCD},
	{0x36DC, 0xFFCD},
	{0x36DE, 0xFFCD},
	{0x36E0, 0xFFCD},
	{0x36E2, 0xFFCD},

	{0x0100, 0x0100},
};

static struct regval_list sensor_1024x768_30_regs[] = {
	{0x6028, 0x2000},
	{0x0114, 0x0100},
	{0x0202, 0x0700}, //fine integration time line
	{0x0204, 0x0040}, //coarse integration time line = gain ?
	{0x0136, 0x1800},

	{0x0304, 0x0006},
	{0x0306, 0x0046}, //0x0069 - 0x004b
	{0x0302, 0x0001}, //0x0001 - 0x0002
	{0x0300, 0x0003}, //0x0003 - 0x0004
	{0x030C, 0x0004},
	{0x030E, 0x0050},
	{0x030A, 0x0001},
	{0x0308, 0x0008},

	{0x0344, 0x0518}, //x addr start
	{0x0346, 0x03D8}, //y addr start
	{0x0348, 0x0D17}, //x addr end
	{0x034A, 0x09D7}, //y addr end
	{0x034C, 0x0400}, //output X
	{0x034E, 0x0300}, //output Y
	{0x0408, 0x0000},
	{0x0900, 0x0122}, //0x0112 binning mode
	{0x0380, 0x0001},
	{0x0382, 0x0003},
	{0x0384, 0x0003},
	{0x0386, 0x0001},
	{0x0400, 0x0000},
	{0x0404, 0x0010}, //scaler
	{0x0342, 0x2800}, //hts 0x12E0 0x1400
	{0x0340, 0x071E}, //vts 0x03C5 0x071E

	{0x602A, 0x1704},
	{0x6F12, 0x8011},
	{0x317A, 0x0007},
	{0x31A4, 0x0102},
	{0x36C4, 0xFFCD},
	{0x36C6, 0xFFCD},
	{0x36C8, 0xFFCD},
	{0x36CA, 0xFFCD},
	{0x36CC, 0xFFCD},
	{0x36CE, 0xFFCD},
	{0x36D0, 0xFFCD},
	{0x36D2, 0xFFCD},
	{0x36D4, 0xFFCD},
	{0x36D6, 0xFFCD},
	{0x36D8, 0xFFCD},
	{0x36DA, 0xFFCD},
	{0x36DC, 0xFFCD},
	{0x36DE, 0xFFCD},
	{0x36E0, 0xFFCD},
	{0x36E2, 0xFFCD},

	{0x0100, 0x0100},
};

static struct regval_list sensor_640x480_30_regs[] = {
	{0x6028, 0x4000},
	{0x0114, 0x0100},
	{0x0202, 0x0700}, //fine integration time line
	{0x0204, 0x0040}, //coarse integration time line
	{0x0136, 0x1800},

	{0x0304, 0x0006}, //pre_pll_clk_div
	{0x0306, 0x0069}, //pll_multiplier
	{0x0302, 0x0002}, //vt_sys_clk_div
	{0x0300, 0x0004}, //vt_pix_clk_div
	{0x030C, 0x0004}, //secnd_pre_pll_clk_div
	{0x030E, 0x0050}, //secnd_pll_multiplier
	{0x030A, 0x0001}, //op_sys_clk_div
	{0x0308, 0x0008}, //op_pix_clk_div
	{0x0344, 0x0698}, //x addr start
	{0x0346, 0x04F8}, //y addr start
	{0x0348, 0x0B97}, //x addr end
	{0x034A, 0x08B7}, //y addr end
	{0x034C, 0x0280}, //output X
	{0x034E, 0x01E0}, //output Y
	{0x0408, 0x0000},
	{0x0900, 0x0122}, //0x0112 binning mode
	{0x0380, 0x0001},
	{0x0382, 0x0003},
	{0x0384, 0x0003},
	{0x0386, 0x0001},
	{0x0400, 0x0000},
	{0x0404, 0x0010}, //scaler
	{0x0342, 0x1400}, //hts
	{0x0340, 0x071E}, //vts

	{0x602A, 0x1704},
	{0x6F12, 0x8011},
	{0x317A, 0x0007},
	{0x31A4, 0x0102},
	{0x36C4, 0xFFCD},
	{0x36C6, 0xFFCD},
	{0x36C8, 0xFFCD},
	{0x36CA, 0xFFCD},
	{0x36CC, 0xFFCD},
	{0x36CE, 0xFFCD},
	{0x36D0, 0xFFCD},
	{0x36D2, 0xFFCD},
	{0x36D4, 0xFFCD},
	{0x36D6, 0xFFCD},
	{0x36D8, 0xFFCD},
	{0x36DA, 0xFFCD},
	{0x36DC, 0xFFCD},
	{0x36DE, 0xFFCD},
	{0x36E0, 0xFFCD},
	{0x36E2, 0xFFCD},

	{0x0100, 0x0100},
};

static int sensor_read_byte(struct v4l2_subdev *sd, unsigned short reg,
	unsigned char *value)
{
	int ret = 0, cnt = 0;

	if (!sd || !sd->entity.use_count) {
		sensor_print("%s error! sensor is not used!\n", __func__);
		return -1;
	}

	ret = cci_read_a16_d8(sd, reg, value);
	while ((ret != 0) && (cnt < 2)) {
		ret = cci_read_a16_d8(sd, reg, value);
		cnt++;
	}
	if (cnt > 0)
		pr_info("%s sensor read retry = %d\n", sd->name, cnt);

	return ret;
}

static int sensor_write_byte(struct v4l2_subdev *sd, unsigned short reg,
	unsigned char value)
{
	int ret = 0, cnt = 0;

	if (!sd || !sd->entity.use_count) {
		sensor_print("%s error! sensor is not used!\n", __func__);
		return -1;
	}

	ret = cci_write_a16_d8(sd, reg, value);
	while ((ret != 0) && (cnt < 2)) {
		ret = cci_write_a16_d8(sd, reg, value);
		cnt++;
	}
	if (cnt > 0)
		pr_info("%s sensor write retry = %d\n", sd->name, cnt);

	return ret;
}

static int s5k3p8_write_array(struct v4l2_subdev *sd, struct regval_list *regs, int array_size)
{
	int i = 0, ret = 0;

	if (!regs)
		return -EINVAL;

	while (i < array_size) {
		if (regs->addr == REG_DLY) {
			usleep_range(regs->data * 1000, regs->data * 1000 + 100);
		} else {
			ret = sensor_write(sd, regs->addr, regs->data);
			if (ret < 0) {
				sensor_print("%s sensor write array error, array_size %d!\n", sd->name, array_size);
				return -1;
			}
		}
		i++;
		regs++;
	}
	return 0;
}

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

static int s5k3p8_sensor_vts;
static int s5k3p8_sensor_hts;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	unsigned int exp_coarse;
	unsigned short exp_fine;
	struct sensor_info *info = to_state(sd);

	if (exp_val > 0xffffff)
		exp_val = 0xfffff0;

	if (exp_val < 16)//min -> 4
		exp_val = 16;

	if (info->exp == exp_val)
		return 0;

	exp_coarse = exp_val >> 4;//rounding to 1
	//exp_fine = (exp_val - exp_coarse * 16) * info->current_wins->hts / 16;
	exp_fine = (unsigned short) (((exp_val - exp_coarse * 16) * s5k3p8_sensor_hts) / 16);

	//sensor_write(sd, 0x0200, exp_fine);
	sensor_write(sd, 0x0202, (unsigned short)exp_coarse);

	sensor_dbg("sensor_set_exp = exp_fine,0x%x, exp_coarse,0x%xDone!\n", exp_fine, exp_coarse);

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

	data_type ana_gain_min, ana_gain_max;

	ana_gain_min = 0x0020;
	ana_gain_max = 0x0200;

	int ana_gain = 0;

	if (info->gain == gain_val)
		return 0;

	if (gain_val <= ana_gain_min)
		ana_gain = ana_gain_min;
	else if (gain_val > ana_gain_min && gain_val <= (ana_gain_max))
		ana_gain = gain_val;
	else
		ana_gain = ana_gain_max;

	ana_gain *= 2;//shift to 1/32 step

	sensor_write(sd, 0x0204, (unsigned short)ana_gain);

	//sensor_dbg("sensor_set_gain = %d, Done!\n", gain_val);
	info->gain = gain_val;
	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)

{
	int exp_val, gain_val;
	int shutter, frame_length;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	shutter = exp_val >> 4;

	if (shutter  > s5k3p8_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = s5k3p8_sensor_vts;

	sensor_write_byte(sd, 0x0104, 0x01);
	sensor_write(sd, 0x0340, frame_length);
	sensor_s_gain(sd, gain_val);
	sensor_s_exp(sd, exp_val);
	sensor_write_byte(sd, 0x0104, 0x00);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;

	return 0;
}

/*
 *set && get sensor flip
 */
static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	unsigned char get_value;

	sensor_read_byte(sd, 0x0101, &get_value);
	printk("[xl]: get fmt %d \n", get_value);
	switch (get_value) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x01:
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x02:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x03:
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	unsigned char get_value;
	unsigned char set_value;

	sensor_print("into set sensor hfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read_byte(sd, 0x0101, &get_value);
	if (enable)
		set_value = get_value | 0x01;
	else
		set_value = get_value & 0xfe;

	sensor_write_byte(sd, 0x0101, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	unsigned char get_value;
	unsigned char set_value;

	sensor_print("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read_byte(sd, 0x0101, &get_value);
	if (enable)
		set_value = get_value | 0x02;
	else
		set_value = get_value & 0xfd;

	sensor_write_byte(sd, 0x0101, set_value);
	return 0;

}


/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

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
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);        /// Pull up PWDN pin initially.
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);        /// Pull down RESET# pin initially.
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);              /// Turn on AVDD.
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, DVDD, ON);             /// DVDD is controlled internally.
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, IOVDD, ON);              /// Turn on DOVDD.

		usleep_range(5000, 5200);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);	    /// Pull up RESET# pin.
		usleep_range(1000, 1200);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(5000, 5200);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, CAMERAVDD, OFF);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
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
		usleep_range(10000, 12000);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval; //unsigned short 16bit
	int ret;

	ret = sensor_read(sd, 0x0000, &rdval);
	sensor_dbg("read 0x0000:0x%x\n", (rdval >> 8));
	if ((rdval >> 8) != (V4L2_IDENT_SENSOR >> 8)) {
		sensor_err(" read 0x0000 return 0x%x\n", (rdval >> 8));
		return -ENODEV;
	}

	ret = sensor_read(sd, 0x0001, &rdval);
	sensor_dbg("read 0x0001:0x%x\n", (rdval >> 8));
	if ((rdval >> 8) != (V4L2_IDENT_SENSOR & 0xff)) {
		sensor_err(" read 0x0001 return 0x%x\n", (rdval >> 8));
		return -ENODEV;
	}

	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1920;
	info->height = 1080;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
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
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
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
		.mbus_code = MEDIA_BUS_FMT_SGRBG10_1X10,
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
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 10240,
		.vts        = 1822,
		.pclk       = 560*1000*1000,
		.mipi_bps   = 660*1000*1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1822 - 4) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 16 << 4,
		.regs       = sensor_1080p30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p30_regs),
		.set_size   = NULL,
	},

	{
		.width      = 1024,
		.height     = 768,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 10240, // 4832  5120
		.vts        = 1822, // 965  1822
		.pclk       = 560*1000*1000,
		.mipi_bps   = 660*1000*1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1822 - 4) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 16 << 4,
		.regs       = sensor_1024x768_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1024x768_30_regs),
		.set_size   = NULL,
	},

	/*{
		.width      = 640,
		.height     = 480,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 5120,
		.vts        = 1822,
		.pclk       = 560*1000*1000,
		.mipi_bps   = 660*1000*1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1822 - 4) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 16 << 4,
		.regs       = sensor_640x480_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_640x480_30_regs),
		.set_size   = NULL,
	},*/
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

__maybe_unused static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
/*	struct sensor_info *info = to_state(sd); */

	cfg->type = V4L2_MBUS_CSI2_DPHY;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cfg->bus.mipi_csi2.num_data_lanes = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif

	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
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
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	case V4L2_CID_HFLIP:
		return 0;//sensor_s_hflip(sd, ctrl->val);
	case V4L2_CID_VFLIP:
		return 0;//sensor_s_vflip(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	__maybe_unused struct sensor_exp_gain exp_gain;

	ret = s5k3p8_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	s5k3p8_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		s5k3p8_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	if (info->exp && info->gain) {
		exp_gain.exp_val = info->exp;
		exp_gain.gain_val = info->gain;
	} else {
		exp_gain.exp_val = 976*16;
		exp_gain.gain_val = 16*16;
	}
	sensor_s_exp_gain(sd, &exp_gain);

	//sensor_write_byte(sd, 0x0100, 0x01);

	info->width = wsize->width;
	info->height = wsize->height;
	s5k3p8_sensor_vts = wsize->vts;

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

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
	//.queryctrl = sensor_queryctrl,
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

static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_16,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_16,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
			      65536 * 16, 1, 0);

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
static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
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
