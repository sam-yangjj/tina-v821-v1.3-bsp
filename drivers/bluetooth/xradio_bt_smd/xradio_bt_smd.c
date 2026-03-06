// SPDX-License-Identifier: GPL-2.0-only
/*
 * xradio_bt_smd.c
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include "xradio_bt_smd.h"

static int xradio_bt_smd_recv(struct hci_dev *hdev, unsigned int type,
								const void *data, size_t count);
static struct xradio_bt_rpmsg_dev *xrbt_rpmsg_dev;
static struct xradio_bt_smd *xrbt_smd_dev;
static struct semaphore rpmsg_up_sem;

int xradio_bt_rpmsg_tx(struct xradio_bt_rpmsg_dev *rpmsg_dev, void *msg, uint len)
{
	int ret;

	XRBT_DBG("bt rpmsg tx len:%d", len);

	if (!rpmsg_dev) {
		XRBT_ERR("xrbt_rpmsg_dev not initialized");
		return -1;
	}

	if (len > rpmsg_dev->len) {
		XRBT_ERR("rpmsg send too long: %d, max:%d", len, rpmsg_dev->len);
		return -EINVAL;
	}

	//will block wait for rpmsg buffer is avaliable then return
	ret = rpmsg_send(rpmsg_dev->msg_dev->ept, msg, len);
	if (ret)
		XRBT_ERR("rpmsg send failed: %d", ret);

	return ret;
}

static int xradio_bt_controller_init(void)
{
	int ret = 0;
	int btc_init_msg_len = 0;
	struct rpmsg_btc_init *btc_init_msg = NULL;

	XRBT_DBG("xradio_bt_controller_init");

	btc_init_msg_len = sizeof(*btc_init_msg);
	btc_init_msg = kmalloc(btc_init_msg_len, GFP_KERNEL);

	if (!btc_init_msg) {
		XRBT_ERR("failed to allocate memory for btc_init_msg");
		return -ENOMEM;
	}

	memset(btc_init_msg, 0, btc_init_msg_len);
	btc_init_msg->cmd_id = XRBT_RPMSG_CMD_BTC_INIT;
	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, btc_init_msg, btc_init_msg_len);
	kfree(btc_init_msg);

	return ret;
}

static int xradio_bt_controller_deinit(void)
{
	int ret = 0;
	int btc_denit_msg_len = 0;
	struct rpmsg_btc_deinit *btc_deinit_msg = NULL;

	XRBT_DBG("xradio_bt_controller_deinit");

	btc_denit_msg_len = sizeof(*btc_deinit_msg);
	btc_deinit_msg = kmalloc(btc_denit_msg_len, GFP_KERNEL);

	if (!btc_deinit_msg) {
		XRBT_ERR("failed to allocate memory for btc_deinit_msg");
		return -ENOMEM;
	}

	memset(btc_deinit_msg, 0, btc_denit_msg_len);
	btc_deinit_msg->cmd_id = XRBT_RPMSG_CMD_BTC_DEINIT;
	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, btc_deinit_msg, btc_denit_msg_len);
	kfree(btc_deinit_msg);

	return ret;
}

static int xradio_bt_set_sdd_size(u32 sdd_size)
{
	int ret = 0;
	int sdd_size_msg_len = 0;
	struct rpmsg_set_sdd_size *set_sdd_size_msg = NULL;

	XRBT_DBG("set_sdd_size :%d", sdd_size);

	sdd_size_msg_len = sizeof(*set_sdd_size_msg);
	set_sdd_size_msg = kmalloc(sdd_size_msg_len, GFP_KERNEL);

	if (!set_sdd_size_msg) {
		XRBT_ERR("failed to allocate memory for set_sdd_size_msg");
		return -ENOMEM;
	}

	memset(set_sdd_size_msg, 0, sdd_size_msg_len);
	set_sdd_size_msg->cmd_id = XRBT_RPMSG_CMD_SDD_SIZE;
	set_sdd_size_msg->sdd_size = sdd_size;
	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, set_sdd_size_msg, sdd_size_msg_len);
	kfree(set_sdd_size_msg);

	return ret;
}

static int xradio_bt_write_sdd(const u8 *data, u32 data_len)
{
	int ret = 0;
	int write_sdd_msg_len = 0;
	struct rpmsg_write_sdd *write_sdd_msg = NULL;

	XRBT_DBG("write sdd len:%d", data_len);

	write_sdd_msg_len = sizeof(*write_sdd_msg) + data_len;
	write_sdd_msg = kmalloc(write_sdd_msg_len, GFP_KERNEL);

	if (!write_sdd_msg) {
		XRBT_ERR("failed to allocate memory for write_sdd_msg");
		return -ENOMEM;
	}

	memset(write_sdd_msg, 0, write_sdd_msg_len);
	write_sdd_msg->cmd_id = XRBT_RPMSG_CMD_SDD_WRITE;
	write_sdd_msg->data_len = data_len;
	if (data != NULL && data_len > 0)
		memcpy(write_sdd_msg->sdd_data, data, data_len);

	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, write_sdd_msg, write_sdd_msg_len);
	kfree(write_sdd_msg);

	return ret;
}

static __attribute__((unused)) int xradio_bt_read_memory(u32 target_addr, u32 len)
{
	int ret = 0;
	int read_mem_msg_len = 0;
	struct rpmsg_read_mem_h2c *read_mem_msg = NULL;

	XRBT_INFO("read memory:%x, len:%d", target_addr, len);

	read_mem_msg_len = sizeof(*read_mem_msg);
	read_mem_msg = kmalloc(read_mem_msg_len, GFP_KERNEL);

	if (!read_mem_msg) {
		XRBT_ERR("failed to allocate memory for read_mem_msg");
		return -ENOMEM;
	}

	memset(read_mem_msg, 0, read_mem_msg_len);
	read_mem_msg->cmd_id = XRBT_RPMSG_CMD_MEM_READ_H2C;
	read_mem_msg->target_addr = target_addr;
	read_mem_msg->data_len = len;
	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, read_mem_msg, read_mem_msg_len);
	kfree(read_mem_msg);

	return ret;
}

static __attribute__((unused)) int xradio_bt_write_memory(u32 target_addr, u8 *data, u32 data_len)
{
	int ret = 0;
	int write_mem_msg_len = 0;
	struct rpmsg_write_mem *write_mem_msg = NULL;

	XRBT_INFO("write memory:%x", target_addr);

	write_mem_msg_len = sizeof(*write_mem_msg) + data_len;
	write_mem_msg = kmalloc(write_mem_msg_len, GFP_KERNEL);

	if (!write_mem_msg) {
		XRBT_ERR("failed to allocate memory for write_mem_msg");
		return -ENOMEM;
	}

	memset(write_mem_msg, 0, write_mem_msg_len);
	write_mem_msg->cmd_id = XRBT_RPMSG_CMD_MEM_WRITE;
	write_mem_msg->target_addr = target_addr;
	write_mem_msg->data_len = data_len;
	if (data != NULL && data_len > 0)
		memcpy(write_mem_msg->data, data, data_len);

	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, write_mem_msg, write_mem_msg_len);
	kfree(write_mem_msg);

	return ret;
}

static int xradio_bt_load_sdd_file(struct device *dev)
{
	int ret;
	const struct firmware *fw;
	u32 load_len = 0;
	u32 chunk_size = 480;
	u32 offset = 0;

	XRBT_DBG("load sdd file start");

	ret = request_firmware(&fw, V821_BT_SDD, dev);
	if (ret < 0) {
		XRBT_ERR("failed to request sdd firmware");
		return ret;
	}

	XRBT_INFO("sdd file size: %zu bytes", fw->size);
	xradio_bt_set_sdd_size(fw->size);

	load_len = fw->size;
	while (load_len > 0) {
		u32 current_chunk_size = (load_len > chunk_size) ? chunk_size : load_len;
		ret = xradio_bt_write_sdd(fw->data + offset, current_chunk_size);
		if (ret < 0) {
			XRBT_ERR("failed to write sdd data chunk");
			release_firmware(fw);
			return ret;
		}

		load_len -= current_chunk_size;
		offset += current_chunk_size;
	}

	release_firmware(fw);

	XRBT_DBG("load sdd file end");

	return 0;
}

static int xradio_bt_handle_hcidata(struct xradio_bt_smd *xrbt_smd,
							u8 hcidata_type, int len, void *data)
{
	/* Update received byte statistics */
	xrbt_smd->hdev->stat.byte_rx += len;

	/* Determine the packet type and call xradio_bt_smd_recv accordingly */
	switch (hcidata_type) {
	case HCI_ACLDATA_PKT:
		/* Call xradio_bt_smd_recv while skipping the first byte */
		return xradio_bt_smd_recv(xrbt_smd->hdev, HCI_ACLDATA_PKT, data, len);
	case HCI_EVENT_PKT:
		/* Call xradio_bt_smd_recv while skipping the first byte */
		return xradio_bt_smd_recv(xrbt_smd->hdev, HCI_EVENT_PKT, data, len);
	default:
		/* Handle unknown packet types accordingly */
		XRBT_ERR("unknown hci packet type: %02x", hcidata_type);
		return -EINVAL;
	}
}

static void xradio_bt_handle_mem_data(struct rpmsg_read_mem_ch2 *read_mem_msg)
{
	int i = 0;

	XRBT_INFO("target memory address: 0x%08X", read_mem_msg->target_addr);
	XRBT_INFO("data length: %u\n", read_mem_msg->data_len);

	XRBT_INFO("get controller data:");
	for (i = 0; i < read_mem_msg->data_len; i++) {
		printk(KERN_CONT "0x%02X ", read_mem_msg->data[i]);
	}
	printk("\n");
}

static int xradio_bt_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len, void *priv, u32 src)
{
	int i = 0;
	u8 *msg_data;
	u8 cmd_id;

	if (len <= 0) {
		XRBT_ERR("invalid packet length: %d", len);
		return -EINVAL;
	}

	msg_data = (u8 *)data;
	cmd_id = msg_data[0];

	if (len <= 0) {
		XRBT_ERR("invalid packet length: %d", len);
		return -EINVAL;
	}

	if (cmd_id == XRBT_RPMSG_CMD_HCI_C2H) {
		struct rpmsg_hci_data *hci_msg = (struct rpmsg_hci_data *)data;
#if XRBT_DBG_FLAG
		XRBT_INFO("recv hci_msg data:");
		for (i = 0; i < len; i++) {
			printk(KERN_CONT "%02X ", *((u8 *)hci_msg + i));
		}
		printk("\n");
#endif
		XRBT_DBG("hcidata_len: %d\n", hci_msg->hci_datalen);
		return xradio_bt_handle_hcidata(xrbt_smd_dev, hci_msg->hci_data[0],
									hci_msg->hci_datalen - HCI_TYPE_LENGTH,
									hci_msg->hci_data + HCI_TYPE_LENGTH);
	} else if (cmd_id == XRBT_RPMSG_CMD_MEM_READ_C2H) {
		struct rpmsg_read_mem_ch2 *read_mem_msg = (struct rpmsg_read_mem_ch2 *)data;
		xradio_bt_handle_mem_data(read_mem_msg);
		return 0;
	} else {
		XRBT_ERR("unknown cmd id: %d", cmd_id);
		return -EINVAL;
	}
}

static int xradio_bt_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct xradio_bt_rpmsg_dev *rpmsg_dev;

	XRBT_INFO("xradio_bt_rpmsg_probe");

	rpmsg_dev = devm_kzalloc(&rpdev->dev, sizeof(struct xradio_bt_rpmsg_dev), GFP_KERNEL);
	if (!rpmsg_dev) {
		XRBT_ERR("no mem");
		return -ENOMEM;
	}

	XRBT_INFO("new channel: 0x%x -> 0x%x", rpdev->src, rpdev->dst);

	/* we need to announce the new ept to remote */
	rpdev->announce = (rpdev->src != RPMSG_ADDR_ANY);

	rpmsg_dev->dev = &rpdev->dev;
	rpmsg_dev->msg_dev = rpdev;
	rpmsg_dev->len = XRADIO_BT_RPMSG_DATA_LEN;
	xrbt_rpmsg_dev = rpmsg_dev;
	dev_set_drvdata(rpmsg_dev->dev, rpmsg_dev);

	XRBT_INFO("xrbt rpmsg up");
	up(&rpmsg_up_sem);
	XRBT_INFO("xrbt rpmsg ready");
	return 0;
}

static void xradio_bt_rpmsg_remove(struct rpmsg_device *rpdev)
{
	if (!xrbt_rpmsg_dev) {
		XRBT_ERR("xrbt_rpmsg_dev is NULL");
		return;
	}
	devm_kfree(&rpdev->dev, xrbt_rpmsg_dev);
	xrbt_rpmsg_dev = NULL;
	XRBT_INFO("xradio bt rpmsg client driver is removed");
}

static struct rpmsg_device_id rpmsg_driver_xradio_id_table[] = {
	{ .name = XRADIO_BT_RPMSG_NAME },
	{ },
};

static struct rpmsg_driver rpmsg_xradio_client = {
	.drv.name	= XRADIO_BT_RPMSG_NAME,
	.id_table	= rpmsg_driver_xradio_id_table,
	.probe		= xradio_bt_rpmsg_probe,
	.callback	= xradio_bt_rpmsg_cb,
	.remove		= xradio_bt_rpmsg_remove,
};

int xradio_bt_rpmsg_init(struct device *dev)
{
	int ret = 0;

	sema_init(&rpmsg_up_sem, 0);
	ret = register_rpmsg_driver(&rpmsg_xradio_client);
	if (ret) {
		XRBT_ERR("register rpmsg driver fail %d", ret);
		return ret;
	}

	if (down_timeout(&rpmsg_up_sem, msecs_to_jiffies(10000)) != 0) {
		unregister_rpmsg_driver(&rpmsg_xradio_client);
		XRBT_ERR("xradio bt rpmsg probe timeout");
		return -1;
	}

	XRBT_INFO("xrbt rpmsg creat succeed");

	xradio_bt_load_sdd_file(dev);
	msleep(10);
	xradio_bt_controller_init();
	msleep(2000);

	return 0;
}

void xradio_bt_rpmsg_deinit(void)
{
	unregister_rpmsg_driver(&rpmsg_xradio_client);
}

static int xradio_bt_smd_hci_send(struct hci_dev *hdev, struct sk_buff *skb)
{
	int i = 0;
	int ret = 0;
	int hci_msg_len = 0;
	struct rpmsg_hci_data *hci_msg;
	u8 hci_type = hci_skb_pkt_type(skb);

	hci_msg_len = sizeof(*hci_msg) + skb->len + HCI_TYPE_LENGTH;
	hci_msg = kmalloc(hci_msg_len, GFP_KERNEL);
	if (!hci_msg) {
		XRBT_ERR("failed to allocate memory for hci_msg\n");
		return -ENOMEM;
	}

	memset(hci_msg, 0, hci_msg_len);
	hci_msg->cmd_id = XRBT_RPMSG_CMD_HCI_H2C;
	hci_msg->hci_datalen = skb->len + HCI_TYPE_LENGTH;
	hci_msg->hci_data[0] = hci_type;
	memcpy(hci_msg->hci_data + HCI_TYPE_LENGTH, skb->data, skb->len);

#if XRBT_DBG_FLAG
	XRBT_INFO("send hci raw data:");
	for (i = 0; i < hci_msg_len; i++) {
		printk(KERN_CONT "%02X ", *((u8 *)hci_msg + i));
	}
	printk("\n");
#endif

	ret = xradio_bt_rpmsg_tx(xrbt_rpmsg_dev, hci_msg, hci_msg_len);
	if (ret) {
		hdev->stat.err_tx++;
	} else {
		switch (hci_skb_pkt_type(skb)) {
		case HCI_ACLDATA_PKT:
			hdev->stat.acl_tx++;
			break;
		case HCI_COMMAND_PKT:
			hdev->stat.cmd_tx++;
			break;
		}
		hdev->stat.byte_tx += skb->len;
		dev_kfree_skb(skb);
	}

	kfree(hci_msg);

	return ret;
}

static int xradio_bt_smd_recv(struct hci_dev *hdev, unsigned int type,
								const void *data, size_t count)
{
	struct sk_buff *skb;
	/* Use GFP_ATOMIC as we're in IRQ context */
	skb = bt_skb_alloc(count, GFP_ATOMIC);
	if (!skb) {
		hdev->stat.err_rx++;
		XRBT_ERR("-ENOMEM");
		return -ENOMEM;
	}

	hci_skb_pkt_type(skb) = type;
	skb_put_data(skb, data, count);

	return hci_recv_frame(hdev, skb);
}

static int xradio_bt_smd_hci_open(struct hci_dev *hdev)
{
	return 0;
}

static int xradio_bt_smd_hci_close(struct hci_dev *hdev)
{
	return 0;
}

static int xradio_bt_smd_hci_setup(struct hci_dev *hdev)
{
	return 0;
}

static int xradio_bt_smd_probe(struct platform_device *pdev)
{
	struct hci_dev *hdev;
	int ret;

	XRBT_INFO("xradio_bt_smd probe start");
	xrbt_smd_dev = devm_kzalloc(&pdev->dev, sizeof(*xrbt_smd_dev), GFP_KERNEL);
	if (!xrbt_smd_dev)
		return -ENOMEM;

	ret = xradio_bt_rpmsg_init(&pdev->dev);
	if (ret < 0) {
		XRBT_ERR("xradio_bt_rpmsg_init failed");
		return -ENOMEM;
	}

	hdev = hci_alloc_dev();
	if (!hdev)
		ret = -ENOMEM;
	hci_set_drvdata(hdev, xrbt_smd_dev);
	xrbt_smd_dev->hdev = hdev;
	SET_HCIDEV_DEV(hdev, &pdev->dev);

	hdev->bus = HCI_SMD;
	hdev->open = xradio_bt_smd_hci_open;
	hdev->close = xradio_bt_smd_hci_close;
	hdev->send = xradio_bt_smd_hci_send;
	hdev->setup = xradio_bt_smd_hci_setup;
	ret = hci_register_dev(hdev);
	if (ret < 0)
		goto hci_free_dev;

	platform_set_drvdata(pdev, xrbt_smd_dev);

	XRBT_INFO("xradio_bt_smd probe end");

	return 0;

hci_free_dev:
	hci_free_dev(hdev);

	return ret;
}

static int xradio_bt_smd_remove(struct platform_device *pdev)
{
	struct xradio_bt_smd *xrbt_smd = platform_get_drvdata(pdev);

	xradio_bt_controller_deinit();
	msleep(3000);
	hci_unregister_dev(xrbt_smd->hdev);
	hci_free_dev(xrbt_smd->hdev);
	msleep(2000);

	xradio_bt_rpmsg_deinit();

	return 0;
}

static const struct of_device_id xradio_bt_smd_of_match[] = {
	{ .compatible = "allwinner,xradio_bt_smd", },
	{ },
};
MODULE_DEVICE_TABLE(of, xradio_bt_smd_of_match);

static struct platform_driver xradio_bt_smd_driver = {
	.probe = xradio_bt_smd_probe,
	.remove = xradio_bt_smd_remove,
	.driver  = {
		.name  = "xradio_bt_smd",
		.of_match_table = xradio_bt_smd_of_match,
	},
};

module_platform_driver(xradio_bt_smd_driver);

MODULE_AUTHOR("luwinkey <luwinkey@allwinnertech.com>");
MODULE_DESCRIPTION("Xradio BT SMD HCI driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
