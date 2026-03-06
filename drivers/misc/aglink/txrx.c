/*
 * aglink/aglink_txrxif.c
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * tx and rx interface implementation for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/version.h>

#include "os_dep/os_intf.h"
#include "aglink.h"
#include "hwio.h"
#include "queue.h"
#include "debug.h"
#include "checksum.h"
#include "txrx.h"
#include <uapi/aglink/ag_proto.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include "uapi/linux/sched/types.h"
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#include "linux/sched/types.h"
#else
#include "linux/sched/rt.h"
#endif
#include "ag_txrx.h"

#define txrx_dump(pref, width, buf, len)     //data_hex_dump(pref, width, buf, len)
#define txrx_err_dump(pref, width, buf, len) data_hex_dump(pref, width, buf, len)
#define THREAD_SHOULD_STOP                   aglink_k_thread_should_stop
#define XR_TX_DATA_RETRY_MAX_CNT             1000
#define XR_TX_CMD_EVTENT_TO_MS               (15 * HZ)

u16 txparse_flags;
u16 rxparse_flags;

static u8 aglink_data_tx_thread_prio;
static u8 aglink_rx_com_thread_prio = 1;

#define AGLINK_FLOW_CTL_TIMEOUT_MAX 200

#define CHECK_HW(hw) \
	do { \
		if (!hw) { \
			txrx_printk(AGLINK_DBG_ERROR, "aglink hw is null\n"); \
			return -1; \
		} \
	} while (0)

void aglink_flow_ctl_timeout_set(int dev_id, u8 time)
{
	struct aglink_hw *hw = aglink_platform_get_hw(dev_id);
	hw->tx_flow_ctl_timeout = time > AGLINK_FLOW_CTL_TIMEOUT_MAX ?
								AGLINK_FLOW_CTL_TIMEOUT_MAX :
								time;
	hw->tx_flow_ctl_timeout *= 5;
}

void aglink_wake_up_data_tx_work(struct aglink_hw *hw)
{
	txrx_printk(AGLINK_DBG_TRC, "aglink_wake_up_data_tx_work\n");
	wake_up_interruptible(&hw->tx_wq);
}

void aglink_wake_up_data_tx_flow_ctl_work(struct aglink_hw *hw)
{
	if (hw->tx_flow_ctl_timeout == 0)
		return;
	txrx_printk(AGLINK_DBG_TRC, "aglink_wake_up_data_tx_flow_ctl_work\n");
	aglink_k_atomic_set(&hw->tx_data_flow_ctl, 1);
	wake_up_interruptible(&hw->tx_flow_ctl_wq);
}

void aglink_wake_up_data_rx_work(struct aglink_hw *aglink_hw)
{
	txrx_printk(AGLINK_DBG_TRC, "aglink_wake_up_data_rx_work\n");
	wake_up_interruptible(&aglink_hw->rx_wq);
}

int aglink_tx_ack(struct aglink_hw *aglink_hw, void *data, u16 data_len)
{
	//TODO
	return 0;
}

static ag_rx_cb rx_cb;

void aglink_register_rx_cb(ag_rx_cb cb)
{
	rx_cb = cb;
}

EXPORT_SYMBOL(aglink_register_rx_cb);

const char *aglink_vd_type_to_string(uint8_t type)
{

	switch (type) {
	case AG_VD_TEST:
		return "AG_VD_TEST";
	case AG_VD_SYNC_1:
		return "AG_VD_SYNC_1";
	case AG_VD_SYNC_2:
		return "AG_VD_SYNC_2";
	case AG_VD_THUMB:
		return "AG_VD_THUMB";
	case AG_VD_THUMB_END:
		return "AG_VD_THUMB_END";
	case AG_VD_POWER_OFF:
		return "AG_VD_POWER_OFF";
	case AG_VD_AP_SUCCESS:
		return "AG_VD_AP_SUCCESS";
	case AG_VD_VIDEO_START:
		return "AG_VD_VIDEO_START";
	case AG_VD_VIDEO_END:
		return "AG_VD_VIDEO_END";
	case AG_VD_STA_SUCCESS:
		return "AG_VD_STA_SUCCESS";
	case AG_VD_STA_FAILED:
		return "AG_VD_STA_FAILED";
	case AG_VD_OTA_SUCCESS:
		return "AG_VD_OTA_SUCCESS";
	case AG_VD_OTA_PG_BAR:
		return "AG_VD_OTA_PG_BAR";
	case AG_VD_OTA_FAILED:
		return "AG_VD_OTA_FAILED";
	case AG_VD_SDP:
		return "AG_VD_SDP";
	case AG_VD_RECORD_AUDIO_START:
		return "AG_VD_RECORD_AUDIO_START";
	case AG_VD_SG_PHOTO_END:
		return "AG_VD_SG_PHOTO_END";
	case AG_VD_SYS_VERSION:
		return "AG_VD_SYS_VERSION";
	case AG_VD_CHIP_TEMP:
		return "AG_VD_CHIP_TEMP";
	case AG_VD_STROGE_CAPACITY:
		return "AG_VD_STROGE_CAPACITY";
	case AG_VD_P2P_SUCCESS:
		return "AG_VD_P2P_SUCCESS";
	case AG_VD_GENERIC_RESPONSE:
		return "AG_VD_GENERIC_RESPONSE";
	case AG_VD_MEDIA_NUM:
		return "AG_VD_MEDIA_NUM";
	case AG_VD_AOV_START:
		return "AG_VD_AOV_START";
	}
	return "Unkown";
}


const char *aglink_ad_type_to_string(uint8_t type)
{

	switch (type) {
	case AG_AD_TEST:
		return "AG_AD_TEST";
	case AG_AD_TIME:
		return "AG_AD_TIME";
	case AG_AD_AP:
		return "AG_AD_AP";
	case AG_AD_STA:
		return "AG_AD_STA";
	case AG_AD_AUDIO:
		return "AG_AD_AUDIO";
	case AG_AD_OTA_START:
		return "AG_AD_OTA_START";
	case AG_AD_VIDEO_STOP:
		return "AG_AD_VIDEO_STOP";
	case AG_AD_DIS_LINE:
		return "AG_AD_DIS_LINE";
	case AG_AD_PAUSE:
		return "AG_AD_PAUSE";
	case AG_AD_RESUME:
		return "AG_AD_RESUME";
	case AG_AD_RECORD_AUDIO_STOP:
		return "AG_AD_RECORD_AUDIO_STOP";
	case AG_AD_DELETE_MEDIA:
		return "AG_AD_DELETE_MEDIA";
	case AG_AD_TAKE_SG_PHOTO:
		return "AG_AD_TAKE_SG_PHOTO";
	case AG_AD_GET_SYS_VERSION:
		return "AW_AD_GET_SYS_VERSION";
	case AG_AD_GET_CHIP_TEMP:
		return "AW_AD_GET_CHIP_TEMP";
	case AG_AD_GET_STROGE_CAPACITY:
		return "AG_AD_GET_STROGE_CAPACITY";
	case AG_AD_P2P:
		return "AG_AD_P2P";
	case AG_AD_DOWNLOAD_STOP:
		return "AG_AD_DOWNLOAD_STOP";
	case AG_AD_GET_MEDIA_NUM:
		return "AG_AD_GET_MEDIA_NUM";
	case AG_AD_SAVE_SYS_LOG:
		return "AG_AD_SAVE_SYS_LOG";
	case AG_AD_AUDIO_NAME:
		return "AG_AD_AUDIO_NAME";
	case AG_AD_RCVD_COMP:
		return "AG_AD_RCVD_COMP";
	case AG_AD_AOV_STOP:
		return "AG_AD_AOV_STOP";
	}
	return "Unkown";
}

const char *aglink_mode_to_string(uint8_t mode)
{
	switch (mode) {
	case AG_MODE_PHOTO:
		return "PHOTO";
	case AG_MODE_VIDEO:
		return "VIDEO";
	case AG_MODE_DOWNLOAD:
		return "DWONLAOD";
	case AG_MODE_OTA:
		return "OTA";
	case AG_MODE_AI:
		return "AI";
	case AG_MODE_NORMAL:
		return "NORMAL";
	case AG_MODE_RECORD_AUDIO:
		return "RECORD_AUDIO";
	case AG_MODE_IDLE:
		return "IDLE";
	case AG_MODE_LIVESTREAM:
		return "LIVESTREAM";
	case AG_MODE_AOV:
		return "AG_MODE_AOV";
	}
	return "Unkown";
}

static int aglink_tx_data_handle(int dev_id, u8 type,
		u8 *buff, u16 len, u8 frag)
{
	struct sk_buff *skb = NULL;
	struct aglink_hdr *hdr = NULL;
	int ret = -1;
	struct aglink_hw *aglink_hw = aglink_platform_get_hw(dev_id);
	int tx_len;

	CHECK_HW(aglink_hw);

	tx_len = len <= AG_LINK_TX_MAX_LEN ? len : AG_LINK_TX_MAX_LEN;

	//data_hex_dump(__func__, 20, buff, len);
	if (!aglink_hw->txrx_enable) {
		txrx_printk(AGLINK_DBG_ERROR, "txrx thread not ready.\n");
		return -1;
	}

	//if (txparse_flags)
	//	aglink_parse_frame(skb->data, 1, txparse_flags, skb->len);

	skb = aglink_alloc_skb(tx_len + XR_HDR_SZ, __func__);
	if (!skb) {
		txrx_printk(AGLINK_DBG_ERROR, "tx data failed to allocate skb\n");
		return -1;
	}

	hdr = (struct aglink_hdr *)skb->data;

	memcpy(hdr, TAG, TAG_LEN);

	hdr->len = tx_len;
	hdr->type = type;
	hdr->ch = dev_id;
	hdr->seq = aglink_hw->data_tx_seq;
	aglink_hw->data_tx_seq++;

	txrx_printk(AGLINK_DBG_TRC, "type:%2.2X, seq number:%d, len:%d\n",
			hdr->type, hdr->seq, tx_len);

	//skb_reserve(skb, XR_HDR_SZ);
	if (buff)
		memcpy(skb->data + XR_HDR_SZ, buff, tx_len);

	skb_put(skb, tx_len + XR_HDR_SZ);

#ifdef AGLINK_HDR_CKSUM
	hdr->checksum = aglink_k_cpu_to_le16(aglink_crc_16((u8 *)hdr + TAG_LEN + CKSUM_LEN, tx_len + 6));
#endif

	// data_hex_dump(__func__, 20, skb->data, skb->len);
	ret = aglink_data_tx_queue_put(&aglink_hw->queue_tx_data, skb);
	if (ret) {
		if (ret > 0) {
			kfree_skb(skb);
			txrx_printk(AGLINK_DBG_NIY,
					"tx data queue push fail, drop data:%x seq:%d!\n", (u32)skb, hdr->seq);
		}
		if (!aglink_k_atomic_read(&aglink_hw->tx_data_pause_master)) {
			aglink_k_atomic_set(&aglink_hw->tx_data_pause_master, 1);
			txrx_printk(AGLINK_DBG_MSG, "tx data queue will full, tx data pause:%d\n",
						aglink_queue_get_queued_num(&aglink_hw->queue_tx_data));
		}
	}
	if (ret <= 0) {
		aglink_wake_up_data_tx_work(aglink_hw);
	}

	return tx_len;
}

int aglink_tx_data(int dev_id, u8 type, u8 *buff, u16 len)
{
	int pos = 0;
	int remain = len;
	int tx_len;

	txrx_printk(AGLINK_DBG_MSG, "type(TX) ----> : %s, len:%d\n",
			aglink_vd_type_to_string(type), len);

	do {
		//TODO: Packet segmentation processing for protocol.
		tx_len = aglink_tx_data_handle(dev_id, type, buff + pos, remain, 0);

		if (tx_len <= 0)
			return tx_len;

		pos += tx_len;

		remain -= tx_len;
	} while (remain > 0);
	return 0;
}

int aglink_rx_process(struct aglink_hw *hw, struct sk_buff *skb)
{
	struct aglink_hdr *hdr = NULL;
	//struct sk_buff *rsp_skb = NULL;
	u8 type_id = 0;
#ifdef AGLINK_HDR_CKSUM
	u16 checksum = 0, c_checksum = 0;
#endif

	if (!hw || !skb) {
		txrx_printk(AGLINK_DBG_ERROR, "err! aglink_hw:%p skb:%p\n", hw, skb);
		if (skb)
			aglink_free_skb(skb, __func__);
		return -EINVAL;
	}
	hdr = (struct aglink_hdr *)skb->data;

#ifdef AGLINK_HDR_CKSUM
	checksum = le16_to_cpu(hdr->checksum);
	c_checksum = aglink_crc_16((u8 *)hdr + TAG_LEN + CKSUM_LEN, hdr->len + 6);
	if (checksum != c_checksum) {
		txrx_printk(AGLINK_DBG_ERROR, "checksum failed S:%x M:%x type:%d seq:%d\n",
					checksum, c_checksum, hdr->type, hdr->seq);
		txrx_err_dump("rx_err:", 20, skb->data, skb->len);
		aglink_free_skb(skb, __func__);
		return 1;
	}
#endif

	type_id = hdr->type;

	skb_pull(skb, XR_HDR_SZ);
	if (type_id != AG_AD_AUDIO)
		txrx_printk(AGLINK_DBG_MSG, "type(RX) <---- : %s, len:%d\n",
				aglink_ad_type_to_string(type_id), skb->len);

	if (type_id == AG_AD_RCVD_COMP) {
		aglink_wake_up_data_tx_flow_ctl_work(hw);
		goto end;
	}

	if (rx_cb) {
		rx_cb(hdr->ch, type_id, skb->data, skb->len);
	}

	if (type_id == AG_AD_PAUSE) {
		if (!aglink_k_atomic_read(&hw->tx_data_pause_slave)) {
			aglink_k_atomic_set(&hw->tx_data_pause_slave, 1);
			txrx_printk(AGLINK_DBG_MSG, "config dev tx pause\n");
		}
		goto end;
	} else if (type_id == AG_AD_RESUME) {
		if (aglink_k_atomic_read(&hw->tx_data_pause_slave)) {
			aglink_k_atomic_set(&hw->tx_data_pause_slave, 0);
			aglink_wake_up_data_tx_work(hw);
			txrx_printk(AGLINK_DBG_MSG, "config dev tx resume\n");
		}
		goto end;
	}

//	ret = aglink_rx_dispatch(hw, skb, type_id, &rsp_skb);
//	if (rsp_skb) {
//		aglink_tx_ack(hw, rsp_skb->data, rsp_skb->len);
//		aglink_free_skb(rsp_skb, __func__);
//	} else {
//		aglink_tx_ack(hw, &ret, sizeof(int));
//	}
end:
	aglink_free_skb(skb, __func__);

	return 0;
}

static void aglink_check_tx_resume(struct aglink_hw *hw)
{
	if (aglink_queue_get_queued_num(&hw->queue_tx_data) <
			(hw->queue_tx_data.capacity * 2 / 5)) {
		if (aglink_k_atomic_read(&hw->tx_data_pause_master)) {
			aglink_k_atomic_set(&hw->tx_data_pause_master, 0);
			txrx_printk(AGLINK_DBG_MSG, "tx data resume:%d, q:%d\n",
					aglink_k_atomic_read(&hw->tx_data_pause_master),
					aglink_queue_get_queued_num(&hw->queue_tx_data));
			;//TODO
			//aglink_net_tx_resume(hw);
		}
	}
}


static __inline__ bool aglink_check_tx_retry(int status)
{
	if ((status == XR_TXD_ST_NO_MEM) || (status == XR_TXD_ST_NO_QUEUE) ||
		(status == XR_TXD_ST_BUS_TX_FAIL))
		return true;
	else
		return false;
}

static int aglink_tx_process(struct aglink_hw *hw, struct aglink_queue_t *queue)
{
	int tx_status = -1;
	int sleep_time = 1;
	bool txq_empty = false;
	struct sk_buff *tx_skb = NULL;
	u32 len;
	txq_empty = aglink_queue_is_empty(queue);
	while (!txq_empty && !aglink_k_atomic_read(&hw->tx_data_pause_slave)) {
		int max_retry = XR_TX_DATA_RETRY_MAX_CNT, retry = 0;
		enum aglink_txbus_op op = XR_TXBUS_OP_AUTO;

		sleep_time = 1;
		tx_skb = aglink_queue_get(queue);
		txq_empty = aglink_queue_is_empty(queue);
		tx_status = 0;
		len = tx_skb->len;
		if (txq_empty)
			op = XR_TXBUS_OP_FORCE_TX;

		while (tx_skb && max_retry-- && !THREAD_SHOULD_STOP(&hw->tx_thread)) {
			txrx_printk(AGLINK_DBG_TRC, "data tx_skb len:%d\n", tx_skb->len);

			tx_status = aglink_platform_tx_data(tx_skb);
			if (tx_status == 0) {
				aglink_free_skb(tx_skb, __func__);
				aglink_check_tx_resume(hw);
				tx_skb = NULL;
				break;
			} else if (!aglink_check_tx_retry(tx_status)) {
				break;
			} else if (max_retry) {
				op = XR_TXBUS_OP_TX_RETRY;
				//AGLINK_DBG_NIY
				txrx_printk(AGLINK_DBG_MSG, "ret:%d data tx sleep:%dms retryed:%d\n",
						tx_status, sleep_time, retry);
				msleep(sleep_time);
				retry++;
				if (retry > 1)
					sleep_time = 2;
				else if (retry > 5)
					sleep_time = 3;
				else if (retry > 100)
					sleep_time = 4;
				sleep_time = max(sleep_time, 10);
			}
		}
		if (tx_status) {
			txrx_printk(AGLINK_DBG_ERROR, "txdata err:%d retryed:%d\n", tx_status, retry);
			op = XR_TXBUS_OP_RESET_BUF;
			tx_status = aglink_platform_tx_data(tx_skb);
			aglink_free_skb(tx_skb, __func__);
			aglink_check_tx_resume(hw);
			break;
		}

		if (hw->tx_flow_ctl_timeout &&
			(len >= (AG_LINK_TX_MAX_LEN + XR_HDR_SZ)) &&
			!txq_empty) {
			//txrx_printk(AGLINK_DBG_ALWY, "wait flow ctl\n");
			wait_event_interruptible_timeout(hw->tx_flow_ctl_wq,
									aglink_k_atomic_read(&hw->tx_data_flow_ctl) ||
									THREAD_SHOULD_STOP(&hw->tx_thread),
									msecs_to_jiffies(hw->tx_flow_ctl_timeout));
			aglink_k_atomic_set(&hw->tx_data_flow_ctl, 0);
		}

		if (THREAD_SHOULD_STOP(&hw->tx_thread))
			break;
	}
	return 0;
}
static int aglink_tx_thread(void *data)
{
	struct aglink_hw *hw = (struct aglink_hw *)data;
	int tx = 0, term = 0;
	int status = 0;
#if 0
	int wait_dev_time = 1, wait_dev_cnt;
#endif
	CHECK_HW(hw);

	if (aglink_data_tx_thread_prio > 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
		sched_set_fifo_low(current);
#else
		struct sched_param param;
		param.sched_priority = (aglink_data_tx_thread_prio < MAX_RT_PRIO) ? aglink_data_tx_thread_prio : (MAX_RT_PRIO - 1);
		sched_setscheduler(current, SCHED_FIFO, &param);
#endif
	}

	while (1) {
		txrx_printk(AGLINK_DBG_TRC, "aglink data tx thread running\n");
		status = wait_event_interruptible(hw->tx_wq,
											({tx = (!aglink_queue_is_empty(&hw->queue_tx_data) &&
												!aglink_k_atomic_read(&hw->tx_data_pause_slave));
												term = THREAD_SHOULD_STOP(&hw->tx_thread);
												(tx || term);
											}));
		if (term) {
			txrx_printk(AGLINK_DBG_ERROR, "aglink data tx thread exit!\n");
			break;
		}

		txrx_printk(AGLINK_DBG_TRC, "status:%d tx: %d\n", status, tx);
		aglink_tx_process(hw, &hw->queue_tx_data);
		msleep(1);
	}
	aglink_k_thread_exit(&hw->tx_thread);
	return 0;
}


static int aglink_rx_pre_process(struct aglink_hw *hw, struct aglink_queue_t *queue)
{
	struct sk_buff *rx_skb = NULL;
	int rx_status = 0;
	if (!aglink_queue_is_empty(queue)) {

		rx_skb = aglink_queue_get(queue);

		if (rx_skb) {
			rx_status = aglink_rx_process(hw, rx_skb);
			if (rx_status) {
				txrx_printk(AGLINK_DBG_ERROR, "hwio exception, rx_status=%d\n", rx_status);
			}
		} else {
			txrx_printk(AGLINK_DBG_ERROR, "Asynchronous issue, queue %s empty:%d queued:%d\n",
						hw->queue_rx_data.name,
						aglink_queue_is_empty(&hw->queue_rx_data),
						aglink_queue_get_queued_num(&hw->queue_rx_data));
			msleep(1);
			return -1;
		}
	}
	return 0;
}
static int aglink_rx_thread(void *data)
{
	struct aglink_hw *hw = (struct aglink_hw *)data;
	int rx, term = 0;
	int status = 0;

	CHECK_HW(hw);
	if (aglink_rx_com_thread_prio > 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
		sched_set_fifo_low(current);
#else
		struct sched_param param;
		param.sched_priority = (aglink_rx_com_thread_prio < MAX_RT_PRIO) ? aglink_rx_com_thread_prio : (MAX_RT_PRIO - 1);
		sched_setscheduler(current, SCHED_FIFO, &param);
#endif
	}

	while (1) {
		status = wait_event_interruptible(hw->rx_wq,
											({rx = !aglink_queue_is_empty(&hw->queue_rx_data);
												term = THREAD_SHOULD_STOP(&hw->rx_thread);
												(rx || term);
											}));
		if (term) {
			txrx_printk(AGLINK_DBG_ALWY, "aglink rx com thread exit!\n");
			break;
		}

		txrx_printk(AGLINK_DBG_TRC, "rx com thread:status:%d rx:%d\n", status, rx);

		aglink_rx_pre_process(hw, &hw->queue_rx_data);
	}
	aglink_k_thread_exit(&hw->rx_thread);
	return 0;
}


int aglink_txrx_init(struct aglink_hw *hw, int dev_id)
{
	int ret = 0;

	char thread_name[16];

	aglink_k_atomic_set(&hw->tx_data_pause_master, 0);
	aglink_k_atomic_set(&hw->tx_data_pause_slave, 0);
	aglink_k_atomic_set(&hw->tx_data_flow_ctl, 0);

	init_waitqueue_head(&hw->tx_wq);
	init_waitqueue_head(&hw->tx_flow_ctl_wq);
	init_waitqueue_head(&hw->rx_wq);

	hw->tx_flow_ctl_timeout = 0;

	snprintf(thread_name, sizeof(thread_name), "ag_tx_%d", dev_id);

	if (aglink_k_thread_create(&hw->tx_thread, thread_name,
				aglink_tx_thread, (void *)hw, 0, 4096)) {
		txrx_printk(AGLINK_DBG_ERROR, "create aglink tx thread failed\n");
		return -1;
	}

	snprintf(thread_name, sizeof(thread_name), "ag_rx_%d", dev_id);
	if (aglink_k_thread_create(&hw->rx_thread, thread_name,
				aglink_rx_thread, (void *)hw, 0, 4096)) {
		txrx_printk(AGLINK_DBG_ERROR, "create xr_data_rx thread failed\n");
		ret = -1;
		goto err0;
	}

	hw->txrx_enable = 1;
	txrx_printk(AGLINK_DBG_ALWY, "aglink tx rx init sucessful.\n");
	return ret;

err0:
	wake_up(&hw->tx_flow_ctl_wq);
	wake_up(&hw->tx_wq);
	aglink_k_thread_delete(&hw->tx_thread);
	hw->tx_thread.handle = NULL;
	return ret;
}

void aglink_txrx_deinit(struct aglink_hw *hw)
{
	hw->txrx_enable = 0;

	if (!IS_ERR_OR_NULL(&hw->rx_thread)) {
		wake_up(&hw->rx_wq);
		aglink_k_thread_delete(&hw->rx_thread);
		hw->rx_thread.handle = NULL;
	}

	if (!IS_ERR_OR_NULL(&hw->tx_thread)) {
		wake_up(&hw->tx_wq);
		aglink_k_thread_delete(&hw->tx_thread);
		hw->tx_thread.handle = NULL;
	}
	txrx_printk(AGLINK_DBG_ALWY, "aglink tx rx deinit sucessful.\n");
}
