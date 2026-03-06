/*
 * aglink/up_user.c
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * usrer protocol flow implementation for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>

#include "os_dep/os_intf.h"
#include "aglink.h"
#include "up_user.h"
#include "ag_txrx.h"
#include "debug.h"

struct up_user_priv {
	struct sock *nlsk;
	u32 user_pid;
	wait_queue_head_t ready_wq;
	atomic_t ready_flag;
	u8 enable;
	//xr_mutex_t tx_mutex;
};

#ifdef CONFIG_AGLINK_DEV_NUM
#define UP_USER_NUM CONFIG_AGLINK_DEV_NUM
#else
#error "CONFIG_AGLINK_DEV_NUM is not defined! UP_USER_NUM is NULL"
#endif

extern struct net init_net;

static struct up_user_priv user_priv[UP_USER_NUM];

#define AG_NETLINK_ID 29

#define XR_NETLINK_TYPE_DATA 0
#define XR_NETLINK_TYPE_ACK 1

#define XR_NETLINK_PREFIX_LEN 2
#define XR_NETLINK_PREFIX_DATA "cd"
#define XR_NETLINK_PREFIX_ACK "ak"

#define PAD_TYPE_LEN sizeof(u16)
#define PAD_DATA_LEN sizeof(u16)

#define CHECK_DEVICE_ID(dev_id) \
	do { \
		if (dev_id < 0 || dev_id >= UP_USER_NUM) { \
			aglink_printk(AGLINK_DBG_ERROR, "device id is invalid%d\n", dev_id); \
			return -1; \
		} \
	} while (0)

#define CHECK_UP_USER_PRIV_ID(enable) \
    do { \
		if (!enable) { \
			aglink_printk(AGLINK_DBG_ERROR, "up user is enable%d\n", enable); \
			return -1; \
		} \
	} while (0)

static int aglink_netlink_send_msg(int dev_id, u8 n_type, u8 type, u8 *buff, u32 len)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int total_len;
	int ret = 0;
	int timeout;

	CHECK_UP_USER_PRIV_ID(user_priv[dev_id].enable);

	if (!user_priv[dev_id].nlsk || !user_priv[dev_id].user_pid) {
		// If not ready, wait up to 1 second
		timeout = wait_event_timeout(
			user_priv[dev_id].ready_wq,
			(atomic_read(&user_priv[dev_id].ready_flag) == 1),
			msecs_to_jiffies(1000)
		);

		if (timeout == 0) {
			aglink_printk(AGLINK_DBG_ERROR, "Wait for user ready timeout, dev_id=%d\n", dev_id);
			return -ETIMEDOUT;
		}
	}

	if (!user_priv[dev_id].nlsk || !user_priv[dev_id].user_pid) {
		aglink_printk(AGLINK_DBG_ERROR,
				"usr channel not ready,id%d, pid:%d\n", dev_id, (user_priv[dev_id].user_pid));
		return -EINVAL;
	}

	aglink_printk(AGLINK_DBG_TRC, "user pid,%d\n", (int)user_priv[dev_id].user_pid);

	if (n_type == XR_NETLINK_TYPE_ACK)
		total_len = XR_NETLINK_PREFIX_LEN;
	else
		total_len = XR_NETLINK_PREFIX_LEN +  PAD_TYPE_LEN + PAD_DATA_LEN + len;

	skb = nlmsg_new(total_len, GFP_ATOMIC);
	if (!skb) {
		aglink_printk(AGLINK_DBG_ERROR, "malloc nl msg failed\n");
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb, 0, 0, AG_NETLINK_ID + dev_id, total_len, 0);

	if (n_type == XR_NETLINK_TYPE_DATA) {
		memcpy(NLMSG_DATA(nlh), XR_NETLINK_PREFIX_DATA, XR_NETLINK_PREFIX_LEN);
		((u16 *)NLMSG_DATA(nlh))[1] = type;
		((u16 *)NLMSG_DATA(nlh))[2] = len;
		if (buff)
			memcpy(NLMSG_DATA(nlh) + XR_NETLINK_PREFIX_LEN + PAD_TYPE_LEN + PAD_DATA_LEN, buff, len);
	} else if (n_type == XR_NETLINK_TYPE_ACK) {
		memcpy(NLMSG_DATA(nlh), XR_NETLINK_PREFIX_ACK, XR_NETLINK_PREFIX_LEN);
	}

	//data_hex_dump("xx-user:", 16, (char *)NLMSG_DATA(nlh), total_len);

	ret = netlink_unicast(user_priv[dev_id].nlsk, skb, user_priv[dev_id].user_pid, MSG_DONTWAIT);
	if (ret < 0)
		aglink_printk(AGLINK_DBG_ERROR, "Error while sending back to user\n");

	//aglink_printk(AGLINK_DBG_MSG, "type %d ret %d\n", type, ret);

	return (ret < 0);
}

int aglink_up_user_push(int dev_id, u8 type, u8 *buf, u32 len)
{
	CHECK_DEVICE_ID(dev_id);
	aglink_printk(AGLINK_DBG_TRC, "rx up user len %d\n", len);
	if (dbg_cfg & AGLINK_DBG_TRC)
		data_hex_dump("up-user:", 16, (void *)buf, len > 64 ? 64 : len);

	return aglink_netlink_send_msg(dev_id, XR_NETLINK_TYPE_DATA, type, buf, len);
}

#define USER_TO_KERNEL_DATA_OFFSET (sizeof(u16) * 2)

static struct ag_time m_time = {
	.mode = AG_MODE_PHOTO,
	.timestamp = 1736996400,
	.timeout = 0,
	.width = 0,
	.height = 0,
};

static void aglink_netlink_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	int dev_id = -1;
	int i;
	struct sock *sk;
	u8 type;

	sk = skb->sk;

	for (i = 0; i < UP_USER_NUM; i++) {
	    if (user_priv[i].nlsk == sk) {
			dev_id = i;
			break;
	    }
	}
	if (dev_id == -1) {
		aglink_printk(AGLINK_DBG_ERROR, "Received message from unknown Netlink socket\n");
		return;
	}

	//aglink_k_mutex_lock(&user_priv[dev_id].tx_mutex);
	if (skb->len >= nlmsg_total_size(0)) {
		nlh = nlmsg_hdr(skb);

		aglink_printk(AGLINK_DBG_TRC, "Received message from user,%d\n", (int)nlh->nlmsg_pid);

		user_priv[dev_id].user_pid = nlh->nlmsg_pid;
		if (nlh) {
			aglink_netlink_send_msg(dev_id, XR_NETLINK_TYPE_ACK, 0, NULL, 0);
			// first sync user.
			if (memcmp((char *)NLMSG_DATA(nlh), "AWWA", 4) == 0) {
				aglink_printk(AGLINK_DBG_MSG, "send ad time\n");
				aglink_netlink_send_msg(dev_id, XR_NETLINK_TYPE_DATA,
						AG_AD_TIME, (u8 *)&m_time, sizeof(struct ag_time));
				atomic_set(&user_priv[dev_id].ready_flag, 1);
				wake_up_all(&user_priv[dev_id].ready_wq);
				return ;
			}

			if (memcmp((char *)NLMSG_DATA(nlh), "AWWA", 4) == 0) {
				return ;
			}

			type = (u8)((u16 *)NLMSG_DATA(nlh))[0];

			aglink_tx_data(dev_id, type,
					NLMSG_DATA(nlh) + USER_TO_KERNEL_DATA_OFFSET, ((u16 *)NLMSG_DATA(nlh))[1]);
		}
	}
	//aglink_k_mutex_unlock(&user_priv[dev_id].tx_mutex);
}

struct netlink_kernel_cfg aglink_up_user = {
	.input = aglink_netlink_recv_msg,
};

void aglink_up_user_save_ad_time(struct ag_time *time)
{
	aglink_printk(AGLINK_DBG_MSG, "update AG_AD_TIME param\n");
	memcpy(&m_time, time, sizeof(struct ag_time));
}

int aglink_up_user_init(int dev_id)
{
	CHECK_DEVICE_ID(dev_id);
	memset(&user_priv[dev_id], 0, sizeof(struct up_user_priv));
	user_priv[dev_id].nlsk = (struct sock *)netlink_kernel_create(&init_net,
			AG_NETLINK_ID + dev_id, &aglink_up_user);

	if (!user_priv[dev_id].nlsk)
		return -ENOMEM;

	//aglink_k_mutex_init(&user_priv[dev_id].tx_mutex);
	atomic_set(&user_priv[dev_id].ready_flag, 0);
	init_waitqueue_head(&user_priv[dev_id].ready_wq);

	user_priv[dev_id].enable = true;

	return 0;
}

int aglink_up_user_deinit(int dev_id)
{
	CHECK_DEVICE_ID(dev_id);
	if (user_priv[dev_id].nlsk) {
		netlink_kernel_release(user_priv[dev_id].nlsk);
		user_priv[dev_id].nlsk = NULL;
		user_priv[dev_id].user_pid = 0;
		//aglink_k_mutex_deinit(&user_priv[dev_id].tx_mutex);
	}

	return 0;
}
