/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 *
 * Copyright (c) 2012 Allwinner.
 * 2012-05-01 Written by sunny (sunny@allwinnertech.com).
 * 2012-10-01 Written by superm (superm@allwinnertech.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <sunxi-log.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <asm/sbi.h>
#include <asm/io.h>
#include <linux/mailbox_client.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_AW_MTD)
#include <linux/mtd/mtd.h>
#include <boot_param.h>
#endif
#include <linux/delay.h>

static u32 time_to_wakeup_ms;
static u32 use_ultra_standby;
static u32 set_ldo_onoff;
static u32 aov_standby_timer_ms;
static u32 set_wdg_period;

struct ae350_standby_debug_dev {
	struct device *dev;
	struct mbox_chan *chan;
	struct mbox_client client;
};
static struct mbox_chan *standby_dbg_chan;

enum pm_msg_type {
	PM_POST_SUSPEND_MSG     = 0x80,
	PM_SUSPEND_PREPARE_MSG  = 0x81,
	PM_FLASH_MSG            = 0x82,
	PM_ENABLE_AOV_TIMER_MSG = 0x83,
};

#define CPUX_WUPIO_IRQ                          (171U)
#define CPUX_WUPTIMER_IRQ                       (166U)
#define CPUX_ALARM0_IRQ                         (167U)
#define CPUX_ALARM1_IRQ                         (168U)
#define CPUX_WLAN_IRQ                           (160U)

typedef struct {
	uint32_t wakeup_irq;
	char *wakeup_reason;
} irq_map_t;

const irq_map_t cpu_irq_map[] = {
	{CPUX_WUPIO_IRQ,     "wakeup io"},
	{CPUX_WUPTIMER_IRQ,  "wakeup timer"},
	{CPUX_ALARM0_IRQ,    "alarm0"},
	{CPUX_ALARM1_IRQ,    "alarm1"},
	{CPUX_WLAN_IRQ,      "wlan"},
};

int send_remote_pm_wuptmr_aov_en(u32 en);

#define AE350_STANDBY_DBG_MBOX_NAME	"ae350-notify"
#define REMOTE_PM_SUSPEND_PREPARE	(0xdbdb0011)
#define REMOTE_PM_POST_SUSPEND		(0xdbdb0021)

#define PMU_NO_INTERRUPT	-1
#define SET_WAKEUP_TIME_MS(ms)  ((3 << 30) | (ms))

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t time_to_wakeup_ms_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t time_to_wakeup_ms_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", time_to_wakeup_ms);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t time_to_wakeup_ms_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t time_to_wakeup_ms_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	u32 value = 0;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		sunxi_err(NULL, "%s,%d err, invalid para!\n", __func__, __LINE__);
		return -EINVAL;
	}

	time_to_wakeup_ms = value;

	sbi_andes_set_wakeup_source(SET_WAKEUP_TIME_MS(time_to_wakeup_ms), 1);

	sunxi_info(NULL, "time_to_wakeup_ms change to %d\n", time_to_wakeup_ms);

	return count;
}
static CLASS_ATTR_RW(time_to_wakeup_ms);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t wakeup_reason_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t wakeup_reason_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;
	u32 i, wakeup_irq = 0;
	char *wakeup_reason;

	wakeup_irq = sbi_andes_get_wakeup_source();
	for (i = 0; i < ARRAY_SIZE(cpu_irq_map); i++) {
		if (cpu_irq_map[i].wakeup_irq == wakeup_irq) {
			wakeup_reason = cpu_irq_map[i].wakeup_reason;
			break;
		}
	}

	if (i == ARRAY_SIZE(cpu_irq_map)) {
		sunxi_info(NULL, "No wakeup reason because system hasn't suspended \n");
		return 0;
	}

	size = sprintf(buf, "wakeup_irq:%d, wakeup_reason:%s\n", wakeup_irq, wakeup_reason);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t wakeup_reason_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t wakeup_reason_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	return count;
}
static CLASS_ATTR_RW(wakeup_reason);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t standby_ldo_onoff_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t standby_ldo_onoff_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;
	size = sprintf(buf, "%u\n", set_ldo_onoff);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t standby_ldo_onoff_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t standby_ldo_onoff_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	u32 value = 0;
	u32 ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		sunxi_err(NULL, "%s,%d err, invalid para!\n", __func__, __LINE__);
		return -EINVAL;
	}

	set_ldo_onoff = value;

	ret = sbi_andes_set_ldo_onoff(set_ldo_onoff);
	return count;
}
static CLASS_ATTR_RW(standby_ldo_onoff);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t use_ultra_standby_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t use_ultra_standby_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", use_ultra_standby);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t use_ultra_standby_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t use_ultra_standby_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	u32 value = 0;
	u32 ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		sunxi_err(NULL, "%s,%d err, invalid para!\n", __func__, __LINE__);
		return -EINVAL;
	}

	use_ultra_standby = !!value;

	ret = sbi_andes_set_ultra_standby(use_ultra_standby);
	return count;
}
static CLASS_ATTR_RW(use_ultra_standby);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t aov_standby_timer_ms_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t aov_standby_timer_ms_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;

	size = sprintf(buf, "Last setting: %u, Need to set before each suspend\n", aov_standby_timer_ms);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t aov_standby_timer_ms_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t aov_standby_timer_ms_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	int ret;
	u32 value = 0;

	if (standby_dbg_chan == NULL) {
		sunxi_err(NULL, "%s, %d not support\n",  __func__, __LINE__);
		return -EFAULT;
	}

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		sunxi_err(NULL, "%s,%d err, invalid para!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = send_remote_pm_wuptmr_aov_en(value);
	if (ret < 0) {
		sunxi_err(NULL, "%s, %d PM_ENABLE_AOV_TIMER_MSG notify failed\n",  __func__, __LINE__);
		return -EFAULT;
	}

	aov_standby_timer_ms = value;
	sbi_andes_set_wakeup_source(SET_WAKEUP_TIME_MS(aov_standby_timer_ms), 1);

	return count;
}
static CLASS_ATTR_RW(aov_standby_timer_ms);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t set_wdg_period_show(struct class *class, struct class_attribute *attr,
		char *buf)
#else
static ssize_t set_wdg_period_show(const struct class *class, const struct class_attribute *attr,
		char *buf)
#endif
{
	ssize_t size = 0;
	size = sprintf(buf, "%u\n", set_wdg_period);

	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t set_wdg_period_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
#else
static ssize_t set_wdg_period_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
#endif
{
	u32 value = 0;
	u32 ret = 0;

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		sunxi_err(NULL, "%s,%d err, invalid para!\n", __func__, __LINE__);
		return -EINVAL;
	}

	set_wdg_period = value;

	ret = sbi_andes_set_wdg_period(set_wdg_period);
	return count;
}
static CLASS_ATTR_RW(set_wdg_period);

static struct attribute *ae350_standby_class_attrs[] = {
	&class_attr_time_to_wakeup_ms.attr,
	&class_attr_wakeup_reason.attr,
	&class_attr_standby_ldo_onoff.attr,
	&class_attr_use_ultra_standby.attr,
	&class_attr_aov_standby_timer_ms.attr,
	&class_attr_set_wdg_period.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ae350_standby_class);

#define CHAN_RECV_BUFF_SIZE                     (20)
#define CHAN_RECV_ACK_TIMEOUT                   (500)
#define ARISC_MESSAGE_INITIALIZED               (0x02)
#define ARISC_MESSAGE_ATTR_SOFTSYN              (1 << 0)
#define ARISC_MESSAGE_ATTR_HARDSYN              (1 << 1)
#define HEAD_COMBINE(result, type, attr, state) (((u32)result << 24) | ((u32)type << 16) | ((u32)attr << 8) | ((u32)state))

static struct device *pae350_dev;
volatile static u32 chan_recv_buff[CHAN_RECV_BUFF_SIZE];
volatile static u32 chan_recv_num;

static void chan_recv_callback(struct mbox_client *cl, void *mssg)
{
	if (chan_recv_num >= CHAN_RECV_BUFF_SIZE) {
		sunxi_err(NULL, "chan recv buff is full\n");
		return ;
	}
	chan_recv_buff[chan_recv_num++] = *(u32 *)mssg;
}

static void pm_wait_ack_reset(void)
{
	chan_recv_num = 0;
}

static int pm_wait_ack(u8 result, u8 type, u8 attr, u8 state, u16 len, u32 *param)
{
	u32 t_head;
	unsigned long end_time;

	t_head = HEAD_COMBINE(0, type, attr, state);

	end_time = jiffies + msecs_to_jiffies(CHAN_RECV_ACK_TIMEOUT);
	/* wait ack finish, only wait a u32 of header */
	while (chan_recv_num < 1) {
		if (time_after(jiffies, end_time)) {
			break;
		}
	}

	if (t_head != chan_recv_buff[0])
		return -1;

	return 0;
}

static int pm_send_msg(u8 result, u8 type, u8 attr, u8 state, u16 len, u32 *param)
{
	u32 i;
	u32 t_head, t_len;

	t_head = HEAD_COMBINE(result, type, attr, state);
	t_len = (u32)len;
	if (mbox_send_message(standby_dbg_chan, &t_head) < 0)
		return -1;

	if (mbox_send_message(standby_dbg_chan, &t_len) < 0)
		return -1;

	for (i = 0; i < t_len; i++) {
		if (mbox_send_message(standby_dbg_chan, &param[i]) < 0)
			return -1;
	}
	return 0;
}

static int pm_send_msg_wait_ack(u16 type, u8 len, u32 *param)
{
	pm_wait_ack_reset();
	if (pm_send_msg(0, type, ARISC_MESSAGE_ATTR_HARDSYN, ARISC_MESSAGE_INITIALIZED, len, param)) {
		sunxi_err(NULL, "pm send msg fail\n");
		return -1;
	}
	if (pm_wait_ack(0, type, ARISC_MESSAGE_ATTR_HARDSYN, ARISC_MESSAGE_INITIALIZED, len, param)) {
		sunxi_err(NULL, "pm wait ack fail\n");
		return -1;
	}
	return 0;
}

int send_remote_pm_wuptmr_aov_en(u32 en)
{
	u32 param[1];

	if (en)
		param[0] = 1;
	else
		param[0] = 0;
	return pm_send_msg_wait_ack(PM_ENABLE_AOV_TIMER_MSG, 1, param);
}

int send_remote_pm_post_suspend(void)
{
	u32 param[1];

	param[0] = REMOTE_PM_POST_SUSPEND;
	return pm_send_msg_wait_ack(PM_POST_SUSPEND_MSG, 1, param);
}

int send_remote_pm_suspend_prepare(void)
{
	u32 param[1];

	param[0] = REMOTE_PM_SUSPEND_PREPARE;
	return pm_send_msg_wait_ack(PM_SUSPEND_PREPARE_MSG, 1, param);
}

#if IS_ENABLED(CONFIG_AW_MTD)
struct mtd_info *__mtd_next_device(int i);

int send_stored_info(void)
{
	struct mtd_info *mtd = __mtd_next_device(0);
	struct device_node *dev_node = NULL;
	u32 reg_val[4];
	u32 param[4];

	if (pae350_dev == NULL) {
		sunxi_err(NULL, "pae350_dev is NULL\n");
		return -1;
	}
	dev_node = of_parse_phandle(pae350_dev->of_node, "memory-region", 0);
	if (dev_node == NULL) {
		sunxi_err(NULL, "find memory-region fail\n");
		return -1;
	}
	if (of_property_read_u32_array(dev_node, "reg", reg_val, ARRAY_SIZE(reg_val))) {
		sunxi_err(NULL, "find reg fail\n");
		return -1;
	}
	param[0] = CONFIG_SPINOR_UBOOT_OFFSET;
	param[1] = ((unsigned int)mtd->size - 0x4000) / 512;
	param[2] = reg_val[1];
	param[3] = reg_val[3];
	return pm_send_msg_wait_ack(PM_FLASH_MSG, 4, param);
}
#endif

static int standby_debug_notifier_callback(struct notifier_block *this, unsigned long event, void *ptr)
{
	int ret;

	switch (event) {
	case PM_POST_HIBERNATION:
		break;
	case PM_POST_SUSPEND:
		if (standby_dbg_chan == NULL) {
			sunxi_warn(NULL, "%s, %d POST_SUSPEND do nothing\n",  __func__, __LINE__);
			break;
		}
		ret = send_remote_pm_post_suspend();
		if (ret < 0) {
			sunxi_err(NULL, "%s, %d POST_SUSPEND notify failed\n",  __func__, __LINE__);
			return NOTIFY_BAD;
		}
		break;
	case PM_HIBERNATION_PREPARE:
		break;
	case PM_SUSPEND_PREPARE:
		if (standby_dbg_chan == NULL) {
			sunxi_warn(NULL, "%s, %d SUSPEND_PREPARE do nothing\n",  __func__, __LINE__);
			break;
		}
#if IS_ENABLED(CONFIG_AW_MTD)
		/* send a message to rtos,
		 * that means linux begins to enter super-standby.
		 */
		ret = send_stored_info();
		if (ret < 0) {
			sunxi_err(NULL, "%s, %d send stored info failed\n",  __func__, __LINE__);
			return NOTIFY_BAD;
		}
#endif
		ret = send_remote_pm_suspend_prepare();
		if (ret < 0) {
			sunxi_err(NULL, "%s, %d SUSPEND_PREPARE notify failed\n",  __func__, __LINE__);
			return NOTIFY_BAD;
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block standby_debug_notifier = {
    .notifier_call = standby_debug_notifier_callback,
};

struct class ae350_standby_class = {
	.name = "ae350_standby",
	.class_groups = ae350_standby_class_groups,
};

static int ae350_standby_debug_probe(struct platform_device *pdev)
{
	int ret;
	struct ae350_standby_debug_dev *standby_dbg;
	struct device *dev = &pdev->dev;

	standby_dbg = devm_kzalloc(dev, sizeof(*standby_dbg), GFP_KERNEL);
	if (IS_ERR_OR_NULL(standby_dbg)) {
		sunxi_err(dev, "alloc failed\n");
		return -ENOMEM;
	}

	pae350_dev = dev;
	standby_dbg->dev = dev;
	standby_dbg->client.rx_callback = chan_recv_callback;
	standby_dbg->client.tx_done = NULL;
	standby_dbg->client.tx_block = NULL;
	standby_dbg->client.dev = dev;
	standby_dbg_chan = mbox_request_channel_byname(&standby_dbg->client,
						AE350_STANDBY_DBG_MBOX_NAME);
	if (IS_ERR(standby_dbg_chan)) {
		standby_dbg_chan = NULL;
		sunxi_err(dev, "request mbox channel failed\n");
		return -EFAULT;
	}
	standby_dbg->chan = standby_dbg_chan;
	platform_set_drvdata(pdev, standby_dbg);

	ret = register_pm_notifier(&standby_debug_notifier);
	if (ret) {
		sunxi_err(dev, "register_pm_notifier failed\n");
		goto mbox_free;
	}

	ret = class_register(&ae350_standby_class);
	if (ret < 0) {
		sunxi_err(dev, "class register err, ret:%d\n", ret);
		goto unregister_notifier;
	}

	return ret;

unregister_notifier:
	unregister_pm_notifier(&standby_debug_notifier);
mbox_free:
	mbox_free_channel(standby_dbg_chan);
	return ret;
}

static int ae350_standby_debug_remove(struct platform_device *pdev)
{
	class_unregister(&ae350_standby_class);
	unregister_pm_notifier(&standby_debug_notifier);
	mbox_free_channel(standby_dbg_chan);

	return 0;
}

static struct of_device_id ae350_standby_debug_ids[] = {
	{ .compatible = "allwinner,sun300iw1-ae350-standby-debug" },
	{}
};

static struct platform_driver ae350_standby_debug_driver = {
	.probe = ae350_standby_debug_probe,
	.remove = ae350_standby_debug_remove,
	.driver = {
		.name = "ae350_standby_debug",
		.owner = THIS_MODULE,
		.of_match_table = ae350_standby_debug_ids,
	}
};

static int __init ae350_standby_debug_init(void)
{
	int err;

	err = platform_driver_register(&ae350_standby_debug_driver);
	if (err)
		sunxi_err(NULL, "register ae350_standby_debug failed\n");

	return err;
}
static void __exit ae350_standby_debug_exit(void)
{
	platform_driver_unregister(&ae350_standby_debug_driver);
}
late_initcall(ae350_standby_debug_init);
module_exit(ae350_standby_debug_exit);

MODULE_DESCRIPTION("ANDES STANDBY DEBUG");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALLWINNER");
MODULE_VERSION("1.0.0");
