/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2024 - 2027 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * A RTC Free Running Counter driver for Sunxi Platform of Allwinner SoC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/err.h>
#include <linux/io.h>
#include <sunxi-log.h>
#include "rtc-sunxi.h"

#define RTC_CTRL_REG			(0x0)
#define	FRUN_CNT_L_REG			(0x60)
#define	FRUN_CNT_H_REG			(0x64)

#define FRUN_CNT_H_MASK		(0xffff)
#define RTC_CTRL_LFCLK_SEL_BIT		(2)

extern u32 ccu_sun300iw1_get_extern32k_freq(void);

struct rtc_frun_counter {
	u32 clkfreq_hz;
	void __iomem *base;
};

struct rtc_frun_counter *frun_cnter;

static u64 rtc_sun300i_frun_cnt_read(struct clocksource *cs)
{
	u32 cnt_l;
	u32 cnt_h;
	u64 cycle;

	if (!frun_cnter || !frun_cnter->clkfreq_hz || IS_ERR_OR_NULL(frun_cnter->base))
		return 0;

	if ((readl(frun_cnter->base + RTC_CTRL_REG) & (0x1 << RTC_CTRL_LFCLK_SEL_BIT)) == 0)
		return 0;

	cnt_l = readl(frun_cnter->base + FRUN_CNT_L_REG);
	cnt_h = readl(frun_cnter->base + FRUN_CNT_H_REG) & FRUN_CNT_H_MASK;

	cycle = (((u64)cnt_h) << 32) + (u64)cnt_l;
	return cycle;
}

/* used for timekeeping suspend time calculation */
static struct clocksource rtc_sun300i_frun_cnt = {
	.name		= "rtc_sun300i_frun_cnt",
	.rating		= 10,
	.mask		= CLOCKSOURCE_MASK(48),
	.flags		= (CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP),
	.read		= rtc_sun300i_frun_cnt_read,
};

int rtc_frun_counter_init(struct sunxi_rtc_dev *chip)
{
	int error;
	u32 extern32k_freq;
	u32 rtc_ctrl;

	if (!chip || IS_ERR_OR_NULL(chip->base)) {
		sunxi_err(NULL, "%s(%d): invalid rtc free running counter Base\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* clocksource registers only if there is an extern 32k oscillator */
	rtc_ctrl = readl(chip->base + RTC_CTRL_REG);
	if ((rtc_ctrl & (0x1 << RTC_CTRL_LFCLK_SEL_BIT)) == 0) {
		sunxi_err(NULL, "%s(%d): rtc frun counter src not from extern32k osc\n", __func__, __LINE__);
		return -ENODEV;
	}

	extern32k_freq = ccu_sun300iw1_get_extern32k_freq();
	if (extern32k_freq <= 0) {
		sunxi_err(NULL, "%s(%d): extern32k osc freq not found\n", __func__, __LINE__);
		return -ENODEV;
	}

	frun_cnter = kzalloc(sizeof(*frun_cnter), GFP_KERNEL);
	if (IS_ERR_OR_NULL(frun_cnter)) {
		sunxi_err(NULL, "%s(%d): kzalloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	frun_cnter->base = chip->base;
	frun_cnter->clkfreq_hz = extern32k_freq;

	error = clocksource_register_hz(&rtc_sun300i_frun_cnt, frun_cnter->clkfreq_hz);
	if (error) {
		frun_cnter->base = NULL;
		frun_cnter->clkfreq_hz = 0;
		kfree(frun_cnter);
		sunxi_err(NULL, "%s(%d): register rtc_sun300i_frun_cnt clocksource failed\n", __func__, __LINE__);
	}

	return 0;
}

void rtc_frun_counter_exit(void)
{
	int error;

	error = clocksource_unregister(&rtc_sun300i_frun_cnt);
	if (error)
		sunxi_err(NULL, "%s(%d): unregister rtc_sun300i_frun_cnt clocksource failed\n", __func__, __LINE__);

	frun_cnter->base = NULL;
	frun_cnter->clkfreq_hz = 0;
	kfree(frun_cnter);
}

