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

#include "lcd_fb_intf.h"
#include "dev_lcd_fb.h"

int sunxi_lcd_fb_register_irq(u32 irq_num, u32 flags, void *handler, void *p_arg, u32 data_size,
			      u32 prio)
{
	LCDFB_INF("%s, irqNo=%d, handler=0x%p, p_arg=0x%p\n", __func__, irq_num, handler, p_arg);
	return request_irq(irq_num, (irq_handler_t)handler, 0x0, "dispaly", p_arg);
}

void sunxi_lcd_fb_unregister_irq(u32 irq_num, void *handler, void *p_arg)
{
	free_irq(irq_num, p_arg);
}

void sunxi_lcd_fb_enable_irq(u32 irq_num)
{
}

void sunxi_lcd_fb_disable_irq(u32 irq_num)
{
}

/* type: 0:invalid, 1: int; 2:str, 3: gpio */
int sunxi_lcd_fb_script_get_item(struct device_node *node, char *sub_name, int value[], int type)
{
	int ret = 0;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(6, 2, 0))
	enum of_gpio_flags flags;
#else
	u32 flags = 0;
#endif

	if (!node) {
		LCDFB_WRN("node is null\n");
		return -EINVAL;
	}

	LCDFB_DBG("try to get item: %s\n", sub_name);

	if (type == 1) {
		if (of_property_read_u32_array(node, sub_name, value, 1))
			LCDFB_INF("of_property_read_u32_array %s fail\n", sub_name);
		else
			ret = type;
	} else if (type == 2) {
		const char *str;

		if (of_property_read_string(node, sub_name, &str))
			LCDFB_INF("of_property_read_string %s fail\n", sub_name);
		else {
			ret = type;
			memcpy((void *)value, str, strlen(str) + 1);
		}
	} else if (type == 3) {
		int gpio;
		struct disp_gpio_info *gpio_info = (struct disp_gpio_info *)value;

		/* gpio is invalid by default */
		gpio_info->gpio = -1;
		gpio_info->name[0] = '\0';
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 2, 0))
		gpio = of_get_named_gpio(node, sub_name, 0);
#else
		gpio = of_get_named_gpio_flags(node, sub_name, 0, &flags);
#endif

		if (!gpio_is_valid(gpio))
			return -EINVAL;

		gpio_info->gpio = gpio;
		memcpy(gpio_info->name, sub_name, strlen(sub_name) + 1);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 2, 0))
		of_property_read_u32_index(node, sub_name, 3, &flags);
		gpio_info->value = (flags == GPIO_ACTIVE_LOW) ? 0 : 1;
#else
		gpio_info->value = (flags == OF_GPIO_ACTIVE_LOW) ? 0 : 1;
#endif
		LCDFB_INF("%s gpio=%d, value:%d\n", sub_name, gpio_info->gpio, gpio_info->value);
	}

	return ret;
}

int sunxi_lcd_fb_get_ic_ver(void)
{
	return 0;
}

int sunxi_lcd_fb_gpio_request(struct disp_gpio_info *gpio_info)
{
	int ret = 0;

	if (!gpio_info) {
		LCDFB_WRN("%s: gpio_info is null\n", __func__);
		return -1;
	}

	/* As some GPIOs are not essential, here return 0 to avoid error */
	if (!strlen(gpio_info->name))
		return 0;

	if (!gpio_is_valid(gpio_info->gpio)) {
		LCDFB_WRN("%s: gpio (%d) is invalid\n", __func__, gpio_info->gpio);
		return -1;
	}
	ret = gpio_direction_output(gpio_info->gpio, gpio_info->value);
	if (ret) {
		LCDFB_WRN("%s failed, gpio_name=%s, gpio=%d, value=%d, ret=%d\n", __func__,
			  gpio_info->name, gpio_info->gpio, gpio_info->value, ret);
		return -1;
	}
	LCDFB_NOTE("%s, gpio_name=%s, gpio=%d, value=%d, ret=%d\n", __func__, gpio_info->name,
		   gpio_info->gpio, gpio_info->value, ret);

	return ret;
}

int sunxi_lcd_fb_gpio_release(struct disp_gpio_info *gpio_info)
{
	if (!gpio_info) {
		LCDFB_WRN("%s: gpio_info is null\n", __func__);
		return -1;
	}
	if (!strlen(gpio_info->name))
		return -1;

	if (!gpio_is_valid(gpio_info->gpio)) {
		LCDFB_WRN("%s: gpio(%d) is invalid\n", __func__, gpio_info->gpio);
		return -1;
	}

	gpio_free(gpio_info->gpio);
	return 0;
}

/* direction: 0:input, 1:output */
int sunxi_lcd_fb_gpio_set_direction(u32 p_handler, u32 direction, const char *gpio_name)
{
	int ret = -1;

	if (p_handler) {
		if (direction) {
			s32 value;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
			value = gpio_get_value(p_handler);
#else
			value = __gpio_get_value(p_handler);
#endif
			ret = gpio_direction_output(p_handler, value);
			if (ret != 0)
				LCDFB_WRN("gpio_direction_output fail!\n");
		} else {
			ret = gpio_direction_input(p_handler);
			if (ret != 0)
				LCDFB_WRN("gpio_direction_input fail!\n");
		}
	} else {
		LCDFB_WRN("OSAL_GPIO_DevSetONEPIN_IO_STATUS, hdl is NULL\n");
		ret = -1;
	}
	return ret;
}

int sunxi_lcd_fb_gpio_get_value(u32 p_handler, const char *gpio_name)
{
	if (p_handler) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
		return gpio_get_value(p_handler);
#else
		return __gpio_get_value(p_handler);
#endif
	}

	LCDFB_WRN("OSAL_GPIO_DevREAD_ONEPIN_DATA, hdl is NULL\n");

	return -1;
}

int sunxi_lcd_fb_gpio_set_value(u32 p_handler, u32 value_to_gpio, const char *gpio_name)
{
	if (p_handler) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
		gpio_set_value(p_handler, value_to_gpio);
#else
		__gpio_set_value(p_handler, value_to_gpio);
#endif
	} else {
		LCDFB_WRN("OSAL_GPIO_DevWRITE_ONEPIN_DATA, hdl is NULL\n");
	}

	return 0;
}

int sunxi_lcd_fb_pin_set_state(struct sunxi_lcd_fb_device *lcd, char *name)
{
	struct pinctrl *pctl;
	struct pinctrl_state *state;
	int ret = -1;

	if (lcd == NULL)
		return -1;

	pctl = pinctrl_get(lcd->dev);
	if (IS_ERR(pctl)) {
		/* not every lcd need pin config */
		LCDFB_INF("pinctrl_get fail\n");
		ret = PTR_ERR(pctl);
		goto exit;
	}

	state = pinctrl_lookup_state(pctl, name);
	if (IS_ERR(state)) {
		LCDFB_WRN("pinctrl_lookup_state fail\n");
		ret = PTR_ERR(state);
		goto exit;
	}

	ret = pinctrl_select_state(pctl, state);
	if (ret < 0) {
		LCDFB_WRN("pinctrl_select_state(%s) fail\n", name);
		goto exit;
	}
	ret = 0;
exit:
	return ret;
}

int sunxi_lcd_fb_power_enable(char *name)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_AW_AXP) || IS_ENABLED(CONFIG_REGULATOR)
	struct regulator *regu = NULL;

#if IS_ENABLED(CONFIG_SUNXI_REGULATOR_DT)
	regu = regulator_get(g_drv_info.device, name);
#else
	regu = regulator_get(NULL, name);
#endif
	if (IS_ERR(regu)) {
		LCDFB_WRN("some error happen, fail to get regulator %s\n", name);
		goto exit;
	}

	/* enalbe regulator */
	ret = regulator_enable(regu);
	if (ret != 0) {
		LCDFB_WRN("some error happen, fail to enable regulator %s!\n", name);
		goto exit1;
	} else {
		LCDFB_INF("suceess to enable regulator %s!\n", name);
	}

exit1:
	/* put regulater, when module exit */
	regulator_put(regu);
exit:
#endif
	return ret;
}

int sunxi_lcd_fb_power_disable(char *name)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_AW_AXP) || IS_ENABLED(CONFIG_REGULATOR)
	struct regulator *regu = NULL;

#if IS_ENABLED(CONFIG_SUNXI_REGULATOR_DT)
	regu = regulator_get(g_drv_info.device, name);
#else
	regu = regulator_get(NULL, name);
#endif
	if (IS_ERR(regu)) {
		LCDFB_WRN("some error happen, fail to get regulator %s\n", name);
		goto exit;
	}
	/* disalbe regulator */
	ret = regulator_disable(regu);
	if (ret != 0) {
		LCDFB_WRN("some error happen, fail to disable regulator %s!\n", name);
		goto exit1;
	} else {
		LCDFB_INF("suceess to disable regulator %s!\n", name);
	}

exit1:
	/* put regulater, when module exit */
	regulator_put(regu);
exit:
#endif
	return ret;
}

s32 sunxi_lcd_fb_disp_delay_ms(u32 ms)
{
	u32 timeout = msecs_to_jiffies(ms);

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(timeout);
	return 0;
}

s32 sunxi_lcd_fb_disp_delay_us(u32 us)
{
	udelay(us);

	return 0;
}

#if IS_ENABLED(CONFIG_PWM_SUNXI) || IS_ENABLED(CONFIG_PWM_SUNXI_NEW) ||                            \
	IS_ENABLED(CONFIG_PWM_SUNXI_GROUP) || IS_ENABLED(CONFIG_AW_PWM)
uintptr_t sunxi_lcd_fb_pwm_request(u32 pwm_id)
{
	uintptr_t ret = 0;

	struct pwm_device *pwm_dev;

	pwm_dev = pwm_request(pwm_id, "lcd");

	if ((pwm_dev == NULL) || IS_ERR(pwm_dev))
		LCDFB_WRN("sunxi_lcd_fb_pwm_request pwm %d fail!\n", pwm_id);
	else
		LCDFB_INF("sunxi_lcd_fb_pwm_request pwm %d success!\n", pwm_id);
	ret = (uintptr_t)pwm_dev;

	return ret;
}

int sunxi_lcd_fb_pwm_free(uintptr_t p_handler)
{
	int ret = 0;
	struct pwm_device *pwm_dev;

	pwm_dev = (struct pwm_device *)p_handler;
	if ((pwm_dev == NULL) || IS_ERR(pwm_dev)) {
		LCDFB_WRN("sunxi_lcd_fb_pwm_free, handle is NULL!\n");
		ret = -1;
	} else {
		pwm_free(pwm_dev);
		LCDFB_INF("sunxi_lcd_fb_pwm_free pwm %d\n", pwm_dev->pwm);
	}

	return ret;
}

int sunxi_lcd_fb_pwm_enable(uintptr_t p_handler)
{
	int ret = 0;
	struct pwm_device *pwm_dev;

	pwm_dev = (struct pwm_device *)p_handler;
	if ((pwm_dev == NULL) || IS_ERR(pwm_dev)) {
		LCDFB_WRN("sunxi_lcd_fb_pwm_enable, handle is NULL!\n");
		ret = -1;
	} else {
		ret = pwm_enable(pwm_dev);
		LCDFB_INF("sunxi_lcd_fb_pwm_enable pwm %d\n", pwm_dev->pwm);
	}

	return ret;
}

int sunxi_lcd_fb_pwm_disable(uintptr_t p_handler)
{
	int ret = 0;
	struct pwm_device *pwm_dev;

	pwm_dev = (struct pwm_device *)p_handler;
	if ((pwm_dev == NULL) || IS_ERR(pwm_dev)) {
		LCDFB_WRN("sunxi_lcd_fb_pwm_disable, handle is NULL!\n");
		ret = -1;
	} else {
		pwm_disable(pwm_dev);
		LCDFB_INF("sunxi_lcd_fb_pwm_disable pwm %d\n", pwm_dev->pwm);
	}

	return ret;
}

int sunxi_lcd_fb_pwm_config(uintptr_t p_handler, int duty_ns, int period_ns)
{
	int ret = 0;
	struct pwm_device *pwm_dev;

	pwm_dev = (struct pwm_device *)p_handler;
	if ((pwm_dev == NULL) || IS_ERR(pwm_dev)) {
		LCDFB_WRN("sunxi_lcd_fb_pwm_config, handle is NULL!\n");
		ret = -1;
	} else {
		ret = pwm_config(pwm_dev, duty_ns, period_ns);
		LCDFB_DBG("sunxi_lcd_fb_pwm_config pwm %d, <%d / %d>\n", pwm_dev->pwm, duty_ns,
			  period_ns);
	}

	return ret;
}

int sunxi_lcd_fb_pwm_set_polarity(uintptr_t p_handler, int polarity)
{
	int ret = 0;
	struct pwm_state state;
	struct pwm_device *pwm_dev;

	pwm_dev = (struct pwm_device *)p_handler;
	if ((pwm_dev == NULL) || IS_ERR(pwm_dev)) {
		LCDFB_WRN("sunxi_lcd_fb_pwm_set_polarity, handle is NULL!\n");
		ret = -1;
	} else {
		memset(&state, 0, sizeof(struct pwm_state));
		pwm_get_state(pwm_dev, &state);
		state.polarity = polarity;
		ret = pwm_apply_state(pwm_dev, &state);
		LCDFB_WRN("sunxi_lcd_fb_pwm_set_polarity pwm %d, active %s\n", pwm_dev->pwm,
			  (polarity == 0) ? "high" : "low");
	}

	return ret;
}
#else
uintptr_t sunxi_lcd_fb_pwm_request(u32 pwm_id)
{
	uintptr_t ret = 0;

	return ret;
}

int sunxi_lcd_fb_pwm_free(uintptr_t p_handler)
{
	int ret = 0;

	return ret;
}

int sunxi_lcd_fb_pwm_enable(uintptr_t p_handler)
{
	int ret = 0;

	return ret;
}

int sunxi_lcd_fb_pwm_disable(uintptr_t p_handler)
{
	int ret = 0;

	return ret;
}

int sunxi_lcd_fb_pwm_config(uintptr_t p_handler, int duty_ns, int period_ns)
{
	int ret = 0;

	return ret;
}

int sunxi_lcd_fb_pwm_set_polarity(uintptr_t p_handler, int polarity)
{
	int ret = 0;

	return ret;
}

#endif
