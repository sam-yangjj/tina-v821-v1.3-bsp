// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * logo/logo.h
 *
 * Copyright (c) 2007-2020 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _LOGO_H
#define _LOGO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "include.h"

struct bmp_pad_header {
	char data[2]; /* pading 2 byte */
	char signature[2];
	u32 file_size;
	u32 reserved;
	u32 data_offset;
	/* InfoHeader */
	u32 size;
	u32 width;
	u32 height;
	u16 planes;
	u16 bit_count;
	u32 compression;
	u32 image_size;
	u32 x_pixels_per_m;
	u32 y_pixels_per_m;
	u32 colors_used;
	u32 colors_important;
} __packed;

struct bmp_header {
	/* Header */
	char signature[2];
	u32 file_size;
	u32 reserved;
	u32 data_offset;
	/* InfoHeader */
	u32 size;
	u32 width;
	u32 height;
	u16 planes;
	u16 bit_count;
	u32 compression;
	u32 image_size;
	u32 x_pixels_per_m;
	u32 y_pixels_per_m;
	u32 colors_used;
	u32 colors_important;
	/* ColorTable */
} __packed;

#if IS_ENABLED(CONFIG_DECOMPRESS_LZMA)
struct lzma_header {
	char signature[4];
	u32 file_size;
	u32 original_file_size;
};
#endif

/**
 * @brief Parses the logo image for the LCD framebuffer.
 *
 * This function parses and processes the logo image data for display on the
 * LCD framebuffer. It initializes necessary resources for displaying the logo.
 *
 * @param info Pointer to the framebuffer information structure.
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_logo_parse(struct fb_info *info);

/**
 * @brief Frees the reserved resources for the LCD logo.
 *
 * This function releases any resources that were reserved during the logo
 * parsing process, ensuring proper cleanup and memory management.
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_logo_free_reserve(void);

#ifdef __cplusplus
}
#endif

#endif /* End of file */
