//SPDX-License-Identifier: GPL-2.0-only
/*
 * xradio_bt_smd.h
 *
 * Copyright (c) 2024
 * Allwinner Technology Co., Ltd. <www.allwinner.com>
 * luwinkey <luwinkey@allwinnertech.com>
 *
 * xradio bt smd driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#define XRBT_DBG_FLAG 0
#define HCI_TYPE_LENGTH 1
#define XRADIO_BT_RPMSG_DATA_LEN 496
#define V821_BT_SDD ("sdd_v821b.bin")
#define XRADIO_BT_RPMSG_NAME "bt_xrlink_rpmsg"

enum xrbt_rpmsg_cmd {
	XRBT_RPMSG_CMD_SDD_SIZE = 1,
	XRBT_RPMSG_CMD_SDD_WRITE,
	XRBT_RPMSG_CMD_BTC_INIT,
	XRBT_RPMSG_CMD_BTC_DEINIT,
	XRBT_RPMSG_CMD_RESERVED,
	XRBT_RPMSG_CMD_HCI_H2C,
	XRBT_RPMSG_CMD_HCI_C2H,
	XRBT_RPMSG_CMD_MEM_READ_H2C,
	XRBT_RPMSG_CMD_MEM_READ_C2H,
	XRBT_RPMSG_CMD_MEM_WRITE,
	XRBT_RPMSG_CMD_MAX,
};

struct rpmsg_set_sdd_size {
	u8 cmd_id;
	u32 sdd_size;
} __packed;

struct rpmsg_write_sdd {
	u8 cmd_id;
	u32 data_len;
	u8 sdd_data[];
} __packed;

struct rpmsg_btc_init {
	u8 cmd_id;
};

struct rpmsg_btc_deinit {
	u8 cmd_id;
};

struct rpmsg_hci_data {
	u8 cmd_id;
	u32 hci_datalen;
	u8  hci_data[];
} __packed;

struct rpmsg_read_mem_h2c {
	u8 cmd_id;
	u32 target_addr;
	u32 data_len;
} __packed;

struct rpmsg_read_mem_ch2 {
	u8 cmd_id;
	u32 target_addr;
	u32 data_len;
	u8 data[];
} __packed;

struct rpmsg_write_mem {
	u8 cmd_id;
	u32 target_addr;
	u32 data_len;
	u8 data[];
} __packed;

struct xradio_bt_rpmsg_dev {
	struct rpmsg_device *msg_dev;
	struct device *dev;
	int len;
};

struct xradio_bt_smd {
	struct hci_dev *hdev;
};

#if XRBT_DBG_FLAG
#define XRBT_DBG(fmt, arg...)   pr_info("xrbt_smd: %s: " fmt "\n", __func__, ## arg)
#else
#define XRBT_DBG(fmt, arg...)
#endif
#define XRBT_INFO(fmt, arg...)  pr_info("xrbt_smd: " fmt "\n", ## arg)
#define XRBT_WARN(fmt, arg...)  pr_warn("xrbt_smd: " fmt "\n", ## arg)
#define XRBT_ERR(fmt, arg...)   pr_err("xrbt_smd: " fmt "\n", ## arg)
