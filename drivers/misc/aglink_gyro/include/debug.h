#ifndef __AGLINK_DEBUG_H__
#define __AGLINK_DEBUG_H__

#include <linux/kernel.h>
#include "aglink_gyro_osintf.h"

/* Message always need to be present even in release version. */
#define AGLINK_GYRO_DBG_ALWY 0x01

/* Error message to report an error, it can hardly works. */
#define AGLINK_GYRO_DBG_ERROR 0x02

/* Warning message to inform us of something unnormal or
 * something very important, but it still work. */
#define AGLINK_GYRO_DBG_WARN 0x04

/* Important message we need to know in unstable version. */
#define AGLINK_GYRO_DBG_NIY 0x08

/* Normal message just for debug in developing stage. */
#define AGLINK_GYRO_DBG_MSG 0x10

/* Trace of functions, for sequence of functions called. Normally,
 * don't set this level because there are too more print. */
#define AGLINK_GYRO_DBG_TRC 0x20

#define AGLINK_GYRO_DBG_LEVEL 0xFF

#define AGLINK_DBG_DEFAULT (AGLINK_GYRO_DBG_ALWY | AGLINK_GYRO_DBG_ERROR | AGLINK_GYRO_DBG_WARN | AGLINK_GYRO_DBG_NIY | AGLINK_GYRO_DBG_MSG)

#define gyro_printk(level, fmt, ...)                                                                          \
	do {                                                                                                     \
		if (!((level) & AGLINK_DBG_DEFAULT))                                                            \
			break;                                                                                      \
		if ((level) == AGLINK_GYRO_DBG_ERROR)                                                            \
			printk(KERN_ERR "[%s,%d][GYRO_ERR] " fmt, __func__, __LINE__, ##__VA_ARGS__);        \
		else if ((level) == AGLINK_GYRO_DBG_WARN)                                                        \
			printk(KERN_WARNING "[%s,%d][GYRO_WRN] " fmt, __func__, __LINE__, ##__VA_ARGS__);    \
		else if ((level) == AGLINK_GYRO_DBG_ALWY)                                                                          \
			printk(KERN_DEBUG "[%s,%d][GYRO_DEG] " fmt, __func__, __LINE__, ##__VA_ARGS__);          \
	} while (0)
#endif
