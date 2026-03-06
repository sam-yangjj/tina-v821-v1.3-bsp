#ifndef __AG_LINK_PLAT_H_
#define __AG_LINK_PLAT_H_

#define AGLNK_RX_BUFF_LEN 4096

struct aglink_bus_ops {
	int (*start) (void);
	void (*stop) (void);
	int (*tx)(int dev_id, unsigned char *buff, unsigned short len);
	int (*rx_cb)(int dev_id, unsigned char *buff, unsigned short len);
	int (*rx_disorder_cb)(int dev_id, unsigned char *buff, unsigned short len);
};

#endif
