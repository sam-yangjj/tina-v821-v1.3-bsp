#ifndef __AGLINK_GYRO_OS_INT_F_H__
#define __AGLINK_GYRO_OS_INT_F_H__

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/unistd.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/list.h>

void *aglink_gyro_zmalloc(u32 sz);
void aglink_gyro_free(void *p);

#endif
