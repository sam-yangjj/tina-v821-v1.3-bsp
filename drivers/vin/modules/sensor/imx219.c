/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for IMX219 Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Yang Feng <yangfeng@allwinnertech.com>
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

MODULE_AUTHOR("Chomoly");
MODULE_DESCRIPTION("A low-level driver for IMX219 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x0219

/*
 * Our nominal (default) frame rate.
 */
#ifdef FPGA
#define SENSOR_FRAME_RATE 15
#else
#define SENSOR_FRAME_RATE 30
#endif

/*
 * The IMX219 sits on i2c with ID 0x6c
 */
#define I2C_ADDR 0x20
#define SENSOR_NAME "imx219"
int imx219_sensor_vts, frame_length_save;
#define ES_GAIN(a, b, c) ((unsigned short)(a * 160) < (c * 10) && (c*10) <= (unsigned short)(b * 160))

struct cfg_array {		/* coming later */
	struct regval_list *regs;
	int size;
};



/*
 * The default register settings
 *
 */

static struct regval_list sensor_default_regs[] = {	/* 3280 x 2464_20fps 4lanes 720Mbps/lane */

};

#if IS_ENABLED(CONFIG_SENSOR_IMX219_TWO_LANE_MIPI) /* two lane */
static struct regval_list imx219_mipi_2lane_800x600_80fps_Binning[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x02},
	{0x0161, 0x72},
	{0x0162, 0x0d},
	{0x0163, 0x14},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x03},
	{0x016D, 0x20},
	{0x016E, 0x02},
	{0x016F, 0x58},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x02},
	{0x0175, 0x02},
	{0x0176, 0x00},
	{0x0177, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x01},
	{0x0305, 0x01},
	{0x0306, 0x00},
	{0x0307, 0x36},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};
static struct regval_list sensor_1440x1920_crop_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x09},
	{0x0161, 0xf0},//2544 VTS
	{0x0162, 0x0d},
	{0x0163, 0x78},//3448 HTS
	{0x0164, 0x03},
	{0x0165, 0x98},//920
	{0x0166, 0x09},
	{0x0167, 0x37},//2359
	{0x0168, 0x01},
	{0x0169, 0x10},//272
	{0x016A, 0x08},
	{0x016B, 0x8F},//2191
	{0x016C, 0x05},
	{0x016D, 0xA0},//1440
	{0x016E, 0x07},
	{0x016F, 0x80},//1920
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list sensor_1600x1200_binning_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x06},
	{0x0161, 0x87},//1671
	{0x0162, 0x0D},
	{0x0163, 0x78},// 3448
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x06},
	{0x016D, 0x40},
	{0x016E, 0x04},
	{0x016F, 0xB0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list sensor_hxga_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x09},
	{0x0161, 0xf0},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0xA0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list sensor_hxga_10fps_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x13},
	{0x0161, 0xab},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0xA0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};
static struct regval_list sensor_hxga_15fps_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x0d},
	{0x0161, 0x1d},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0xA0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list IMX219_MIPI_2lane_2592x1944_Crop[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x09},
	{0x0161, 0xf0},//2544 VTS
	{0x0162, 0x0d},
	{0x0163, 0x78},//3448 HTS
	{0x0164, 0x01},
	{0x0165, 0x58},//920 -576
	{0x0166, 0x0B},
	{0x0167, 0x77},//2359 + 576
	{0x0168, 0x01},
	{0x0169, 0x04},//272 -12
	{0x016A, 0x08},
	{0x016B, 0x9B},//2191 + 12
	{0x016C, 0x0A},
	{0x016D, 0x20},//2592
	{0x016E, 0x07},
	{0x016F, 0x98},//1944
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list IMX219_MIPI_2lane_2592x1944_15fps_Crop[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x0d},
	{0x0161, 0x1d},//3357 VTS
	{0x0162, 0x0d},
	{0x0163, 0x78},//3448 HTS
	{0x0164, 0x01},
	{0x0165, 0x58},//920 -576
	{0x0166, 0x0B},
	{0x0167, 0x77},//2359 + 576
	{0x0168, 0x01},
	{0x0169, 0x04},//272 -12
	{0x016A, 0x08},
	{0x016B, 0x9B},//2191 + 12
	{0x016C, 0x0A},
	{0x016D, 0x20},//2592
	{0x016E, 0x07},
	{0x016F, 0x98},//1944
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list sensor_720p_binning_60fps_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x03},
	{0x0161, 0x43},// 835
	{0x0162, 0x0D},
	{0x0163, 0x78},// 3448
	{0x0164, 0x01},
	{0x0165, 0x68},  //360
	{0x0166, 0x0b},
	{0x0167, 0x67},  // 2919
	{0x0168, 0x02},
	{0x0169, 0x00}, //512
	{0x016A, 0x07},
	{0x016B, 0x9F}, // 1951
	{0x016C, 0x05},
	{0x016D, 0x00},  //1280
	{0x016E, 0x02},
	{0x016F, 0xd0},   //720
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

static struct regval_list sensor_720p_2lane_regs[] = {
	{0x0100, 0x00},
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},//03 4lane //01 2lane
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x04},
	{0x0161, 0xf8},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x01},
	{0x0169, 0x32},
	{0x016A, 0x08},
	{0x016B, 0x6D},
	{0x016C, 0x06},
	{0x016D, 0x68},
	{0x016E, 0x03},
	{0x016F, 0x9E},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0172, 0x00},//add for 2lane
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x36}, //0x51 //53 4lane //36 2lane
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x6c},//0x54  //56 4lane //6c 2lane
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},
	{0x0100, 0x01},
};

#else

static struct regval_list sensor_hxga_regs[] = {

	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},

	{0x0160, 0x0F},
	{0x0161, 0xC5},
	{0x0162, 0x0D},
	{0x0163, 0x78},

	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xCF},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016A, 0x09},
	{0x016B, 0x9F},
	{0x016C, 0x0C},
	{0x016D, 0xD0},
	{0x016E, 0x09},
	{0x016F, 0xA0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x47B4, 0x14},

	{0x0100, 0x01},

};

static struct regval_list sensor_sxga_regs[] = {

	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x0a},
	{0x0161, 0x2f},
	{0x0162, 0x0d},
	{0x0163, 0xe8},
	{0x0164, 0x03},
	{0x0165, 0xe8},
	{0x0166, 0x08},
	{0x0167, 0xe7},
	{0x0168, 0x02},
	{0x0169, 0xf0},
	{0x016A, 0x06},
	{0x016B, 0xaF},
	{0x016C, 0x05},
	{0x016D, 0x00},
	{0x016E, 0x03},
	{0x016F, 0xc0},

	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x47B4, 0x14},
	{0x0100, 0x01},
};


static struct regval_list sensor_1080p_regs[] = {
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},

	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},

	{0x0160, 0x0A},
	{0x0161, 0x2F},
	{0x0162, 0x0D},
	{0x0163, 0xE8},
	{0x0164, 0x02},
	{0x0165, 0xA8},
	{0x0166, 0x0A},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xB4},
	{0x016A, 0x06},
	{0x016B, 0xEB},
	{0x016C, 0x07},
	{0x016D, 0x80},
	{0x016E, 0x04},
	{0x016F, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x47B4, 0x14},
	{0x0100, 0x01},
};

static struct regval_list sensor_720p_regs[] = {
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x02},
	{0x0161, 0x00},
	{0x0162, 0x0d}, /* 0D */
	{0x0163, 0xE8},
	{0x0164, 0x03},
	{0x0165, 0xE8},
	{0x0166, 0x08},
	{0x0167, 0xE7},
	{0x0168, 0x03},
	{0x0169, 0x68},
	{0x016A, 0x06},
	{0x016B, 0x37},
	{0x016C, 0x05},
	{0x016D, 0x00},
	{0x016E, 0x02},
	{0x016F, 0xD0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x57},
	{0x0309, 0x05}, /* 0A */
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x47B4, 0x14},
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
static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	data_type frame_length = 0, act_vts = 0;

	sensor_read(sd, 0x0161, &frame_length);
	act_vts = frame_length << 8;
	sensor_read(sd, 0x0160, &frame_length);
	act_vts |= frame_length;
	fps->fps = wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	unsigned char explow, exphigh;
	struct sensor_info *info = to_state(sd);

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;
	exp_val >>= 4;
	exphigh = (unsigned char)((0x00ff00 & exp_val) >> 8);
	explow = (unsigned char)((0x0000ff & exp_val));
	sensor_write(sd, 0x015b, explow);
	sensor_write(sd, 0x015a, exphigh);

	info->exp = exp_val;
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	unsigned int ana_gain;

	if (gain_val < 1 * 16)
		gain_val = 16;
	if (gain_val > 10 * 16)
		gain_val = 10 * 16;

	ana_gain = 256 - (4096 / gain_val);
	sensor_write(sd, 0x0157, (unsigned char)(ana_gain & 0xff));

	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val, frame_length, shutter;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > imx219_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = imx219_sensor_vts;
	if (frame_length_save != frame_length) {
		frame_length_save = frame_length;
		sensor_write(sd, 0x0161, frame_length & 0xff);
		sensor_write(sd, 0x0160, frame_length >> 8);
	}

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

/*
static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == CSI_GPIO_LOW)
		ret = sensor_write(sd, 0x0100, rdval & 0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval | 0x01);
	return ret;
}
*/
/*
 * Stuff that knows about the sensor.
 */

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret;

	ret = 0;
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		cci_unlock(sd);
		vin_set_mclk(sd, OFF);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		msleep(20);
		cci_unlock(sd);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AFVDD, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(30000, 31000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
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
#if !IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	data_type rdval;

	sensor_read(sd, 0x0000, &rdval);
	if ((rdval & 0x0f) != 0x02)
		return -ENODEV;

	sensor_read(sd, 0x0001, &rdval);
	if (rdval != 0x19)
		return -ENODEV;
	sensor_print("find the sony IMX219 ***********\n");
#endif
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
	info->width = HXGA_WIDTH;
	info->height = HXGA_HEIGHT;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;

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
			memcpy(arg,
			       info->current_wins,
			       sizeof(*info->current_wins));
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
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
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
#if IS_ENABLED(CONFIG_SENSOR_IMX219_TWO_LANE_MIPI) /* two lane */
	{
		.width      = 800,
		.height     = 600,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 3348,
		.vts        = 626,
		.pclk       = 172800000,
		.mipi_bps   = 864 * 1000 * 1000,
		.fps_fixed  = 80,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (626 - 4) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 10 << 4,
		.regs	    = imx219_mipi_2lane_800x600_80fps_Binning,
		.regs_size  = ARRAY_SIZE(imx219_mipi_2lane_800x600_80fps_Binning),
		.set_size = NULL,
	},
	/* 1440x1920 */
	{
		.width = 1440,
		.height = 1920,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 2544,
		.pclk = 265600000,
		.mipi_bps = 1325 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (2544 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_1440x1920_crop_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_1440x1920_crop_2lane_regs),
		.set_size = NULL,
	},

	/* 1600x1200 */
	{
		.width = 1600,
		.height = 1200,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 1671,
		.pclk = 172848240,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1671 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_1600x1200_binning_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_1600x1200_binning_2lane_regs),
		.set_size = NULL,
	},

	/* 3280x2464 */
	{
		.width = 3280,
		.height = 2464,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 2544,
		.pclk = 173624040,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 20,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (2544 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_hxga_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_hxga_2lane_regs),
		.set_size = NULL,
	},

	/* 3280x2464 */
	{
		.width = 3280,
		.height = 2464,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 3357,
		.pclk = 173624040,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (3357 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_hxga_15fps_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_hxga_15fps_2lane_regs),
		.set_size = NULL,
	},

	/* 3280x2464 */
	{
		.width = 3280,
		.height = 2464,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 5035,
		.pclk = 173624040,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 10,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (5035 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_hxga_10fps_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_hxga_10fps_2lane_regs),
		.set_size = NULL,
	},
	/* 2592x1944 */
	{
		.width = 2592,
		.height = 1944,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 2544,
		.pclk = 173624040,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 20,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (2544 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = IMX219_MIPI_2lane_2592x1944_Crop,
		.regs_size = ARRAY_SIZE(IMX219_MIPI_2lane_2592x1944_Crop),
		.set_size = NULL,
	},

	/* 2592x1944 */
	{
		.width = 2592,
		.height = 1944,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 3357,
		.pclk = 173624040,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (3357 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = IMX219_MIPI_2lane_2592x1944_15fps_Crop,
		.regs_size = ARRAY_SIZE(IMX219_MIPI_2lane_2592x1944_15fps_Crop),
		.set_size = NULL,
	},

	/* 1280x720 */
	{
		.width = 1280,
		.height = 720,
		.hoffset = 0,
		.voffset = 0,
		.hts = 3448,
		.vts = 835,
		.pclk = 172744800,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 60,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (835 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_720p_binning_60fps_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_720p_binning_60fps_2lane_regs),
		.set_size = NULL,
	},

	/* 1632x918 */
	{
		.width = 1632,
		.height = 918,
		.hoffset = 4,
		.voffset = 4,
		.hts = 3448,
		.vts = 1272,
		.pclk = 265600000,
		.mipi_bps = 864 * 1000 * 1000,
		.fps_fixed = 60,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1272 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 10 << 4,
		.regs = sensor_720p_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_720p_2lane_regs),
		.set_size = NULL,
	},
#else
	/* 3280*2464 */
	{
	 .width = 3264,
	 .height = 2448,
	 .hoffset = (3280 - 3264) / 2,
	 .voffset = (2464 - 2448) / 2,
	 .hts = 3448,
	 .vts = 4037,
	 .pclk = (278 * 1000 * 1000),
	 .mipi_bps = 720 * 1000 * 1000,
	 .fps_fixed = 1,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (4037 - 4) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 10 << 4,
	 .regs = sensor_hxga_regs,
	 .regs_size = ARRAY_SIZE(sensor_hxga_regs),
	 .set_size = NULL,
	 },

	/* 1080P */
	{
	 .width = HD1080_WIDTH,
	 .height = HD1080_HEIGHT,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 3560,
	 .vts = 2607,
	 .pclk = (278 * 1000 * 1000),
	 .mipi_bps = 720 * 1000 * 1000,
	 .fps_fixed = 1,
	 .bin_factor = 2,
	 .intg_min = 1 << 4,
	 .intg_max = (2607 - 4) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 10 << 4,
	 .regs = sensor_1080p_regs,
	 .regs_size = ARRAY_SIZE(sensor_1080p_regs),
	 .set_size = NULL,
	 },
	/* SXGA */
	{
	 .width = SXGA_WIDTH,
	 .height = SXGA_HEIGHT,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 3560,
	 .vts = 2607,
	 .pclk = (278 * 1000 * 1000),
	 .mipi_bps = 720 * 1000 * 1000,
	 .fps_fixed = 1,
	 .bin_factor = 2,
	 .intg_min = 1 << 4,
	 .intg_max = 2607 << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 10 << 4,
	 .regs = sensor_sxga_regs,
	 .regs_size = ARRAY_SIZE(sensor_sxga_regs),
	 .set_size = NULL,
	 },
	/* 720p */
	{
	 .width = HD720_WIDTH,
	 .height = HD720_HEIGHT,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 2560,
	 .vts = 1303,
	 .pclk = (200 * 1000 * 1000),
	 .mipi_bps = 720 * 1000 * 1000,
	 .fps_fixed = 1,
	 .bin_factor = 2,
	 .intg_min = 1 << 4,
	 .intg_max = (1303 - 4) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 10 << 4,
	 .regs = sensor_720p_regs,
	 .regs_size = ARRAY_SIZE(sensor_720p_regs),
	 .set_size = NULL,
	 },
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_reg_init(struct sensor_info *info)
{

	int ret = 0;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
#if IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	__maybe_unused struct sensor_exp_gain exp_gain;
#endif

	ret = sensor_write_array(sd, sensor_default_regs,
			       ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

#if IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	if (info->preview_first_flag) {
		info->preview_first_flag = 0;
	} else {
		if (wsize->regs)
			sensor_write_array(sd, wsize->regs, wsize->regs_size);
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
		} else {
			exp_gain.exp_val = 11200;
			exp_gain.gain_val = 32;
		}
		// sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
#endif

	info->width = wsize->width;
	info->height = wsize->height;
	imx219_sensor_vts = wsize->vts;
	frame_length_save = 0;
	sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
		      wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d %x\n", __func__, enable,
		  info->current_wins->width,
		  info->current_wins->height, info->fmt->mbus_code);

	if (!enable)
		return 0;
	return sensor_reg_init(info);
}

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
			struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cfg->bus.mipi_csi2.num_data_lanes = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
#if IS_ENABLED(CONFIG_SENSOR_IMX219_TWO_LANE_MIPI) /* one lane */
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif

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
	}
	return -EINVAL;
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
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	.addr_width = CCI_BITS_16,
	.data_width = CCI_BITS_8,
};

static const struct v4l2_ctrl_config sensor_custom_ctrls[] = {
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_FRAME_RATE,
		.name = "frame rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 15,
		.max = 120,
		.step = 1,
		.def = 120,
	},
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int i;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2 + ARRAY_SIZE(sensor_custom_ctrls));

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
			      65536 * 16, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);
	for (i = 0; i < ARRAY_SIZE(sensor_custom_ctrls); i++)
		v4l2_ctrl_new_custom(handler, &sensor_custom_ctrls[i], NULL);

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

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);
	sensor_init_controls(sd, &sensor_ctrl_ops);
	mutex_init(&info->lock);

#if IS_ENABLED(CONFIG_SAME_I2C)
	info->sensor_i2c_addr = I2C_ADDR >> 1;
#endif
	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
#if IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	info->preview_first_flag = 1;
#else
	info->preview_first_flag = 0;
#endif
	info->af_first_flag = 1;
	info->first_power_flag = 1;

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
