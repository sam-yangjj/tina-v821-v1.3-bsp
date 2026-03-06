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
#include <generated/uapi/linux/version.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <sunxi-sid.h>

#include "aglink.h"
#include "debug.h"
#include "hwio.h"
#include "os_dep/os_intf.h"
#include "txrx.h"
#include "ag_version.h"
#include "up_user.h"

typedef int (*xr_net_data_tx_t)(void *priv, struct sk_buff *skb);

int aglink_all_queue_init(struct aglink_hw *aglink_hw)
{
	int ret = 0;

	aglink_printk(AGLINK_DBG_MSG, "rx_data queue init\n");
	ret = aglink_queue_init(&aglink_hw->queue_rx_data,
			XR_DATA_RX_QUEUE_SZ, "rx_data", aglink_free_skb);
	if (ret)
		goto err1;
	aglink_printk(AGLINK_DBG_MSG, "tx_data queue init\n");

	ret = aglink_queue_init(&aglink_hw->queue_tx_data,
			XR_DATA_TX_QUEUE_SZ, "tx_data", aglink_free_skb);
	if (ret)
		goto err2;
	return ret;
err2:
	aglink_queue_deinit(&aglink_hw->queue_rx_data);
err1:
	return ret;
}

void aglink_all_queue_deinit(struct aglink_hw *aglink_hw)
{
	aglink_queue_deinit(&aglink_hw->queue_tx_data);
	aglink_queue_deinit(&aglink_hw->queue_rx_data);
}

extern void loop_dev_init(void);
extern void uart_dev_init(void);
extern void uart_kernel_dev_init(void);
extern int aglink_app_init(void);
extern void aglink_app_deinit(void);

static int aglink_core_init(void)
{
	int ret = 0;

	struct aglink_hw *aglink_hw;

	int dev_num;

	int i;

	aglink_printk(AGLINK_DBG_ALWY, "aglink core init\n");
	ret = aglink_platform_init();
	if (ret) {
		aglink_printk(AGLINK_DBG_ERROR, "aglink platform init fail %d\n", ret);
		return -1;
	}

	dev_num = aglink_platform_dev_num();

	for (i = 0; i < dev_num; i++) {

		aglink_hw = aglink_platform_get_hw(i);

		if (NULL == aglink_hw)
			continue;

		aglink_printk(AGLINK_DBG_ALWY, "deviceID %d, debug interface init\n", i);
		aglink_debug_init_common(aglink_hw, i);

		aglink_printk(AGLINK_DBG_ALWY, "deviceID %d, aglink queue init\n", i);
		aglink_all_queue_init(aglink_hw);

		aglink_printk(AGLINK_DBG_ALWY, "deviceID:%d, register tx and rx task\n", i);

		aglink_txrx_init(aglink_hw, i);
		aglink_printk(AGLINK_DBG_ALWY, "deviceID:%d, int sucessful.\n", i);

		aglink_up_user_init(i);
		aglink_printk(AGLINK_DBG_ALWY, "deviceID:%d, up user int sucessful.\n", i);
	}
	return 0;
}

static int aglink_device_init(void)
{
#ifdef CONFIG_AG_LOOPBACK
	loop_dev_init();
#endif
#ifdef CONFIG_AG_UART
	uart_dev_init();
#endif
#ifdef CONFIG_AG_UART_KERNEL
	uart_kernel_dev_init();
#endif
	return 0;
}

static void aglink_core_deinit(void)
{
	int dev_num;
	int i;
	struct aglink_hw *aglink_hw;

	dev_num = aglink_platform_dev_num();

	for (i = 0; i < dev_num; i++) {
		aglink_hw = aglink_platform_get_hw(i);

		if (NULL == aglink_hw)
			continue;

		aglink_up_user_deinit(i);
		aglink_printk(AGLINK_DBG_ALWY, "deviceID %d, up user deinit\n", i);

		aglink_printk(AGLINK_DBG_ALWY, "deviceID %d, tx rx deinit \n", i);
		aglink_txrx_deinit(aglink_hw);

		aglink_printk(AGLINK_DBG_ALWY, "deviceID %d, aglink queue deinit\n", i);
		aglink_all_queue_deinit(aglink_hw);

		aglink_debug_deinit_common(aglink_hw, i);
	}
	aglink_printk(AGLINK_DBG_ALWY, "aglink platform deinit\n");
	aglink_platform_deinit();

	aglink_printk(AGLINK_DBG_ALWY, "aglink core deinit over\n");
}

static int __init aglink_mod_init(void)
{
	aglink_printk(AGLINK_DBG_ALWY, "aglink driver version:%s\n", AGLINK_DRIVER_VERSION);
	aglink_printk(AGLINK_DBG_ALWY, "aglink protocol version:%s\n", AGLINK_VERSION);

	if (aglink_device_init())
		return -1;
	if (aglink_core_init())
		return -1;

	if (aglink_app_init())
		goto end;

	return 0;
end:
	aglink_core_deinit();
	return -1;
}

static void __exit aglink_mod_exit(void)
{
	aglink_app_deinit();
	aglink_core_deinit();
}

//module_init(aglink_mod_init);
arch_initcall(aglink_mod_init);
module_exit(aglink_mod_exit);

MODULE_AUTHOR("Allwinner");
MODULE_DESCRIPTION("Allwinner AGLINK driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("aglink");
