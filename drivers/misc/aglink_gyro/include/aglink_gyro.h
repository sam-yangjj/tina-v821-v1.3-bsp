#ifndef _AGLINK_GYRO_H_
#define _AGLINK_GYRO_H_

#include <linux/types.h>

#define GYRO_INT1 0x1
#define GYRO_INT2 0x2
#define GYRO_INT_MASK 0x3
//#define GYRO_DEBUG

struct gyro_data {
	s64 x;
	s64 y;
	s64 z;
};

struct gyro_pkt {
	struct gyro_data gyr_dat;
	u64 time;
};

typedef union {
	s16 i16bit[3];
	u8 u8bit[6];
} gyro3bit16_t;

struct gyro_config {
	u8 xl_en;
	u8 gy_en;
	u8 speed;
	u8 ts_en;
	u8 int_sel;
	u8 gpio_active;
	u16 addr;
	u32 reg;
	s32 gpio_num;
	u32 fifo_deep;
};

struct aglink_gyro_hwio_ops {
	int (*write)(u16 addr, u8 reg, const u8 *bufp, u16 len);
	int (*read)(u16 addr, u8 reg, u8 *bufp, u16 len);
};

struct gyro_ops {
	int (*probe)(struct aglink_gyro_hwio_ops *gyro_hwio, struct gyro_config config);
	int (*remove)(struct gyro_config config);
	int (*read_fifo_start)(void);
	int (*read_fifo_stop)(void);
	struct gyro_pkt *(*get_fifo_dat)(s16 *dat_cnt);
	u64 (*get_speed)(void);
};

enum aglink_gyro_hw_id {
	AGLINK_ICM42607_ID,
	AGLINK_LSM6DSOX_ID,
	AGLINK_SC7U22_ID,
	AGLINK_GYRO_ID_MAX,
};

/* define in aglink_gyro.c, call for gyro driver*/
u64 aglink_gyro_get_base_time(void);

/* define in aglink_gyro.c, call for aglink_gyro_hwio.c*/
void aglink_gyro_set_ops(u32 dev_id);
void aglink_gyro_clear_ops(void);
s32 aglink_gpro_chip_probe(struct aglink_gyro_hwio_ops *gyro_hwio, struct gyro_config gyroconfig);
s32 aglink_gpro_chip_remove(struct gyro_config gyroconfig);

/* define in aglink_gyro.c, call for aglink_take_gyro.c*/
int aglink_gyro_read_fifo_start(void);
int aglink_gyro_read_fifo_stop(void);
struct gyro_pkt *aglink_gyro_get_fifo_dat(s16 *dat_cnt);
u64 aglink_gyro_get_speed(void);
#endif
