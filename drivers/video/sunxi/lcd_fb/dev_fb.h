// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
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
#ifndef _DEV_FB_H
#define _DEV_FB_H

#include "dev_lcd_fb.h"

/**
 * @brief Initializes the LCD framebuffer.
 *
 * This function initializes the LCD framebuffer with the given configuration
 * provided in the structure `sunxi_lcd_fb_dev_lcd_fb_t`. It sets up necessary
 * resources and prepares the framebuffer for use.
 *
 * @param p_info Pointer to a structure containing the LCD framebuffer device information.
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_init(struct sunxi_lcd_fb_dev_lcd_fb_t *p_info);

/**
 * @brief Exits the LCD framebuffer.
 *
 * This function releases any resources allocated for the framebuffer and
 * performs any necessary cleanup before shutting down the framebuffer.
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_exit(void);

/**
 * @brief Turns the screen frame flush.
 *
 * This function makes the entire LCD display black. but not modify fb memory
 *
 * @param sel Selection parameter (may correspond to a specific screen or display).
 */
void sunxi_lcd_fb_sync_frame(u32 sel);

/**
 * @brief Turns the screen frame flush but for frame buffer init
 *
 * This function makes the entire LCD display black. but not modify fb memory
 *
 * @param sel Selection parameter (may correspond to a specific screen or display).
 */
void sunxi_lcd_fb_black_screen(u32 sel);

/**
 * @brief Draws a black screen.
 *
 * This function draws a black screen on the LCD, which could be used
 * to reset or clear the screen contents.
 *
 * @param sel Selection parameter (may correspond to a specific screen or display).
 */
void sunxi_lcd_fb_draw_black_screen(u32 sel);

/**
 * @brief Draws a color bar on the LCD.
 *
 * This function draws a standard color bar pattern on the LCD to test the display.
 *
 * @param sel Selection parameter (may correspond to a specific screen or display).
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_lcd_draw_colorbar(u32 sel);

/**
 * @brief Draws a black white flip on the LCD.
 *
 * This function draws a standard  black white flip on the LCD to test the display.
 *
 * @param sel Selection parameter (may correspond to a specific screen or display).
 *
 * @return 0 on success, negative value on failure.
 */
void sunxi_lcd_fb_draw_test_screen(u32 sel);

#endif /* End of file */
