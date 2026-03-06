/*
 * v821_wlan/hwio.h
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * aglink common private APIs for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef _AGLINK_HWIO_H_
#define _AGLINK_HWIO_H_

#include <uapi/aglink/ag_proto.h>
#include "aglink.h"
#include "queue.h"
#include "platform.h"

#ifdef CONFIG_AGLINK_DEV_NUM
#define AGLINK_DEV_NUM CONFIG_AGLINK_DEV_NUM
#else
#error "CONFIG_AGLINK_DEV_NUM is not defined! AGLINK_DEV_NUM is NULL"
#endif

enum aglink_txbus_op {
	XR_TXBUS_OP_AUTO = 0,
	XR_TXBUS_OP_FORCE_TX,
	XR_TXBUS_OP_TX_RETRY,
	XR_TXBUS_OP_RESET_BUF,
	XR_TXBUS_OP_FLUSH_BUF
};

enum aglink_txdata_status {
	XR_TXD_ST_SUCESS = 0,
	XR_TXD_ST_NO_MEM = 1,
	XR_TXD_ST_NO_QUEUE = 2,
	XR_TXD_ST_DATA_ERR = 3,
	XR_TXD_ST_NO_RSP = 4,
	XR_TXD_ST_BUS_TX_FAIL = 5,
};

struct aglink_bus {
	struct aglink_bus_ops *ops;
	u8 rx_buff[AGLNK_RX_BUFF_LEN];
	u32 rx_pos;
    bool enabled;
};

struct aglink_plat {
    struct aglink_hw hw_priv[AGLINK_DEV_NUM];
    struct aglink_bus bus_if[AGLINK_DEV_NUM];
	int dev_num;
};

int aglink_platform_tx_data(struct sk_buff *skb);

int aglink_platform_init(void);

void aglink_platform_deinit(void);

struct aglink_hw *aglink_platform_get_hw(int dev_id);

int aglink_platform_dev_num(void);
#endif
