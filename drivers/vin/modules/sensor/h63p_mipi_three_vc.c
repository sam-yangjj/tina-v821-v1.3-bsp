/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved.
 */
/*
 * A V4L2 driver for sc031gs_mipi cameras.
 *
 * Copyright (c) 2019 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "../../utility/vin_log.h"
#include "camera.h"
#include "sensor_helper.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>

MODULE_AUTHOR("ywj");
MODULE_DESCRIPTION("A low-level driver for h63p sensors");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

//#define CONFIG_SENSOR_H63_SM_VSYNC_MODE 1
//#define CONFIG_SENSOR_H63_VSYNC_MODE 1

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
#define MCLK (27 * 1000 * 1000)
#else
#define MCLK (24 * 1000 * 1000)
#endif
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH 0x0A
#define ID_REG_LOW 0x0B
#define ID_VAL_HIGH 0x08
#define ID_VAL_LOW 0x48
#define SENSOR_FRAME_RATE 20

#define H63_REG_GAIN (0x00) /*!< H63 Gain Register			*/

/*
 * The h63p i2c address
 */
#define I2C_ADDR 0x80

#define SENSOR_NUM 0x2
#define SENSOR_NAME "h63p_mipi"
#define SENSOR_NAME_2 "h63p_mipi_2"
#define SENSOR_NAME_3 "h63p_mipi_3"
#define _SOI_MIRROR_FLIP

#if (CONFIG_SENSOR_H63P_NUM == 3)
#define SENSOR_NUM 0x3
#endif

data_type sensor_flip_status;

#define SWITCH_SENSOR_A 0
#define SWITCH_SENSOR_B 1
#define SWITCH_SENSOR_MAX 2

#define CSI_GPIO_LOW 0
#define CSI_GPIO_HIGH 1
#define GPIO_SWITCH_SENSOR_A_STATUS CSI_GPIO_LOW
#define GPIO_SWITCH_SENSOR_B_STATUS CSI_GPIO_HIGH

#define GPIO_SWITCH_NUM 113 // GPIO D

#define A_I2C_ADDR 0x88
#define B_I2C_ADDR 0x8C

static int
    change_flags; /* 0: isp1 & SWITCH_SENSOR_B(SENSOR_NAME_3), 1: isp2 & SWITCH_SENSOR_A(SENSOR_NAME_2) */

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
static int change_flags_select_sensor[2] = {SWITCH_SENSOR_B, SWITCH_SENSOR_A};
static int change_flags_select_sensor_id[2] = {2, 1};
//static int last_exp_val_save[2] = {0, 0};
#endif

static struct sensor_switch_cfg_t sensor_switch_cfg = {
    .i2c_addr_container = {A_I2C_ADDR, B_I2C_ADDR},
    .frame_length_save = {0x2ee, 0x2ee},
    .switch_gpio_ctl = { CSI_GPIO_LOW /*A*/,
			CSI_GPIO_HIGH /*B*/}, //使用mipi_switch gpio切换的电平状态对应sensor
};

// 初始化GPIO
static int init_switch_gpio(void)
{
    int ret;

    // 请求GPIO引脚
    ret = gpio_request(GPIO_SWITCH_NUM, "mipi_switch");
    if (ret) {
	printk(KERN_ERR "Failed to request GPIO pin\n");
	return ret;
    }
    sensor_dbg(" sensor_switch_gpio :%d\n", GPIO_SWITCH_NUM);
    // 设置GPIO引脚为输出模式
    ret = gpio_direction_output(GPIO_SWITCH_NUM, 0);
    if (ret) {
	printk(KERN_ERR "Failed to set GPIO direction\n");
	gpio_free(GPIO_SWITCH_NUM);
	return ret;
    }

    return 0;
}

// 控制GPIO输出状态
static void control_gpio(int value)
{
    unsigned int read_data;
    unsigned int write_data;
    int bit_data;
    // 设置GPIO引脚输出状态
    gpio_set_value(GPIO_SWITCH_NUM, value);
}

// 清理GPIO资源
static void cleanup_gpio(void)
{
	gpio_free(GPIO_SWITCH_NUM);
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

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_720p15_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x1E},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0x40},
	{0x21, 0x06},
	{0x22, 0xEE},
	{0x23, 0x02},
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0xB6},
	{0x28, 0x15},
	{0x29, 0x04},
	{0x2A, 0xAB},
	{0x2B, 0x14},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0x8D, 0x49},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x50},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x8C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x46},
	{0x74, 0x00},
	{0x78, 0x8E},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x00},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	{0x12, 0x00},
};

static struct regval_list sensor_720p30_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x1E},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0x20},
	{0x21, 0x03},
	{0x22, 0xEE},
	{0x23, 0x02},
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0x8B},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x80},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0x8D, 0x49},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x50},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x8C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x46},
	{0x74, 0x00},
	{0x78, 0x8E},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x00},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	{0x12, 0x00},
};

static struct regval_list sensor_720p30_sync_mode_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x20},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0xEE},
	{0x23, 0x02},
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0xD6},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0xCB},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0xAB, 0x00},
	{0x8D, 0x49},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x50},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x8C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x46},
	{0x74, 0x00},
	{0x78, 0x8E},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x00},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	{0x12, 0x00},
};

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
static struct regval_list sensor_720p15_smvs_master_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x20},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0xDC},
	{0x23, 0x05},
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0xD6},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0xCB},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0xAB, 0x00},
	{0x8D, 0x49},
	{0x8D, 0x49},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x50},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x8C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x46},
	{0x74, 0x00},
	{0x78, 0x8E},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x00},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	/* set the sensor to mastert and output vsync */
	{0x46, 0x00},
	{0x47, 0x42},
};

static struct regval_list sensor_720p15_smvs_slave_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x20},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0xDC},
	{0x23, 0x05},
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0xD6},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0xCB},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0xAB, 0x00},
	{0x8D, 0x49},
	{0x8D, 0x49},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x50},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x8C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x46},
	{0x74, 0x00},
	{0x78, 0x8E},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x00},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	{0x43, 0xee},
	{0x44, 0x42},
	{0x1e, 0x00},
	{0x80, 0x81},

};
#endif
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static struct cci_driver cci_drv[] = {
    {
	.name = SENSOR_NAME, .addr_width = CCI_BITS_8, .data_width = CCI_BITS_8,
    },
    {
	.name = SENSOR_NAME_2,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
    },
    {
	.name = SENSOR_NAME_3,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
    }};

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
    int again = 0;
    int tmp = 0;
    unsigned char regdata = 0;

    /// gain: 16=1X, 32=2X, 48=3X, 64=4X, ......, 240=15X, 256=16X, ......
    again = gain;
    while (again > 31) {
	again = again >> 1;
	tmp++;
    }
    if (again > 15) {
	again = again - 16;
    }
    regdata = (unsigned char)((tmp << 4) | again);
    sensor_write(sd, H63_REG_GAIN, regdata & 0xFF);

    return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
    struct sensor_info *info = to_state(sd);
    //	printk("h63p sensor gain value is %d\n",gain_val);

    if (gain_val == info->gain)
	return 0;

    // sensor_dbg( "gain_val:%d\n", gain_val );
    setSensorGain(sd, gain_val);
    info->gain = gain_val;

    return 0;
}

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
    struct sensor_info *info = to_state(sd);
    struct sensor_win_size *wsize = info->current_wins;
    data_type frame_length = 0, act_vts = 0;
    struct v4l2_subdev *sd_temp = sd;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sd_temp = cci_drv[change_flags_select_sensor_id[change_flags]].sd;
    }
#endif

    sensor_read(sd_temp, 0x23, &frame_length);
    act_vts = frame_length << 8;
    sensor_read(sd_temp, 0x22, &frame_length);
    act_vts |= frame_length;
    fps->fps = wsize->pclk / (wsize->hts * act_vts);
    // sensor_dbg("fps = %d\n", fps->fps);

    return 0;
}

static int h63p_sensor_vts;

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
    struct sensor_info *info = to_state(sd);
    __maybe_unused struct sensor_switch_cfg_t *sensor_switch_config;
    *value = info->exp;
#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sensor_switch_config = &sensor_switch_cfg;
	*value = sensor_switch_config
		     ->exp_val_save[change_flags_select_sensor[change_flags]];
    }
#endif
    sensor_dbg("sensor_get_exposure = %d\n", info->exp);
    return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
    struct sensor_info *info = to_state(sd);
    //	data_type		reg_0x01, reg_0x02;
    //	int		max_exp_val;
    int tmp_exp_val;

    tmp_exp_val = exp_val / 16;

    sensor_write(sd, 0x02, (tmp_exp_val >> 8) & 0xFF);
    sensor_write(sd, 0x01, (tmp_exp_val)&0xFF);

    info->exp = exp_val;
    return 0;
}

static int sensor_s_exp_t(struct v4l2_subdev *sd, unsigned int exp_val)
{
    struct sensor_info *info = to_state(sd);
    int tmp_exp_val = exp_val / 16;
    struct v4l2_subdev *sd_temp = sd;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sd_temp = cci_drv[change_flags_select_sensor_id[change_flags]].sd;
    }
#endif

    sensor_write(sd_temp, 0x02, (tmp_exp_val >> 8) & 0xFF);
    sensor_write(sd_temp, 0x01, (tmp_exp_val)&0xFF);
    // sensor_dbg("exp_val:%d\n", exp_val);
    info->exp = exp_val;
    return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
    struct sensor_info *info = to_state(sd);
    *value = info->gain;
    __maybe_unused struct sensor_switch_cfg_t *sensor_switch_config;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sensor_switch_config = &sensor_switch_cfg;
	*value = sensor_switch_config
		     ->gain_val_save[change_flags_select_sensor[change_flags]];
    }
#endif
    sensor_dbg("sensor_get_gain = %d\n", info->gain);
    return 0;
}

static int sensor_s_gain_t(struct v4l2_subdev *sd, int gain_val)
{
    struct sensor_info *info = to_state(sd);
    struct v4l2_subdev *sd_temp = sd;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sd_temp = cci_drv[change_flags_select_sensor_id[change_flags]].sd;
    }
#endif

    // sensor_dbg("gain_val:%d\n", gain_val);
    setSensorGain(sd_temp, gain_val);
    info->gain = gain_val;

    return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
    int exp_val, gain_val;
    int shutter, frame_length;
    struct sensor_info *info = to_state(sd);

    exp_val = exp_gain->exp_val;
    gain_val = exp_gain->gain_val;

    if (gain_val < 1 * 16)
	gain_val = 16;

    if (exp_val > 0xfffff)
	exp_val = 0xfffff;

    shutter = exp_val >> 4;
    if (shutter > h63p_sensor_vts - 4)
	frame_length = shutter + 4;
    else
	frame_length = h63p_sensor_vts;

    // sensor_print("frame_length = %d\n", frame_length);
    /* write vts */
    // sensor_write(sd, 0x22, frame_length & 0xff);
    // sensor_write(sd, 0x23, frame_length >> 8);
    if (SENSOR_NUM == 3) {
	struct sensor_switch_cfg_t *sensor_switch_config;
	sensor_switch_config = &sensor_switch_cfg;
	if (((exp_gain->r_gain >> 16) & 0xff) == 1 &&
	    ((exp_gain->b_gain >> 16) & 0xff) == 1) {
	    // sensor_dbg("%s:sensor_set_gain exp = %d, gain = %d \n",
	    // "sensorC", sensor_switch_config->exp_val_save[SWITCH_SENSOR_B],
	    // sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
	    if (sensor_switch_config->exp_val_save[SWITCH_SENSOR_B] !=
		    exp_val ||
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_B] !=
		    gain_val) {
		// if (last_exp_val_save[SWITCH_SENSOR_B] != exp_val ) {
		//last_exp_val_save[SWITCH_SENSOR_B] =
		    sensor_switch_config->exp_val_save[SWITCH_SENSOR_B];
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_B] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_B] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = true;
		sensor_switch_config->frame_length_save[SWITCH_SENSOR_B] =
		    frame_length;
		// }
	    }
	} else if (((exp_gain->r_gain >> 16) & 0xff) == 2 &&
		   ((exp_gain->b_gain >> 16) & 0xff) == 2) {
	    // sensor_dbg("%s:sensor_set_gain exp = %d, gain = %d \n",
	    // "sensorB", sensor_switch_config->exp_val_save[SWITCH_SENSOR_A],
	    // sensor_switch_config->gain_val_save[SWITCH_SENSOR_A]);
	    if (sensor_switch_config->exp_val_save[SWITCH_SENSOR_A] !=
		    exp_val ||
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_A] !=
		    gain_val) {
		// if (last_exp_val_save[SWITCH_SENSOR_A] != exp_val ) {
		//last_exp_val_save[SWITCH_SENSOR_A] =
		    sensor_switch_config->exp_val_save[SWITCH_SENSOR_A];
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_A] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_A] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = true;
		sensor_switch_config->frame_length_save[SWITCH_SENSOR_A] =
		    frame_length;
		// }
	    }
	} else {
	    sensor_s_exp(sd, exp_val);
	    sensor_s_gain(sd, gain_val);
	}
    } else {
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
    }
    info->exp = exp_val;
    info->gain = gain_val;
    return 0;
}

/*
 *set && get sensor flip
 */

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
#if VIN_FALSE
    struct sensor_info *info = to_state(sd);
    data_type get_value, flip_mirror;

    sensor_read(sd, 0x012, &get_value);
    flip_mirror = (get_value >> 4) & 0x3;
    // bit[5]bit[4] 00:normal 01:flip 10:mirror 11:flip&mirror
    switch (flip_mirror) {
    case 0x0:
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	break;
    case 0x1:
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	break;
    case 0x2:
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	break;
    case 0x3:
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	break;
    default:
	*code = info->fmt->mbus_code;
    }
#else
    struct sensor_info *info = to_state(sd);
    *code = info->fmt->mbus_code;
#endif
    return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
    data_type get_value;
    data_type set_value;
    struct v4l2_subdev *sd_temp = sd;

    if (!(enable == 0 || enable == 1))
	return -1;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sd_temp = cci_drv[change_flags_select_sensor_id[change_flags]].sd;
    }
#endif

    get_value = sensor_flip_status;
    // printk("--sensor hfilp set[%d] read value:0x%X --\n", enable, get_value);
    if (enable)
	set_value = get_value | 0x10;
    else
	set_value = get_value & 0xEF;

    sensor_flip_status = set_value;
    sensor_print("--set sensor hfilp the value:0x%X \n--", set_value);
    sensor_write(sd_temp, 0x12, set_value);
    return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
    data_type get_value;
    data_type set_value;
    struct v4l2_subdev *sd_temp = sd;

    if (!(enable == 0 || enable == 1))
	return -1;

#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3 && !strcmp(sd->name, cci_drv[1].name)) {
	sd_temp = cci_drv[change_flags_select_sensor_id[change_flags]].sd;
    }
#endif

    get_value = sensor_flip_status;
    // printk("--sensor vfilp set[%d] read value:0x%X --\n", enable, get_value);
    if (enable)
	set_value = get_value | 0x20;
    else
	set_value = get_value & 0xDF;

    sensor_flip_status = set_value;
    sensor_print("--set sensor vfilp the value:0x%X --\n", set_value);
    sensor_write(sd_temp, 0x12, set_value);
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
	sensor_dbg("PWR_ON!\n");
	cci_lock(sd);
	if ((SENSOR_NUM == 3) && (!strcmp(sd->name, SENSOR_NAME_2))) {
	    sensor_dbg("PWR_ON!22\n");
	    vin_gpio_set_status(sd, PWDN, 1);
	    vin_gpio_set_status(sd, RESET, 1);
	    vin_gpio_set_status(sd, POWER_EN, 1);
	    usleep_range(1000, 1200);
	    vin_set_pmu_channel(sd, AVDD, ON); // Turn on AVDD
	    usleep_range(100, 120);
	    vin_set_pmu_channel(sd, DVDD, ON); // DVDD is controlled internally
	    usleep_range(1000, 1200);
	    vin_set_pmu_channel(sd, IOVDD, ON); // Turn on DOVDD
	    vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	    vin_set_mclk(sd, ON);
	    usleep_range(1000, 1200);
	    vin_set_mclk_freq(sd, MCLK);
	    usleep_range(1000, 1200);
	    usleep_range(10000, 12000);
	    usleep_range(1000, 1200);
	    usleep_range(10000, 12000);
	} else {
	    sensor_dbg("PWR_ON!\n");
	    vin_gpio_set_status(sd, PWDN, 1);
	    vin_gpio_set_status(sd, RESET, 1);
	    vin_gpio_set_status(sd, POWER_EN, 1);
	    vin_gpio_write(sd, PWDN,
			   CSI_GPIO_HIGH); // Pull up PWDN pin initially
	    vin_gpio_write(sd, RESET,
			   CSI_GPIO_LOW); // Pull down RESET# pin initially
	    usleep_range(1000, 1200);
	    vin_set_pmu_channel(sd, AVDD, ON); // Turn on AVDD
	    usleep_range(100, 120);
	    vin_set_pmu_channel(sd, DVDD, ON); // DVDD is controlled internally
	    usleep_range(1000, 1200);
	    vin_set_pmu_channel(sd, IOVDD, ON);       // Turn on DOVDD
	    vin_gpio_write(sd, RESET, CSI_GPIO_HIGH); // Pull up RESET# pin
	    vin_set_mclk(sd, ON);
	    usleep_range(1000, 1200);
	    vin_set_mclk_freq(sd, MCLK);
	    usleep_range(1000, 1200);
	    vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	    usleep_range(10000, 12000);
	    vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	    usleep_range(1000, 1200);
	    vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
	    usleep_range(10000, 12000);
	}
	cci_unlock(sd);
	break;
    case PWR_OFF:
	sensor_dbg("PWR_OFF!do nothing\n");
	break;
	cci_lock(sd);

	usleep_range(10000, 12000);		 /// > 512 MCLK cycles
	vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH); /// Pull up PWDN pin.
	usleep_range(1000, 1200);		 /// Add delay.
	vin_gpio_write(sd, RESET, CSI_GPIO_LOW); /// Pull down RESET# pin.
	usleep_range(1000, 1200);		 /// Add delay.

	vin_set_mclk(sd, OFF); /// Disable MCLK.
	vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);

	usleep_range(1000, 1200);	    /// Add delay.
	vin_set_pmu_channel(sd, IOVDD, OFF); /// Turn off DOVDD.
	vin_set_pmu_channel(sd, DVDD, OFF);

	vin_set_pmu_channel(sd, AVDD, OFF); /// Turn off AVDD.
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
    int eRet;
    int times_out = 3;

    do {
	eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_print("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval,
		     times_out);
	usleep_range(200000, 220000);
	times_out--;
    } while (eRet < 0 && times_out > 0);

    sensor_read(sd, ID_REG_HIGH, &rdval);
    sensor_print("ID_VAL_HIGH = %2x, Done!\n", rdval);
    if (rdval != ID_VAL_HIGH)
	return -ENODEV;
    sensor_read(sd, ID_REG_LOW, &rdval);
    sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
    if (rdval != ID_VAL_LOW)
	return -ENODEV;

    sensor_print("Done!\n");
#endif
    return 0;
}

static irqreturn_t sensor_vsync_irq_func(int irq, void *priv)
{
    struct v4l2_subdev *sd = priv;
    struct sensor_info *info = to_state(sd);
    unsigned long flags;

    struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
    struct sensor_switch_cfg_t *sensor_switch_config;

    sensor_switch_config = &sensor_switch_cfg;
    sensor_switch_config->vsync_cnt++;
    // sensor_dbg(" sensor_vysnc vsync_cnt
    // :%d\n",sensor_switch_config->vsync_cnt);

    if (!strcmp(sd->name, SENSOR_NAME_2)) {
	if (sensor_switch_config->vsync_cnt % 250 == 0) {
	    // sensor_print("ywj_debug: %s vysnc come\n", sd->name);
	}
	control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
	sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
	schedule_work(
	    &sensor_vysnc_cfg
		 ->s_sensor_switch_change_task); //触发队列去写sensor寄存器
    }

    return IRQ_HANDLED;
}

static int sensor_group_s_exp(struct v4l2_subdev *sd, unsigned int exp_val,
			      unsigned int which_sensor)
{
    struct sensor_info *info = to_state(sd);
    int tmp_exp_val;
    struct sensor_switch_cfg_t *sensor_switch_config;
    sensor_switch_config = &sensor_switch_cfg;

    if (which_sensor >= SWITCH_SENSOR_MAX) {
	printk("para err! which_sensor=%d\n", which_sensor);
	return -1;
    }

    tmp_exp_val = exp_val / 16;
    sensor_switch_config->sensor_group_reg[which_sensor][0] = 0x02;
    sensor_switch_config->sensor_group_reg[which_sensor][1] =
	(tmp_exp_val >> 8) & 0xFF;
    sensor_switch_config->sensor_group_reg[which_sensor][2] = 0x01;
    sensor_switch_config->sensor_group_reg[which_sensor][3] =
	tmp_exp_val & 0xFF;
    info->exp = exp_val;

    return 0;
}

static int groupsetSensorGain(struct v4l2_subdev *sd, int gain,
			      unsigned int which_sensor)
{
    int again = 0;
    int tmp = 0;
    unsigned char regdata = 0;

    struct sensor_switch_cfg_t *sensor_switch_config;
    sensor_switch_config = &sensor_switch_cfg;

    if (which_sensor >= SWITCH_SENSOR_MAX) {
	printk("para err! which_sensor=%d\n", which_sensor);
	return -1;
    }

    /// gain: 16=1X, 32=2X, 48=3X, 64=4X, ......, 240=15X, 256=16X, ......
    again = gain;
    while (again > 31) {
	again = again >> 1;
	tmp++;
    }
    if (again > 15) {
	again = again - 16;
    }
    regdata = (unsigned char)((tmp << 4) | again);
    sensor_switch_config->sensor_group_reg[which_sensor][4] = 0x00;
    sensor_switch_config->sensor_group_reg[which_sensor][5] = regdata & 0xFF;

    return 0;
}

static int sensor_group_s_gain(struct v4l2_subdev *sd, int gain_val,
			       unsigned int which_sensor)
{
    struct sensor_info *info = to_state(sd);

    groupsetSensorGain(sd, gain_val, which_sensor);
    info->gain = gain_val;

    return 0;
}

static void sensor_switch_change(struct v4l2_subdev *sd)
{
    unsigned int max_frame_length;
    struct sensor_switch_cfg_t *sensor_switch_config;
    data_type get_value;
    data_type set_value;
    data_type value_12;
    int ret, i, c;
    bool bGroupWrite[2] = {false, false};

    sensor_switch_config = &sensor_switch_cfg;

    // expgain
    for (c = 0; c < 32; c++) {
	// sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_A][c] = 0x0A;
	sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_B][c] = 0x0A;
    }

    if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B]) {
	sensor_group_s_exp(sd,
			   sensor_switch_config->exp_val_save[SWITCH_SENSOR_B],
			   SWITCH_SENSOR_B);
	sensor_group_s_gain(
	    sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_B],
	    SWITCH_SENSOR_B);
	bGroupWrite[SWITCH_SENSOR_B] = true;
	sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = false;
	// sensor_print("%s:sensor_set_gain exp = %d, gain = %d done\n",
	// "sensorC", sensor_switch_config->exp_val_save[SWITCH_SENSOR_B],
	// sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
    }

    if (bGroupWrite[SWITCH_SENSOR_B]) {
	sensor_i2c_addr_set(
	    sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
	for (c = 0; c < 8; c++) {
	    sensor_write(
		sd, 0xC0 + c,
		sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_B][c]);
	}
	sensor_write(sd, 0x1F, 0x80);
    }
    return;
}

static void sensor_switch_change_2(struct v4l2_subdev *sd)
{
    unsigned int max_frame_length;
    struct sensor_switch_cfg_t *sensor_switch_config;
    data_type get_value;
    data_type set_value;
    data_type value_12;
    int ret, i, c;
    bool bGroupWrite[2] = {false, false};

    sensor_switch_config = &sensor_switch_cfg;

    // expgain
    for (c = 0; c < 32; c++) {
	sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_A][c] = 0x0A;
	// sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_B][c] = 0x0A;
    }

    if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A]) {
	sensor_group_s_exp(sd,
			   sensor_switch_config->exp_val_save[SWITCH_SENSOR_A],
			   SWITCH_SENSOR_A);
	sensor_group_s_gain(
	    sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_A],
	    SWITCH_SENSOR_A);
	bGroupWrite[SWITCH_SENSOR_A] = true;
	sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = false;
	// sensor_print("%s:sensor_set_gain exp = %d, gain = %d done\n",
	// "sensorC", sensor_switch_config->exp_val_save[SWITCH_SENSOR_B],
	// sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
    }

    if (bGroupWrite[SWITCH_SENSOR_A]) {
	sensor_i2c_addr_set(
	    sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	for (c = 0; c < 8; c++) {
	    sensor_write(
		sd, 0xC0 + c,
		sensor_switch_config->sensor_group_reg[SWITCH_SENSOR_A][c]);
	}
	sensor_write(sd, 0x1F, 0x80);
    }
    return;
}

/*中断触发的队列，用于设置sensor寄存器*/
static void __s_sensor_switch_change_handle(struct work_struct *work)
{
    struct sensor_vysnc_config *sensor_vysnc_cfg = container_of(
	work, struct sensor_vysnc_config, s_sensor_switch_change_task);
    struct sensor_info *info =
	container_of(sensor_vysnc_cfg, struct sensor_info, sensor_vysnc_cfg);
    struct v4l2_subdev *sd = &info->sd;
    sensor_switch_change(sd);
    return;
}

/*中断触发的队列，用于设置sensor寄存器*/
static void __s_sensor_switch_change_handle_2(struct work_struct *work)
{
    struct sensor_vysnc_config *sensor_vysnc_cfg = container_of(
	work, struct sensor_vysnc_config, s_sensor_switch_change_task_2);
    struct sensor_info *info =
	container_of(sensor_vysnc_cfg, struct sensor_info, sensor_vysnc_cfg);
    struct v4l2_subdev *sd = &info->sd;
    sensor_switch_change_2(sd);
    return;
}

static int sensor_vysnc_init(struct v4l2_subdev *sd)
{
    int ret;

    struct sensor_info *info = to_state(sd);
    struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
    sensor_dbg(" sensor_vysnc_init\n");
    sensor_vysnc_cfg->gpio_irq = gpio_to_irq(GPIOA(2));
    if (sensor_vysnc_cfg->gpio_irq <= 0) {
	sensor_err("gpio %d get irq err\n", sensor_vysnc_cfg->gpio);
	return -1;
    }
    sensor_dbg(" sensor_vysnc_init IRQ :%d\n", sensor_vysnc_cfg->gpio_irq);
    devm_request_irq(sd->dev, sensor_vysnc_cfg->gpio_irq, sensor_vsync_irq_func,
		     IRQF_TRIGGER_FALLING, sd->name, sd);

    INIT_WORK(&sensor_vysnc_cfg->s_sensor_switch_change_task,
	      __s_sensor_switch_change_handle);
    INIT_WORK(&sensor_vysnc_cfg->s_sensor_switch_change_task_2,
	      __s_sensor_switch_change_handle_2);

    return 0;
}

static int sensor_init_with_switch(struct v4l2_subdev *sd)
{
    struct sensor_info *info = to_state(sd);
    unsigned int switch_choice;
    unsigned int sensor_num = 0;
    int ret;
    struct sensor_switch_cfg_t *sensor_switch_config;
    sensor_switch_config = &sensor_switch_cfg;

    init_switch_gpio();

    for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX;
	 switch_choice++) {
	sensor_print("sensor_detect 0x%x\n",
		     sensor_switch_config->i2c_addr_container[switch_choice]);
	sensor_i2c_addr_set(
	    sd, sensor_switch_config->i2c_addr_container[switch_choice]);
	// sensor_write(sd, 0x12, 0x80);
	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
	    sensor_err("chip found is not an target chip, iic_addr = 0x%x\n",
		       sensor_switch_config->i2c_addr_container[switch_choice]);
	    sensor_switch_config->sensor_detect_flag[switch_choice] = false;
	} else {
	    sensor_print(
		"[0:A, 1:B] detect sensor%d i2c_addr = 0x%x\n", switch_choice,
		sensor_switch_config->i2c_addr_container[switch_choice]);
	    sensor_switch_config->sensor_detect_flag[switch_choice] = true;
	    sensor_num++;
	}
    }
    sensor_print("detect switch sensor num : %d\n", sensor_num);
    if (sensor_num == 0) {
	sensor_switch_config->sensor_is_detected = false;
	return -ENODEV;
    } else if (sensor_num >= 1 &&
	       sensor_switch_config->sensor_detect_flag[SWITCH_SENSOR_A]) {
	sensor_switch_config->sensor_with_switch_en = true;
	sensor_print("maybe with mipi switch, so sensor_with_switch_en = %d\n",
		     sensor_switch_config->sensor_with_switch_en);
    } else {
	sensor_switch_config->sensor_with_switch_en = false;
	sensor_print("without mipi switch, so sensor_with_switch_en = %d\n",
		     sensor_switch_config->sensor_with_switch_en);
    }
    sensor_i2c_addr_set(
	sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
    sensor_switch_config->sensor_is_detected = true;
    if (sensor_num > SWITCH_SENSOR_MAX) {
	sensor_switch_config->sensor_num = SWITCH_SENSOR_MAX;
	sensor_err("sensor_num is %d, which is greater than SWITCH_SENSOR_MAX, "
		   "please check the config\n",
		   sensor_num);
    } else {
	sensor_switch_config->sensor_num = sensor_num;
    }
    sensor_print("sensor_switch_config->sensor_num = %d\n",
		 sensor_switch_config->sensor_num);

    return 0;
}

static int sensor_check_embed_data(struct v4l2_subdev *sd,
				   void *virt_raw_data)
{
    struct sensor_info *info = to_state(sd);
    unsigned int sensor_num = 0;
    static int last_switch_choice;
    struct sensor_switch_cfg_t *sensor_switch_config;
    sensor_switch_config = &sensor_switch_cfg;
    struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;

    if (GPIO_SWITCH_SENSOR_A_STATUS == sensor_switch_config->switch_status) {
	control_gpio(GPIO_SWITCH_SENSOR_B_STATUS); // switch  B
	sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_B_STATUS;
	schedule_work(
	    &sensor_vysnc_cfg
		 ->s_sensor_switch_change_task_2); //触发队列去写sensor寄存器
	return 1;

    } else if (GPIO_SWITCH_SENSOR_B_STATUS ==
	       sensor_switch_config->switch_status) {
	// control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);//switch  A
	return 0;
    }
    return 1;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
    int ret;
    struct sensor_info *info = to_state(sd);

    printk("------------------------\r\n");
    printk("this is h63p sensor init\r\n");
    printk("------------20250506------------\r\n");
    printk("**********************************\r\n");
    printk("***********sensor init************\r\n");
    printk("**********************************\r\n");
    printk("sensor_init:%s\n", sd->name);

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
    /* sensor_dbg("info->settle_time = 0x%x\n", info->settle_time); */
    info->tpf.numerator = 1;
    info->tpf.denominator = 20; /* 30fps */
    info->preview_first_flag = 1;
    return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
    int ret = 0;
    struct sensor_info *info = to_state(sd);

    switch (cmd) {
    case GET_CURRENT_WIN_CFG:
	sensor_dbg("%s: GET_CURRENT_WIN_CFG, info->current_wins=%p\n", __func__,
		   info->current_wins);

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
    case GET_SOFTWARE_TDM_CHANGE:
	*(int *)arg = change_flags;
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
    case CHECK_SENSOR_EMBED_DATA:
	ret = sensor_check_embed_data(sd, arg);
	break;
    case VIDIOC_VIN_SET_SOFTWARE_TDM_CHANGE:
	change_flags = (*(int *)arg) ? 1 : 0;
	sensor_print("set change_flags is %d\n", change_flags);
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
    {.desc = "Raw RGB Bayer",
     .mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code =
						 MEDIA_BUS_FMT_SBGGR10_1X10,*/
     .regs = sensor_fmt_raw,
     .regs_size = ARRAY_SIZE(sensor_fmt_raw),
     .bpp = 1},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    {
	.width = 1280,
	.height = 720,
	.hoffset = 0,
	.voffset = 0,
	.hts = 1920,
	.vts = 1576, // 1125
	.pclk = 43 * 1000 * 1000,
	.mipi_bps = 216 * 1000 * 1000,
	.fps_fixed = 15,
	.bin_factor = 1,
	.intg_min = 1 << 4,
	.intg_max = 1576 << 4,
	.gain_min = 1 << 4,
	.gain_max = 15 << 4,
	.regs = sensor_default_regs,
	.regs_size = ARRAY_SIZE(sensor_default_regs),
	.set_size = NULL,
    },
#else
    {
	.width = 1280,
	.height = 720,
	.hoffset = 0,
	.voffset = 0,
	.hts = 3200,
	.vts = 750, // 1125
	.pclk = 36 * 1000 * 1000,
	.mipi_bps = 180 * 1000 * 1000,
	.fps_fixed = 15,
	.bin_factor = 1,
	.intg_min = 1 << 4,
	.intg_max = 750 << 4,
	.gain_min = 1 << 4,
	.gain_max = 15 << 4,
	.regs = sensor_720p15_regs,
	.regs_size = ARRAY_SIZE(sensor_720p15_regs),
	.set_size = NULL,
    },
    {
	.width = 1280,
	.height = 720,
	.hoffset = 0,
	.voffset = 0,
	.hts = 1600,
	.vts = 750, // 1125
	.pclk = 36 * 1000 * 1000,
	.mipi_bps = 216 * 1000 * 1000,
	.fps_fixed = 30,
	.bin_factor = 1,
	.intg_min = 1 << 4,
	.intg_max = 750 << 4,
	.gain_min = 1 << 4,
	.gain_max = 15 << 4,
	.regs = sensor_720p30_regs,
	.regs_size = ARRAY_SIZE(sensor_720p30_regs),
	.set_size = NULL,
    },
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
    cfg->type = V4L2_MBUS_CSI2_DPHY;
    cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

    return 0;
}

#if VIN_FALSE
static int sensor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
    /* Fill in min, max, step and default value for these controls. */
    /* see include/linux/videodev2.h for details */

    switch (qc->id) {
    case V4L2_CID_GAIN:
	return v4l2_ctrl_query_fill(qc, 1 * 16, 256 * 16, 1, 16);
    case V4L2_CID_EXPOSURE:
	return v4l2_ctrl_query_fill(qc, 0, 65535 * 16, 1, 0);
    }
    return -EINVAL;
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
	return sensor_s_gain_t(sd, ctrl->val);
    case V4L2_CID_EXPOSURE:
	return sensor_s_exp_t(sd, ctrl->val);
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
    __maybe_unused struct v4l2_subdev *sd_temp;
    struct sensor_switch_cfg_t *sensor_switch_config;
    sensor_switch_config = &sensor_switch_cfg;

    if ((SENSOR_NUM == 3) && (!strcmp(sd->name, SENSOR_NAME_2))) {
	sensor_vysnc_init(sd);
	sensor_init_with_switch(sd);
    }

    sensor_dbg("sensor_reg_init, ARRAY_SIZE(sensor_default_regs)=%d\n",
	       ARRAY_SIZE(sensor_default_regs));

    ret = sensor_write_array(sd, sensor_default_regs,
			     ARRAY_SIZE(sensor_default_regs));
    if (ret < 0) {
	sensor_err("write sensor_default_regs error\n");
	return ret;
    }

    sensor_dbg(
	"sensor_reg_init, wsize=%p, wsize->regs=0x%x, wsize->regs_size=%d\n",
	wsize, wsize->regs, wsize->regs_size);

    sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

    sensor_flip_status = 0x20; /// Normal mode (mirror off, flip off)
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
	sensor_s_exp_gain(sd, &exp_gain);
    }
#else
    if (wsize->regs) {
	sensor_dbg("%s: start sensor_write_array(wsize->regs)\n", __func__);
	sensor_write_array(sd, wsize->regs, wsize->regs_size);
    }
#if IS_ENABLED(CONFIG_SENSOR_H63_SM_VSYNC_MODE)
    if (SENSOR_NUM == 3) {

	if (!strcmp(sd->name, SENSOR_NAME_2)) {
	    sensor_dbg(
		"%s second  camera  master self smvs  mode reg  init. \n",
		sd->name);
	    sensor_write_array(sd, sensor_720p15_smvs_master_regs,
			       ARRAY_SIZE(sensor_720p15_smvs_master_regs));
	    sensor_write(sd, 0x12, 00);
	    // sd_temp = cci_drv[2].sd;
	    cci_drv[2].sd->entity.use_count = 1;
	    sensor_dbg("%s third camera  slave self smvs  mode reg init. \n",
		       sd->name);
	    // sensor_write_array(sd_temp, sensor_720p15_smvs_master_regs,
	    // ARRAY_SIZE(sensor_720p15_smvs_master_regs));
	    sensor_i2c_addr_set(
		sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
	    sensor_write_array(sd, sensor_720p15_smvs_slave_regs,
			       ARRAY_SIZE(sensor_720p15_smvs_slave_regs));
	    sensor_write(sd, 0x12, 00);
	    sensor_i2c_addr_set(
		sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	} else {
	    sensor_dbg("%s one  camera  master  mode reg  init. \n", sd->name);
	    sensor_write_array(sd, sensor_720p15_smvs_master_regs,
			       ARRAY_SIZE(sensor_720p15_smvs_master_regs));
	    sensor_write(sd, 0x46, 0x08);
	    sensor_write(sd, 0x47, 0x46);
	    sensor_write(sd, 0x12, 0x00);
	}
    }
#endif

#if IS_ENABLED(CONFIG_SENSOR_H63_VSYNC_MODE)
    if (SENSOR_NUM == 3) {
	if (!strcmp(sd->name, SENSOR_NAME_2)) {
	    sensor_dbg("%s second  camera  master sync   mode reg  init. \n",
		       sd->name);
	    sensor_write_array(sd, sensor_720p30_sync_mode_regs,
			       ARRAY_SIZE(sensor_720p30_sync_mode_regs));
	    sensor_write(sd, 0x46, 0x00);
	    sensor_write(sd, 0x47, 0x42);
	    sensor_write(sd, 0x1E, 0x1c);
	    sensor_write(sd, 0x12, 0x00);
	    // sd_temp = cci_drv[2].sd;
	    // cci_drv[2].sd->entity.use_count = 1;
	    sensor_dbg("%s third camera  slave sync   mode reg init. \n",
		       sd->name);
	    sensor_i2c_addr_set(
		sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
	    sensor_write_array(sd, sensor_720p30_sync_mode_regs,
			       ARRAY_SIZE(sensor_720p30_sync_mode_regs));
	    sensor_write(sd, 0x80, 0x81);
	    sensor_write(sd, 0x1E, 0x08);
	    usleep_range(60000, 60000);
	    sensor_write(sd, 0x12, 00);
	    sensor_i2c_addr_set(
		sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	} else {
	    sensor_dbg("%s one  camera  master  mode reg  init. \n", sd->name);
	    sensor_write_array(sd, sensor_720p15_regs,
			       ARRAY_SIZE(sensor_720p15_regs));
	    sensor_write(sd, 0x1e, 04);
	    sensor_write(sd, 0x12, 00);
	}
	change_flags = 1;
    }
#endif

#endif
    /* sensor_read_array_test(sd, wsize->regs, wsize->regs_size); */
    if (wsize->set_size)
	wsize->set_size(sd);

    info->width = wsize->width;
    info->height = wsize->height;
    h63p_sensor_vts = wsize->vts;
    sensor_dbg("h63p_sensor_vts = %d\n", h63p_sensor_vts);

    sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
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
    .s_stream = sensor_s_stream, .g_mbus_config = sensor_g_mbus_config,
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

static int sensor_init_controls(struct v4l2_subdev *sd,
				const struct v4l2_ctrl_ops *ops)
{
    struct sensor_info *info = to_state(sd);
    struct v4l2_ctrl_handler *handler = &info->handler;
    struct v4l2_ctrl *ctrl;
    int ret = 0;

    v4l2_ctrl_handler_init(handler, 4);

    ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600, 256 * 1600,
			     1, 1 * 1600);

    if (ctrl != NULL)
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

    ctrl =
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1, 65536 * 16, 1, 1);
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

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
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
	cci_dev_probe_helper(sd, client, &sensor_ops,
			     &cci_drv[sensor_dev_id++]);
    }

    sensor_init_controls(sd, &sensor_ctrl_ops);

    mutex_init(&info->lock);

    info->fmt = &sensor_formats[0];
    info->fmt_pt = &sensor_formats[0];
    info->win_pt = &sensor_win_sizes[0];
    info->fmt_num = N_FMTS;
    info->win_size_num = N_WIN_SIZES;
    info->sensor_field = V4L2_FIELD_NONE;
    // use CMB_PHYA_OFFSET2  also ok
    info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
    // info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
    info->stream_seq = MIPI_BEFORE_SENSOR;
    info->af_first_flag = 1;
    info->time_hs = 0x11;
    info->deskew = 0x00;
    info->exp = 0;
    info->gain = 0;
    info->preview_first_flag = 1;
    info->first_power_flag = 1;

    return 0;
}

static int sensor_remove(struct i2c_client *client)
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
    return 0;
}

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};

static const struct i2c_device_id sensor_id_2[] = {{SENSOR_NAME_2, 0}, {}};

static const struct i2c_device_id sensor_id_3[] = {{SENSOR_NAME_3, 0}, {}};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);
MODULE_DEVICE_TABLE(i2c, sensor_id_3);

static struct i2c_driver sensor_driver[] = {
    {
	.driver = {
		.owner = THIS_MODULE, .name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
    },
    {
	.driver = {
		.owner = THIS_MODULE, .name = SENSOR_NAME_2,
	    },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id_2,
    },
    {
	.driver = {
		.owner = THIS_MODULE, .name = SENSOR_NAME_3,
	    },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id_3,
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
