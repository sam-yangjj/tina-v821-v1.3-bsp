/*
 * aglink/main.c
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * Main code init for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/module.h>

#include "hwio.h"
#include "debug.h"
#include "txrx.h"

#define CHECK_DEVICE_ID(dev_id) \
    do { \
		if (dev_id < 0 || dev_id >= AGLINK_DEV_NUM) { \
			iface_printk(AGLINK_DBG_ERROR, "the device ID is invalid:%d\n", dev_id); \
			return -1; \
		} \
	} while (0)

#define CHECK_DEVICE_NUM() \
	do { \
		if (ag_plat.dev_num <= 0) { \
			iface_printk(AGLINK_DBG_ERROR, "No device available :%d\n", ag_plat.dev_num); \
			return -1; \
		} \
	} while (0)

static struct aglink_plat ag_plat;


/*     skb struct
 *
 *     [     ]
 *     [     ] - skb->head
 *     [     ]
 *     [     ]  head buffer
 *     [     ]
 *     [     ] - skb->data
 *     [     ]       |
 *     [     ]       |
 *     [     ]   skb->len
 *     [     ]       |
 *     [     ]       |
 *     [     ] - skb->tail
 *     [     ]
 *
 *
 *   skb_put:     skb->tail +=len;  skb->len+=len;
 *   skb_push:    skb->data -=len;  skb->len+=len;
 *   skb_pull:    skb->data +=len;  skb->len-=len;
 *   skb_reserve: skb->data +=len;  skb->tail+=len;
 *
 */


int aglink_platform_tx_data(struct sk_buff *skb)
{
	struct aglink_hdr *hdr;
	int dev_id;
//	int i;
//	for (i = 0; i < skb->len; i++) {
//		printk("%x ", skb->data[i]);
//	}
//	printk("\n");

	CHECK_DEVICE_NUM();

	hdr = (struct aglink_hdr *)skb->data;

	dev_id = hdr->ch;

	CHECK_DEVICE_ID(dev_id);

	if (ag_plat.bus_if[dev_id].ops && ag_plat.bus_if[dev_id].ops->tx)
		return ag_plat.bus_if[dev_id].ops->tx(dev_id, skb->data, skb->len);

	return 0;
}

int aglink_bus_rx_raw_data(int dev_id, u8 *buff, u16 len)
{

	struct aglink_hdr *hdr;
	struct sk_buff *skb = NULL;
	struct aglink_hw *hw_priv;
    int ret = -1;

	CHECK_DEVICE_NUM();

	CHECK_DEVICE_ID(dev_id);

	hw_priv = &ag_plat.hw_priv[dev_id];

	skb = __dev_alloc_skb(len + XR_HDR_SZ, GFP_KERNEL);
	if (!skb) {
		iface_printk(AGLINK_DBG_ERROR, "skb alloc failed");
		return -ENOMEM;
	}
	hdr = (struct aglink_hdr *)skb->data;
	memset(hdr, 0, XR_HDR_SZ);

	hdr->len = len;
	hdr->type = 0;
	hdr->ch = dev_id;

	memcpy(skb->data + XR_HDR_SZ, buff, len);
	skb_put(skb, len + XR_HDR_SZ);

	iface_printk(AGLINK_DBG_MSG, "xr type:%d id:%d\n", hdr->type, hdr->ch);

	ret = aglink_data_rx_queue_put(&hw_priv->queue_rx_data, skb);

	if (ret) {
		kfree_skb(skb);
		iface_printk(AGLINK_DBG_WARN, "queue fail:%d, discard type %d!\n",
				ret, hdr->type);
	}

	aglink_wake_up_data_rx_work(hw_priv);
	return 0;
}

static int aglink_bus_rx_data(int dev_id, u8 *buff, u16 len)
{
	struct aglink_hdr *hdr;
	struct sk_buff *skb = NULL;
	struct aglink_hw *hw_priv;
    int ret = -1;

	CHECK_DEVICE_NUM();

	CHECK_DEVICE_ID(dev_id);

	iface_printk(AGLINK_DBG_TRC, "aglink rx:%d\n", len);

	//data_hex_dump(__func__, 20, buff, len);

	hw_priv = &ag_plat.hw_priv[dev_id];

	skb = __dev_alloc_skb(len, GFP_KERNEL);
	if (!skb) {
		iface_printk(AGLINK_DBG_ERROR, "skb alloc failed");
		return -ENOMEM;
	}
	memcpy(skb->data, buff, len);
	skb_put(skb, len);

	hdr = (struct aglink_hdr *)skb->data;

	hdr->ch = dev_id;

	iface_printk(AGLINK_DBG_TRC, "xr type:%d id:%d\n", hdr->type, hdr->ch);

	ret = aglink_data_rx_queue_put(&hw_priv->queue_rx_data, skb);

	if (ret) {
		kfree_skb(skb);
		iface_printk(AGLINK_DBG_WARN, "queue fail:%d, discard type %d!\n",
				ret, hdr->type);
	}

	aglink_wake_up_data_rx_work(hw_priv);
	return 0;
}


static int aglink_bus_rx_disorder(int dev_id, u8 *buff, u16 len)
{
	int bytes_read;
	int remain;
	int pkt_len;
	struct aglink_hdr *hdr;
	struct aglink_bus *bus;

	CHECK_DEVICE_ID(dev_id);

	if (len > AGLNK_RX_BUFF_LEN) {
		iface_printk(AGLINK_DBG_ERROR, "Data length is too long\n");
		return -1;
	}

	bus = &ag_plat.bus_if[dev_id];

	//data_hex_dump(__func__, 20, buff, len);

	remain = AGLNK_RX_BUFF_LEN - bus->rx_pos;
	bytes_read = len > remain ? remain : len;

	memcpy(bus->rx_buff + bus->rx_pos, buff, bytes_read);

	bus->rx_pos += bytes_read;

re_try:
		//The buffer does not have enough data < TAG_LEN
	if (bus->rx_pos < TAG_LEN) {
		return 0;
	}
		// > TAG_LEN
	if (bus->rx_buff[0] == 'A' && bus->rx_buff[1] == 'W') {

		if (bus->rx_pos >= XR_HDR_SZ) {

			hdr = (struct aglink_hdr *) bus->rx_buff;
			pkt_len = hdr->len + XR_HDR_SZ;

			if (hdr->len > AGLNK_RX_BUFF_LEN) {  /* over AGLNK_RX_BUFF_LEN-->4096, will drop */
				data_hex_dump("EEE", 20, bus->rx_buff, 20);
				bus->rx_pos = 0;
				return 0;
			}

			if (bus->rx_pos >= pkt_len) {

				aglink_bus_rx_data(dev_id, bus->rx_buff, pkt_len);

				if (bus->rx_pos > pkt_len) {

					memmove(bus->rx_buff, bus->rx_buff + pkt_len, bus->rx_pos - pkt_len);

					bus->rx_pos -= pkt_len;

					goto re_try;
				}

				bus->rx_pos = 0;
			}
		}
	} else {
		iface_printk(AGLINK_DBG_ERROR, "tag not found:pos %d, bytes_read:%d, data drop!!!\n",
				bus->rx_pos, bytes_read);
		data_hex_dump("error", 20, bus->rx_buff, 10);
		//memmove(bus->rx_buff, bus->rx_buff + 1, bus->rx_pos - 1);
		bus->rx_pos = 0;
	}
	return 0;
}

int aglink_platform_init(void)
{
	struct aglink_plat *aglink_plat;
	int i;

	iface_printk(AGLINK_DBG_NIY, "aglink_platform_init start\n");

	aglink_plat = &ag_plat;

	for (i = 0; i < AGLINK_DEV_NUM; i++) {
		if (ag_plat.bus_if[i].ops != NULL) {
			iface_printk(AGLINK_DBG_MSG, "device id:%d\n", i);
			ag_plat.bus_if[i].enabled = true;
			ag_plat.bus_if[i].ops->rx_cb = aglink_bus_rx_data;
			ag_plat.bus_if[i].ops->rx_disorder_cb = aglink_bus_rx_disorder;
			ag_plat.bus_if[i].rx_pos = 0;
			ag_plat.hw_priv[i].plat = aglink_plat;
			if (ag_plat.bus_if[i].ops->start)
				ag_plat.bus_if[i].ops->start();
		}
	}

	iface_printk(AGLINK_DBG_ALWY, "aglink_platform_init sucess\n");
	return 0;
}

void aglink_platform_deinit(void)
{
	int i;

	for (i = 0; i < AGLINK_DEV_NUM; i++) {
		if (ag_plat.bus_if[i].ops && ag_plat.bus_if[i].ops->stop) {
			ag_plat.bus_if[i].ops->stop();
		}
	}
	ag_plat.dev_num = -1;
}

struct aglink_hw *aglink_platform_get_hw(int dev_id)
{
	struct aglink_plat *aglink_plat = &ag_plat;

    if (dev_id < 0 || dev_id >= AGLINK_DEV_NUM || ag_plat.dev_num <= 0) {
		iface_printk(AGLINK_DBG_ERROR, "the device ID is invalid:%d\n", dev_id);
		return NULL;
    }

	if (ag_plat.bus_if[dev_id].enabled)
		return &aglink_plat->hw_priv[dev_id];

	return NULL;
}

int aglink_platform_dev_num(void)
{
	return AGLINK_DEV_NUM;
}

int aglink_get_device_id(int dev_id)
{
	int i;

	CHECK_DEVICE_NUM();

	CHECK_DEVICE_ID(dev_id);

	if (ag_plat.bus_if[dev_id].ops)
		return dev_id;

	for (i = 0; i < AGLINK_DEV_NUM; i++) {
		if (ag_plat.bus_if[i].ops != NULL) {
			return i;
		}
	}
	return -1;
}

EXPORT_SYMBOL(aglink_get_device_id);

int aglink_register_dev_ops(struct aglink_bus_ops *ops, int dev_id)
{
	CHECK_DEVICE_ID(dev_id);

	if (ag_plat.bus_if[dev_id].ops == NULL) {
		iface_printk(AGLINK_DBG_ALWY, "probe device id:%d\n", dev_id);
		ag_plat.bus_if[dev_id].ops = ops;
		ag_plat.dev_num++;
	} else {
		iface_printk(AGLINK_DBG_WARN,
				"device ID :%d has been registered\n", dev_id);
	}
	return 0;
}

EXPORT_SYMBOL(aglink_register_dev_ops);
