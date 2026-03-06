/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A V4L2 driver for TP9963 YUV cameras.
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
MODULE_DESCRIPTION("A low-level driver for TP9963 sensors");
MODULE_LICENSE("GPL");

//mipi接口

#define MCLK              (27*1000*1000)
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_FALLING
#define V4L2_IDENT_SENSOR 0x6328
#define WAIT_VIDEO_INPUT_STATUS (0)

#define ENABLE_REG_FS

/* enable tp9963 sensor detect */
#define SENSOR_DETECT_KTHREAD 1
/* USE DETECT BY GPIO_IRQ OR POLLING
 * DET_USE_POLLING 0 meant by gpio_irq
 * DET_USE_POLLING 1 meant by POLLING
 * */
#define DET_USE_POLLING 1
//#define ENABLE_REG_FS

#if SENSOR_DETECT_KTHREAD
struct sensor_indetect_tp9963 {
	struct class *sensor_class;
	struct task_struct *sensor_task;
	struct device *dev;
	struct cdev *cdev;
	struct gpio_config detect_power;
	struct gpio_config detect_power_2;
	struct gpio_config detect_gpio;
	struct gpio_config detect_reverse;
	struct delayed_work tp9963_work;
#if DET_USE_POLLING
	data_type   last_status;
#else
	unsigned int detect_irq;
#endif
    int gpio_detect_flag;
    struct i2c_client *client;
	dev_t devno;
    struct v4l2_subdev *sd;
} sensor_indetect;
static DEFINE_MUTEX(det_mutex);
#endif

/*
 * Our nominal (default) frame rate.
 */
#define REAR_1080P_PAL  	1
#define REAR_1080P_NTSC  	2
#define REAR_720P_PAL  		3

#define SENSOR_FRAME_RATE REAR_1080P_PAL //RECORD_FPS_REAR

/*
 * The TP9920 sits on i2c with ID 0x88 or 0x8a
 * SAD-low:0x88 SAD-high:0x8a
 */
#define I2C_ADDR 0x88
#define SENSOR_NAME "tp9963"

#if (SENSOR_FRAME_RATE == REAR_1080P_NTSC)
static struct regval_list reg_1080p30_1ch[] = {
};
#endif

#if (SENSOR_FRAME_RATE == REAR_1080P_PAL)
static struct regval_list reg_1080p25_1ch[] = {
	{0x40, 0x04},
	{0x06, 0x12}, //default value
	{0x50, 0x00}, //VIN1/3
	{0x51, 0x00},
	{0x54, 0x03},
	// tmp = tp28xx_read_reg{0x42};
	// tmp &= MASK42_43[1];
	// {0x42, tmp};
	{0x42, 0xf0},
	// tmp = tp28xx_read_reg{0x43};
	// tmp &= MASK42_43[1];
	// {0x43, tmp};
	{0x43, 0x00},
	{0x02, 0x40},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x50},
	{0x15, 0x03},
	{0x16, 0xd2},
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x1c, 0x0a},  //1920*1080, 25fps
	{0x1d, 0x50},
	{0x20, 0x30},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3c},
/*
 * 0x2a寄存器
 * 不设置,寄存器默认值为0x30
 * 设置0x30,无信号出黑屏,有信号出图
 * 设置0x34,无信号出蓝屏,有信号出图
 * 设置0x3c,有无信号都出蓝屏
 */
	//{0x2a, 0x34},
	{0x2b, 0x60},
	{0x2c, 0x2a},
	{0x2d, 0x30},
	{0x2e, 0x70},
	{0x30, 0x48},
	{0x31, 0xbb},
	{0x32, 0x2e},
	{0x33, 0x90},
	{0x35, 0x05},
	{0x39, 0x0C},
	{0x02, 0x44},
	{0x0d, 0x73},
	{0x15, 0x01},
	{0x16, 0xf0},
	{0x18, 0x2a},
	{0x20, 0x3c},
	{0x21, 0x46},
	{0x25, 0xfe},
	{0x26, 0x0d},
	{0x2c, 0x3a},
	{0x2d, 0x54},
	{0x2e, 0x40},
	{0x30, 0xa5},
	{0x31, 0x86},
	{0x32, 0xfb},
	{0x33, 0x60},
	//mipi setting
	{0x40, 0x08}, //MIPI page
	{0x02, 0x78},
	{0x03, 0x70},
	{0x04, 0x70},
	{0x05, 0x70},
	{0x06, 0x70},
	{0x13, 0xef},
	{0x20, 0x00},
	{0x21, 0x22},
	{0x22, 0x20},
	{0x23, 0x9e},
	{0x21, 0x24},
	{0x14, 0x41},
	{0x15, 0x01},
	{0x2a, 0x04},
	{0x2b, 0x03},
	{0x2c, 0x09},
	{0x2e, 0x02},
	{0x10, 0xa0},
	{0x10, 0x20},
	/* Enable MIPI CSI2 output */
	{0x28, 0x02},
	{0x28, 0x00},
};
#endif


static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	/* if (on_off)
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	else
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH); */
	return 0;
}

static bool flag_power_on = true;

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
		sensor_print("CSI_SUBDEV_PWR_ON!\n");
		if (flag_power_on) {
			flag_power_on = false;
			sensor_print("do_power_on\n");
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
		}
		break;
	case PWR_OFF:
		sensor_print("CSI_SUBDEV_PWR_OFF!\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	sensor_print("sensor_reset\n");
	// vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	// usleep_range(5000, 6000);
	// vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	// usleep_range(5000, 6000);
	return 0;
}
/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "UYVY 4:2:2",
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,//MEDIA_BUS_FMT_UYVY8_2X8,
		.regs = NULL,
		.regs_size = 0,
		// .bpp = 1,
		.bpp = 2,
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if (SENSOR_FRAME_RATE == REAR_1080P_NTSC)
	{
		.width = HD1080_WIDTH,
		.height = HD1080_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 30,
		.regs = reg_1080p30_1ch,
		.regs_size = ARRAY_SIZE(reg_1080p30_1ch),
		.set_size = NULL,
	},
#elif (SENSOR_FRAME_RATE == REAR_1080P_PAL)
	{
		.width = HD1080_WIDTH,
		.height = HD1080_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 25,
		.regs = reg_1080p25_1ch,
		.regs_size = ARRAY_SIZE(reg_1080p25_1ch),
		.set_size = NULL,
	},
#elif (SENSOR_FRAME_RATE == REAR_720P_NTSC)
	{
		.width = 1280,
		.height = 720,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 25,
		.regs = reg_720p25_1ch,
		.regs_size = ARRAY_SIZE(reg_720p25_1ch),
		.set_size = NULL,
	},
#else
	/*{
		.width = 720,
		.height = 576,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 25,
		.regs = reg_pal_1ch,
		.regs_size = ARRAY_SIZE(reg_pal_1ch),
		.set_size = NULL,
		},*/
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static bool detect_flag;
static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval, rdval1, rdval2;
	int cnt = 0;

	rdval = 0;
	rdval1 = 0;
	rdval2 = 0;

	if (!detect_flag) {
		detect_flag = true;
	} else {
		sensor_print("it will not detect repeatly\n");
		return 0;
	}

	vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	msleep(600);
	vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	msleep(100);

	sensor_read(sd, 0xfe, &rdval1);
	sensor_read(sd, 0xff, &rdval2);
	rdval = ((rdval2<<8) & 0xff00) | rdval1;
	sensor_print("rdval = 0x%x\n", rdval);

	while ((rdval != V4L2_IDENT_SENSOR) && (cnt < 2)) {
		sensor_read(sd, 0xfe, &rdval1);
		sensor_read(sd, 0xff, &rdval2);
		rdval = ((rdval2<<8) & 0xff00) | rdval1;
		sensor_print("retry = %d, rdval = %x\n", cnt, rdval);
		cnt++;
	}

	if (rdval != V4L2_IDENT_SENSOR) {
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		msleep(600);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		msleep(100);
		pr_err("tp995x id err, reset!");
		sensor_read(sd, 0xfe, &rdval1);
		sensor_read(sd, 0xff, &rdval2);
		rdval = ((rdval2<<8) & 0xff00) | rdval1;
		sensor_print("rdval = 0x%x\n", rdval);
		if (rdval != V4L2_IDENT_SENSOR) {
			return -ENODEV;
		}
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
	info->width = HD1080_WIDTH;
	info->height = HD1080_HEIGHT;
	info->hflip = 0;
	info->vflip = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;	/* 25fps */

	if ((sensor_indetect.gpio_detect_flag == 0) && (info->preview_first_flag == 0)) {
#if DET_USE_POLLING
	sensor_write_array(sd, sensor_win_sizes[0].regs, sensor_win_sizes[0].regs_size);
	pr_info("use polling detect!\n");
	schedule_delayed_work(&sensor_indetect.tp9963_work, 0);
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

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type  = V4L2_MBUS_CSI2_DPHY;
	cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
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

	mutex_lock(&det_mutex);
	//sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

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
			if (((rdval&0x60) == 0x60) || (cnt > 2000)) {
				sensor_print("ck %d,0x%x\n", cnt, rdval);
				break;
			}
			++cnt;
			usleep_range(1000, 1200);
		}
	}
#endif
	usleep_range(100000, 110000);
	mutex_unlock(&det_mutex);
	return 0;
}

static int tp9963_reg_read(struct i2c_client *client, unsigned char addr,
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

static int tp9963_reg_write(struct i2c_client *client, unsigned char addr,
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

int tp9963_write_array(struct i2c_client *client, struct regval_list *regs, int array_size)
{
	int i;
	int ret;
	for (i = 0; i < array_size; ++i) {
		ret = tp9963_reg_write(client, regs[i].addr, regs[i].data);
		if (ret < 0) {
			pr_err("tp9963 sensor write array error, array_size %d!\n", array_size);
			return -1;
		}
	}
	return 0;
}

#if SENSOR_DETECT_KTHREAD
static int __sensor_insert_detect(data_type *val)
{
	#if 1
		*val = 1;
	#else
	if (sensor_indetect.gpio_detect_flag) {
		int ret;
		mutex_lock(&det_mutex);
		/*detecting the sensor insert by detect_gpio
		* if detect_gpio was height meant sensor was insert
		* if detect_gpio was low, meant sensor was remove
		* */
		ret = gpio_get_value_cansleep(sensor_indetect.detect_gpio.gpio);
		if (ret) {
			*val = 1;
		} else {
			*val = 0;
		}
		mutex_unlock(&det_mutex);
	} else {
	if (sensor_indetect.client) {
		unsigned char rdval = 0;
		unsigned char rdval_1 = 0;
		mutex_lock(&det_mutex);
		tp9963_reg_read(sensor_indetect.client, 0x01, &rdval);
		// pr_err("%s,%d:rdval = 0x%x\n", __func__, __LINE__, rdval);
		if ((rdval & 0xE0) == 0x60) {
			*val = 1;
				//每个Decoder的01bit7，=0有信号，=1无信号，无信号的时候要把0x26bit0设为0，避免噪声的干扰导致误判，
				tp9963_reg_read(sensor_indetect.client, 0x26, &rdval_1);
				if ((rdval_1 & 0x01) == 0x00) {
					//H/V锁定（01bit6和bit5都=1）以后必须要再把0x26bit0改回1
					rdval_1 |= 0x01;
					tp9963_reg_write(sensor_indetect.client, 0x26, rdval_1);
				}
		} else {
			*val = 0;
			tp9963_reg_read(sensor_indetect.client, 0x26, &rdval_1);
				if ((rdval_1 & 0x01) == 0x01) {
					//当0x01没信号时，要设置0x26bit0设为0，避免噪声的干扰导致误判
					rdval_1 &= 0xFE;
					tp9963_reg_write(sensor_indetect.client, 0x26, rdval_1);
				}
			}
			mutex_unlock(&det_mutex);
		}
	}
	#endif
	return 0;
}

// void sensor_msg_sent(char *buf)
// {
// 	char *envp[2];

// 	envp[0] = buf;
// 	envp[1] = NULL;
// 	kobject_uevent_env(&sensor_indetect.dev->kobj, KOBJ_CHANGE, envp);
// }

static void sensor_det_work(struct work_struct *work)
{
	//char buf[32];
	//int ret;
	data_type val;

	__sensor_insert_detect(&val);
	sensor_indetect.last_status = val;
#if DET_USE_POLLING
	//if (sensor_indetect.last_status != val) {
		//memset(buf, 0, 32);
		//snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x%x", val);

		//sensor_msg_sent(buf);
		//sensor_indetect.last_status = val;
		//vin_print("val = 0x%x, Sent msg to user\n", val);
	//}

	schedule_delayed_work(&sensor_indetect.tp9963_work, (msecs_to_jiffies(1 * 1000)));
#else
	//memset(buf, 0, 32);
	//snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x%x", val);

	//sensor_msg_sent(buf);
	//vin_print("val = 0x%x, Sent msg to user\n", val);
#endif

}


#if !DET_USE_POLLING
static irqreturn_t sensor_det_irq_func(int irq, void *priv)
{
	/* the work of detected was be run in workquen */
	schedule_delayed_work(&sensor_indetect.tp9963_work, 0);
	return IRQ_HANDLED;
}
#endif

#endif

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d %x fps_fixed=%d\n", __func__, enable,
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
	//data_type val;
	//__sensor_insert_detect(&val);
	return sprintf(buf, "%d\n", sensor_indetect.last_status);
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
static unsigned char tp9963_reg_addr;

static ssize_t tp9963_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char rdval = 0;
    tp9963_reg_read(sensor_indetect.client, tp9963_reg_addr, &rdval);
    return sprintf(buf, "tp9963:REG[0x%x]=0x%x\n", tp9963_reg_addr, rdval);
}

static ssize_t tp9963_reg_store(struct device *dev,
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

	printk("[tp9963_reg_store]tmp = 0x%x\n", tmp);

	if (tmp == 0) {
		printk("page0\n");
		tp9963_reg_write(sensor_indetect.client, 0x40, 0);
		for (i = 0; i < 0xff; i++) {
			tp9963_reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
	} else if (tmp < 0xff) {
		//for read
		tp9963_reg_addr = tmp;
	} else if (tmp <= 0xffff) {
		//for read or write
		val = tmp & 0x00FF;
		tp9963_reg_addr = (tmp >> 8) & 0x00FF;
		tp9963_reg_write(sensor_indetect.client, tp9963_reg_addr, val);
		// sensor_write(sensor_indetect.sd, tp9963_reg_addr, val);
	} else {
		tp9963_reg_write(sensor_indetect.client, 0x40, 0);
		printk("page0\n");
		for (i = 0; i < 0xff; i++) {
			tp9963_reg_read(sensor_indetect.client, i, &return_value);
			printk("read reg[0x%x] = 0x%x\n", i, return_value);
		}
		printk("page8\n");
		tp9963_reg_write(sensor_indetect.client, 0x40, 8);
		for (i = 0; i < 0xff; i++) {
			tp9963_reg_read(sensor_indetect.client, i, &return_value);
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
	.show =  tp9963_reg_show,
	.store = tp9963_reg_store,

};
#endif

static int tp9963_sensor_det_init(struct i2c_client *client)
{
	int ret;
	struct device_node *np = NULL;

	/* enable detect work queue */
	INIT_DELAYED_WORK(&sensor_indetect.tp9963_work, sensor_det_work);
	np = of_find_node_by_name(NULL, "ahd_detect");
	if (np == NULL) {
		sensor_err("can not find the tp9963_detect node, will not enable detect kthread\n");
		return -1;
	}

	sensor_indetect.detect_power.gpio = of_get_named_gpio_flags(
		np, "gpio_power", 0, (enum of_gpio_flags *)(&(sensor_indetect.detect_power)));

	if (!gpio_is_valid(sensor_indetect.detect_power.gpio) ||
	     sensor_indetect.detect_power.gpio < 0) {
		sensor_err("enable tp9963 sensor power fail!!\n");
		//return -1;
	} else {
		/* enable the detect power*/
		ret = gpio_request(sensor_indetect.detect_power.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable tp9963 sensor detect fail!!\n");
			return -1;
		}
		gpio_direction_output(sensor_indetect.detect_power.gpio, 1);
	}

	sensor_indetect.detect_power_2.gpio = of_get_named_gpio_flags(
		np, "gpio_power_2", 0, (enum of_gpio_flags *)(&(sensor_indetect.detect_power_2)));

	if (!gpio_is_valid(sensor_indetect.detect_power_2.gpio) ||
		sensor_indetect.detect_power_2.gpio < 0) {
		sensor_err("enable tp9963 sensor power2 fail!!\n");
		//return -1;
	} else {
		/* enable the detect power*/
		ret = gpio_request(sensor_indetect.detect_power_2.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable tp9963 sensor detect fail!!\n");
			return -1;
		}
		gpio_direction_output(sensor_indetect.detect_power_2.gpio, 1);
	}

	sensor_indetect.detect_gpio.gpio = of_get_named_gpio_flags(
		np, "gpio_detect", 0, (enum of_gpio_flags *)(&(sensor_indetect.detect_gpio)));

	sensor_indetect.gpio_detect_flag = 0;
	if (gpio_is_valid(sensor_indetect.detect_gpio.gpio) &&
		sensor_indetect.detect_gpio.gpio >= 0) {
		ret = gpio_request(sensor_indetect.detect_gpio.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable  gpio_tp9963  fail! \n");
			return -1;
		}

		gpio_direction_input(sensor_indetect.detect_gpio.gpio);
#if DET_USE_POLLING
		/* start detect work  */
		schedule_delayed_work(&sensor_indetect.tp9963_work, 0);
#else
		/* request gpio to irq  */
		sensor_indetect.detect_irq = gpio_to_irq(sensor_indetect.detect_gpio.gpio);
		ret = request_irq(sensor_indetect.detect_irq, sensor_det_irq_func,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tp9963_sensor_detect", client);
	}
#endif
	pr_info("use gpio detect!\n");
	sensor_indetect.gpio_detect_flag = 1;
	}
	return 0;
}

static void tp9963_sensor_det_remove(struct v4l2_subdev *sd)
{
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
	cancel_delayed_work_sync(&sensor_indetect.tp9963_work);
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
	if (sensor_indetect.gpio_detect_flag &&
		gpio_is_valid(sensor_indetect.detect_gpio.gpio)) {
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
    sensor_indetect.sd = sd;
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

	sensor_indetect.dev =
	    device_create(sensor_indetect.sensor_class, NULL,
			  sensor_indetect.devno, NULL, "ahd");
	if (IS_ERR(sensor_indetect.dev)) {
		sensor_err("device_create fail!\n");
		goto free_class;
	}

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
    sensor_indetect.client = client;
	/* init tp9963 detect mode */
	ret = tp9963_sensor_det_init(client);
	if (ret) {
		goto free_det;
	}

	return 0;


free_det:
	tp9963_sensor_det_remove(sd);

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
	tp9963_sensor_det_remove(sd);

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
	tp9963_sensor_det_remove(sd);

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

VIN_INIT_DRIVERS(init_sensor);
module_exit(exit_sensor);
