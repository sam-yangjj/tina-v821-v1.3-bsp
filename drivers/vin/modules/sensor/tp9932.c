/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for tp9932 YUV cameras.
 *
 * Copyright (c) 2019 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zheng Zequn <zequnzheng@allwinnertech.com>
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

MODULE_AUTHOR("zw");
MODULE_DESCRIPTION("A low-level driver for tp9932 sensors");
MODULE_LICENSE("GPL");

//dvp接口

#define MCLK              (27*1000*1000)


//
#define REAR_TP9932 1

#define TP9932_1CH_1080P 	0
#define TP9932_1CH_2K 		1
#define TP9932_2CH_1080P 	2
#define TP9932_2CH_2K 		3
#define TP9932_RESOLUTION 	TP9932_2CH_2K


// #define CLK_POL V4L2_MBUS_PCLK_SAMPLE_FALLING ////下降沿采集         other use
#define CLK_POL (V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_PCLK_SAMPLE_FALLING) //上下沿同时采集   2CH_2K use
#define V4L2_IDENT_SENSOR_9932 0x3628

#define WAIT_VIDEO_INPUT_STATUS (1)

#define ENABLE_REG_FS

/* enable tp9932 sensor detect */
#define SENSOR_DETECT_KTHREAD 1
/* USE DETECT BY GPIO_IRQ OR POLLING
 * DET_USE_POLLING 0 meant by gpio_irq
 * DET_USE_POLLING 1 meant by POLLING
 * */
#define DET_USE_POLLING 1

#define DELAY_TIME	3

#if SENSOR_DETECT_KTHREAD
struct sensor_indetect_tp9932 {
	struct class *sensor_class;
	struct task_struct *sensor_task;
	struct device *dev;
	struct cdev *cdev;
	struct gpio_config detect_power;
	struct gpio_config detect_power_mid;
	struct gpio_config detect_gpio;
	struct delayed_work tp9932_work;
#if DET_USE_POLLING
	data_type   last_status;
#else
	unsigned int detect_irq;
#endif
    int gpio_detect_flag;
    struct i2c_client *client;
	dev_t devno;
} sensor_indetect;
static DEFINE_MUTEX(det_mutex);
#endif

/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 30

/*
 * The TP9920 sits on i2c with ID 0x88 or 0x8a
 * SAD-low:0x88 SAD-high:0x8a
 */
#define I2C_ADDR 0x88
#define SENSOR_NAME "tp9932"

static unsigned int count1;
static unsigned int reset_flag1;
static unsigned int count2;
static unsigned int reset_flag2;
//TP9932_2CH_1080P
static struct regval_list tp9932_reg_1080p25_2ch[] = {
	//TP9932的 AHD1080P25帧 16位的BT656 2通道
	//video setting
	{0x40, 0x00},//channe1 AHD1080P25
	{0x02, 0x44},
	{0x07, 0x80},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x73},

	{0x0F, 0x00},
	{0x10, 0x00},
	{0x11, 0x40},
	{0x12, 0x40},
	{0x13, 0x00},
	{0x14, 0x40},

	{0x15, 0x01}, //01
	{0x16, 0xee}, //f0
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x1c, 0x0a},
	{0x1d, 0x50},

	{0x20, 0x3c},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x24, 0x04},
	{0x25, 0xfe},
	{0x26, 0x0d},
	{0x27, 0x2d},
	{0x28, 0x00},
	{0x29, 0x48},
	{0x2a, 0x30},
	{0x2b, 0x60},
	{0x2c, 0x3a},
	{0x2d, 0x54},
	{0x2e, 0x40},

	{0x30, 0xa5},
	{0x31, 0x86},
	{0x32, 0xfb},
	{0x33, 0x60},
	{0x35, 0x05},

	{0x39, 0x0c},
	{0x3a, 0x32},

//video setting
	{0x40, 0x01},//channel2 AHD1080P27.5
	{0x02, 0x40},
	{0x07, 0x80},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x50},

	{0x0F, 0x00},
	{0x10, 0x00},
	{0x11, 0x40},
	{0x12, 0x40},
	{0x13, 0x00},
	{0x14, 0x40},

	{0x15, 0x01}, //11
	{0x16, 0xca},//d2
	{0x17, 0x80},
	{0x18, 0x2a},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x1c, 0x09},
	{0x1d, 0x60},

	{0x20, 0x38},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x24, 0x04},
	{0x25, 0xfe},
	{0x26, 0x0d},
	{0x27, 0x2d},
	{0x28, 0x00},
	{0x29, 0x48},
	{0x2a, 0x30},
	{0x2b, 0x60},
	{0x2c, 0x3a},
	{0x2d, 0x54},
	{0x2e, 0x40},

	{0x30, 0x29},
	{0x31, 0x85},
	{0x32, 0x1e},
	{0x33, 0xb0},
	{0x35, 0x05},

	{0x39, 0x0c},
	{0x3a, 0x32},

	//channel ID
	{0x40, 0x00},
	{0x34, 0x10},
	{0x40, 0x01},
	{0x34, 0x11},
	{0x40, 0x02},
	{0x34, 0x12},
	{0x3d, 0xff},  //power down VIN3
	{0x40, 0x03},
	{0x34, 0x13},
	{0x3d, 0xff},  //power down VIN4

	//output format
	{0x43, 0x10},
	{0x44, 0x10},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x00},
	{0x4f, 0x03},
	{0x50, 0x90},
	{0x52, 0xd4},
	{0xe8, 0x00},
	{0xef, 0x00},
	{0xf1, 0x04},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf5, 0xf0},

	{0xf6, 0x01},
	{0xf8, 0x45},
	{0xfa, 0x01},
	{0xfb, 0x01},
	{0xf4, 0x80},
	//output enable
	{0x4d, 0x77},
	{0x4e, 0x05},
};

//TP9932_2CH_2K
static struct regval_list tp9932_reg_2K25_2ch[] = {
	//video setting
	{0x40, 0x04},
	{0x02, 0x58},
	{0x05, 0x00},
	{0x06, 0x12},
	{0x07, 0x80},
	{0x08, 0x00},
	{0x09, 0x24},
	{0x0a, 0x48},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x70},
	{0x0e, 0x10},
	{0x10, 0x00},
	{0x11, 0x40},
	{0x12, 0x40},
	{0x13, 0x00},
	{0x14, 0x00},
	{0x15, 0x23},
	{0x16, 0x16},
	{0x17, 0x04},
	{0x18, 0x32},
	{0x19, 0xa0},
	{0x1a, 0x5a},
	{0x1c, 0x8f},
	{0x1d, 0x76},

	{0x20, 0x80},
	{0x21, 0x86},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x24, 0x04},
	{0x25, 0xff},
	{0x26, 0x05},
	{0x27, 0xad},
	{0x28, 0x00},
	{0x29, 0x48},
	{0x2a, 0x30},
	{0x2b, 0x60},
	{0x2c, 0x3a},
	{0x2d, 0xa0},
	{0x2e, 0x40},

	{0x30, 0x48},
	{0x31, 0x6f},
	{0x32, 0xb5},
	{0x33, 0x80},
	{0x35, 0x15},
	{0x36, 0xdc},

	{0x38, 0x40},
	{0x39, 0x60},
	{0x3a, 0x12},
	{0x3b, 0x25},

	//channel ID and power pown down unused channel
	{0x40, 0x00},
	{0x34, 0x10},
	{0x40, 0x01},
	{0x34, 0x11},
	{0x40, 0x02},
	{0x3d, 0xff},
	{0x40, 0x03},
	{0x3d, 0xff},
	//output format
	{0x43, 0x10},
	{0x44, 0x10},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x00},
	{0x4f, 0x03},
	{0x50, 0x00},
	{0x52, 0x00},
	{0xe8, 0x00},
	{0xef, 0x55},
	{0xf1, 0x04},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf5, 0xf0},

	{0xf6, 0x45},  //01
	{0xf8, 0x01}, //45
	{0xfa, 0x01},
	{0xfb, 0x01},
	{0xf4, 0x80},
	//output enable
	{0x4d, 0x77},
	{0x4e, 0x05},
};


//TP9932_1CH_1080P
static struct regval_list tp9932_reg_1080p25_1ch[] = {
	//TP9932的 AHD1080P25帧 16位的BT1120 1通道
	{0x40, 0x04},
	{0x02, 0x44},
	{0x07, 0x80},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x73},
	{0x10, 0x00},
	{0x11, 0x40},
	{0x12, 0x40},
	{0x13, 0x00},
	{0x14, 0x00},
	{0x15, 0x01},
	{0x16, 0xf0},
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x1c, 0x0a},
	{0x1d, 0x50},

	{0x20, 0x3c},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x24, 0x04},
	{0x25, 0xfe},
	{0x26, 0x0d},
	{0x27, 0x2d},
	{0x28, 0x00},
	{0x29, 0x48},

/*
 * 0x2a寄存器
 * 不设置,寄存器默认值为0x30
 * 设置0x30,无信号出黑屏,有信号出图
 * 设置0x34,无信号出蓝屏,有信号出图
 * 设置0x3c,有无信号都出蓝屏
 */
	{0x2a, 0x30},

	{0x2a, 0x30},
	{0x2b, 0x60},
	{0x2c, 0x3a},
	{0x2d, 0x54},
	{0x2e, 0x40},

	{0x30, 0xa5},
	{0x31, 0x86},
	{0x32, 0xfb},
	{0x33, 0x60},
	{0x35, 0x05},

	{0x39, 0x0c},
	{0x3a, 0x32},

	{0x40, 0x00},
	{0x34, 0x00},
	{0x40, 0x01},
	{0x34, 0x11},
	{0x3d, 0xff},
	{0x40, 0x02},
	{0x34, 0x12},
	{0x3d, 0xff},
	{0x40, 0x03},
	{0x34, 0x13},
	{0x3d, 0xff},

	{0x43, 0x10},
	{0x44, 0x10},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x00},
	{0x4f, 0x03},
	// {0x50, 0x80},
	{0x50, 0x00},
	// {0x52, 0xc4},
	{0x52, 0x00},
	{0xe8, 0x00},
	{0xef, 0x00},
	{0xf1, 0x04},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf5, 0xf0},

	// {0xf6, 0x00},
	// {0xf8, 0x44},
	{0xf6, 0x44}, //颜色是绿色的可能是yc反了,跟这样修改0xf6和0xf8就好
	{0xf8, 0x00},
	// {0xfa, 0x01},
	{0xfa, 0x04},
	{0xfb, 0x01},
	{0xf4, 0x80},

	{0x4d, 0x77},
	{0x4e, 0x05},
};

//TP9932_1CH_2K
static struct regval_list tp9932_reg_2k25_1ch[] = {
	//TP9932的 AHD1440P25帧 16位的BT1120 1通道
	{0x40, 0x04},
	{0x02, 0x58},
	{0x05, 0x00},
	{0x06, 0x12},
	{0x07, 0x80},
	{0x08, 0x00},
	{0x09, 0x24},
	{0x0a, 0x48},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x70},
	{0x0e, 0x10},
	{0x10, 0x00},
	{0x11, 0x40},
	{0x12, 0x40},
	{0x13, 0x00},
	{0x14, 0x00},
	{0x15, 0x23},
	{0x16, 0x16},
	{0x17, 0x00},
	{0x18, 0x32},
	{0x19, 0xa0},
	{0x1a, 0x5a},
	{0x1c, 0x8f},
	{0x1d, 0x76},

	{0x20, 0x80},
	{0x21, 0x86},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x24, 0x04},
	{0x25, 0xff},
	{0x26, 0x05},
	{0x27, 0xad},
	{0x28, 0x00},
	{0x29, 0x48},
	{0x2a, 0x30},
	{0x2b, 0x60},
	{0x2c, 0x3a},
	{0x2d, 0xa0},
	{0x2e, 0x40},

	{0x30, 0x48},
	{0x31, 0x6f},
	{0x32, 0xb5},
	{0x33, 0x80},
	{0x35, 0x15},
	{0x36, 0xdc},

	{0x38, 0x40},
	{0x39, 0x60},
	{0x3a, 0x12},
	{0x3b, 0x25},

	{0x43, 0x10},
	{0x44, 0x10},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x49, 0x00},
	{0x4f, 0x03},
	{0x50, 0x00},
	{0x52, 0x00},
	{0xe8, 0x00},
	{0xef, 0x55},
	{0xf1, 0x04},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf5, 0xf0},

	// {0xf6, 0x00},
	// {0xf8, 0x44},
	{0xf6, 0x44}, //颜色是绿色的可能是yc反了,跟这样修改0xf6和0xf8就好
	{0xf8, 0x00},
	{0xfa, 0x01},
	{0xfb, 0x01},
	{0xf4, 0x80},

	{0x4d, 0x77},
	{0x4e, 0x05},

};



static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	if (on_off)
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	else
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("CSI_SUBDEV_STBY_ON!\n");
		if (sensor_indetect.gpio_detect_flag) {
			sensor_s_sw_stby(sd, ON);
		}
		break;
	case STBY_OFF:
		sensor_dbg("CSI_SUBDEV_STBY_OFF!\n");
		if (sensor_indetect.gpio_detect_flag) {
			sensor_s_sw_stby(sd, OFF);
		}
		break;
	case PWR_ON:
		sensor_dbg("CSI_SUBDEV_PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, SM_HS, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, SM_HS, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(2000, 2100);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(20000, 21000);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(20000, 21000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(40000, 41000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("CSI_SUBDEV_PWR_OFF!\n");
		if (sensor_indetect.gpio_detect_flag) {
			cci_lock(sd);
			vin_set_mclk(sd, OFF);
			vin_set_pmu_channel(sd, DVDD, OFF);
			vin_set_pmu_channel(sd, AVDD, OFF);
			vin_set_pmu_channel(sd, IOVDD, OFF);
			usleep_range(100, 120);
			vin_gpio_write(sd, SM_HS, CSI_GPIO_LOW);
			vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
			vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
			vin_gpio_set_status(sd, SM_HS, 0);
			vin_gpio_set_status(sd, RESET, 0);
			vin_gpio_set_status(sd, PWDN, 0);
			usleep_range(2000, 2100);
			cci_unlock(sd);
		} else {
			pr_info("PWR_OFF, no detected pin!\n");
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	usleep_range(5000, 6000);
	vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	usleep_range(5000, 6000);
	return 0;
}
/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	#if (TP9932_RESOLUTION == TP9932_2CH_1080P)
	{
	.desc = "BT1120 2CH",
	// .mbus_code = MEDIA_BUS_FMT_UYVY8_1X16, //只有这里会是设置16bit数据传输，还需要修改sysconfig.fex里面的csi..
	.mbus_code = MEDIA_BUS_FMT_YUYV8_1X16, //只有这里会是设置16bit数据传输，还需要修改sysconfig.fex里面的csi..
	.regs = NULL,
	.regs_size = 0,
	.bpp = 2,
	},
	#elif (TP9932_RESOLUTION == TP9932_1CH_1080P)
	{
	.desc = "BT1120 1CH",
	.mbus_code = MEDIA_BUS_FMT_UYVY8_1X16, //只有这里会是设置16bit数据传输，还需要修改sysconfig.fex里面的csi..
	.regs = NULL,
	.regs_size = 0,
	.bpp = 1,
	},
	#elif (TP9932_RESOLUTION == TP9932_1CH_2K)
	{
	.desc = "BT1120 1CH",
	.mbus_code = MEDIA_BUS_FMT_UYVY8_1X16, //只有这里会是设置16bit数据传输，还需要修改sysconfig.fex里面的csi..
	.regs = NULL,
	.regs_size = 0,
	.bpp = 1,
	},
	#else
	{
	.desc = "BT1120 2CH",
	.mbus_code = MEDIA_BUS_FMT_UYVY8_1X16, //只有这里会是设置16bit数据传输，还需要修改sysconfig.fex里面的csi..
	.regs = NULL,
	.regs_size = 0,
	.bpp = 2,
	},
	#endif
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
	#if (TP9932_RESOLUTION == TP9932_2CH_1080P)
	{
	 .width = HD1080_WIDTH,
	 .height = HD1080_HEIGHT,
	 .hoffset = 0,
	 .voffset = 0,
	 .fps_fixed = 25,
	 .regs = tp9932_reg_1080p25_2ch,
	 .regs_size = ARRAY_SIZE(tp9932_reg_1080p25_2ch),
	 .pclk_dly = 0x06,
	 .set_size = NULL,
	},
	#elif (TP9932_RESOLUTION == TP9932_1CH_1080P)
	{
	 .width = HD1080_WIDTH,
	 .height = HD1080_HEIGHT,
	 .hoffset = 0,
	 .voffset = 0,
	 .fps_fixed = 25,
	 .regs = tp9932_reg_1080p25_1ch,
	 .regs_size = ARRAY_SIZE(tp9932_reg_1080p25_1ch),
	 .pclk_dly = 0x06,
	 .set_size = NULL,
	},
	#elif (TP9932_RESOLUTION == TP9932_1CH_2K)
	{
	 .width = 2560,
	 .height = 1440,
	 .hoffset = 0,
	 .voffset = 0,
	 .fps_fixed = 25,
	 .regs = tp9932_reg_2k25_1ch,
	 .regs_size = ARRAY_SIZE(tp9932_reg_2k25_1ch),
	 .pclk_dly = 0x06,
	 .set_size = NULL,
	},
	#else
	{
	 .width = 2560,
	 .height = 1440,
	 .hoffset = 0,
	 .voffset = 0,
	 .fps_fixed = 25,
	 .regs = tp9932_reg_2K25_2ch,
	 .regs_size = ARRAY_SIZE(tp9932_reg_2K25_2ch),
	 .pclk_dly = 0x06,
	 .set_size = NULL,
	},
	#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval, rdval1, rdval2;
	int i = 0;

	rdval = 0;
	rdval1 = 0;
	rdval2 = 0;

	mutex_lock(&det_mutex);
	sensor_write(sd, 0x40, 0x00);
	mutex_unlock(&det_mutex);

	sensor_read(sd, 0xfe, &rdval1);
	sensor_read(sd, 0xff, &rdval2);
	rdval = ((rdval2<<8) & 0xff00) | rdval1;
	sensor_print("sensor_detect ID = 0x%x\n", rdval);

////xyd
	#if REAR_TP9932
	while ((rdval != V4L2_IDENT_SENSOR_9932) && (i < 5)) {
		sensor_read(sd, 0xfe, &rdval1);
		sensor_read(sd, 0xff, &rdval2);
		rdval = ((rdval2<<8) & 0xff00) | rdval1;
		sensor_print("retry = %d, sensor_detect ID = 0x%x\n", i, rdval);
		i++;
	}

	// struct sensor_win_size *win_size;
	// if (rdval == V4L2_IDENT_SENSOR) {
	// 	for (i == 0; i < N_WIN_SIZES; ++i) {
	// 		win_size = sensor_win_sizes + i;
	// 		if ((win_size->width == HD1080_WIDTH)
	// 			/* && (win_size->height == HD1080_HEIGHT) */) {
	// 			/*if (win_size->fps_fixed == 30) {
	// 				win_size->regs = tp9932_reg_1080p30_1ch;
	// 				win_size->regs_size = ARRAY_SIZE(tp9932_reg_1080p30_1ch);
	// 				sensor_print("tp50 %d 30fps\n", i);
	// 			} else*/ {
	// 				win_size->regs = tp9932_reg_1080p25_2ch;
	// 				win_size->regs_size = ARRAY_SIZE(tp9932_reg_1080p25_2ch);
	// 				sensor_print("tp9932 %d 25fps\n", i);
	// 			}
	// 		}
	// 	}
	// }

	#else
	while ((rdval != V4L2_IDENT_SENSOR_9950)
		&& (rdval != V4L2_IDENT_SENSOR_9951)
		&& (i < 5)) {
		sensor_read(sd, 0xfe, &rdval1);
		sensor_read(sd, 0xff, &rdval2);
		rdval = ((rdval2<<8) & 0xff00) | rdval1;
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n", i, rdval);
		i++;
	}
    if (chip_id != rdval) {
		struct sensor_win_size *win_size;
		if (rdval == V4L2_IDENT_SENSOR_9950) {
			for (i == 0; i < N_WIN_SIZES; ++i) {
				win_size = sensor_win_sizes + i;
				if ((win_size->width == HD1080_WIDTH)
				/* && (win_size->height == HD1080_HEIGHT) */) {
					if (win_size->fps_fixed == 30) {
						win_size->regs = tp9932_reg_1080p30_1ch;
						win_size->regs_size = ARRAY_SIZE(tp9932_reg_1080p30_1ch);
						sensor_print("tp50 %d 30fps\n", i);
					} else {
						win_size->regs = tp9932_reg_1080p25_1ch;
						win_size->regs_size = ARRAY_SIZE(tp9932_reg_1080p25_1ch);
						sensor_print("tp50 %d 25fps\n", i);
					}
				}
			}
			chip_id = rdval;
		} else if (rdval == V4L2_IDENT_SENSOR_9951) {
			for (i == 0; i < N_WIN_SIZES; ++i) {
				win_size = sensor_win_sizes + i;
				if ((win_size->width == HD1080_WIDTH)
				/* && (win_size->height == HD1080_HEIGHT) */) {
					if (win_size->fps_fixed == 30) {
						win_size->regs = tp9951_reg_1080p30_1ch;
						win_size->regs_size = ARRAY_SIZE(tp9951_reg_1080p30_1ch);
						sensor_print("tp51 %d 30fps\n", i);
					} else {
						win_size->regs = tp9951_reg_1080p25_1ch;
						win_size->regs_size = ARRAY_SIZE(tp9951_reg_1080p25_1ch);
						sensor_print("tp51 %d 25fps\n", i);
					}
				}
			}
			chip_id = rdval;
		}
	}
	#endif
	/* if (rdval != V4L2_IDENT_SENSOR)
		return -ENODEV; */

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
	#if ((TP9932_RESOLUTION == TP9932_1CH_2K) || (TP9932_RESOLUTION == TP9932_2CH_2K))
	info->width = 2560;
	info->height = 1440;
	#else //TP9932_1CH_1080P || TP9932_2CH_1080P
	info->width = HD1080_WIDTH;
	info->height = HD1080_HEIGHT;
	#endif
	info->hflip = 0;
	info->vflip = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;	/* 25fps */

	if ((sensor_indetect.gpio_detect_flag == 0) && (info->preview_first_flag == 0)) {
#if DET_USE_POLLING
	pr_info("use polling detect!\n");
	schedule_delayed_work(&sensor_indetect.tp9932_work, 0);
#endif
	}
	info->preview_first_flag = 1;

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg,
			       info->current_wins,
			       sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

////xyd
static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_BT656;
	#if ((TP9932_RESOLUTION == TP9932_2CH_1080P) || (TP9932_RESOLUTION == TP9932_2CH_2K))
	cfg->flags = CLK_POL | CSI_CH_0 | CSI_CH_1;
	#else //TP9932_1CH_1080P || TP9932_1CH_2K
    cfg->flags = CLK_POL | CSI_CH_0;
	#endif
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->fmt = sensor_fmt;
	info->width = wsize->width;
	info->height = wsize->height;
#if WAIT_VIDEO_INPUT_STATUS
	{
		data_type rdval;
		int cnt = 0;
		while (1) {
			rdval = 0;
			sensor_read(sd, 0x01, &rdval);
			if (((rdval&0x60) == 0x60) || (cnt > 1500)) {
				sensor_print("ck %d,0x%x\n", cnt, rdval);
				break;
			}
			++cnt;
			usleep_range(1000, 1200);
		}
	}
#endif
	return 0;
}

int reg_read(struct i2c_client *client, unsigned char addr,
		unsigned char *value)
{
	unsigned char data[2];
	struct i2c_msg msg[2];
	int ret;

	data[0] = addr;
	data[1] = 0x0;
	/*
	 * Send out the register address...
	 */
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &data[0];
	/*
	 * ...then read back the result.
	 */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &data[1];

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*value = data[1];
		ret = 0;
	}
	return ret;
}

int reg_write(struct i2c_client *client, unsigned char addr,
		   unsigned char value)
{
	struct i2c_msg msg;
	unsigned char data[2];
	int ret;

	data[0] = addr;
	data[1] = value;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		ret = 0;
	return ret;
}




#if SENSOR_DETECT_KTHREAD
static int __sensor_insert_detect(data_type *val)
{
	if (sensor_indetect.gpio_detect_flag) {
		int ret;

		mutex_lock(&det_mutex);
		/*detecting the sensor insert by detect_gpio
		* if detect_gpio was height meant sensor was insert
		* if detect_gpio was low, meant sensor was remove
		* */
		ret = gpio_get_value_cansleep(sensor_indetect.detect_gpio.gpio);
		if (ret) {
			*val = 0x03;
		} else {
			*val = 0x04;
		}

		mutex_unlock(&det_mutex);
    } else {
		*val = 0;
		if (sensor_indetect.client) {
#if 1
			unsigned char rdval = 0;
			mutex_lock(&det_mutex);
			reg_write(sensor_indetect.client, 0x40, 1);
			reg_read(sensor_indetect.client, 0x01, &rdval);
			//pr_info(" ***************读page1的0x01********************   %s,%d:0x%x\n", __func__, __LINE__, rdval);
			if ((reset_flag1 == 0) && (rdval & 0x80) == 0x00) {
				//sensor_print("*val |= 0x02;\n");
				*val |= 0x02;
				count1 = 0;
			} else {
#if 1
				count1++;
				if (count1 == DELAY_TIME) {
					reset_flag1 = 1;
					if (gpio_is_valid(sensor_indetect.detect_power_mid.gpio)) {
						gpio_direction_output(sensor_indetect.detect_power_mid.gpio, 0);
					}
					//sensor_print("low1\n");
				} else if (count1 == DELAY_TIME + 2) {
					if (gpio_is_valid(sensor_indetect.detect_power_mid.gpio)) {
						gpio_direction_output(sensor_indetect.detect_power_mid.gpio, 1);
					}
					//sensor_print("high1\n");
					count1 = 0;
					reset_flag1 = 0;
				}
#endif
			}
			reg_write(sensor_indetect.client, 0x40, 0);
			reg_read(sensor_indetect.client, 0x01, &rdval);
			//pr_info("***************读page0的0x01**********************   %s,%d:0x%x\n", __func__, __LINE__, rdval);
			if ((reset_flag2 == 0) && (rdval & 0x80) == 0x00) {
				//sensor_print("*val |= 0x01;\n");
				*val |= 0x01;
				count2 = 0;
			} else {
#if 1
				count2++;
				if (count2 == DELAY_TIME) {
					reset_flag2 = 1;
					gpio_direction_output(sensor_indetect.detect_power.gpio, 0);
					//sensor_print("low2\n");
				} else if (count2 == DELAY_TIME + 2) {
					gpio_direction_output(sensor_indetect.detect_power.gpio, 1);
					//sensor_print("high2\n");
					count2 = 0;
					reset_flag2 = 0;
				}
#endif
			}
			mutex_unlock(&det_mutex);
#else
			*val = 0x3;
#endif
		usleep_range(2000, 2100);

		}
	}
	return 0;
}

void sensor_msg_sent(char *buf)
{
	char *envp[2];

	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&sensor_indetect.dev->kobj, KOBJ_CHANGE, envp);
}

static void sensor_det_work(struct work_struct *work)
{
	char buf[32];
	data_type val;

	__sensor_insert_detect(&val);

#if DET_USE_POLLING
	if (sensor_indetect.last_status != val) {
		//data_type rear_status = 0;
		memset(buf, 0, 32);
		/*if (val & 0x02) {
			rear_status = 0x03;
		} else {
			rear_status = 0x04;
		}*/
		snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x5");

		sensor_msg_sent(buf);
		sensor_indetect.last_status = val;
		vin_print("val = 0x%x, Sent msg to user\n", val);
	}

	schedule_delayed_work(&sensor_indetect.tp9932_work, (msecs_to_jiffies(1 * 1000)));
#else
	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x%x", val);

	sensor_msg_sent(buf);
	vin_print("val = 0x%x, Sent msg to user\n", val);
#endif

}


#if !DET_USE_POLLING
static irqreturn_t sensor_det_irq_func(int irq, void *priv)
{
	/* the work of detected was be run in workquen */
	schedule_delayed_work(&sensor_indetect.tp9932_work, 0);
	return IRQ_HANDLED;
}
#endif

#endif

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	vin_print("\n%s on = %d, %d*%d %x fps_fixed=%d\n", __func__, enable,
		  info->current_wins->width,
		  info->current_wins->height, info->fmt->mbus_code, info->current_wins->fps_fixed);

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
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
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
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
};

static int sensor_init_controls(struct v4l2_subdev *sd,
				const struct v4l2_ctrl_ops *ops)
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


#if SENSOR_DETECT_KTHREAD
static int sensor_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sensor_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t sensor_dev_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t sensor_dev_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int sensor_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static long sensor_dev_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return 0;
}

static const struct file_operations sensor_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = sensor_dev_open,
	.release        = sensor_dev_release,
	.write          = sensor_dev_write,
	.read           = sensor_dev_read,
	.unlocked_ioctl = sensor_dev_ioctl,
	.mmap           = sensor_dev_mmap,
};

static ssize_t get_det_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	data_type val;
	if (sensor_indetect.gpio_detect_flag) {
		__sensor_insert_detect(&val);
	} else {
		val = sensor_indetect.last_status;
		/*if (sensor_indetect.last_status & 0x02) {
			val = 0x03;
		} else {
			val = 0x04;
		}*/
	}
	return sprintf(buf, "%d\n", val);
}

static struct device_attribute  detect_dev_attrs = {
	.attr = {
		.name = "online",
		.mode =  S_IRUGO,
	},
	.show =  get_det_status_show,
	.store = NULL,

};

#ifdef ENABLE_REG_FS
static unsigned char reg_addr;

static ssize_t reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned char rdval = 0;
	reg_read(sensor_indetect.client, reg_addr, &rdval);
	return sprintf(buf, "tp9932:REG[0x%x]=0x%x\n", reg_addr, rdval);
}

static ssize_t reg_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	s32 tmp;
	u8 val;
	int err;
	int i;
	unsigned char return_value = 0;

	err = kstrtoint(buf, 16, &tmp);
	if (err)
		return err;

	printk("[tp9932_reg_store]tmp = 0x%x\n", tmp);

	if (tmp == 0) {
		printk("page0\n");
		reg_write(sensor_indetect.client, 0x40, 0);
		for (i = 0; i < 0xff; i++) {
			reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
	} else if (tmp < 0xff) {
		//for read
		reg_addr = tmp;
	} else if (tmp <= 0xffff) {
		//for read or write
		val = tmp & 0x00FF;
		reg_addr = (tmp >> 8) & 0x00FF;
		reg_write(sensor_indetect.client, reg_addr, val);
		// sensor_write(sensor_indetect.sd, reg_addr, val);
	} else {
		reg_write(sensor_indetect.client, 0x40, 0);
		printk("page0\n");
		for (i = 0; i < 0xff; i++) {
			reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
		printk("page1\n");
		reg_write(sensor_indetect.client, 0x40, 1);
		for (i = 0; i < 0xff; i++) {
			reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
		printk("page4\n");
		reg_write(sensor_indetect.client, 0x40, 4);
		for (i = 0; i < 0xff; i++) {
			reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}

		printk("page8\n");
		reg_write(sensor_indetect.client, 0x40, 8);
		for (i = 0; i < 0xff; i++) {
			reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
	}

	return count;
}

static struct device_attribute  reg_dev_attrs = {
	.attr = {
		.name = "reg",
		.mode =  S_IWUSR | S_IRUGO,
	},
	.show =  reg_show,
	.store = reg_store,

};
#endif

static ssize_t tp9932_rpower_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	s32 tmp;
	u8 val;
	int err;

	err = kstrtoint(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp & (1 << 0)) {
		pr_info("enable rear power!\n");
		gpio_direction_output(sensor_indetect.detect_power.gpio, 1);
	} else {
		pr_info("disable rear power!\n");
		gpio_direction_output(sensor_indetect.detect_power.gpio, 0);
	}
	if (gpio_is_valid(sensor_indetect.detect_power_mid.gpio)) {
		if (tmp & (1 << 1)) {
			pr_info("enable mid power!\n");
			gpio_direction_output(sensor_indetect.detect_power_mid.gpio, 1);
		} else {
			pr_info("disable mid power!\n");
			gpio_direction_output(sensor_indetect.detect_power_mid.gpio, 0);
		}
	}

	return count;
}

static struct device_attribute  rpower_dev_attrs = {
	.attr = {
		.name = "rpower",
		.mode =  S_IWUSR,
	},
	.show = NULL,
	.store = tp9932_rpower_store,

};

static int tp9932_sensor_det_init(struct i2c_client *client)
{
	int ret;
	struct device_node *np = NULL;

	/* enable detect work queue */
	INIT_DELAYED_WORK(&sensor_indetect.tp9932_work, sensor_det_work);
	np = of_find_node_by_name(NULL, "ahd_detect");
	if (np == NULL) {
		sensor_err("can not find the tp9932_detect node, will not enable detect kthread\n");
		return -1;
	}

	sensor_indetect.detect_power.gpio = of_get_named_gpio_flags(
	    np, "gpio_power", 0,
	    (enum of_gpio_flags *)(&(sensor_indetect.detect_power)));

	sensor_indetect.detect_power_mid.gpio = of_get_named_gpio_flags(
	    np, "gpio_power_mid", 0,
	    (enum of_gpio_flags *)(&(sensor_indetect.detect_power_mid)));

	sensor_indetect.detect_gpio.gpio = of_get_named_gpio_flags(
	    np, "gpio_detect", 0,
	    (enum of_gpio_flags *)(&(sensor_indetect.detect_gpio)));

	if (!gpio_is_valid(sensor_indetect.detect_power.gpio) ||
	     sensor_indetect.detect_power.gpio < 0) {
		sensor_err("enable tp9932 sensor detect fail!!\n");
		//return -1;
	} else {
		/* enable the detect power*/
		ret = gpio_request(sensor_indetect.detect_power.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable tp9932 sensor detect fail!!\n");
		return -1;
		}
		gpio_direction_output(sensor_indetect.detect_power.gpio, 1);

	}

	if (!gpio_is_valid(sensor_indetect.detect_power_mid.gpio) ||
	     sensor_indetect.detect_power_mid.gpio < 0) {
		sensor_err("enable tp9932 sensor mid io fail!!\n");
		//return -1;
	} else {
		/* enable the detect power*/
		ret = gpio_request(sensor_indetect.detect_power_mid.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable tp9932 sensor detect fail!!\n");
		return -1;
		}
		gpio_direction_output(sensor_indetect.detect_power_mid.gpio, 1);

	}

    sensor_indetect.gpio_detect_flag = 0;
    if (gpio_is_valid(sensor_indetect.detect_gpio.gpio) &&
		sensor_indetect.detect_gpio.gpio >= 0) {
		ret = gpio_request(sensor_indetect.detect_gpio.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable  gpio_tp9932  fail! \n");
			return -1;
		}
		/* enable irq to detect */
		gpio_direction_input(sensor_indetect.detect_gpio.gpio);
#if DET_USE_POLLING
		/* start detect work  */
		schedule_delayed_work(&sensor_indetect.tp9932_work, 0);
#else
		/* request gpio to irq  */
		sensor_indetect.detect_irq = gpio_to_irq(sensor_indetect.detect_gpio.gpio);
		ret = request_irq(
			sensor_indetect.detect_irq, sensor_det_irq_func,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"tp9932_sensor_detect", client);
	}
#endif
		pr_info("use gpio detect!\n");
		sensor_indetect.gpio_detect_flag = 1;
	}
	return 0;
}

static void tp9932_sensor_det_remove(void)
{
	cancel_delayed_work_sync(&sensor_indetect.tp9932_work);

#if !DET_USE_POLLING
	/*free irq*/
	if (free_irq > 0) {
		disable_irq(sensor_indetect.detect_irq);
		free_irq(sensor_indetect.detect_irq, NULL);
	}
#endif

	if (gpio_is_valid(sensor_indetect.detect_power.gpio)) {
		gpio_free(sensor_indetect.detect_power.gpio);
	}
	if (gpio_is_valid(sensor_indetect.detect_power_mid.gpio)) {
		gpio_free(sensor_indetect.detect_power_mid.gpio);
	}
	if (gpio_is_valid(sensor_indetect.detect_gpio.gpio)) {
		gpio_free(sensor_indetect.detect_gpio.gpio);
	}
}

#endif

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int ret;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
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
	info->sensor_field = V4L2_FIELD_NONE;

#if SENSOR_DETECT_KTHREAD
	alloc_chrdev_region(&sensor_indetect.devno, 0, 1, "csi");
	sensor_indetect.cdev = cdev_alloc();
	if (IS_ERR(sensor_indetect.cdev)) {
		sensor_err("cdev_alloc fail!\n");
		goto free_devno;
	}

	cdev_init(sensor_indetect.cdev, &sensor_dev_fops);
	sensor_indetect.cdev->owner = THIS_MODULE;
	ret = cdev_add(sensor_indetect.cdev, sensor_indetect.devno, 1);
	if (ret) {
		sensor_err("cdev_add fail.\n");
		goto free_cdev;
	}

	sensor_indetect.sensor_class = class_create(THIS_MODULE, "csi");
	if (IS_ERR(sensor_indetect.sensor_class)) {
		sensor_err("class_create fail!\n");
		goto unregister_cdev;
	}

	vin_print("[xyd]create csi ...\n");


	sensor_indetect.dev =
	    device_create(sensor_indetect.sensor_class, NULL,
			  sensor_indetect.devno, NULL, "ahd");
	if (IS_ERR(sensor_indetect.dev)) {
		sensor_err("device_create fail!\n");
		goto free_class;
	}
	vin_print("[xyd]create ahd ...\n");

	ret = device_create_file(sensor_indetect.dev, &detect_dev_attrs);
	if (ret) {
		sensor_err("class_create  file fail!\n");
		goto free_class;
	}
#ifdef ENABLE_REG_FS
	ret = device_create_file(sensor_indetect.dev, &reg_dev_attrs);
	if (ret) {
		sensor_err("class_create  file fail!\n");
		goto free_class;
	}
#endif
	ret = device_create_file(sensor_indetect.dev, &rpower_dev_attrs);
	if (ret) {
		sensor_err("class_create  file fail!\n");
		goto free_class;
	}
	vin_print("[xyd]create success ...\n");
    sensor_indetect.client = client;
	/* init tp9932 detect mode */
	ret = tp9932_sensor_det_init(client);
	if (ret) {
		goto free_det;
	}

	return 0;


free_det:
	tp9932_sensor_det_remove();

free_class:
	class_destroy(sensor_indetect.sensor_class);

unregister_cdev:
	cdev_del(sensor_indetect.cdev);

free_cdev:
	kfree(sensor_indetect.cdev);

free_devno:
	unregister_chrdev_region(sensor_indetect.devno, 1);


#endif

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
    sensor_indetect.client = NULL;
	sd = cci_dev_remove_helper(client, &cci_drv);

#if SENSOR_DETECT_KTHREAD
	tp9932_sensor_det_remove();

	device_destroy(sensor_indetect.sensor_class, sensor_indetect.devno);
	class_destroy(sensor_indetect.sensor_class);
	cdev_del(sensor_indetect.cdev);
	kfree(sensor_indetect.cdev);
	unregister_chrdev_region(sensor_indetect.devno, 1);
#endif

	kfree(to_state(sd));
	return 0;
}


static void sensor_shutdown(struct i2c_client *client)
{
	struct v4l2_subdev *sd;

	sd = cci_dev_remove_helper(client, &cci_drv);

#if SENSOR_DETECT_KTHREAD
	tp9932_sensor_det_remove();

	device_destroy(sensor_indetect.sensor_class, sensor_indetect.devno);
	class_destroy(sensor_indetect.sensor_class);
	cdev_del(sensor_indetect.cdev);
	kfree(sensor_indetect.cdev);
	unregister_chrdev_region(sensor_indetect.devno, 1);

#endif
	kfree(to_state(sd));
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
	.shutdown = sensor_shutdown,
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

module_init(init_sensor);
module_exit(exit_sensor);
