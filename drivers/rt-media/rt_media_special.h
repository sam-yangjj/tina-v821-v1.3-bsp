/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2025 - 2028 Allwinner Technology Co.,Ltd. All rights reserved. */
#ifndef _RT_MEDIA_SPECIAL_H_
#define _RT_MEDIA_SPECIAL_H_

#include <uapi/rt-media/uapi_rt_media.h>

int rt_media_catch_jpeg_start_special(int channel, catch_jpeg_config *jpeg_config);
int rt_media_catch_jpeg_stop_special(int channel);
int rt_media_catch_jpeg_get_data_special(int channel, video_stream_s *video_stream, int timeout_ms);
int rt_media_catch_jpeg_return_data_special(int channel, video_stream_s *video_stream);
int rt_media_catch_jpeg_start_next_special(int channel);
int rt_media_catch_jpeg_set_exif(int channel, EXIFInfo *exif);
int rt_media_catch_jpeg_get_exif(int channel, EXIFInfo *exif);

int rt_media_configure_channel_special(int channel, rt_media_config_s *config);
int rt_media_start_channel_special(int channel);
int rt_media_stop_channel_special(int channel);
int rt_media_get_channel_config(int channel, rt_media_config_s *config);
int rt_media_get_preset_config(int channel, encode_config *presetChnConfig);

#endif

