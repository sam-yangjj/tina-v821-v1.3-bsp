/*
 * aglink/app.c
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
#include <linux/types.h>
#include <sunxi-rtc.h>
#include "os_dep/os_intf.h"
#include "ag_txrx.h"
#include <uapi/aglink/ag_proto.h>
#include "up_user.h"
#include "debug.h"
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include "ag_led.h"
#include "ag_take_photo.h"

int dev_id;

static int mode = -1;

ag_led_ctrl_cb led_ctrl_cb;

void aglink_app_sync(void)
{
	struct ag_sync sync = {
		.mode = AG_MODE_IDLE,
		.str = "K_OK",
	};

	dev_id = aglink_get_device_id(dev_id);

	aglink_printk(AGLINK_DBG_ALWY, "get dev id:%d\n", dev_id);

	if (dev_id < 0)
		return ;
	aglink_tx_data(dev_id, AG_VD_SYNC_1, (u8 *)&sync, sizeof(struct ag_sync));
}

void aglink_app_generic_rsp(u8 mode, u8 code)
{
	struct ag_generic_rsp rsp;

	rsp.mode = mode;
	rsp.code = code;

	aglink_tx_data(dev_id, AG_VD_GENERIC_RESPONSE,
			(u8 *)&rsp, sizeof(struct ag_generic_rsp));

	if ((mode == AG_MODE_PHOTO) && code)
		aglink_up_user_push(dev_id, AG_AD_SAVE_SYS_LOG, NULL, 0); //To user
}

static void aglink_app_send_thumb(uint16_t width, uint16_t height)
{

	struct sk_buff *skb = NULL;

	enum TAKE_PHOTO_CODE code;

	code = aglink_take_photo(width, height, 5, &skb);

	switch (code) {
	case TP_SUCCESS:
		if (skb) {
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
			aglink_tx_data(dev_id, AG_VD_THUMB, ((struct wrapper_buff *)skb)->data, ((struct wrapper_buff *)skb)->len);
#else
			aglink_tx_data(dev_id, AG_VD_THUMB, skb->data, skb->len);
#endif
#ifdef CONFIG_AG_SAVE_THUMBNAIL
			aglink_take_photo_cache(skb);
#else
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
			aglink_free_wrapeer_buff(skb, __func__);
#else
			aglink_free_skb(skb, __func__);
#endif
#endif
			aglink_tx_data(dev_id, AG_VD_THUMB_END, NULL, 0);
		}
		break;
	case TP_NO_MEM:
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_NOMEMORY);
		break;
	case TP_NO_READY:
	case TP_TIME_OUT:
	case TP_FAILED:
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_PROCESS);
		break;
	}

	if (mode == AG_MODE_AI)
		aglink_tx_data(dev_id, AG_VD_POWER_OFF, NULL, 0);
}

static void aglink_app_take_photo_handle(void)
{
	struct sk_buff *skb = NULL;
	enum TAKE_PHOTO_CODE code;
	int width, height;
	int channel = 1;

	if (aglink_get_rt_media_jpg_resolution(channel, &width, &height) < 0) {
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_PROCESS);
		return;
	}
	code = aglink_take_photo(width, height, channel, &skb);

	switch (code) {
	case TP_SUCCESS:
		if (skb) {
			aglink_tx_data(dev_id, AG_VD_SG_PHOTO_END, NULL, 0);
			aglink_take_photo_cache(skb);
		}
		break;
	case TP_NO_MEM:
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_NOMEMORY);
		break;
	case TP_NO_READY:
	case TP_TIME_OUT:
	case TP_FAILED:
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_PROCESS);
		break;
	}
}

static void aglink_app_take_photo_save_finish_cb(void)
{
	aglink_printk(AGLINK_DBG_ALWY, "notify user save log\n");
	dev_id = aglink_get_device_id(dev_id);
	aglink_up_user_push(dev_id, AG_AD_SAVE_SYS_LOG, NULL, 0); //To user
}

static void aglink_app_save_ad_time(int dev_id, u8 *buff, u16 len)
{
	struct timespec64 new_time;
	struct tm time_tm;
	struct ag_time *at = (struct ag_time *)buff;

	new_time.tv_sec = at->timestamp;
	new_time.tv_nsec = 0;

	aglink_printk(AGLINK_DBG_ALWY, "time second: %lld\n", new_time.tv_sec);

	mode = at->mode;

	time64_to_tm(new_time.tv_sec, 8 * 3600, &time_tm);

	aglink_printk(AGLINK_DBG_ALWY, "New time: %04ld-%02d-%02d %02d:%02d:%02d\n",
			time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec);

	aglink_printk(AGLINK_DBG_ALWY, "boot mode:%s\n", aglink_mode_to_string(mode));

	if (do_settimeofday64(&new_time)) {
		aglink_printk(AGLINK_DBG_ALWY, "set new time failed\n");
	}

	if (at->timeout) {
		aglink_flow_ctl_timeout_set(dev_id, at->timeout);
	}
	aglink_up_user_save_ad_time(at);
}

int aglink_app_return_mode(void)
{
	u32 val = 0;

	if (mode == -1) {
		sunxi_rtc_get_gpr_val(CONFIG_AG_MODE_RTC_IDX, &val);
		val >>= CONFIG_AG_MODE_RTC_OFS;
		val &= ((1 << CONFIG_AG_MODE_RTC_WIDTH) - 1);
		mode = val;
	}

	aglink_printk(AGLINK_DBG_ALWY, "get mode:%s\n", aglink_mode_to_string(mode));
	return mode;
}

EXPORT_SYMBOL(aglink_app_return_mode);

static void aglink_app_response_test_data(int dev_id, u8 *buff, u16 len)
{
	aglink_tx_data(dev_id, AG_VD_TEST, buff, len);
}

void aglink_app_state_handle(int dev_id, u8 type, u8 *buff, u16 len)
{

	aglink_printk(AGLINK_DBG_ALWY, "type: %s\n", aglink_ad_type_to_string(type));

	switch (type) {
	case AG_AD_TEST:
		aglink_app_response_test_data(dev_id, buff, len);
		break;
	case AG_AD_TIME:

		aglink_app_save_ad_time(dev_id, buff, len);

		if (mode == AG_MODE_PHOTO) {
			aglink_app_take_photo_handle();
			if (((struct ag_time *)buff)->width && ((struct ag_time *)buff)->height)
				aglink_app_send_thumb(((struct ag_time *)buff)->width, ((struct ag_time *)buff)->height);
			else
				aglink_printk(AGLINK_DBG_ALWY, "thume width&height has no set, donot send thumb\n");
		}

		if (mode == AG_MODE_AI) {
			if (((struct ag_time *)buff)->width && ((struct ag_time *)buff)->height)
				aglink_app_send_thumb(((struct ag_time *)buff)->width, ((struct ag_time *)buff)->height);
			else
				aglink_app_send_thumb(640, 360);
		}

		break;
	case AG_AD_TAKE_SG_PHOTO:
			led_ctrl_cb(AG_LED_SHORT_ON);
			aglink_app_take_photo_handle();
		break;
	}
}

static int aglink_app_rx_dispath(int dev_id, u8 type, u8 *buff, u16 len)
{
	int ret = 0;

	if (!buff)
		return -EFAULT;

	if (type >= AG_AD_DIS_LINE || type == AG_AD_TEST ||
			type == AG_AD_TIME || type == AG_AD_TAKE_SG_PHOTO) {

		aglink_app_state_handle(dev_id, type, buff, len); //To kernel
	} else {
		if (led_ctrl_cb && (type == AG_AD_VIDEO_STOP ||
							type == AG_AD_RECORD_AUDIO_STOP ||
							type == AG_AD_RT_MEDIA_STOP)) {
			led_ctrl_cb(AG_LED_OFF);
		} else if (led_ctrl_cb && type == AG_AD_TAKE_SG_PHOTO) {
			led_ctrl_cb(AG_LED_SHORT_ON);
		}
		aglink_up_user_push(dev_id, type, buff, len); //To user
	}
	return ret;
}

static ssize_t driver_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int mode = 0;

	mode = aglink_app_return_mode();
	return snprintf(buf, PAGE_SIZE, "%d\n", mode);
}

static struct kobj_attribute driver_mode_attr =
    __ATTR(aglink_mode, 0444, driver_mode_show, NULL);


static int aglink_modefs_init(void)
{
	int ret = 0;

	ret = sysfs_create_file(kernel_kobj, &driver_mode_attr.attr);
	if (ret) {
		aglink_printk(AGLINK_DBG_ERROR, "Failed to create sysfs file: %d\n", ret);
		return ret;
	}
	return 0;
}

void aglinnk_register_led_ctrl(ag_led_ctrl_cb cb)
{
	led_ctrl_cb = cb;
}
EXPORT_SYMBOL(aglinnk_register_led_ctrl);

void aglinnk_unregister_led_ctrl(void)
{
	led_ctrl_cb = NULL;
}
EXPORT_SYMBOL(aglinnk_unregister_led_ctrl);

static void aglink_modefs_deinit(void)
{
	sysfs_remove_file(kernel_kobj, &driver_mode_attr.attr);
}

int aglink_app_init(void)
{
	int s_mode;

	aglink_register_rx_cb(aglink_app_rx_dispath);
	//aglink_app_sync();

	s_mode = aglink_app_return_mode();

#ifdef CONFIG_AG_SAVE_THUMBNAIL
	if (s_mode == AG_MODE_PHOTO || s_mode == AG_MODE_AI)
#else
	if (s_mode == AG_MODE_PHOTO)
#endif
		aglink_take_photo_save_init(aglink_app_take_photo_save_finish_cb);

	aglink_modefs_init();
	return 0;
}

void aglink_app_deinit(void)
{
	aglink_modefs_deinit();
#ifdef CONFIG_AG_SAVE_THUMBNAIL
	if (mode == AG_MODE_PHOTO || mode == AG_MODE_AI)
#else
	if (mode == AG_MODE_PHOTO)
#endif
		aglink_take_photo_save_deinit();

	aglink_printk(AGLINK_DBG_ALWY, "app deinit\n");
}
