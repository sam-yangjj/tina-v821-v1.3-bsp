// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2023 - 2024 Allwinner Technology Co.,Ltd. All rights reserved.
 * Copyright (c) 2024 geyoupengs@allwinnertech.com
 * Copyright (c) 2024 zhangyuanjing@allwinnertech.com
 */

/*
ircut_pin_a   : IRCUT PIN+
ircut_pin_b   : IRCUT PIN-
ircut_pin_drv : Set the driver strength for the IR cut pins
ircut_open    : Define the GPIO states for opening the IR cut
hold_time     : Set the hold time for the IR cut operation (in milliseconds)
&ircut {
	ircut_pin_a = <&pio PD 22 GPIO_ACTIVE_HIGH>;
	ircut_pin_b = <&pio PD 23 GPIO_ACTIVE_HIGH>;
	ircut_pin_drv = <3>;
	ircut_open = <GPIO_ACTIVE_HIGH GPIO_ACTIVE_LOW>;
	hold_time = <1000>;
	status = "okay";
};
*/

/* #define DEBUG */
#define SUNXI_MODNAME "ircut"
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <sunxi-log.h>

#define DRV_NAME "sunxi_ircut"
#define DRV_VERSION "0.1.0"

#define PIN_LOW 0
#define PIN_HIGH 1

/* IR CUT Regs */
#define IRCUT_EN (1 << 0)
#define IRCUT_PROTECT_EN (1 << 1)

static struct class sunxi_ircut_class = {
	.name = DRV_NAME,
	.owner = THIS_MODULE,
};

/* IRCUT EN and Protect EN */
enum {
	IRCUT_STATUS_OPEN,
	IRCUT_STATUS_CLOSE,
};

/* IRCUT Status */
enum {
	IRCUT_STATUS_DISABLE,
	IRCUT_STATUS_ENABLE,
};

struct sunxi_ircut_hw_data {
	bool has_ircut_ctl;
	u32 *ircut_ctl_pin;
	u32 ircut_ctl_pin_count;
};

struct ircut_prvdata {
	u32 __iomem *addr;
	struct resource *res;
	struct platform_device *pdev;
	struct device *dev;
	struct class *class;
	struct device *class_dev;
	raw_spinlock_t lock;

	/* ir cut status data */
	const struct sunxi_ircut_hw_data *data;
	int ctl_status;
	bool use_ctl;

	/* pinctrl */
	int ircut_pin_a;
	int ircut_pin_b;
	int ircut_pin_drv;

	/* ir cut config */
	u32 ircut_open[2];
	u32 hold_time;
	int ircut_status;
};

static void sunxi_ircut_set_pin_low(struct ircut_prvdata *chip)
{
	gpio_set_value(chip->ircut_pin_a, PIN_LOW);
	gpio_set_value(chip->ircut_pin_b, PIN_LOW);
}

static void sunxi_ircut_set_pin_status(struct ircut_prvdata *chip)
{
	sunxi_ircut_set_pin_low(chip);

	if (chip->ircut_status == IRCUT_STATUS_OPEN) {
		gpio_set_value(chip->ircut_pin_a, chip->ircut_open[0]);
		gpio_set_value(chip->ircut_pin_b, chip->ircut_open[1]);
	} else {
		gpio_set_value(chip->ircut_pin_a, !(chip->ircut_open[0]));
		gpio_set_value(chip->ircut_pin_b, !(chip->ircut_open[1]));
	}

	sunxi_debug(chip->dev, "Set IRCUT Pin Status %d, default A%d B%d\n",
		    chip->ircut_status, chip->ircut_open[0],
		    chip->ircut_open[1]);

	msleep(chip->hold_time);

	sunxi_debug(chip->dev, "Set IRCUT Pin Status %d done, set low\n",
		    chip->ircut_status);

	sunxi_ircut_set_pin_low(chip);
}

static int sunxi_ircut_set_pin_drv(struct ircut_prvdata *chip, int drv)
{
	int ret = 0;
	unsigned long config_set;
	int drv_tmp;

	/* convert to gpio driver */
	drv_tmp = (drv + 1) * 10;

	config_set =
		pinconf_to_config_packed(PIN_CONFIG_DRIVE_STRENGTH, drv_tmp);
	ret = pinctrl_gpio_set_config(chip->ircut_pin_a, config_set);
	if (ret < 0) {
		sunxi_warn(chip->dev, "set pinctrl dlevel fail, ret = %d\n",
			   ret);
		return -EINVAL;
	}

	config_set =
		pinconf_to_config_packed(PIN_CONFIG_DRIVE_STRENGTH, drv_tmp);
	ret = pinctrl_gpio_set_config(chip->ircut_pin_b, config_set);
	if (ret < 0) {
		sunxi_warn(chip->dev, "set pinctrl dlevel fail, ret = %d\n",
			   ret);
		return -EINVAL;
	}

	chip->ircut_pin_drv = drv;

	return 0;
}

static int sunxi_ircut_get_ctl_status(struct ircut_prvdata *chip)
{
	unsigned long flags;
	u32 reg_val = 0;

	if (chip->use_ctl) {
		raw_spin_lock_irqsave(&chip->lock, flags);
		reg_val = readl(chip->addr);
		raw_spin_unlock_irqrestore(&chip->lock, flags);

		if (reg_val & (IRCUT_EN | IRCUT_PROTECT_EN))
			chip->ctl_status = IRCUT_STATUS_ENABLE;
		else
			chip->ctl_status = IRCUT_STATUS_DISABLE;
	}

	return chip->ctl_status;
}

static void sunxi_ircut_set_ctl_status(struct ircut_prvdata *chip)
{
	unsigned long flags;
	u32 reg_val = 0;

	/* if disable, set GPIO to low first */
	if (chip->ctl_status == IRCUT_STATUS_DISABLE) {
		sunxi_ircut_set_pin_low(chip);
	}

	if (chip->use_ctl) {
		/* close gpio */
		raw_spin_lock_irqsave(&chip->lock, flags);
		reg_val = readl(chip->addr);

		if (chip->ctl_status == IRCUT_STATUS_ENABLE) {
			reg_val |= (IRCUT_EN | IRCUT_PROTECT_EN);
		} else {
			reg_val &= ~(IRCUT_EN | IRCUT_PROTECT_EN);
		}

		writel(reg_val, chip->addr);
		raw_spin_unlock_irqrestore(&chip->lock, flags);
	}
}

static ssize_t sunxi_ircut_ctr_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);

	if (likely(chip->ctl_status == sunxi_ircut_get_ctl_status(chip))) {
		if (chip->ctl_status == IRCUT_STATUS_ENABLE)
			sunxi_info(chip->dev, "enabled\n");
		else
			sunxi_info(chip->dev, "disabled\n");
	} else {
		sunxi_err(chip->dev, "device driver internal erro\n");
	}

	return 0;
}

static ssize_t sunxi_ircut_ctr_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	char param[10];

	if (count > 10) {
		sunxi_info(chip->dev, "param is only enable or disable\n");
		return -EINVAL;
	}
	strlcpy(param, buf, count);

	if (!strcmp(param, "enable")) {
		chip->ctl_status = IRCUT_STATUS_ENABLE;
	} else if (!strcmp(param, "disable")) {
		chip->ctl_status = IRCUT_STATUS_DISABLE;
	} else {
		sunxi_info(chip->dev, "param is only enable or disable\n");
		return -EINVAL;
	}

	sunxi_ircut_set_ctl_status(chip);

	return count;
}

static ssize_t sunxi_ircut_status_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	char *status_str;

	if (chip->ircut_status == IRCUT_STATUS_OPEN)
		status_str = "open";
	else if (chip->ircut_status == IRCUT_STATUS_CLOSE)
		status_str = "close";
	else
		status_str = "unknow";

	return sprintf(buf, "%s\n", status_str);
}

static ssize_t sunxi_ircut_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	char param[10];

	if (chip->ctl_status == IRCUT_STATUS_DISABLE) {
		sunxi_err(chip->dev, "IR-CUT Not Enabled\n");
		return -EINVAL;
	}

	if (count > 10) {
		sunxi_info(chip->dev, "param is only open or close\n");
		return -EINVAL;
	}
	strlcpy(param, buf, count);

	if (!strcmp(param, "open")) {
		chip->ircut_status = IRCUT_STATUS_OPEN;
	} else if (!strcmp(param, "close")) {
		chip->ircut_status = IRCUT_STATUS_CLOSE;
	} else {
		sunxi_info(chip->dev, "param is only open or close\n");
		return -EINVAL;
	}

	sunxi_ircut_set_pin_status(chip);

	return count;
}

static ssize_t sunxi_ircut_pin_a_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;
	value = gpio_get_value(chip->ircut_pin_a);
	return sprintf(buf, "%d\n", value);
}

static ssize_t sunxi_ircut_pin_a_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;

	if (chip->ctl_status == IRCUT_STATUS_DISABLE) {
		sunxi_err(chip->dev, "IR-CUT Not Enabled\n");
		return -EINVAL;
	}

	if (kstrtoint(buf, 10, &value)) {
		sunxi_err(chip->dev, "Invalid input: %s, only support 1, 0\n",
			  buf);
		return -EINVAL;
	}

	if (value == 1) {
		gpio_set_value(chip->ircut_pin_a, PIN_HIGH);
		sunxi_debug(chip->dev, "Set Pin A to HIGH\n");
	} else if (value == 0) {
		gpio_set_value(chip->ircut_pin_a, PIN_LOW);
		sunxi_debug(chip->dev, "Set Pin A to LOW\n");
	} else {
		sunxi_err(chip->dev, "Invalid value for Pin A: %d\n", value);
		return -EINVAL;
	}

	return count;
}

static ssize_t sunxi_ircut_pin_b_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;
	value = gpio_get_value(chip->ircut_pin_b);
	return sprintf(buf, "%d\n", value);
}

static ssize_t sunxi_ircut_pin_b_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;

	if (chip->ctl_status == IRCUT_STATUS_DISABLE) {
		sunxi_err(chip->dev, "IR-CUT Not Enabled\n");
		return -EINVAL;
	}

	if (kstrtoint(buf, 10, &value)) {
		sunxi_err(chip->dev, "Invalid input: %s, only support 1, 0\n",
			  buf);
		return -EINVAL;
	}

	if (value == 1) {
		gpio_set_value(chip->ircut_pin_b, PIN_HIGH);
		sunxi_debug(chip->dev, "Set Pin B to HIGH\n");
	} else if (value == 0) {
		gpio_set_value(chip->ircut_pin_b, PIN_LOW);
		sunxi_debug(chip->dev, "Set Pin B to LOW\n");
	} else {
		sunxi_err(chip->dev, "Invalid value for Pin B: %d\n", value);
		return -EINVAL;
	}

	return count;
}

static ssize_t sunxi_ircut_pin_drv_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", chip->ircut_pin_drv);
}

static ssize_t sunxi_ircut_pin_drv_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;

	if (kstrtoint(buf, 10, &value)) {
		sunxi_err(chip->dev, "Invalid input: %s\n", buf);
		return -EINVAL;
	}

	if (value < 0 || value > 3) {
		sunxi_err(chip->dev,
			  "Invalid input: %d, only support 0,1,2,3\n", value);
		return -EINVAL;
	}

	sunxi_ircut_set_pin_drv(chip, value);

	return count;
}

static ssize_t sunxi_ircut_hold_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", chip->hold_time);
}

static ssize_t sunxi_ircut_hold_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct ircut_prvdata *chip = dev_get_drvdata(dev);
	int value;

	if (kstrtoint(buf, 10, &value)) {
		sunxi_err(chip->dev, "Invalid input: %s\n", buf);
		return -EINVAL;
	}

	chip->hold_time = value;

	return count;
}

static struct device_attribute ircut_attrs[] = {
	__ATTR(ircut_ctr, 0644, sunxi_ircut_ctr_show, sunxi_ircut_ctr_store),
	__ATTR(ircut_status, 0644, sunxi_ircut_status_show,
	       sunxi_ircut_status_store),
	__ATTR(ircut_pin_a, 0644, sunxi_ircut_pin_a_show,
	       sunxi_ircut_pin_a_store),
	__ATTR(ircut_pin_b, 0644, sunxi_ircut_pin_b_show,
	       sunxi_ircut_pin_b_store),
	__ATTR(ircut_pin_drv, 0644, sunxi_ircut_pin_drv_show,
	       sunxi_ircut_pin_drv_store),
	__ATTR(ircut_hold_time, 0644, sunxi_ircut_hold_time_show,
	       sunxi_ircut_hold_time_store),
	__ATTR_NULL,
};

static int sunxi_ircut_sysfs_crete(struct ircut_prvdata *chip)
{
	int ret = 0;
	int i = 0;

	ret = class_register(&sunxi_ircut_class);
	if (ret) {
		sunxi_err(NULL, "register sunxi_ircut_class failed. err=%d\n",
			  ret);
		return ret;
	}

	chip->class_dev =
		device_create(&sunxi_ircut_class, chip->dev, 1, chip, "ircut");

	/* need some class specific sysfs attributes */
	while (ircut_attrs[i].attr.name) {
		ret = device_create_file(chip->class_dev, &ircut_attrs[i]);
		if (ret) {
			sunxi_err(chip->dev,
				  "%s(): device_create_file() failed. err=%d\n",
				  __func__, ret);
			/* Remove previously created sysfs files */
			while (i > 0) {
				i--;
				device_remove_file(chip->class_dev,
						   &ircut_attrs[i]);
			}
			/* Clean up device and class */
			device_destroy(&sunxi_ircut_class, 1);
			class_unregister(&sunxi_ircut_class);
			return ret;
		}
		i++;
	}

	return 0;
}

static void sunxi_ircut_sysfs_remove(struct ircut_prvdata *chip)
{
	int i = 0;
	while (ircut_attrs[i].attr.name) {
		device_remove_file(chip->class_dev, &ircut_attrs[i]);
		i++;
	}
	device_destroy(&sunxi_ircut_class, 1);
	class_unregister(&sunxi_ircut_class);
}

static int sunxi_ircut_of_parse(struct ircut_prvdata *chip)
{
	int ret;

	struct platform_device *pdev = chip->pdev;
	struct device_node *np = pdev->dev.of_node;

	chip->data = of_device_get_match_data(&pdev->dev);
	if (!chip->data) {
		sunxi_err(chip->dev, "failed to get device data\n");
		return -ENODEV;
	}

	/* request gpio for device */
	chip->ircut_pin_a = of_get_named_gpio_flags(np, "ircut_pin_a", 0, NULL);
	if (!gpio_is_valid(chip->ircut_pin_a)) {
		sunxi_err(chip->dev, "could not get ircut_pin_a gpio\n");
		return -ENOMEM;
	}

	chip->ircut_pin_b = of_get_named_gpio_flags(np, "ircut_pin_b", 0, NULL);
	if (!gpio_is_valid(chip->ircut_pin_b)) {
		sunxi_err(chip->dev, "could not get ircut_pin_b gpio\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(np, "ircut_pin_drv", &chip->ircut_pin_drv);
	if (ret < 0) {
		sunxi_warn(
			chip->dev,
			"Failed to read ircut_pin_drv property, using default 1\n");
		chip->ircut_pin_drv = 1;
	}

	ret = of_property_read_u32_array(np, "ircut_open", chip->ircut_open, 2);
	if (ret < 0) {
		sunxi_warn(
			chip->dev,
			"Failed to read ircut_open property, using default: [a:0, b:1]\n");
		chip->ircut_open[0] = 0;
		chip->ircut_open[1] = 1;
	} else {
		/* flag == GPIO_ACTIVE_HIGH == 0, so we reverse the flag to get the real level */
		chip->ircut_open[0] = !chip->ircut_open[0];
		chip->ircut_open[1] = !chip->ircut_open[1];
	}

	ret = of_property_read_u32(np, "hold_time", &chip->hold_time);
	if (ret < 0) {
		sunxi_warn(
			chip->dev,
			"Failed to read hold_time property, using default 100ms\n");
		chip->hold_time = 100;
	}

	return 0;
}

static bool sunxi_ircut_pin_is_support_ctl(int arr[], int size, int pin)
{
	int i = 0;

	for (i = 0; i < size; i++) {
		if (arr[i] == pin) {
			return true;
		}
	}
	return false;
}

static bool sunxi_ircut_check_gpio(struct ircut_prvdata *chip)
{
	if (chip->data->has_ircut_ctl) {
		if (sunxi_ircut_pin_is_support_ctl(
			    chip->data->ircut_ctl_pin,
			    chip->data->ircut_ctl_pin_count,
			    chip->ircut_pin_a) &&
		    sunxi_ircut_pin_is_support_ctl(
			    chip->data->ircut_ctl_pin,
			    chip->data->ircut_ctl_pin_count,
			    chip->ircut_pin_b)) {
			return true;
		}
	}

	return false;
}

static int sunxi_ircut_init_gpio(struct ircut_prvdata *chip)
{
	int ret;

	ret = devm_gpio_request(chip->dev, chip->ircut_pin_a, "ircut_pin_a");
	if (ret < 0) {
		sunxi_err(chip->dev, "could not request ircut_pin_a gpio\n");
		return -ENOMEM;
	}

	ret = gpio_direction_output(chip->ircut_pin_a, 1);
	if (ret) {
		sunxi_err(chip->dev,
			  "failed to set gpio ircut_pin_a as output\n");
		return -ENOMEM;
	}

	ret = devm_gpio_request(chip->dev, chip->ircut_pin_b, "ircut_pin_b");
	if (ret < 0) {
		sunxi_err(chip->dev, "could not request ircut_pin_b gpio\n");
		return -ENOMEM;
	}

	ret = gpio_direction_output(chip->ircut_pin_b, 1);
	if (ret) {
		sunxi_err(chip->dev,
			  "failed to set gpio ircut_pin_b as output\n");
		return -ENOMEM;
	}

	return 0;
}

static int sunxi_ircut_probe(struct platform_device *pdev)
{
	int ret;
	struct ircut_prvdata *chip;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (IS_ERR_OR_NULL(chip))
		return -ENOMEM;

	chip->pdev = pdev;
	chip->dev = &pdev->dev;
	chip->ctl_status = IRCUT_STATUS_DISABLE;
	chip->use_ctl = false;

	chip->res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ircut");
	if (chip->res) {
		chip->addr =
			ioremap(chip->res->start, resource_size(chip->res));
		if (!chip->addr)
			return -ENXIO;
	}

	ret = sunxi_ircut_of_parse(chip);
	if (ret) {
		sunxi_err(chip->dev, "of parse fail\n");
		return ret;
	}

	ret = sunxi_ircut_init_gpio(chip);
	if (ret) {
		sunxi_err(chip->dev, "init gpio fail\n");
		return ret;
	}

	/* Check GPIO support ctl */
	chip->use_ctl = sunxi_ircut_check_gpio(chip);

	ret = sunxi_ircut_sysfs_crete(chip);
	if (ret) {
		sunxi_err(chip->dev, "failed to node_init\n");
		return ret;
	}

	sunxi_ircut_set_pin_low(chip);

	sunxi_ircut_set_pin_drv(chip, chip->ircut_pin_drv);

	platform_set_drvdata(pdev, chip);
	sunxi_info(NULL, "sunxi ircut version: %s, use ctrl = %d\n",
		   DRV_VERSION, chip->use_ctl);

	return 0;
}

static int sunxi_ircut_remove(struct platform_device *pdev)
{
	struct ircut_prvdata *chip = platform_get_drvdata(pdev);
	sunxi_ircut_sysfs_remove(chip);

	if (chip->addr)
		iounmap(chip->addr);

	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, chip);

	sunxi_info(chip->dev, "sunxi ircut driver remove success\n");
	return 0;
}

static struct sunxi_ircut_hw_data sunxi_ircut_default_data = {
	.has_ircut_ctl = false,
	.ircut_ctl_pin = NULL,
	.ircut_ctl_pin_count = 0,
};

static u32 sun300iw1_ircut_ctl_pin[] = { 118, 119 };

static struct sunxi_ircut_hw_data sunxi_ircut_sun300iw1_data = {
	.has_ircut_ctl = true,
	.ircut_ctl_pin = sun300iw1_ircut_ctl_pin,
	.ircut_ctl_pin_count = 2,
};

static struct of_device_id sunxi_ircut_match[] = {
	{
		.compatible = "allwinner,ircut",
		.data = &sunxi_ircut_default_data,
	},
	{
		.compatible = "allwinner,sun300iw1-ircut",
		.data = &sunxi_ircut_sun300iw1_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_ircut_match);

static struct platform_driver sunxi_ircut_driver = {
		.probe = sunxi_ircut_probe,
		.remove = sunxi_ircut_remove,
		.driver = {
				.name = DRV_NAME,
				.of_match_table = sunxi_ircut_match,
		},
};

static int __init sunxi_ircut_init(void)
{
	return platform_driver_register(&sunxi_ircut_driver);
}

static void __exit sunxi_ircut_exit(void)
{
	return platform_driver_unregister(&sunxi_ircut_driver);
}

module_init(sunxi_ircut_init);
module_exit(sunxi_ircut_exit);

MODULE_DESCRIPTION("Allwinner ir-cut driver");
MODULE_AUTHOR("zhangyuanjing<zhangyuanjing@allwinnertech.com>");
MODULE_AUTHOR("<geyoupengs@allwinnertech>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
