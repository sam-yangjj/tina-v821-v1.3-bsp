#include "aglink_gyro.h"
#include "icm42607_driver.h"
#include "debug.h"

static struct aglink_gyro_hwio_ops *hwio;
static inv_imu_device_t  imu_dev;

#define ICM_I2C_ADDR 0x69
#define SERIF_TYPE UI_I2C
#define ICM42607_GYRO_FS

struct icm_imu_pkt_t {
	u8 hires_en;
	u8 use_ln;
	u8 gyro_fs;
	u8 accel_fs;
	u32 pkt_num;
	u64 int_timestamp;
	u64 synctime_offset;
	struct gyro_pkt *gyro_dat;
};

static struct gyro_config gyro_attri;
struct icm_imu_pkt_t icm_pkt;

static void sensor_event_cb(inv_imu_sensor_event_t *event);
extern const struct icm_info icm42607_ids[];
extern const size_t icm42607_ids_arrsize;

#define LSM6DSOX_SPEED_CNT_MAX 12
#define LSM6DSOX_SPEED_CNT_MIN 5

float gyro_speed[8] = {1600.0, 800.0, 400.0, 200.0, 100.0, 50.0, 25.0, 12.5};

void inv_imu_sleep_us(u32 us)
{
	usleep_range(us, us);
}

uint64_t inv_imu_get_time_us(void)
{
	return aglink_gyro_get_base_time() * 1000;
}

static char *icm42607_get_gyro_fs(void)
{
	switch (icm_pkt.gyro_fs) {
	case GYRO_CONFIG0_FS_SEL_2000dps:
		return "2000dps";
	case GYRO_CONFIG0_FS_SEL_1000dps:
		return "1000dps";
	case GYRO_CONFIG0_FS_SEL_500dps:
		return "500dps";
	case GYRO_CONFIG0_FS_SEL_250dps:
		return "250dps";
	default:
		return "unknow";
	}
}

static char *icm42607_get_accel_fs(void)
{
	switch (icm_pkt.accel_fs) {
	case ACCEL_CONFIG0_FS_SEL_2g:
		return "2g";
	case ACCEL_CONFIG0_FS_SEL_4g:
		return "4g";
	case ACCEL_CONFIG0_FS_SEL_8g:
		return "8g";
	case ACCEL_CONFIG0_FS_SEL_16g:
		return "16g";
	default:
		return "unknow";
	}
}

static s64 icm42607_from_fs_to_uradps(s32 lsb)
{
	switch (icm_pkt.gyro_fs) {
	case GYRO_CONFIG0_FS_SEL_2000dps:
		return lsb * 1065; // 61 * pai(3.1415926535) / 180 * 1000 = urad/s // fs2000
	case GYRO_CONFIG0_FS_SEL_1000dps:
		return lsb * 532; // 30.5 * pai(3.1415926535) / 180 * 1000 = urad/s // fs1000
	case GYRO_CONFIG0_FS_SEL_500dps:
		return lsb * 267; // 15.3 * pai(3.1415926535) / 180 * 1000 = urad/s // fs500
	case GYRO_CONFIG0_FS_SEL_250dps:
		return lsb * 133; // 7.6 * pai(3.1415926535) / 180 * 1000 = urad/s // fs250
	default:
		return lsb;
	}
}

int icm42607_read_reg(struct inv_imu_serif *serif, uint8_t reg, uint8_t *buf, uint32_t len)
{
	return hwio->read(ICM_I2C_ADDR, reg, buf, len);
}

int icm42607_write_reg(struct inv_imu_serif *serif, uint8_t reg, const uint8_t *buf, uint32_t len)
{
	return hwio->write(ICM_I2C_ADDR, reg, buf, len);
}

static int configure_fifo(void)
{
	int rc = 0;
	inv_imu_interrupt_parameter_t int1_config = { (inv_imu_interrupt_value)0 };

	rc |= inv_imu_configure_fifo(&imu_dev, INV_IMU_FIFO_ENABLED);

	icm_pkt.int_timestamp = aglink_gyro_get_base_time();

	rc |= inv_imu_configure_fifo_wm(&imu_dev, gyro_attri.fifo_deep);

	/* Configure interrupts sources */
	int1_config.INV_FIFO_THS = INV_IMU_ENABLE;
	rc |= inv_imu_set_config_int1(&imu_dev, &int1_config);

	return rc;
}

static int configure_hires(u8 hires_en)
{
	int rc = 0;

	if (hires_en)
		rc |= inv_imu_enable_high_resolution_fifo(&imu_dev);
	else
		rc |= inv_imu_disable_high_resolution_fifo(&imu_dev);

	return rc;
}

static int configure_power_mode(u8 use_ln)
{
	int rc = 0;

	if (use_ln)
		rc |= inv_imu_enable_accel_low_noise_mode(&imu_dev);
	else
		rc |= inv_imu_enable_accel_low_power_mode(&imu_dev);

	rc |= inv_imu_enable_gyro_low_noise_mode(&imu_dev);

	return rc;
}

static s32 icm42607_probe(struct aglink_gyro_hwio_ops *gyro_hwio, struct gyro_config config)
{
	int rc = 0;
	inv_imu_serif_t imu_serif;
	uint8_t whoami;
	inv_imu_int1_pin_config_t int1_pin_config;
	const struct icm_info *info;
	int tmp;

	hwio = gyro_hwio;

	imu_serif.context    = 0; /* no need */
	imu_serif.read_reg   = icm42607_read_reg;
	imu_serif.write_reg  = icm42607_write_reg;
	imu_serif.max_read   = 1024 * 32; /* maximum number of bytes allowed per serial read */
	imu_serif.max_write  = 1024 * 32; /* maximum number of bytes allowed per serial write */
	imu_serif.serif_type = SERIF_TYPE;

	inv_imu_init(&imu_dev, &imu_serif, sensor_event_cb);

	inv_imu_get_who_am_i(&imu_dev, &whoami);

	for (tmp = 0; tmp < icm42607_ids_arrsize; tmp++) {
		info = &icm42607_ids[tmp];
		if (info->ids == whoami) {
			gyro_printk(AGLINK_GYRO_DBG_ALWY, "get whoami[0x%x] ic[%s]\n", info->ids, info->name);
			goto check_done;
		}
	}

	gyro_printk(AGLINK_GYRO_DBG_ERROR, "get whoami[0x%x] failed\n", whoami);
	return INV_ERROR;

check_done:

	memset(&gyro_attri, 0x00, sizeof(gyro_attri));
	memcpy(&gyro_attri, &config, sizeof(gyro_attri));

	if ((gyro_attri.gpio_num >= 0) && (gyro_attri.int_sel & GYRO_INT_MASK) == GYRO_INT1) {
		int1_pin_config.int_polarity =
			gyro_attri.gpio_active ? INT_CONFIG_INT1_POLARITY_HIGH : INT_CONFIG_INT1_POLARITY_LOW;
		int1_pin_config.int_mode     = INT_CONFIG_INT1_MODE_PULSED;
		int1_pin_config.int_drive    = INT_CONFIG_INT1_DRIVE_CIRCUIT_PP;
		rc |= inv_imu_set_pin_config_int1(&imu_dev, &int1_pin_config);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "init int1\n");
	}

	/* Configure FSR (doesn't apply if FIFO is used in highres mode) */
	icm_pkt.accel_fs = ACCEL_CONFIG0_FS_SEL_4g;
	icm_pkt.gyro_fs = GYRO_CONFIG0_FS_SEL_2000dps;
	rc |= inv_imu_set_accel_fsr(&imu_dev, icm_pkt.accel_fs);
	rc |= inv_imu_set_gyro_fsr(&imu_dev, icm_pkt.gyro_fs);
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro fs: %s; accel fs: %s\n",
					icm42607_get_gyro_fs(), icm42607_get_accel_fs());

    /* Configure ODR */
	if (gyro_attri.speed > LSM6DSOX_SPEED_CNT_MAX)
		gyro_attri.speed = LSM6DSOX_SPEED_CNT_MAX;
	if (gyro_attri.speed < LSM6DSOX_SPEED_CNT_MIN)
		gyro_attri.speed = LSM6DSOX_SPEED_CNT_MIN;
	rc |= inv_imu_set_accel_frequency(&imu_dev, gyro_attri.speed);
	rc |= inv_imu_set_gyro_frequency(&imu_dev, gyro_attri.speed);

	rc |= inv_imu_config_apex(&imu_dev, SENSOR_APEX_DISABLE);

	icm_pkt.hires_en = 0;
	icm_pkt.use_ln = 1;
	/* Variable configuration */
	rc |= configure_fifo();
	rc |= configure_hires(icm_pkt.hires_en);
	rc |= configure_power_mode(icm_pkt.use_ln);

	return rc;
}

static s32 icm42607_remove(struct gyro_config config)
{
	return 0;
}

static s32 icm42607_fifo_start(void)
{
	return 0;
}

static s32 icm42607_fifo_stop(void)
{
	int rc = 0;
	inv_imu_interrupt_parameter_t int1_config = { (inv_imu_interrupt_value)0 };

	int1_config.INV_FIFO_THS = INV_IMU_DISABLE;
	rc |= inv_imu_set_config_int1(&imu_dev, &int1_config);

	rc |= inv_imu_configure_fifo(&imu_dev, INV_IMU_FIFO_DISABLED);
	return rc;
}

static void sensor_event_cb(inv_imu_sensor_event_t *event)
{
	static uint64_t fifo_timestamp;
	static uint64_t last_init_timestamp;
	s32 gyro_raw[3];
	//s32 acc_raw[3];

	/* Compute timestamp in us */
	if (last_init_timestamp != icm_pkt.int_timestamp) {
		fifo_timestamp = icm_pkt.int_timestamp;
		last_init_timestamp = icm_pkt.int_timestamp;
	} else {
		fifo_timestamp += icm_pkt.synctime_offset;
	}

	if (!icm_pkt.gyro_dat)
		return;

	icm_pkt.gyro_dat[icm_pkt.pkt_num].time = fifo_timestamp;

	if (icm_pkt.hires_en) {
		gyro_raw[0] = ((int32_t)event->gyro[0] << 4) | event->gyro_high_res[0];
		gyro_raw[1] = ((int32_t)event->gyro[1] << 4) | event->gyro_high_res[1];
		gyro_raw[2] = ((int32_t)event->gyro[2] << 4) | event->gyro_high_res[2];

		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.x =
			icm42607_from_fs_to_uradps(gyro_raw[0]);
		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.y =
			icm42607_from_fs_to_uradps(gyro_raw[1]);
		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.z =
			icm42607_from_fs_to_uradps(gyro_raw[2]);
	} else {
		gyro_raw[0] = event->gyro[0];
		gyro_raw[1] = event->gyro[1];
		gyro_raw[2] = event->gyro[2];
		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.x =
			icm42607_from_fs_to_uradps(gyro_raw[0]);
		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.y =
			icm42607_from_fs_to_uradps(gyro_raw[1]);
		icm_pkt.gyro_dat[icm_pkt.pkt_num].gyr_dat.z =
			icm42607_from_fs_to_uradps(gyro_raw[2]);
	}

	icm_pkt.pkt_num++;
}

static struct gyro_pkt *icm42607_get_fifo_data(s16 *dat_cnt)
{
	int pkt_cnt;
	static uint8_t first_wm;

	u64 sync_timestamp;

	pkt_cnt = inv_imu_get_frame_count(&imu_dev);

	if (pkt_cnt <= 0)
		goto out;

	sync_timestamp = aglink_gyro_get_base_time();

	icm_pkt.synctime_offset = (sync_timestamp - icm_pkt.int_timestamp) / pkt_cnt;

	if (first_wm) {
		icm_pkt.gyro_dat = (struct gyro_pkt *)aglink_gyro_zmalloc(pkt_cnt * sizeof(struct gyro_pkt));
		if (!icm_pkt.gyro_dat) {
			gyro_printk(AGLINK_GYRO_DBG_ERROR, "malloc gyro buffer failed\n");
			goto out;
		}
		*dat_cnt = pkt_cnt;
	} else {
		icm_pkt.gyro_dat = NULL;
		*dat_cnt = 0;
		first_wm = 1;
	}

	icm_pkt.pkt_num = 0;

	inv_imu_get_data_from_fifo(&imu_dev, pkt_cnt);

	icm_pkt.int_timestamp = sync_timestamp;

	return icm_pkt.gyro_dat;
out:
	return NULL;
}

static u64 icm42607_get_sample_speed(void)
{
	float speed;
	if (gyro_attri.speed > LSM6DSOX_SPEED_CNT_MAX)
		return 0;
	speed = gyro_speed[gyro_attri.speed - 5];
	return (u64)(((float)gyro_attri.fifo_deep / speed) * 1000000000.0);
}

struct gyro_ops icm42607_ops = {
	.probe = icm42607_probe,
	.remove = icm42607_remove,
	.read_fifo_start = icm42607_fifo_start,
	.read_fifo_stop = icm42607_fifo_stop,
	.get_fifo_dat = icm42607_get_fifo_data,
	.get_speed = icm42607_get_sample_speed,
};

