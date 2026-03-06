/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for OV02B10 Raw cameras.
 *
 * Copyright (c) 2023 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zheng ZeQun <zequnzheng@allwinnertech.com>
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

MODULE_AUTHOR("myf");
MODULE_DESCRIPTION("A low-level driver for OV02B10 sensors");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR (0x002B)

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The  i2c address
 */
#define OV02B1B_I2C_ADDR  (0x78>>1)
#define OV02B10_I2C_ADDR  (0x7A>>1)

#define SENSOR_NUM		2
#define SENSOR_NAME		"ov02b1b_mipi"
#define SENSOR_NAME_2	"ov02b10_mipi"

#define SENSOR_1600x1200_30FPS  1
#define SENSOR_1280x720_30FPS	0
#define SENSOR_1280x720_15FPS	0
#define SENSOR_800x600_15FPS	0
#define SENSOR_640x480_15FPS	0
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

#if SENSOR_1280x720_30FPS
static struct regval_list sensor_720p_30fps_regs[] = {
	/* 1280x720 30fps 1 lane MIPI 600Mbps/lane */
	{0xfc, 0x01},  //soft reset
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x24, 0x02},  //pll_mc
	{0x25, 0x06},  //pll_nc,dpll clk 72M
	{0x29, 0x01},
	{0x2a, 0xAF},  //mpll_nc, mpll clk 600M
	{0x2b, 0x00},
	{0x1e, 0x17},  //vlow 0.53v
	{0x33, 0x07},  //ipx 2.84u
	{0x35, 0x07},
	{0x4a, 0x0c},  //ncp -1.4v
	{0x3a, 0x05},  //icomp1 4.25u
	{0x3b, 0x02},  //icomp2 1.18u
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x04},
	{0x0f, 0xc5},  //exp
	{0x12, 0x03},  //mirror and flip
	{0x14, 0x01},
	{0x15, 0xe8},
	{0x18, 0x00},  //un fixed-fps
	{0x22, 0x40},  //analog gain
	{0x23, 0x02},  //adc_range 0.595v
	{0x17, 0x2c},  //pd reset row address time
	{0x19, 0x20},  //dac_d0 1024
	{0x1b, 0x06},  //rst_num1 96
	{0x1c, 0x04},  //rst_num2 64
	{0x20, 0x03},
	{0x30, 0x01},  //p0
	{0x33, 0x01},  //p3
	{0x31, 0x0a},  //p1
	{0x32, 0x09},  //p2
	{0x38, 0x01},
	{0x39, 0x01},  //p9
	{0x3a, 0x01},  //p10
	{0x3b, 0x01},
	{0x4f, 0x04},  //p24
	{0x4e, 0x05},  //p23
	{0x50, 0x01},  //p25
	{0x35, 0x0c},  //p5
	{0x45, 0x2a},  //sc1,p20_1
	{0x46, 0x2a},  //p20_2
	{0x47, 0x2a},  //p20_3
	{0x48, 0x2a},  //p20_4
	{0x4a, 0x2c},  //sc2,p22_1
	{0x4b, 0x2c},  //p22_2
	{0x4c, 0x2c},  //p22_3
	{0x4d, 0x2c},  //p22_4
	{0x56, 0x3a},  //p31, 1st d0
	{0x57, 0x0a},  //p32, 1st d1
	{0x58, 0x24},  //col_en1
	{0x59, 0x20},  //p34 2nd d0
	{0x5a, 0x0a},  //p34 2nd d1
	{0x5b, 0xff},  //col_en2
	{0x37, 0x0a},  //p7, tx
	{0x42, 0x0e},  //p17, psw
	{0x68, 0x90},
	{0x69, 0xcd},  //blk en, no sig_clamp
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x09},  //0a
	{0x7e, 0x09},  //0a
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},  //vbl2_4
	{0x88, 0x0f},
	{0x94, 0x02},  //evsync del frame
	{0x98, 0xd1},  //del bad frame
	{0xfe, 0x02},
	{0xfd, 0x03},  //RegPage
	{0x97, 0x78},
	{0x98, 0x78},
	{0x99, 0x78},
	{0x9a, 0x78},
	{0xa1, 0x40},
	{0xb1, 0x30},
	{0xae, 0x0d},  //bit0=1,high 8bit
	{0x88, 0x5b},  //BLC_ABL
	{0x89, 0x7c},  //bit6=1 trigger en
	{0xb4, 0x05},  //mean trigger 5
	{0x8c, 0x40},  //BLC_BLUE_SUBOFFSET_8lsb
	{0x8e, 0x40},  //BLC_RED_SUBOFFSET_8lsb
	{0x90, 0x40},  //BLC_GR_SUBOFFSET_8lsb
	{0x92, 0x40},  //BLC_GB_SUBOFFSET_8lsb
	{0x9b, 0x46},  //digtal gain {0x9b, 0x46},zcy
	{0xac, 0x40},  //blc random noise rpc_th 4x
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},  //PD_MIPI:turn on mipi phy

	{0xfd, 0x00},  //crop to 1280x720
	{0x4f, 0x05},
	{0x50, 0x00},  //mipi hszie low 8bit
	{0x51, 0x02},
	{0x52, 0xd0},  //mipi vsize low 8bit
	{0x55, 0x2b},  //raw10 output

	{0xfd, 0x01},
	{0x03, 0xc0},  //window hstart low 8bit
	{0x04, 0x01},
	{0x05, 0x00},  //window vstart low 8bit
	{0x06, 0x02},
	{0x07, 0x80},  //window hsize low 8bit
	{0x08, 0x02},
	{0x09, 0xd0},  //window vsize low 8bit
	{0xfe, 0x02},
	{0xfb, 0x01},

	{0xfd, 0x03},
//	{0x81, 0x01},  //Color Bar
	{0xc2, 0x01},  //MIPI_EN
	{0xfd, 0x01},
};
#endif
#if SENSOR_1280x720_15FPS
static struct regval_list sensor_720p_15fps_regs[] = {
	/* 1280x720 15fps 1 lane MIPI 600Mbps/lane */
	{0xfc, 0x01},  //soft reset
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x24, 0x02},  //pll_mc
	{0x25, 0x06},  //pll_nc,dpll clk 72M
	{0x29, 0x01},
	{0x2a, 0xAF},  //mpll_nc, mpll clk 600M
	{0x2b, 0x00},
	{0x1e, 0x17},  //vlow 0.53v
	{0x33, 0x07},  //ipx 2.84u
	{0x35, 0x07},
	{0x4a, 0x0c},  //ncp -1.4v
	{0x3a, 0x05},  //icomp1 4.25u
	{0x3b, 0x02},  //icomp2 1.18u
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x04},
	{0x0f, 0xb0},  //exp
	{0x12, 0x03},  //mirror and flip
	{0x14, 0x05},
	{0x15, 0xd4},
	{0x18, 0x00},  //un fixed-fps
	{0x22, 0x20},  //analog gain
	{0x23, 0x02},  //adc_range 0.595v
	{0x17, 0x2c},  //pd reset row address time
	{0x19, 0x20},  //dac_d0 1024
	{0x1b, 0x06},  //rst_num1 96
	{0x1c, 0x04},  //rst_num2 64
	{0x20, 0x03},
	{0x30, 0x01},  //p0
	{0x33, 0x01},  //p3
	{0x31, 0x0a},  //p1
	{0x32, 0x09},  //p2
	{0x38, 0x01},
	{0x39, 0x01},  //p9
	{0x3a, 0x01},  //p10
	{0x3b, 0x01},
	{0x4f, 0x04},  //p24
	{0x4e, 0x05},  //p23
	{0x50, 0x01},  //p25
	{0x35, 0x0c},  //p5
	{0x45, 0x2a},  //sc1,p20_1
	{0x46, 0x2a},  //p20_2
	{0x47, 0x2a},  //p20_3
	{0x48, 0x2a},  //p20_4
	{0x4a, 0x2c},  //sc2,p22_1
	{0x4b, 0x2c},  //p22_2
	{0x4c, 0x2c},  //p22_3
	{0x4d, 0x2c},  //p22_4
	{0x56, 0x3a},  //p31, 1st d0
	{0x57, 0x0a},  //p32, 1st d1
	{0x58, 0x24},  //col_en1
	{0x59, 0x20},  //p34 2nd d0
	{0x5a, 0x0a},  //p34 2nd d1
	{0x5b, 0xff},  //col_en2
	{0x37, 0x0a},  //p7, tx
	{0x42, 0x0e},  //p17, psw
	{0x68, 0x90},
	{0x69, 0xcd},  //blk en, no sig_clamp
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x09},  //0a
	{0x7e, 0x09},  //0a
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},  //vbl2_4
	{0x88, 0x0f},
	{0x94, 0x02},  //evsync del frame
	{0x98, 0xd1},  //del bad frame
	{0xfe, 0x02},
	{0xfd, 0x03},  //RegPage
	{0x97, 0x78},
	{0x98, 0x78},
	{0x99, 0x78},
	{0x9a, 0x78},
	{0xa1, 0x40},
	{0xb1, 0x30},
	{0xae, 0x0d},  //bit0=1,high 8bit
	{0x88, 0x5b},  //BLC_ABL
	{0x89, 0x7c},  //bit6=1 trigger en
	{0xb4, 0x05},  //mean trigger 5
	{0x8c, 0x40},  //BLC_BLUE_SUBOFFSET_8lsb
	{0x8e, 0x40},  //BLC_RED_SUBOFFSET_8lsb
	{0x90, 0x40},  //BLC_GR_SUBOFFSET_8lsb
	{0x92, 0x40},  //BLC_GB_SUBOFFSET_8lsb
	{0x9b, 0x46},  //digtal gain
	{0xac, 0x40},  //blc random noise rpc_th 4x
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},  //PD_MIPI:turn on mipi phy

	{0xfd, 0x00},  //crop to 1280x720
	{0x4f, 0x05},
	{0x50, 0x00},  //mipi hszie low 8bit
	{0x51, 0x02},
	{0x52, 0xd0},  //mipi vsize low 8bit
	{0x55, 0x2b},  //raw10 output

	{0xfd, 0x01},
	{0x03, 0xc0},  //window hstart low 8bit
	{0x04, 0x01},
	{0x05, 0x00},  //window vstart low 8bit
	{0x06, 0x02},
	{0x07, 0x80},  //window hsize low 8bit
	{0x08, 0x02},
	{0x09, 0xd0},  //window vsize low 8bit
	{0xfe, 0x02},
	{0xfb, 0x01},
	{0xfd, 0x03},
	{0xc2, 0x01},  //MIPI_EN
	{0xfd, 0x01},
};
#endif
#if SENSOR_800x600_15FPS
static struct regval_list sensor_800x600_15fps_regs[] = {
	{0xfc, 0x01},  //soft reset
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x24, 0x02},  //pll_mc
	{0x25, 0x06},  //pll_nc,dpll clk 72M
	{0x29, 0x01},
	{0x2a, 0xAF},  //mpll_nc, mpll clk 600M
	{0x2b, 0x00},
	{0x1e, 0x17},  //vlow 0.53v
	{0x33, 0x07},  //ipx 2.84u
	{0x35, 0x07},
	{0x4a, 0x0c},  //ncp -1.4v
	{0x3a, 0x05},  //icomp1 4.25u
	{0x3b, 0x02},  //icomp2 1.18u
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x02},
	{0x0f, 0x1a},  //exp
	{0x12, 0x03},  //mirror and flip
	{0x14, 0x06},
	{0x15, 0x4c},
	{0x18, 0x00},  //un fixed-fps
	{0x22, 0xff},  //analog gain
	{0x23, 0x02},  //adc_range 0.595v
	{0x17, 0x2c},  //pd reset row address time
	{0x19, 0x20},  //dac_d0 1024
	{0x1b, 0x06},  //rst_num1 96
	{0x1c, 0x04},  //rst_num2 64
	{0x20, 0x03},
	{0x30, 0x01},  //p0
	{0x33, 0x01},  //p3
	{0x31, 0x0a},  //p1
	{0x32, 0x09},  //p2
	{0x38, 0x01},
	{0x39, 0x01},  //p9
	{0x3a, 0x01},  //p10
	{0x3b, 0x01},
	{0x4f, 0x04},  //p24
	{0x4e, 0x05},  //p23
	{0x50, 0x01},  //p25
	{0x35, 0x0c},  //p5
	{0x45, 0x2a},  //sc1,p20_1
	{0x46, 0x2a},  //p20_2
	{0x47, 0x2a},  //p20_3
	{0x48, 0x2a},  //p20_4
	{0x4a, 0x2c},  //sc2,p22_1
	{0x4b, 0x2c},  //p22_2
	{0x4c, 0x2c},  //p22_3
	{0x4d, 0x2c},  //p22_4
	{0x56, 0x3a},  //p31, 1st d0
	{0x57, 0x0a},  //p32, 1st d1
	{0x58, 0x24},  //col_en1
	{0x59, 0x20},  //p34 2nd d0
	{0x5a, 0x0a},  //p34 2nd d1
	{0x5b, 0xff},  //col_en2
	{0x37, 0x0a},  //p7, tx
	{0x42, 0x0e},  //p17, psw
	{0x68, 0x90},
	{0x69, 0xcd},  //blk en, no sig_clamp
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x09},  //0a
	{0x7e, 0x09},  //0a
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},  //vbl2_4
	{0x88, 0x0f},
	{0x94, 0x02},  //evsync del frame
	{0x98, 0xd1},  //del bad frame
	{0xfe, 0x02},
	{0xfd, 0x03},  //RegPage
	{0x97, 0x78},
	{0x98, 0x78},
	{0x99, 0x78},
	{0x9a, 0x78},
	{0xa1, 0x40},
	{0xb1, 0x30},
	{0xae, 0x0d},  //bit0=1,high 8bit
	{0x88, 0x5b},  //BLC_ABL
	{0x89, 0x7c},  //bit6=1 trigger en
	{0xb4, 0x05},  //mean trigger 5
	{0x8c, 0x40},  //BLC_BLUE_SUBOFFSET_8lsb
	{0x8e, 0x40},  //BLC_RED_SUBOFFSET_8lsb
	{0x90, 0x40},  //BLC_GR_SUBOFFSET_8lsb
	{0x92, 0x40},  //BLC_GB_SUBOFFSET_8lsb
	{0x9b, 0x46},  //digtal gain
	{0xac, 0x40},  //blc random noise rpc_th 4x
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},  // //PD_MIPI:turn on mipi phy
	{0xfd, 0x00},  //crop to 800x600
	{0x4f, 0x03},
	{0x50, 0x20},  //mipi hszie low 8bit
	{0x51, 0x02},
	{0x52, 0x58},  //mipi vsize low 8bit
	{0xfd, 0x01},
	{0x02, 0x01},
	{0x03, 0x38},  //window hstart low 8bit
	{0x04, 0x01},
	{0x05, 0x3c},  //window vstart low 8bit
	{0x06, 0x01},
	{0x07, 0x90},  //window hsize low 8bit
	{0x08, 0x02},
	{0x09, 0x58},  //window vsize low 8bit
	{0xfd, 0x03},  //
	{0xc2, 0x01},  //MIPI_EN
	{0xfb, 0x01},
	{0xfd, 0x01},
};
#endif
#if SENSOR_640x480_15FPS
static struct regval_list sensor_640x480_30fps_regs[] = {
	{0xfc, 0x01},  //soft reset
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x24, 0x02},  //pll_mc
	{0x25, 0x06},  //pll_nc,dpll clk 72M
	{0x28, 0x00},
	{0x29, 0x03},
	{0x2a, 0xb4},  //mpll_nc, mpll clk 330M
	{0x1e, 0x17},  //vlow 0.53v
	{0x33, 0x07},  //ipx 2.84u
	{0x35, 0x07},  //pcp off
	{0x4a, 0x0c},  //ncp -1.4v
	{0x3a, 0x05},  //icomp1 4.25u
	{0x3b, 0x02},  //icomp2 1.18u
	{0x3e, 0x00},
	{0x46, 0x01},
	{0xfd, 0x01},
	{0x0e, 0x04},
	{0x0f, 0xb0},  //exp
	{0x12, 0x03},  //mirror and flip
	{0x14, 0x0a},
	{0x15, 0x8e},
	{0x18, 0x00},  //un fixed-fps
	{0x22, 0x10},  //analog gain
	{0x23, 0x02},  //adc_range 0.595v
	{0x17, 0x2c},  //pd reset row address time
	{0x19, 0x20},  //dac_d0 1024
	{0x1b, 0x06},  //rst_num1 96
	{0x1c, 0x04},  //rst_num2 64
	{0x20, 0x03},
	{0x30, 0x01},  //p0
	{0x33, 0x01},  //p3
	{0x31, 0x0a},  //p1
	{0x32, 0x09},  //p2
	{0x38, 0x01},
	{0x39, 0x01},  //p9
	{0x3a, 0x01},  //p10
	{0x3b, 0x01},
	{0x4f, 0x04},  //p24
	{0x4e, 0x05},  //p23
	{0x50, 0x01},  //p25
	{0x35, 0x0c},  //p5
	{0x45, 0x2a},  //sc1,p20_1
	{0x46, 0x2a},  //p20_2
	{0x47, 0x2a},  //p20_3
	{0x48, 0x2a},  //p20_4
	{0x4a, 0x2c},  //sc2,p22_1
	{0x4b, 0x2c},  //p22_2
	{0x4c, 0x2c},  //p22_3
	{0x4d, 0x2c},  //p22_4
	{0x56, 0x3a},  //p31, 1st d0
	{0x57, 0x0a},  //p32, 1st d1
	{0x58, 0x24},  //col_en1
	{0x59, 0x20},  //p34 2nd d0
	{0x5a, 0x0a},  //p34 2nd d1
	{0x5b, 0xff},  //col_en2
	{0x37, 0x0a},  //p7, tx
	{0x42, 0x0e},  //p17, psw
	{0x68, 0x90},
	{0x69, 0xcd},  //blk en, no sig_clamp
	{0x7c, 0x08},
	{0x7d, 0x08},
	{0x7e, 0x08},
	{0x7f, 0x08},  //vbl1_4
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},  //vbl2_4
	{0x88, 0x0f},
	{0x94, 0x02},  //evsync del frame
	{0x98, 0xd1},  //del bad frame
	{0xfe, 0x02},
	{0xfd, 0x03},  //RegPage
	{0x97, 0x6c},
	{0x98, 0x60},
	{0x99, 0x60},
	{0x9a, 0x6c},
	{0xae, 0x0d},  //bit0=1,high 8bit
	{0x88, 0x49},  //BLC_ABL
	{0x89, 0x7c},  //bit6=1 trigger en
	{0xb4, 0x05},  //mean trigger 5
	{0xbd, 0x0d},  //blc_rpc_coe
	{0x8c, 0x40},  //BLC_BLUE_SUBOFFSET_8lsb
	{0x8e, 0x40},  //BLC_RED_SUBOFFSET_8lsb
	{0x90, 0x40},  //BLC_GR_SUBOFFSET_8lsb
	{0x92, 0x40},  // BLC_GB_SUBOFFSET_8lsb
	{0x9b, 0x49},  //digtal gain
	{0xac, 0x40},  //blc random noise rpc_th 4x
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},  //PD_MIPI:turn on mipi phy
	{0xfd, 0x00},  //binning 640x480
	{0x4f, 0x02},  //mipi size
	{0x50, 0x80},
	{0x51, 0x01},
	{0x52, 0xe0},
	{0x55, 0x2b},  //raw10 output
	{0xfd, 0x01},
	{0x03, 0xc0},  //window hstart low 8bit
	{0x04, 0x01},
	{0x05, 0x88},  //window vstart low 8bit
	{0x06, 0x02},
	{0x06, 0x02},
	{0x07, 0x80},  //window hsize low 8bit
	{0x08, 0x01},
	{0x09, 0xe0},  //window vsize low 8bit
	{0xfe, 0x02},
	{0xfb, 0x01},
	{0xfd, 0x03},
	{0xc2, 0x01},  //MIPI_EN
	{0xfd, 0x01},
};
#endif

#if SENSOR_1600x1200_30FPS
#if 0
static struct regval_list sensor_1600x1200_30fps_regs[] = {
	{0xfc, 0x01},//   ;soft reset
//;delay 5ms
//sl 5 5
	{0xfd, 0x00},
	{0x24, 0x02},   //;pll_mc
	{0x25, 0x06},   //;pll_nc,dpll clk 72M
	{0x29, 0x01},
	{0x2a, 0xb4},   //;mpll_nc, mpll clk 660M
	{0x2b, 0x00},
	{0x1e, 0x17},   //;vlow 0.53v
	{0x33, 0x07},   //;ipx 2.84u
	{0x35, 0x07},
	{0x4a, 0x0c},   //;ncp -1.4v
	{0x3a, 0x05},   //;icomp1 4.25u
	{0x3b, 0x02},   //;icomp2 1.18u
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x02},
	{0x0f, 0x1a},   //;exp
	{0x18, 0x00},   //;un fixed-fps
	{0x22, 0xff},   //;analog gain
	{0x23, 0x02},   //;adc_range 0.595v
	{0x17, 0x2c},   //;pd reset row address time
	{0x19, 0x20},   //;dac_d0 1024
	{0x1b, 0x06},   //;rst_num1 96
	{0x1c, 0x04},   //;rst_num2 64
	{0x20, 0x03},
	{0x30, 0x01},   //;p0
	{0x33, 0x01},   //;p3
	{0x31, 0x0a},   //;p1
	{0x32, 0x09},   //;p2
	{0x38, 0x01},
	{0x39, 0x01},   //;p9
	{0x3a, 0x01},   //;p10
	{0x3b, 0x01},
	{0x4f, 0x04},   //;p24
	{0x4e, 0x05},   //;p23
	{0x50, 0x01},   //;p25
	{0x35, 0x0c},   //;p5
	{0x45, 0x2a},   //;sc1,p20_1
	{0x46, 0x2a},   //;p20_2
	{0x47, 0x2a},   //;p20_3
	{0x48, 0x2a},   //;p20_4
	{0x4a, 0x2c},   //;sc2,p22_1
	{0x4b, 0x2c},   //;p22_2
	{0x4c, 0x2c},   //;p22_3
	{0x4d, 0x2c},   //;p22_4
	{0x56, 0x3a},   //;p31, 1st d0
	{0x57, 0x0a},   //;p32, 1st d1
	{0x58, 0x24},   //;col_en1
	{0x59, 0x20},   //;p34 2nd d0
	{0x5a, 0x0a},   //;p34 2nd d1
	{0x5b, 0xff},   //;col_en2
	{0x37, 0x0a},   //;p7, tx
	{0x42, 0x0e},   //;p17, psw
	{0x68, 0x90},
	{0x69, 0xcd},   //;blk en, no sig_clamp
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x09},	//;0a
	{0x7e, 0x09},	//;0a
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},   //;vbl2_4
	{0x88, 0x0f},
	{0x94, 0x02},   //;evsync del frame
	{0x98, 0xd1},   //;del bad frame
	{0xfe, 0x02},
	{0xfd, 0x03},   //;RegPage
	{0x97, 0x78},
	{0x98, 0x78},
	{0x99, 0x78},
	{0x9a, 0x78},
	{0xa1, 0x40},
	{0xb1, 0x30},
	{0xae, 0x0d},   //;bit0=1,high 8bit
	{0x88, 0x5b},   //;BLC_ABL
	{0x89, 0x7c},   //;bit6=1 trigger en
	{0xb4, 0x05},   //;mean trigger 5
	{0x8c, 0x40},   //;BLC_BLUE_SUBOFFSET_8lsb
	{0x8e, 0x40},   //;BLC_RED_SUBOFFSET_8lsb
	{0x90, 0x40},   //;BLC_GR_SUBOFFSET_8lsb
	{0x92, 0x40},   //; BLC_GB_SUBOFFSET_8lsb
	{0x9b, 0x46},   //;digtal gain
	{0xac, 0x40},   //;blc random noise rpc_th 4x
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},   //; //PD_MIPI:turn on mipi phy

	{0xfd, 0x00},  //;crop to 1600x1200
	{0x50, 0x40},  //;mipi hszie low 8bit
	{0x52, 0xb0},  //;mipi vsize low 8bit
	{0xfd, 0x01},
	{0x03, 0x70},  //;window hstart low 8bit
	{0x05, 0x10},  //;window vstart low 8bit
	{0x07, 0x20},  //;window hsize low 8bit
	{0x09, 0xb0},  //;window vsize low 8bit


	{0xfb, 0x01},

	//;stream on
	{0xfd, 0x03},
	{0xc2, 0x01},  //;MIPI_EN
	{0xfd, 0x01},
};
#else
static struct regval_list sensor_1600x1200_30fps_regs[] = {
	{0xfc, 0x01},
	{0xfd, 0x00},
	{0xfd, 0x00},
	{0x24, 0x02},
	{0x25, 0x06},
	{0x29, 0x03},
	{0x2a, 0x34},
	{0x1e, 0x17},
	{0x33, 0x07},
	{0x35, 0x07},
	{0x4a, 0x0c},
	{0x3a, 0x05},
	{0x3b, 0x02},
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x02},
	{0x0f, 0x1a},
	{0x18, 0x00},
	{0x22, 0xff},
	{0x23, 0x02},
	{0x17, 0x2c},
	{0x19, 0x20},
	{0x1b, 0x06},
	{0x1c, 0x04},
	{0x20, 0x03},
	{0x30, 0x01},
	{0x33, 0x01},
	{0x31, 0x0a},
	{0x32, 0x09},
	{0x38, 0x01},
	{0x39, 0x01},
	{0x3a, 0x01},
	{0x3b, 0x01},
	{0x4f, 0x04},
	{0x4e, 0x05},
	{0x50, 0x01},
	{0x35, 0x0c},
	{0x45, 0x2a},
	{0x46, 0x2a},
	{0x47, 0x2a},
	{0x48, 0x2a},
	{0x4a, 0x2c},
	{0x4b, 0x2c},
	{0x4c, 0x2c},
	{0x4d, 0x2c},
	{0x56, 0x3a},
	{0x57, 0x0a},
	{0x58, 0x24},
	{0x59, 0x20},
	{0x5a, 0x0a},
	{0x5b, 0xff},
	{0x37, 0x0a},
	{0x42, 0x0e},
	{0x68, 0x90},
	{0x69, 0xcd},
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x0a},
	{0x7e, 0x0a},
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},
	{0x88, 0x0f},
	{0x94, 0x02},
	{0x98, 0xd1},
	{0xfe, 0x02},
	{0xfd, 0x03},
	{0x97, 0x6c},	//;B
	{0x98, 0x60},	//;R
	{0x99, 0x60},	//;Gr
	{0x9a, 0x6c},	//;Gb
	{0xa1, 0x40},
	{0xb1, 0x40},
	{0xaf, 0x04},	//;blc max update frame
	{0xae, 0x0d},
	{0x88, 0x5b},
	{0x89, 0x7c},
	{0xb4, 0x05},
	{0x8c, 0x40},
	{0x8e, 0x40},
	{0x90, 0x40},
	{0x92, 0x40},
	{0x9b, 0x46},
	{0xac, 0x40},
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},

	{0xfd, 0x00},  //;crop to 1600 1200
	{0x50, 0x40},  //;mipi hszie low 8bit
	{0x52, 0xb0},  //;mipi vsize low 8bit
	{0xfd, 0x01},
	{0x03, 0x70},  //;window hstart low 8bit
	{0x05, 0x10},  //;window vstart low 8bit
	{0x07, 0x20},  //;window hsize low 8bit
	{0x09, 0xb0},  //;window vsize low 8bit

	{0xfd, 0x03},  //;
	{0xc2, 0x01},  //;MIPI_EN
	{0xfb, 0x01},
	{0xfd, 0x01},
};
#endif
#endif

static struct regval_list sensor_fmt_raw[] = {

};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}
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
/*
	sensor_read(sd, 0x0d41, &frame_length);
	act_vts = frame_length << 8;
	sensor_read(sd, 0x0d42, &frame_length);
	act_vts |= frame_length;
*/
	fps->fps = 30;//wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int ov02b10_sensor_vts;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);

	if (exp_val < 16) {
		exp_val = 16;
	} else if (exp_val > (ov02b10_sensor_vts-7)) {
		exp_val = (ov02b10_sensor_vts-7);
	}

	sensor_write(sd, 0xfd, 0x01);
	sensor_write(sd, 0x0f, exp_val & 0xff);
	sensor_write(sd, 0x0e, (exp_val >> 8) & 0xff);
	sensor_write(sd, 0xfe, 0x02);
	sensor_dbg("sensor_s_exp 0x%04x\n", exp_val);

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

	if (gain_val < 16) {
		gain_val = 16;
	} else if (gain_val > 0xff) {
		gain_val = 0xff;
	}

	sensor_dbg("11sensor_s_gain 0x%02x\n", gain_val);
	sensor_write(sd, 0xfd, 0x01);
	sensor_write(sd, 0x22, gain_val);
	sensor_write(sd, 0xfe, 0x02);
	sensor_dbg("sensor_s_gain 0x%02x\n", gain_val);

	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				 struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}



/*
 *set && get sensor flip
 */

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	//*code = MEDIA_BUS_FMT_SRGGB10_1X10;
	//sensor_print("%s,l:%d will change bayer format by itself! MEDIA_BUS_FMT_SRGGB10_1X10\n", __func__, __LINE__);

	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	sensor_print("%s,l:%d will change bayer format by itself! MEDIA_BUS_FMT_SBGGR10_1X10\n", __func__, __LINE__);
	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{


	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{

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
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
		break;

	case STBY_OFF:
		sensor_print("STBY_OFF!\n");
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
		sensor_print("PWR_ON!!!\n");
		cci_lock(sd);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		//vin_set_pmu_channel(sd, CMBCSI, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;

	case PWR_OFF:
		sensor_print("PWR_OFF!!do nothing\n");
		#if 0
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		//vin_set_pmu_channel(sd, CMBCSI, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		cci_unlock(sd);
		#endif
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
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
	int eRet, times_out = 3;
	unsigned int sensor_id;

	sensor_write(sd, 0xfd, 0x00);
	do {
		eRet = sensor_read(sd, 0x02, &rdval);
		sensor_print("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(100 * 1000, 120 * 1000);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);

	sensor_id = (rdval << 8);
	sensor_read(sd, 0x03, &rdval);
	sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
	sensor_id |= rdval;
	if (sensor_id != V4L2_IDENT_SENSOR)
		return -ENODEV;

	sensor_print("Done!\n");
#endif
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
	info->low_speed    = 0;
	info->width        = 1600;
	info->height       = 1200;
	info->hflip        = 0;
	info->vflip        = 0;
	info->tpf.numerator      = 1;
	info->tpf.denominator    = 30;	/* 30fps */
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
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
		break;
//	case VIDIOC_VIN_SENSOR_SET_FPS:
//		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
//		break;
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		//sensor_get_temp(sd, (struct sensor_temp *)arg);
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SET_IR:
		sensor_set_ir(sd, (struct ir_switch *)arg);
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
		.desc      = "Raw RGB Bayer",
		//.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.regs      = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp       = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if SENSOR_1600x1200_30FPS
	/* 1600x1200 */
	{
		.width      = 1600,
		.height     = 1200,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 448,//zcy md 463 to 448,2688
		.vts        = 1221,
		//.pclk       = 66*1000*1000,
		//.mipi_bps	= 660*1000*1000,
		.pclk       = 33*1000*1000,
		.mipi_bps	= 330*1000*1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 16,
		.intg_max   = (1228-7),
		.gain_min   = 16,
		.gain_max   = 16<<2,
		.regs		= sensor_1600x1200_30fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_30fps_regs),
		.set_size	= NULL,
	},
#endif
#if SENSOR_1280x720_30FPS
	/* 720p */
	{
		.width      = HD720_WIDTH,
		.height     = HD720_HEIGHT,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 448,//zcy md 463 to 448,2688
		.vts        = 1228,
		.pclk       = 16*1000*1000,//md 72*1000*1000 to 16501320=16*1000*1000
		.mipi_bps	= 600*1000*1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 16,
		.intg_max   = (1228-7),
		.gain_min   = 16,
		.gain_max   = 16<<2,
		.regs		= sensor_720p_30fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_720p_30fps_regs),
		.set_size	= NULL,
	},
#endif
#if SENSOR_1280x720_15FPS
	{
		.width      = HD720_WIDTH,
		.height     = HD720_HEIGHT,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 463,
		.vts        = 2232,
		.pclk       = 72*1000*1000,
		.mipi_bps	= 600*1000*1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 16,
		.intg_max   = (2232-7),
		.gain_min   = 16,
		.gain_max   = 16<<4,
		.regs		= sensor_720p_15fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_720p_15fps_regs),
		.set_size	= NULL,
	},
#endif
#if SENSOR_800x600_15FPS
	{
		.width      = 800,
		.height     = 600,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 0,
		.vts        = 0,
		.pclk       = 72*1000*1000,
		.mipi_bps	= 600*1000*1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 16,
		.intg_max   = (1228-7),
		.gain_min   = 16,
		.gain_max   = 16<<4,
		.regs		= sensor_800x600_15fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_800x600_15fps_regs),
		.set_size	= NULL,
	},
#endif
#if SENSOR_640x480_15FPS
	{
		.width      = 640,
		.height     = 480,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 448,
		.vts        = 1569,
		.pclk       = 72*1000*1000,
		.mipi_bps	= 330*1000*1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 16,
		.intg_max   = (1228-7),
		.gain_min   = 16,
		.gain_max   = 16<<4,
		.regs		= sensor_640x480_30fps_regs,
		.regs_size  = ARRAY_SIZE(sensor_640x480_30fps_regs),
		.set_size	= NULL,
	},
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
__maybe_unused static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{

	cfg->type  = V4L2_MBUS_CSI2_DPHY;
	cfg->bus.mipi_csi2.num_data_lanes = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

	return 0;
}
#else
__maybe_unused static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

	return 0;
}
#endif

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
#if VIN_FALSE
	struct v4l2_queryctrl qc;
	int ret;
#endif
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;
#if VIN_FALSE
	qc.id = ctrl->id;
	ret = sensor_queryctrl(sd, &qc);

	if (ret < 0)
		return ret;

	if (ctrl->val < qc.minimum || ctrl->val > qc.maximum)
		return -ERANGE;
#endif

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
	__maybe_unused struct sensor_exp_gain exp_gain;
	sensor_dbg("sensor_reg_init, ARRAY_SIZE(sensor_default_regs)=%d\n", ARRAY_SIZE(sensor_default_regs));

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
			exp_gain.exp_val = 6000;
			exp_gain.gain_val = 32;
		}
		sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
#endif

	info->width = wsize->width;
	info->height = wsize->height;
	ov02b10_sensor_vts = wsize->vts;

	sensor_print("s_fmt set width = %d, height = %d\n", wsize->width,
			 wsize->height);

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
	.enum_frame_interval = sensor_enum_frame_interval,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
#if IS_ENABLED(CONFIG_VIN_INIT_MELIS)
	info->preview_first_flag = 1;
#else
	info->preview_first_flag = 0;
#endif
	info->exp = 0;
	info->gain = 0;
	info->first_power_flag = 1;

	sensor_print("%s(),l:%d\n", __func__, __LINE__);

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
