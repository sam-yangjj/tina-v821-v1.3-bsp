// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs display driver.
 *
 * Copyright (c) 2007-2017 Allwinnertech Co., Ltd.
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

#include "include.h"
#ifndef __DISP_DISPLAY_H__
#define __DISP_DISPLAY_H__

struct sunxi_lcd_fb_disp_dev_t {
	u32 print_level;
	u32 lcd_registered[3];
};

extern struct sunxi_lcd_fb_disp_dev_t g_lcd_fb_disp;

/**
 * @brief Open the LCD functionality
 *
 * This function is used to enable a specified LCD functionality for the given screen.
 *
 * @param screen_id ID of the LCD screen
 * @param func The LCD functionality to enable
 * @param delay Delay time in milliseconds
 */
void LCD_FB_OPEN_FUNC(u32 screen_id, LCD_FUNC func, u32 delay);

/**
 * @brief Close the LCD functionality
 *
 * This function is used to disable a specified LCD functionality for the given screen.
 *
 * @param screen_id ID of the LCD screen
 * @param func The LCD functionality to disable
 * @param delay Delay time in milliseconds
 */
void LCD_FB_CLOSE_FUNC(u32 screen_id, LCD_FUNC func, u32 delay);

/**
 * @brief Get the physical width of the LCD screen
 *
 * This function is used to retrieve the physical width of the specified display.
 *
 * @param disp Display ID
 * @return The physical width of the display
 */
s32 sunxi_lcd_fb_bsp_disp_get_screen_physical_width(u32 disp);

/**
 * @brief Get the physical height of the LCD screen
 *
 * This function is used to retrieve the physical height of the specified display.
 *
 * @param disp Display ID
 * @return The physical height of the display
 */
s32 sunxi_lcd_fb_bsp_disp_get_screen_physical_height(u32 disp);

/**
 * @brief Get the logical width of the LCD screen
 *
 * This function is used to retrieve the logical width of the specified display.
 *
 * @param disp Display ID
 * @return The logical width of the display
 */
s32 sunxi_lcd_fb_bsp_disp_get_screen_width(u32 disp);

/**
 * @brief Get the logical height of the LCD screen
 *
 * This function is used to retrieve the logical height of the specified display.
 *
 * @param disp Display ID
 * @return The logical height of the display
 */
s32 sunxi_lcd_fb_bsp_disp_get_screen_height(u32 disp);

/**
 * @brief Set LCD panel functions
 *
 * This function is used to configure the functions of the specified LCD panel.
 *
 * @param name Name of the panel
 * @param lcd_cfg Configuration of the LCD functions
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_set_panel_funs(char *name,
					     struct sunxi_lcd_fb_disp_lcd_panel_fun *lcd_cfg);

/**
 * @brief Enable LCD backlight
 *
 * This function is used to enable the backlight for the specified display.
 *
 * @param disp Display ID
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_backlight_enable(u32 disp);

/**
 * @brief Disable LCD backlight
 *
 * This function is used to disable the backlight for the specified display.
 *
 * @param disp Display ID
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_backlight_disable(u32 disp);

/**
 * @brief Enable LCD PWM
 *
 * This function is used to enable the PWM for the specified display's backlight.
 *
 * @param disp Display ID
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_pwm_enable(u32 disp);

/**
 * @brief Disable LCD PWM
 *
 * This function is used to disable the PWM for the specified display's backlight.
 *
 * @param disp Display ID
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_pwm_disable(u32 disp);

/**
 * @brief Enable LCD power
 *
 * This function is used to enable the power for the specified LCD display.
 *
 * @param disp Display ID
 * @param power_id ID of the power source
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_power_enable(u32 disp, u32 power_id);

/**
 * @brief Disable LCD power
 *
 * This function is used to disable the power for the specified LCD display.
 *
 * @param disp Display ID
 * @param power_id ID of the power source
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_power_disable(u32 disp, u32 power_id);

/**
 * @brief Set the brightness of the LCD
 *
 * This function is used to adjust the brightness of the specified LCD display.
 *
 * @param disp Display ID
 * @param bright Brightness level (0 to 100)
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_set_bright(u32 disp, u32 bright);

/**
 * @brief Get the brightness of the LCD
 *
 * This function is used to retrieve the current brightness level of the specified LCD display.
 *
 * @param disp Display ID
 * @return The current brightness level
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_get_bright(u32 disp);

/**
 * @brief Configure the LCD pin settings
 *
 * This function is used to configure the pin settings for the specified display.
 *
 * @param disp Display ID
 * @param en Enable or disable the pin configuration
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_pin_cfg(u32 disp, u32 en);

/**
 * @brief Set GPIO value for LCD
 *
 * This function is used to set the GPIO value for the specified LCD display.
 *
 * @param disp Display ID
 * @param io_index GPIO index
 * @param value The value to set (high or low)
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_gpio_set_value(u32 disp, u32 io_index, u32 value);

/**
 * @brief Set GPIO direction for LCD
 *
 * This function is used to set the GPIO direction (input or output) for the specified LCD display.
 *
 * @param disp Display ID
 * @param io_index GPIO index
 * @param direction GPIO direction (input or output)
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_gpio_set_direction(u32 disp, u32 io_index, u32 direction);

/**
 * @brief Get the LCD panel open flow
 *
 * This function is used to get the flow of operations to open the LCD panel.
 *
 * @param disp Display ID
 * @return LCD panel open flow
 */
struct sunxi_lcd_fb_disp_lcd_flow *bsp_disp_lcd_get_open_flow(u32 disp);

/**
 * @brief Get the LCD panel close flow
 *
 * This function is used to get the flow of operations to close the LCD panel.
 *
 * @param disp Display ID
 * @return LCD panel close flow
 */
struct sunxi_lcd_fb_disp_lcd_flow *bsp_disp_lcd_get_close_flow(u32 disp);

/**
 * @brief Get LCD panel information
 *
 * This function is used to retrieve the detailed information about the LCD panel of the specified display.
 *
 * @param disp Display ID
 * @param info LCD panel information structure
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_get_panel_info(u32 disp, struct sunxi_lcd_fb_disp_panel_para *info);

/**
 * @brief Get the display size (width and height)
 *
 * This function is used to get the size (width and height) of the specified display.
 *
 * @param disp Display ID
 * @param width Pointer to store the width of the display
 * @param height Pointer to store the height of the display
 * @return Operation status
 */
int sunxi_lcd_fb_bsp_disp_get_display_size(u32 disp, u32 *width, u32 *height);

/**
 * @brief Allocate DMA memory
 *
 * This function is used to allocate memory for DMA operations.
 *
 * @param num_bytes Number of bytes to allocate
 * @param phys_addr Optional physical address (can be NULL)
 * @return Virtual address of the allocated memory
 */
void *sunxi_lcd_fb_dma_malloc(u32 num_bytes, void *phys_addr);

/**
 * @brief Free DMA memory
 *
 * This function is used to free the DMA memory that was previously allocated.
 *
 * @param virt_addr Virtual address of the allocated memory
 * @param phys_addr Physical address of the allocated memory
 * @param num_bytes Number of bytes to free
 */
void sunxi_lcd_fb_dma_free(void *virt_addr, void *phys_addr, u32 num_bytes);

/**
 * @brief Get the LCD display device
 *
 * This function is used to retrieve the LCD display device for the specified display.
 *
 * @param disp Display ID
 * @return The LCD display device
 */
struct sunxi_lcd_fb_device *sunxi_sunxi_lcd_fb_device_get(int disp);

/**
 * @brief Register the LCD display device
 *
 * This function is used to register an LCD display device.
 *
 * @param dispdev Pointer to the LCD display device
 * @return Operation status
 */
s32 sunxi_sunxi_lcd_fb_device_register(struct sunxi_lcd_fb_device *dispdev);

/**
 * @brief Unregister the LCD display device
 *
 * This function is used to unregister an LCD display device.
 *
 * @param dispdev Pointer to the LCD display device
 * @return Operation status
 */
s32 sunxi_sunxi_lcd_fb_device_unregister(struct sunxi_lcd_fb_device *dispdev);

/**
 * @brief Set the LCD layer
 *
 * This function is used to set the specified LCD layer for the display.
 *
 * @param disp Display ID
 * @param p_info Pointer to the layer information
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_set_layer(u32 disp, struct fb_info *p_info);

/**
 * @brief Control the LCD blanking
 *
 * This function is used to enable or disable the blanking for the specified LCD display.
 *
 * @param disp Display ID
 * @param en Enable or disable the blanking
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_blank(u32 disp, u32 en);

/**
 * @brief Set LCD variable settings
 *
 * This function is used to set the variable settings for the specified LCD display.
 *
 * @param disp Display ID
 * @param p_info Pointer to the display settings
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_set_var(u32 disp, struct fb_info *p_info);

/**
 * @brief Write LCD command
 *
 * This function is used to send a command to the specified LCD screen.
 *
 * @param screen_id Screen ID
 * @param cmd The command to send
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_cmd_write(u32 screen_id, u8 cmd);

/**
 * @brief Write LCD parameter
 *
 * This function is used to send a parameter to the specified LCD screen.
 *
 * @param screen_id Screen ID
 * @param para The parameter to send
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_para_write(u32 screen_id, u8 para);

/**
 * @brief Read LCD command
 *
 * This function is used to read the response to a command from the LCD.
 *
 * @param screen_id Screen ID
 * @param cmd The command to read the response for
 * @param rx_buf Buffer to store the response
 * @param len Length of the response buffer
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_cmd_read(u32 screen_id, u8 cmd, u8 *rx_buf, u8 len);

/**
 * @brief Write QSPI command to LCD
 *
 * This function is used to send a QSPI command to the specified LCD screen.
 *
 * @param screen_id Screen ID
 * @param cmd The QSPI command to send
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_qspi_lcd_cmd_write(u32 screen_id, u8 cmd);

/**
 * @brief Write QSPI parameter to LCD
 *
 * This function is used to send a QSPI parameter to the specified LCD screen.
 *
 * @param screen_id Screen ID
 * @param cmd The QSPI command to send
 * @param para The parameter to send
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_qspi_lcd_para_write(u32 screen_id, u8 cmd, u8 para);

/**
 * @brief Write multiple QSPI parameters to LCD
 *
 * This function is used to send multiple QSPI parameters to the specified LCD screen.
 *
 * @param screen_id Screen ID
 * @param cmd The QSPI command to send
 * @param tx_buf Buffer containing the parameters to send
 * @param len Length of the buffer
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_qspi_lcd_multi_para_write(u32 screen_id, u8 cmd, u8 *tx_buf, u32 len);

/**
 * @brief Read QSPI command from LCD
 *
 * This function is used to read a QSPI command response from the specified LCD.
 *
 * @param screen_id Screen ID
 * @param cmd The QSPI command to read the response for
 * @param rx_buf Buffer to store the response
 * @param len Length of the response buffer
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_qspi_lcd_cmd_read(u32 screen_id, u8 cmd, u8 *rx_buf, u8 len);

/**
 * @brief Wait for LCD VSYNC
 *
 * This function is used to wait for the vertical synchronization (VSYNC) signal from the specified LCD.
 *
 * @param disp Display ID
 * @return Operation status
 */
s32 sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(u32 disp);

#endif
