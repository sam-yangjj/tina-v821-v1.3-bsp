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
#include "include.h"

#ifndef _LCD_FB_INTF_
#define _LCD_FB_INTF_

struct disp_gpio_info {
	unsigned gpio;
	char name[32];
	int value;
};

#define DISP_IRQ_RETURN IRQ_HANDLED
#define DISP_PIN_STATE_ACTIVE "active"
#define DISP_PIN_STATE_SLEEP "sleep"

/**
 * @brief Registers an IRQ handler for the LCD framebuffer.
 *
 * This function registers an interrupt handler for a specified IRQ number. It also
 * configures the interrupt with specified flags, priority, and other relevant parameters.
 *
 * @param irq_num IRQ number to register.
 * @param flags Configuration flags for the interrupt.
 * @param handler Pointer to the IRQ handler function.
 * @param p_arg Argument passed to the handler.
 * @param data_size Size of data associated with the interrupt.
 * @param prio Interrupt priority.
 *
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_register_irq(u32 irq_num, u32 flags, void *handler, void *p_arg, u32 data_size,
			      u32 prio);

/**
 * @brief Unregisters an IRQ handler for the LCD framebuffer.
 *
 * This function unregisters the interrupt handler for a specified IRQ number.
 *
 * @param irq_num IRQ number to unregister.
 * @param handler Pointer to the IRQ handler function.
 * @param p_arg Argument passed to the handler.
 */
void sunxi_lcd_fb_unregister_irq(u32 irq_num, void *handler, void *p_arg);

/**
 * @brief Disables the specified IRQ for the LCD framebuffer.
 *
 * This function disables the interrupt for the specified IRQ number.
 *
 * @param irq_num IRQ number to disable.
 */
void sunxi_lcd_fb_disable_irq(u32 irq_num);

/**
 * @brief Enables the specified IRQ for the LCD framebuffer.
 *
 * This function enables the interrupt for the specified IRQ number.
 *
 * @param irq_num IRQ number to enable.
 */
void sunxi_lcd_fb_enable_irq(u32 irq_num);

/**
 * @brief Retrieves a specified item from the device node's script.
 *
 * This function retrieves an item from the device tree script based on the given
 * sub-name. The item can be an interrupt, string, or GPIO value.
 *
 * @param node Pointer to the device node.
 * @param sub_name Sub-name of the item to retrieve.
 * @param value Array to store the retrieved value(s).
 * @param count Number of values to retrieve.
 * @return 0 if invalid, 1 if interrupt, 2 if string, 3 if GPIO.
 */
int sunxi_lcd_fb_script_get_item(struct device_node *node, char *sub_name, int value[], int count);

/**
 * @brief Retrieves the version of the LCD controller IC.
 *
 * This function returns the version of the IC used in the LCD controller.
 *
 * @return IC version as an integer.
 */
int sunxi_lcd_fb_get_ic_ver(void);

/**
 * @brief Requests a GPIO resource for the LCD framebuffer.
 *
 * This function requests a GPIO resource based on the provided configuration.
 *
 * @param gpio_info Pointer to the `disp_gpio_info` structure containing GPIO information.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_gpio_request(struct disp_gpio_info *gpio_info);

/**
 * @brief Releases a GPIO resource for the LCD framebuffer.
 *
 * This function releases a previously requested GPIO resource.
 *
 * @param gpio_info Pointer to the `disp_gpio_info` structure containing GPIO information.
 */
int sunxi_lcd_fb_gpio_release(struct disp_gpio_info *gpio_info);

/**
 * @brief Sets the direction of a GPIO pin.
 *
 * This function sets the direction of a GPIO pin, either as input or output.
 *
 * @param p_handler GPIO handler.
 * @param direction Direction to set (0 for input, 1 for output).
 * @param gpio_name GPIO pin name.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_gpio_set_direction(u32 p_handler, u32 direction, const char *gpio_name);

/**
 * @brief Gets the value of a GPIO pin.
 *
 * This function retrieves the current value (high or low) of a specified GPIO pin.
 *
 * @param p_handler GPIO handler.
 * @param gpio_name GPIO pin name.
 * @return GPIO value (0 or 1), or a negative value on error.
 */
int sunxi_lcd_fb_gpio_get_value(u32 p_handler, const char *gpio_name);

/**
 * @brief Sets the value of a GPIO pin.
 *
 * This function sets a specified GPIO pin to a high or low value.
 *
 * @param p_handler GPIO handler.
 * @param value_to_gpio Value to set (0 or 1).
 * @param gpio_name GPIO pin name.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_gpio_set_value(u32 p_handler, u32 value_to_gpio, const char *gpio_name);

/**
 * @brief Sets the state of a pin for the LCD framebuffer.
 *
 * This function configures the state of a specified pin for the LCD framebuffer.
 *
 * @param lcd Pointer to the `sunxi_lcd_fb_device` structure for the LCD device.
 * @param name Name of the pin to set the state for.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pin_set_state(struct sunxi_lcd_fb_device *lcd, char *name);

/**
 * @brief Enables power for a specified component.
 *
 * This function enables power for a specified component (e.g., LCD, display).
 *
 * @param name Name of the component to power on.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_power_enable(char *name);

/**
 * @brief Disables power for a specified component.
 *
 * This function disables power for a specified component (e.g., LCD, display).
 *
 * @param name Name of the component to power off.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_power_disable(char *name);

/**
 * @brief Requests a PWM resource for the LCD framebuffer.
 *
 * This function requests a PWM resource identified by the specified `pwm_id`.
 *
 * @param pwm_id PWM identifier.
 * @return Pointer to the PWM resource, or NULL on failure.
 */
uintptr_t sunxi_lcd_fb_pwm_request(u32 pwm_id);

/**
 * @brief Frees a previously requested PWM resource.
 *
 * This function frees a previously requested PWM resource.
 *
 * @param p_handler PWM handler.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pwm_free(uintptr_t p_handler);

/**
 * @brief Enables a PWM resource for the LCD framebuffer.
 *
 * This function enables a previously requested PWM resource.
 *
 * @param p_handler PWM handler.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pwm_enable(uintptr_t p_handler);

/**
 * @brief Disables a PWM resource for the LCD framebuffer.
 *
 * This function disables a previously requested PWM resource.
 *
 * @param p_handler PWM handler.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pwm_disable(uintptr_t p_handler);

/**
 * @brief Configures the PWM resource for the LCD framebuffer.
 *
 * This function configures a previously requested PWM resource with the specified
 * duty cycle and period.
 *
 * @param p_handler PWM handler.
 * @param duty_ns Duty cycle in nanoseconds.
 * @param period_ns Period in nanoseconds.
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pwm_config(uintptr_t p_handler, int duty_ns, int period_ns);

/**
 * @brief Sets the polarity of the PWM signal for the LCD framebuffer.
 *
 * This function sets the polarity of the PWM signal for a specified PWM resource.
 *
 * @param p_handler PWM handler.
 * @param polarity Polarity to set (0 for normal, 1 for inverted).
 * @return 0 on success, negative value on failure.
 */
int sunxi_lcd_fb_pwm_set_polarity(uintptr_t p_handler, int polarity);

/**
 * @brief Delays the LCD framebuffer operation by a specified number of milliseconds.
 *
 * This function introduces a delay in the LCD framebuffer operation for a given number
 * of milliseconds.
 *
 * @param ms Delay duration in milliseconds.
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_delay_ms(u32 ms);

/**
 * @brief Delays the LCD framebuffer operation by a specified number of microseconds.
 *
 * This function introduces a delay in the LCD framebuffer operation for a given number
 * of microseconds.
 *
 * @param us Delay duration in microseconds.
 * @return 0 on success, negative value on failure.
 */
s32 sunxi_lcd_fb_disp_delay_us(u32 us);

#endif
