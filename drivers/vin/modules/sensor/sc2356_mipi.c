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

MODULE_AUTHOR("yr");
MODULE_DESCRIPTION("A low-level driver for sc2356 sensors");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0xeb52

//define the registers
#define EXP_HIGH		0xff
#define EXP_MID			0x03
#define EXP_LOW			0x04
#define GAIN_HIGH		0xff
#define GAIN_LOW		0x24
#define GAIN_STEP_BASE 128
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x3107
#define ID_REG_LOW		0x3108
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)

#define SC2356_EXP_H_ADDR                    0x3e00
#define SC2356_EXP_M_ADDR                    0x3e01
#define SC2356_EXP_L_ADDR                    0x3e02
#define SC2356_AGAIN_ADDR                    0x3e09
#define SC2356_DGAIN_ADDR                    0x3e06
#define SC2356_DGAIN_FINE_ADDR               0x3e07
#define SC2356_VMAX_H_ADDR                   0x320e
#define SC2356_VMAX_L_ADDR                   0x320f
#define SC2356_HOLD_ADDR                     0x3812
/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 30

/*
 * The sc2356 i2c address
 */
#define I2C_ADDR 0x6C


#define SENSOR_NUM 0x1
#define SENSOR_NAME "sc2356_mipi"

#define SENSOR_HOR_VER_CFG0_REG1  (1)   // Sensor HOR and VER Config by write_hw g_productinfo_s

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_1600x1200p25_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36ea, 0x0a},
	{0x36eb, 0x0c},
	{0x36ec, 0x01},
	{0x36ed, 0x18},
	{0x36e9, 0x10},
	{0x301f, 0x1b},
	{0x320c, 0x07},//hts=1920
	{0x320d, 0x80},
	//0x320e,0x04,//vts=1250	30fps
	//0x320f,0xe2,
	{0x320e, 0x05},//vts=1500	25fps
	{0x320f, 0xdc},
	//{0x320e, 0x07},//vts=1875	20fps
	//{0x320f, 0x53},
	{0x3301, 0xff},
	{0x3304, 0x68},
	{0x3306, 0x40},
	{0x3308, 0x08},
	{0x3309, 0xa8},
	{0x330b, 0xb0},
	{0x330c, 0x18},
	{0x330d, 0xff},
	{0x330e, 0x20},
	{0x331e, 0x59},
	{0x331f, 0x99},
	{0x3333, 0x10},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x1f},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x338f, 0xa0},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x1f},
	{0x3393, 0xff},
	{0x3394, 0xff},
	{0x3395, 0xff},
	{0x33a2, 0x04},
	{0x33ad, 0x0c},
	{0x33b1, 0x20},
	{0x33b3, 0x38},
	{0x33f9, 0x40},
	{0x33fb, 0x48},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x03},
	{0x34a7, 0x1f},
	{0x34a8, 0x38},
	{0x34a9, 0x30},
	{0x34ab, 0xb0},
	{0x34ad, 0xb0},
	{0x34f8, 0x1f},
	{0x34f9, 0x20},
	{0x3630, 0xa0},
	{0x3631, 0x92},
	{0x3632, 0x64},
	{0x3633, 0x43},
	{0x3637, 0x49},
	{0x363a, 0x85},
	{0x363c, 0x0f},
	{0x3650, 0x31},
	{0x3670, 0x0d},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x92},
	{0x3678, 0x96},
	{0x3679, 0x9a},
	{0x367c, 0x03},
	{0x367d, 0x0f},
	{0x367e, 0x01},
	{0x367f, 0x0f},
	{0x3698, 0x83},
	{0x3699, 0x86},
	{0x369a, 0x8c},
	{0x369b, 0x94},
	{0x36a2, 0x01},
	{0x36a3, 0x03},
	{0x36a4, 0x07},
	{0x36ae, 0x0f},
	{0x36af, 0x1f},
	{0x36bd, 0x22},
	{0x36be, 0x22},
	{0x36bf, 0x22},
	{0x36d0, 0x01},
	{0x370f, 0x02},
	{0x3721, 0x6c},
	{0x3722, 0x8d},
	{0x3725, 0xc5},
	{0x3727, 0x14},
	{0x3728, 0x04},
	{0x37b7, 0x04},
	{0x37b8, 0x04},
	{0x37b9, 0x06},
	{0x37bd, 0x07},
	{0x37be, 0x0f},
	{0x3901, 0x02},
	{0x3903, 0x40},
	{0x3905, 0x8d},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391f, 0x41},
	{0x3933, 0x80},
	{0x3934, 0x02},
	{0x3937, 0x6f},
	{0x393a, 0x01},
	{0x393d, 0x01},
	{0x393e, 0xc0},
	{0x39dd, 0x41},
	{0x3e00, 0x00},
	{0x3e01, 0x4d},
	{0x3e02, 0xc0},
	{0x3e09, 0x00},
	{0x4509, 0x28},
	{0x450d, 0x61},
	{0x0100, 0x01},
};

static struct regval_list sensor_fmt_raw[] = {

};


/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function , retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

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

	sensor_read(sd, SC2356_VMAX_H_ADDR, &frame_length);
	act_vts = frame_length << 8;
	sensor_read(sd, SC2356_VMAX_L_ADDR, &frame_length);
	act_vts |= frame_length;
	fps->fps = wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

static int sc2356_sensor_vts;
static int sc2356_fps_change_flag;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	exphigh = (unsigned char) (0x0f & (exp_val >> 16));
	expmid = (unsigned char) (0xff & (exp_val >> 8));
	explow = (unsigned char) (0xf0 & (exp_val << 0));

	sensor_write(sd, SC2356_EXP_L_ADDR, explow);
	sensor_write(sd, SC2356_EXP_M_ADDR, expmid);
	sensor_write(sd, SC2356_EXP_H_ADDR, exphigh);

	sensor_dbg("exp_val = %d\n", exp_val);
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

unsigned char analog_Gain_Reg[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f};

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	int ana_gain = gain / GAIN_STEP_BASE;
	int dig_Gain;
	int gain_flag = 1;

	if (ana_gain >= 16) {
		gain_flag = 4;
	} else if (ana_gain >= 8) {
		gain_flag = 3;
	} else if (ana_gain >= 4) {
		gain_flag = 2;
	} else if (ana_gain >= 2) {
		gain_flag = 1;
	} else {
		gain_flag = 0;
	}

	sensor_write(sd, SC2356_AGAIN_ADDR, analog_Gain_Reg[gain_flag]);
	dig_Gain = gain >> gain_flag;
	if (dig_Gain < 2 * GAIN_STEP_BASE) {
		//step1/128
		sensor_write(sd, SC2356_DGAIN_ADDR, 0x00);
		sensor_write(sd, SC2356_DGAIN_FINE_ADDR, dig_Gain - GAIN_STEP_BASE + 0x80);
	} else if (dig_Gain < 4 * GAIN_STEP_BASE) {
		//step1/64
		sensor_write(sd, SC2356_DGAIN_ADDR, 0x01);
		sensor_write(sd, SC2356_DGAIN_FINE_ADDR, (dig_Gain - GAIN_STEP_BASE * 2) / 2 + 0x80);
	} else {
		sensor_write(sd, SC2356_DGAIN_ADDR, 0x01);
		sensor_write(sd, SC2356_DGAIN_FINE_ADDR, 0xfc);
	}
    return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	int tem_gain_val;

	//gain min step is 1/128gain
	tem_gain_val = gain_val * GAIN_STEP_BASE;
	if ((tem_gain_val - tem_gain_val / 16 * 16) > 0) {
		tem_gain_val = tem_gain_val / 16 + 1;
	} else {
		tem_gain_val = tem_gain_val / 16;
	}

	sensor_dbg("%s(), L:%d, gain_val:%d, tem_gain_val:%d\n",
		__func__, __LINE__, gain_val, tem_gain_val);

	setSensorGain(sd, tem_gain_val);
	info->gain = gain_val;
	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int shutter, frame_length;

	if (!sc2356_fps_change_flag) {
		shutter = exp_gain->exp_val >> 4;
		if (shutter > sc2356_sensor_vts - 8) {
			frame_length = shutter + 8;
		} else {
			frame_length = sc2356_sensor_vts;
		}
		sensor_write(sd, SC2356_VMAX_L_ADDR, (frame_length & 0xff));
		sensor_write(sd, SC2356_VMAX_H_ADDR, (frame_length >> 8));
	}

	sensor_s_exp(sd, exp_gain->exp_val);
	sensor_s_gain(sd, exp_gain->gain_val);
	info->exp = exp_gain->exp_val;
	info->gain = exp_gain->gain_val;
	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", exp_gain->exp_val, exp_gain->gain_val);

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
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
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_set_mclk(sd, ON);
		usleep_range(100, 120);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(100, 120);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(100, 120);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(300, 310);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		cci_lock(sd);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_set_status(sd, RESET, 0);
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
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	int cnt = 0;

	sensor_read(sd, ID_REG_HIGH, &rdval);
	SENSOR_ID |= (rdval << 8);
	sensor_read(sd, ID_REG_LOW, &rdval);
	SENSOR_ID |= (rdval);
	sensor_dbg("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);

	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		sensor_read(sd, ID_REG_HIGH, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, ID_REG_LOW, &rdval);
		SENSOR_ID |= (rdval);
		sensor_dbg("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
		}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;
#endif
	return 0;
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
	temp->temp = ((rdval_total - 3500) / 20) + 50;

	return ret;
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
	info->width = 1600;
	info->height = 1200;
	info->hflip = 0;
	info->vflip = 0;
	info->tpf.numerator      = 1;
	info->tpf.denominator    = 20;	/* 20fps */

	return 0;
}

/*
 *set && get sensor flip
 */
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
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	case 0x60:
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x66:
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc      = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /* .mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
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
	{
		.width = 1600,
		.height = 1200,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1920,
		.vts = 1500,
		.pclk = 72000000,
		.mipi_bps = 371250000,
		.fps_fixed = 25,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1500 - 6) << 4,
		.gain_min = 1 << 4,
		.gain_max = 64 << 4,
		.regs = sensor_1600x1200p25_regs,
		.regs_size = ARRAY_SIZE(sensor_1600x1200p25_regs),
		.set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2_DPHY;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cfg->bus.mipi_csi2.num_data_lanes = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
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

static int sensor_s_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	int sc2356_sensor_target_vts = 0;

	sc2356_fps_change_flag = 1;
	sc2356_sensor_target_vts = wsize->pclk/fps->fps/wsize->hts;

	if (sc2356_sensor_target_vts <= wsize->vts) {// the max fps = 20fps
		sc2356_sensor_target_vts = wsize->vts;
	} else if (sc2356_sensor_target_vts >= (wsize->pclk/wsize->hts)) {// the min fps = 1fps
		sc2356_sensor_target_vts = (wsize->pclk/wsize->hts) - 8;
	}

	sc2356_sensor_vts = sc2356_sensor_target_vts;
	sensor_dbg("target_fps = %d, sc2356_sensor_target_vts = %d, 0x320e = 0x%x, 0x320f = 0x%x\n", fps->fps,
		sc2356_sensor_target_vts, sc2356_sensor_target_vts >> 8, sc2356_sensor_target_vts & 0xff);
	sensor_write(sd, SC2356_VMAX_L_ADDR, (sc2356_sensor_target_vts & 0xff));
	sensor_write(sd, SC2356_VMAX_H_ADDR, (sc2356_sensor_target_vts >> 8));

	sc2356_fps_change_flag = 0;

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_hflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1)//h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0xf9;
	else
		set_value = get_value | 0x06;
#else
	if (enable)
		set_value = get_value | 0x06;
	else
		set_value = get_value & 0xf9;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_vflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1)//h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0x9f;
	else
		set_value = get_value | 0x60;
#else
	if (enable)
		set_value = get_value | 0x60;
	else
		set_value = get_value & 0x9f;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;

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

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc2356_sensor_vts = wsize->vts;
	sc2356_fps_change_flag = 0;

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
			 wsize->height);

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
	case VIDIOC_VIN_SET_IR:
		sensor_set_ir(sd, (struct ir_switch *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
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
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	},
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

	info = kzalloc(sizeof(*info), GFP_KERNEL);
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
	info->time_hs = 0x28; /* 0x09 */
	info->preview_first_flag = 1;
	info->first_power_flag = 1;
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

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	},
};

static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++) {
		ret = cci_dev_init_helper(&sensor_driver[i]);
	}

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
