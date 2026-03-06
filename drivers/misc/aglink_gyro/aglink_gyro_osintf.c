#include "aglink_gyro_osintf.h"

void *aglink_gyro_zmalloc(u32 sz)
{
	return kzalloc(sz, GFP_ATOMIC | GFP_DMA);
}

void aglink_gyro_free(void *p)
{
	kfree(p);
}
