/*
 * aglink/take_photo.c
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
#include <linux/time.h>
#include <linux/path.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/string.h>
#include "os_dep/os_intf.h"
#include "queue.h"
#include "debug.h"
#include "ag_take_photo.h"

extern void aglink_app_generic_rsp(u8 mode, u8 code);
static struct aglink_queue_t queue_photo_data;
static wait_queue_head_t photo_wq;
static xr_thread_handle_t photo_thread;
static photo_save_cb photo_save_finish;
static int take_photo_save_enable;

#define SAVE_FILE_PATH "/mnt/UDISK"
#define SAVE_FILE_LIST "/mnt/UDISK/media.config"
#define SAVE_FILE_LIST_SIZE 1024

static int kern_unlink(const char *pathname)
{
	int error;
	struct path parent;
	struct dentry *dentry;

	dentry = kern_path_create(AT_FDCWD, pathname, &parent, 0);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	error = vfs_unlink(d_inode(parent.dentry), dentry, NULL);
	done_path_create(&parent, dentry);

	return error;
}

static int is_mounted_writable(const char *path)
{
	struct path dir_path;
	int ret = 0;

	if (kern_path(path, LOOKUP_FOLLOW, &dir_path) == 0) {
		ret = !(dir_path.mnt->mnt_flags & MNT_READONLY);
		path_put(&dir_path);
	}
	return ret;
}

static int has_enough_space(const char *path)
{
	struct kstatfs stat;
	struct path dir_path;
	unsigned long long free_space_byte;
	int ret = 0;

	ret = kern_path(path, LOOKUP_FOLLOW, &dir_path);
	if (ret) {
		photo_printk(AGLINK_DBG_ERROR, "Get %s path failed!\n", path);
		return AG_UDISK_FAILED;
	}

	ret = vfs_statfs(&dir_path, &stat);
	if (ret < 0) {
		photo_printk(AGLINK_DBG_ERROR, "Get %s free space failed:%d\n", path, ret);
		ret = AG_UDISK_FAILED;
		goto out;
	}

	free_space_byte = stat.f_bavail * stat.f_bsize;
	photo_printk(AGLINK_DBG_ALWY, "udisk free space:%llu\n", free_space_byte);

	if (free_space_byte > MIN_FREE_SPACE)
		ret = AG_UDISK_ENOUGH;
	else
		ret = AG_UDISK_NOT_ENOUGH;

out:
	path_put(&dir_path);
	return ret;
}

static void get_timestamp_string(char *buf, size_t len)
{
    struct timespec64 ts;
    struct tm tm_time;
    time64_t seconds;

    ktime_get_real_ts64(&ts);
    seconds = ts.tv_sec;

    time64_to_tm(seconds, 8 * 3600, &tm_time);

    snprintf(buf, len, "%04ld%02d%02d%02d%02d%02d%03ld",
			tm_time.tm_year + 1900,
			tm_time.tm_mon + 1,
			tm_time.tm_mday,
			tm_time.tm_hour,
			tm_time.tm_min,
			tm_time.tm_sec,
			ts.tv_nsec / 1000000);
}

static int append_filename(const char *photo_name)
{
    struct file *filp;
    loff_t pos;
    char buf[256];
    mm_segment_t oldfs;
    int ret = 0;
	int len;

    int flags = O_WRONLY | O_APPEND | O_CREAT | O_SYNC;

    umode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(SAVE_FILE_LIST, flags, mode);
    if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		photo_printk(AGLINK_DBG_ERROR, "File open error: %d\n", ret);
		goto out;
	}

	len = snprintf(buf, sizeof(buf), "%s\n", photo_name);

	pos = filp->f_pos;
	ret = kernel_write(filp, buf, len, &pos);
	if (ret != len) {
		photo_printk(AGLINK_DBG_ERROR, "Write failed: %d\n", ret);
		ret = (ret < 0) ? ret : -EIO;
	}

	vfs_fsync(filp, 1);

	filp_close(filp, NULL);

out:
	set_fs(oldfs);
	return ret;
}

static int save_photo_to_disk(void *data, size_t size)
{
    struct file *fp;
    loff_t pos = 0;
    int ret = 0;
	char time_buf[32];
	char filename[64];

	get_timestamp_string(time_buf, sizeof(time_buf));

	snprintf(filename, sizeof(filename), "%s/%s.jpg", SAVE_FILE_PATH, time_buf);

	fp = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
	if (IS_ERR(fp)) {
		ret = PTR_ERR(fp);
		photo_printk(AGLINK_DBG_ERROR, "Open file failed: %d\n", ret);
		return ret;
	}

	ret = kernel_write(fp, data, size, &pos);
	if (ret < 0) {
		photo_printk(AGLINK_DBG_ERROR, "kernel_write failed: %d\n", ret);
		goto write_failed;
	}

	if (ret != size) {
		photo_printk(AGLINK_DBG_ERROR, "Write incomplete: %d/%zu\n", ret, size);
		ret = -ENOSPC;
		goto write_failed;
	}

	if (vfs_fsync(fp, 0)) {
		photo_printk(AGLINK_DBG_ERROR, "Fsync failed\n");
	}

	photo_printk(AGLINK_DBG_ALWY, "save %s success\n", filename);

	snprintf(filename, sizeof(filename), "%s.jpg", time_buf);

	if (append_filename(filename) < 0) {
		photo_printk(AGLINK_DBG_ERROR, "append filename %s failed\n", filename);
	}

write_failed:
	filp_close(fp, NULL);
	if (ret == -ENOSPC) {
		if (kern_unlink(filename) == 0) {
			photo_printk(AGLINK_DBG_ERROR, "File %s deleted due to disk full\n", filename);
		} else {
			photo_printk(AGLINK_DBG_ERROR, "Failed to delete file %s\n", filename);
		}
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_DISK_FULL);
	} else {
		if (photo_save_finish)
			photo_save_finish();
	}

	return ret;
}

static void aglink_wake_up_save_work(void)
{
	wake_up_interruptible(&photo_wq);
}

#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO

struct sk_buff *app_get_wrapper_buff(u32 len, const char *func)
{
	struct wrapper_buff *buff = NULL;

	buff = (struct wrapper_buff *)kmalloc(sizeof (struct wrapper_buff), GFP_KERNEL);
	if (!buff)
		return NULL;

	memset(buff, 0, sizeof(struct wrapper_buff));
	buff->data = vmalloc(len);
	if (!buff->data) {
		kfree(buff);
		return NULL;
	}
	buff->len = len;

	return (struct sk_buff *)buff;
}

void aglink_free_wrapeer_buff(struct sk_buff *skb, const char *func)
{
	struct wrapper_buff *buff;

	buff = (struct wrapper_buff *)skb;
	if (buff->data)
		vfree(buff->data);

	kfree(buff);
}
#endif

static int aglink_photo_data_handle(void)
{
	struct sk_buff *tx_skb = NULL;
	bool txq_empty = false;
	int ret = 0;

	txq_empty = aglink_queue_is_empty(&queue_photo_data);

	if (!txq_empty)
		tx_skb = aglink_queue_get(&queue_photo_data);
	else
		return -1;

	if (has_enough_space(SAVE_FILE_PATH) == AG_UDISK_NOT_ENOUGH) {
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_DISK_FULL);
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
		aglink_free_wrapeer_buff(tx_skb, __func__);
#else
		aglink_free_skb(tx_skb, __func__);
#endif
		return -1;
	}

#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
	ret = save_photo_to_disk(((struct wrapper_buff *)tx_skb)->data,
			((struct wrapper_buff *)tx_skb)->len);
	if (ret == -ENOSPC)
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_DISK_FULL);
	aglink_free_wrapeer_buff(tx_skb, __func__);
#else
	ret = save_photo_to_disk(tx_skb->data, tx_skb->len);
	if (ret == -ENOSPC)
		aglink_app_generic_rsp(AG_MODE_PHOTO, AG_FAILED_DISK_FULL);
	aglink_free_skb(tx_skb, __func__);
#endif

	return 0;
}

#define MOUNT_WAIT_MAX_RETRIES   500
#define MOUNT_WAIT_MAX_DELAY_MS  80
#define MOUNT_WAIT_DECREASE_STEP 20

static int aglink_photo_save_thread(void *data)
{
	int rx, term = 0;
	int status = 0;
	int retries = 0;
	int delay_time;

	msleep(500);

	do {
		if (is_mounted_writable(SAVE_FILE_PATH)) {
			photo_printk(AGLINK_DBG_ALWY,
					"UDISK is mounted and writable\n");
			break;
		}

		delay_time = MOUNT_WAIT_MAX_DELAY_MS - retries * MOUNT_WAIT_DECREASE_STEP;

		if (delay_time < 10)
			delay_time = 10;

		photo_printk(AGLINK_DBG_WARN,
				"waiting mount %s, retrying... (%d/%d), delay:%dms\n",
				SAVE_FILE_PATH, retries, MOUNT_WAIT_MAX_RETRIES, delay_time);

		retries++;

		msleep(delay_time);

	} while (retries < MOUNT_WAIT_MAX_RETRIES);

	while (1) {
		status = wait_event_interruptible(photo_wq,
					({rx = !aglink_queue_is_empty(&queue_photo_data);
						term = aglink_k_thread_should_stop(&photo_thread);
						(rx || term);
					}));
		if (term) {
			photo_printk(AGLINK_DBG_ALWY, "aglink photo saving thread exit!\n");
			break;
		}

		photo_printk(AGLINK_DBG_TRC, "photo saving thread, status:%d rx:%d\n", status, rx);

		aglink_photo_data_handle();

		msleep(1);
	}

	aglink_k_thread_exit(&photo_thread);

	return 0;
}

#include <uapi/rt-media/uapi_rt_media.h>

extern int rt_media_catch_jpeg_get_data_special(int channel,
		video_stream_s *video_stream, int timeout_ms);

extern int rt_media_catch_jpeg_start_special(int channel,
		catch_jpeg_config *jpeg_config);

extern int rt_media_catch_jpeg_stop_special(int channel);

extern int rt_media_catch_jpeg_return_data_special(int channel,
		video_stream_s *video_stream);

extern int rt_media_get_preset_config(int channel, encode_config *presetChnConfig);

extern int vin_set_isp_save_ae_special(int id);

int aglink_take_photo_cache(struct sk_buff *skb)
{
	int ret;

	ret = aglink_data_tx_queue_put(&queue_photo_data, skb);
	if (ret) {
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
		aglink_free_wrapeer_buff(skb, __func__);
#else
		kfree_skb(skb);
#endif
		return -1;
	}

	aglink_wake_up_save_work();

	return 0;
}

#define TAKE_PHOTO_MAX_RETRIES 500
#define TAKE_PHOTO_MAX_DELAY_MS 50
#define TAKE_PHOTO_DECREASE_STEP 5

static int save_ae;

int aglink_take_photo(int width,
		int height, int channel, struct sk_buff **skb)
{
    struct sk_buff *in_skb = NULL;
	int ret;
    uint8_t *buff;
	int retries = 0;
	int delay_time;

    video_stream_s *video_stream = kmalloc(sizeof(video_stream_s), GFP_KERNEL);
    catch_jpeg_config *config = kmalloc(sizeof(catch_jpeg_config), GFP_KERNEL);

	photo_printk(AGLINK_DBG_ALWY,
			"width:%d, height:%d, channel:%d\n",  width, height, channel);

    if (!video_stream || !config) {
		photo_printk(AGLINK_DBG_ERROR, "catch jpeg failed\n");
		return TP_NO_MEM;
    }

	memset(config, 0, sizeof(catch_jpeg_config));
	memset(video_stream, 0, sizeof(video_stream_s));

	config->channel_id = channel;
	config->width = width;
	config->height = height;
	config->qp = 80;

	if (width * height > 6000000) {
		config->b_enable_sharp = 0;
		msleep(700);
	} else {
		config->b_enable_sharp = 1;
	}

	do {
		ret = rt_media_catch_jpeg_start_special(channel, config);
		if (!ret)
			break;

		delay_time = TAKE_PHOTO_MAX_DELAY_MS - retries * TAKE_PHOTO_DECREASE_STEP;

		if (delay_time < 5)
			delay_time = 5;

		photo_printk(AGLINK_DBG_WARN,
				"catch jpeg start failed, retrying... (%d/%d),delay:%dms\n",
				retries, TAKE_PHOTO_MAX_RETRIES, delay_time);

		retries++;

		msleep(delay_time);
	} while (retries < TAKE_PHOTO_MAX_RETRIES);

    if (ret) {
		photo_printk(AGLINK_DBG_ERROR, "catch jpeg start failed\n");
		kfree(video_stream);
		kfree(config);
		return TP_NO_READY;
    }

	photo_printk(AGLINK_DBG_ALWY, "catch jpeg start\n");

	ret = rt_media_catch_jpeg_get_data_special(channel, video_stream, 3000);
	if (ret) {
		photo_printk(AGLINK_DBG_ERROR, "catch jpeg get data timeout\n");
		rt_media_catch_jpeg_stop_special(channel);
		kfree(video_stream);
		kfree(config);
		return TP_TIME_OUT;
	}

    photo_printk(AGLINK_DBG_ALWY,
		"VIDEO, id:%d, pts: %llu, size:%u-%u-%u,key:%d, num:%d\n",
		video_stream->id, video_stream->pts,
		video_stream->size0, video_stream->size1, video_stream->size2,
		video_stream->keyframe_flag, video_stream->nDataNum);
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
	in_skb = app_get_wrapper_buff(video_stream->size0 +
			video_stream->size1 + video_stream->size2, __func__);
#else
    in_skb = aglink_alloc_skb(video_stream->size0 + \
			video_stream->size1 + video_stream->size2, __func__);
#endif
    if (!in_skb) {
		photo_printk(AGLINK_DBG_ERROR, "photo malloc failed\n");
		kfree(video_stream);
		kfree(config);
		return TP_NO_MEM;
    }
#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
	buff = ((struct wrapper_buff *)in_skb)->data;
#else
	buff = in_skb->data;
#endif

    if (video_stream->size0 && video_stream->data0)
		memcpy(buff, video_stream->data0, video_stream->size0);

    if (video_stream->size1 && video_stream->data1)
		memcpy(buff + video_stream->size0,
				video_stream->data1, video_stream->size1);

    if (video_stream->size2 && video_stream->data2)
		memcpy(buff + video_stream->size0 + video_stream->size1,
				video_stream->data2, video_stream->size2);
#ifndef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
    skb_put(in_skb, video_stream->size0 + video_stream->size1 + video_stream->size2);
#endif
	*skb = in_skb;

    rt_media_catch_jpeg_return_data_special(channel, video_stream);

    rt_media_catch_jpeg_stop_special(channel);

	if (!save_ae) {
		photo_printk(AGLINK_DBG_ALWY, "save ae info\n");
		vin_set_isp_save_ae_special(channel);
		save_ae = 1;
	}

	photo_printk(AGLINK_DBG_ALWY, "catch jpeg finish\n");

	kfree(video_stream);
	kfree(config);

    return TP_SUCCESS;
}

int aglink_take_photo_save_init(photo_save_cb cb)
{
	int ret = 0;

#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
	ret = aglink_queue_init(&queue_photo_data,
			XR_DATA_RX_QUEUE_SZ, "ph_data", aglink_free_wrapeer_buff);
#else
	ret = aglink_queue_init(&queue_photo_data,
			XR_DATA_RX_QUEUE_SZ, "ph_data", aglink_free_skb);
#endif
	if (ret)
		return -1;

	init_waitqueue_head(&photo_wq);

	if (aglink_k_thread_create(&photo_thread, "photo_save",
				aglink_photo_save_thread, NULL, 0, 4096)) {
		photo_printk(AGLINK_DBG_ERROR, "create aglink tx thread failed\n");
		goto err0;
	}

	take_photo_save_enable = 1;

	photo_save_finish = cb;

	return 0;
err0:
	aglink_queue_deinit(&queue_photo_data);
	return -1;
}

void aglink_take_photo_save_deinit(void)
{
	if (!take_photo_save_enable)
		return ;

	if (!IS_ERR_OR_NULL(&photo_thread)) {
		wake_up(&photo_wq);
		aglink_k_thread_delete(&photo_thread);
	}

	aglink_queue_deinit(&queue_photo_data);
}

int aglink_get_rt_media_jpg_resolution(int channel, int *width, int *height)
{
	int ret;
	encode_config preset_config;

	ret = rt_media_get_preset_config(channel, &preset_config);
	if (ret < 0) {
		photo_printk(AGLINK_DBG_ERROR, "get resolution failed!\n");
		return ret;
	}

	*width = preset_config.jpg_w;
	*height = preset_config.jpg_h;

	return ret;
}
