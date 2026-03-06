#include "sc7u22_reg.h"
#include "aglink_gyro.h"
#include "debug.h"

#define SC7U22_SPEED_CNT_MAX 13

static struct gyro_config gyro_attri;
float gyro_speed[8] = {25.0, 50.0, 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0};

static s32 sc7u22_probe(struct aglink_gyro_hwio_ops *gyro_hwio, struct gyro_config config)
{
	s32 ret = 0;
	sl_sc7u22_get_hwio_ops(gyro_hwio);

	memset(&gyro_attri, 0x00, sizeof(gyro_attri));
	memcpy(&gyro_attri, &config, sizeof(gyro_attri));

	if (!sl_sc7u22_config(config)) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "config failed\n");
		ret = -1;
		goto fail;
	}

	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sc7u22 init succces\n");
	return 0;

fail:
	sl_sc7u22_get_hwio_ops(NULL);
	return ret;
}

static s32 sc7u22_remove(struct gyro_config config)
{
	sl_sc7u22_power_down();
	sl_sc7u22_get_hwio_ops(NULL);
	return 0;
}

static s32 sc7u22_fifo_start(void)
{
	return 0;
}

static s32 sc7u22_fifo_stop(void)
{
	sl_sc7u22_open_close_set(0, 0);
	return 0;
}

static struct gyro_pkt *sc7u22_get_fifo_data(s16 *dat_cnt)
{
	return sl_sc7u22_fifo_read(dat_cnt);
}

static u64 sc7u22_get_sample_speed(void)
{
	float speed;
	if (gyro_attri.speed > SC7U22_SPEED_CNT_MAX)
		return 0;
	speed = gyro_speed[gyro_attri.speed - 6];
	return (u64)(((float)gyro_attri.fifo_deep / speed) * 1000000000.0);
}

struct gyro_ops sc7u22_ops = {
	.probe = sc7u22_probe,
	.remove = sc7u22_remove,
	.read_fifo_start = sc7u22_fifo_start,
	.read_fifo_stop = sc7u22_fifo_stop,
	.get_fifo_dat = sc7u22_get_fifo_data,
	.get_speed = sc7u22_get_sample_speed,
};
