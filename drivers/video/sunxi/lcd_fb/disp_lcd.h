// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __DISP_LCD_H__
#define __DISP_LCD_H__

#include "include.h"
#include "lcd_fb_intf.h"
#include "dev_lcd_fb.h"

#define LCD_GPIO_NUM 8
#define LCD_POWER_NUM 4
#define LCD_POWER_STR_LEN 32
#define LCD_GPIO_REGU_NUM 3
#define LCD_GPIO_SCL (LCD_GPIO_NUM - 2)
#define LCD_GPIO_SDA (LCD_GPIO_NUM - 1)

struct sunxi_lcd_fb_disp_lcd_cfg {
	bool lcd_used;

	bool lcd_bl_en_used;
	struct disp_gpio_info lcd_bl_en;
	int lcd_bl_gpio_hdl;
	char lcd_bl_en_power[LCD_POWER_STR_LEN];

	u32 lcd_power_used[LCD_POWER_NUM];
	char lcd_power[LCD_POWER_NUM][LCD_POWER_STR_LEN];

	/* 0: invalid, 1: gpio, 2: regulator */
	u32 lcd_fix_power_used[LCD_POWER_NUM];
	char lcd_fix_power[LCD_POWER_NUM][LCD_POWER_STR_LEN];

	bool lcd_gpio_used[LCD_GPIO_NUM];
	struct disp_gpio_info lcd_gpio[LCD_GPIO_NUM];
	int gpio_hdl[LCD_GPIO_NUM];
	char lcd_gpio_power[LCD_GPIO_REGU_NUM][LCD_POWER_STR_LEN];
	char lcd_pin_power[LCD_GPIO_REGU_NUM][LCD_POWER_STR_LEN];

	struct disp_gpio_info lcd_spi_dc_pin;
	int spi_dc_pin_hdl;

	struct disp_gpio_info lcd_spi_te_pin;
	int spi_te_pin_hdl;

	u32 backlight_bright;
	/*
	 * IEP-drc backlight dimming rate:
	 * 0 -256 (256: no dimming; 0: the most dimming)
	 */
	u32 backlight_dimming;
	u32 backlight_curve_adjust[101];

	u32 lcd_bright;
	u32 lcd_contrast;
	u32 lcd_saturation;
	u32 lcd_hue;
};

/**
 * @brief Initializes the LCD display.
 *
 * This function initializes the LCD display based on the provided configuration.
 *
 * @param p_info Pointer to the LCD framebuffer device structure containing configuration data.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_init_lcd(struct sunxi_lcd_fb_dev_lcd_fb_t *p_info);

/**
 * @brief Exits the LCD display initialization.
 *
 * This function performs necessary cleanup and disables the LCD display.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_exit_lcd(void);

/**
 * @brief Sets the brightness of the LCD display.
 *
 * This function sets the brightness level of the specified LCD display.
 *
 * @param lcd Pointer to the LCD device structure.
 * @param bright Brightness level to set (0-255).
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_set_bright(struct sunxi_lcd_fb_device *lcd, u32 bright);

/**
 * @brief Gets the current brightness of the LCD display.
 *
 * This function retrieves the current brightness level of the specified LCD display.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return Brightness level (0-255).
 */
s32 sunxi_lcd_fb_disp_lcd_get_bright(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Initializes the TE GPIOs for the LCD display.
 *
 * This function initializes the TE GPIO pins required for the LCD display operation.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_te_gpio_init(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Exits the TE GPIO initialization for the LCD display.
 *
 * This function performs cleanup and releases TE GPIO resources.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_te_gpio_exit(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Initializes the GPIOs for the LCD display.
 *
 * This function initializes the GPIO pins required for the LCD display operation.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_gpio_init(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Exits the GPIO initialization for the LCD display.
 *
 * This function performs cleanup and releases GPIO resources.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_gpio_exit(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Sets the direction of a specific GPIO pin for the LCD display.
 *
 * This function sets the direction (input/output) of a GPIO pin used by the LCD display.
 *
 * @param lcd Pointer to the LCD device structure.
 * @param io_index Index of the GPIO pin.
 * @param direction Direction to set (0 for input, 1 for output).
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_gpio_set_direction(struct sunxi_lcd_fb_device *lcd, u32 io_index,
					     u32 direction);

/**
 * @brief Gets the value of a specific GPIO pin for the LCD display.
 *
 * This function retrieves the current value (high/low) of a GPIO pin used by the LCD display.
 *
 * @param lcd Pointer to the LCD device structure.
 * @param io_index Index of the GPIO pin.
 *
 * @return GPIO value (0 or 1).
 */
s32 sunxi_lcd_fb_disp_lcd_gpio_get_value(struct sunxi_lcd_fb_device *lcd, u32 io_index);

/**
 * @brief Sets the value of a specific GPIO pin for the LCD display.
 *
 * This function sets the value (high/low) of a GPIO pin used by the LCD display.
 *
 * @param lcd Pointer to the LCD device structure.
 * @param io_index Index of the GPIO pin.
 * @param data Value to set (0 or 1).
 *
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_lcd_gpio_set_value(struct sunxi_lcd_fb_device *lcd, u32 io_index, u32 data);

/**
 * @brief Checks if the LCD display is enabled.
 *
 * This function checks whether the LCD display is currently enabled.
 *
 * @param lcd Pointer to the LCD device structure.
 *
 * @return 1 if enabled, 0 if disabled.
 */
s32 sunxi_lcd_fb_disp_lcd_is_enabled(struct sunxi_lcd_fb_device *lcd);

/**
 * @brief Retrieves the LCD device structure for a specific display.
 *
 * This function returns the LCD device structure associated with a given display index.
 *
 * @param disp Display index.
 *
 * @return Pointer to the LCD device structure.
 */
struct sunxi_lcd_fb_device *sunxi_lcd_fb_disp_get_lcd(u32 disp);

#endif
