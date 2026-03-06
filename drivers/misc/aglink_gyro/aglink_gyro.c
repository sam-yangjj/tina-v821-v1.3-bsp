#include "aglink_gyro.h"
#include <stddef.h>
#include <linux/export.h>
#include "debug.h"

#ifdef CONFIG_GYRO_ICM42607
extern struct gyro_ops icm42607_ops;
#endif
#ifdef CONFIG_GYRO_LSM6DSOX
extern struct gyro_ops lsm6dsox_ops;
#endif
#ifdef CONFIG_GYRO_SC7U22
extern struct gyro_ops sc7u22_ops;
#endif

struct gyro_ops *gyro;

struct gyro_ops *gyro_ops_list[AGLINK_GYRO_ID_MAX] = {
#ifdef CONFIG_GYRO_ICM42607
	[AGLINK_ICM42607_ID] = &icm42607_ops,
#endif

#ifdef CONFIG_GYRO_LSM6DSOX
	[AGLINK_LSM6DSOX_ID] = &lsm6dsox_ops,
#endif

#ifdef CONFIG_GYRO_SC7U22
	[AGLINK_SC7U22_ID] = &sc7u22_ops,
#endif
};

u64 aglink_gyro_get_base_time(void)
{
	ktime_t boot_time;
	u64 timestamp;
	boot_time = ktime_get_boottime();
	timestamp = ktime_to_ns(boot_time);
#ifdef GYRO_DEBUG
	struct timespec ts;
	ts.tv_sec = timestamp / 1000000000;
	ts.tv_nsec = timestamp % 1000000000;
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro sampling initial time :%ld.%06ld s\n", ts.tv_sec, ts.tv_nsec / 1000);
#endif
	return timestamp;
}

void aglink_gyro_set_ops(u32 dev_id)
{
	gyro = gyro_ops_list[dev_id];
}

void aglink_gyro_clear_ops(void)
{
	gyro = NULL;
}

s32 aglink_gpro_chip_probe(struct aglink_gyro_hwio_ops *gyro_hwio, struct gyro_config gyroconfig)
{
	if (gyro)
		return gyro->probe(gyro_hwio, gyroconfig);
	else
		return -1;
}

s32 aglink_gpro_chip_remove(struct gyro_config gyroconfig)
{
	if (gyro)
		return gyro->remove(gyroconfig);
	else
		return -1;
}

s32 aglink_gyro_read_fifo_start(void)
{
	if (gyro)
		return gyro->read_fifo_start();
	else
		return -1;
}

s32 aglink_gyro_read_fifo_stop(void)
{
	if (gyro)
		return gyro->read_fifo_stop();
	else
		return -1;
}

struct gyro_pkt *aglink_gyro_get_fifo_dat(s16 *dat_cnt)
{
	if (gyro)
		return gyro->get_fifo_dat(dat_cnt);
	else
		return NULL;
}

u64 aglink_gyro_get_speed(void)
{
	if (gyro)
		return gyro->get_speed();
	else
		return 0;
}
