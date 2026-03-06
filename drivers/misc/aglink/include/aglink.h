/*
 * v821_wlan/aglink.h
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

#ifndef __AG_LINK_HW_H__
#define __AG_LINK_HW_H__

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include <uapi/aglink/ag_proto.h>
#include "queue.h"

/* Xradio xrlink Queue */
#define XR_RX_COM_QUEUE_SZ   (24)
#define XR_CMD_RSP_QUEUE_SZ  (2)
#define XR_DATA_TX_QUEUE_SZ  (160)
#define XR_DATA_RX_QUEUE_SZ  (256)

struct aglink_plat;
struct aglink_debug_common;
struct aglink_hw {
	struct aglink_plat *plat;

	u8 txrx_enable;
	u32 tx_flow_ctl_timeout;

	struct aglink_queue_t queue_tx_data;
	struct aglink_queue_t queue_rx_data;

	wait_queue_head_t rx_wq;
	xr_thread_handle_t rx_thread;

	wait_queue_head_t tx_wq;
	wait_queue_head_t tx_flow_ctl_wq;
	xr_thread_handle_t tx_thread;

	xr_atomic_t tx_data_flow_ctl; // tx flow ctl
	xr_atomic_t tx_data_pause_master; // linux no buffer
	xr_atomic_t tx_data_pause_slave; // rtos no buffer

	u16 data_tx_seq;

	struct aglink_debug_common *debug;
};

#endif
