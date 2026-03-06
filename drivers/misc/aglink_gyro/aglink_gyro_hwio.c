#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <uapi/aglink/ag_proto.h>
#include <stddef.h>
#include "aglink_gyro.h"
#include "debug.h"

#define AGLINK_GYRO_VERSION   "1.1.3.2510150943"
#define OF_ATTRIBUTE_GET_U8(x) ((x) & 0xff)
#define OF_ATTRIBUTE_GET_U16(x) ((x) & 0xffff)

static struct aglink_gyro_data {
	struct gyro_config gyroconfig;
	struct i2c_adapter *twi_adapter;
	struct aglink_gyro_hwio_ops *gyro_hwio;
} *gyro_data;

extern int aglink_video_subdirfs_init(void);
extern void aglink_video_subdirfs_deinit(void);
extern void aglink_gyro_sample_stop(void);
extern int aglink_gyro_sample_kick_start(struct gyro_config gyroconfig);

static int fast_ipc_get_mode(void)
{
#ifdef CONFIG_AG_LINK
extern int aglink_app_return_mode(void);
	return aglink_app_return_mode();
#else
	return AG_MODE_IDLE;
#endif
}

static int aglink_gyro_twi_write(u16 addr, u8 reg, const u8 *bufp, u16 len)
{
	int ret = 0;
	u16 gyro_addr;
	u16 w_len = len + 1;
	u8 *buf = NULL;
	struct i2c_msg msg;

	if (!gyro_data || !gyro_data->twi_adapter) {
		ret = -1;
		goto fail0;
	}

	buf = kmalloc(sizeof(u8) * w_len, GFP_KERNEL);
	if (!buf) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "malloc failed\n");
		ret = -1;
		goto fail0;
	}

	buf[0] = reg;
	memcpy(buf + 1, bufp, len);

	gyro_addr = gyro_data->gyroconfig.addr;

	msg.addr = gyro_addr;
	msg.flags = 0;
	msg.len = w_len;
	msg.buf = buf;

	ret = i2c_transfer(gyro_data->twi_adapter, &msg, 1);
	if (ret < 0) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "write failed\n");
		goto fail1;
	}

	kfree(buf);
	return 0;
fail1:
	kfree(buf);
fail0:
	return ret;
}

static int aglink_gyro_twi_read(u16 addr, u8 reg, u8 *bufp, u16 len)
{
	int ret = 0;
	u16 gyro_addr;
	struct i2c_msg msg[2];

	gyro_addr = gyro_data->gyroconfig.addr;

	msg[0].addr = gyro_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = gyro_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = bufp;

	if (!gyro_data || !gyro_data->twi_adapter) {
		return -1;
	}

	ret = i2c_transfer(gyro_data->twi_adapter, msg, 2);
	if (ret < 0) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "read failed\n");
		return ret;
	}
	return 0;
}

static struct aglink_gyro_hwio_ops  gyro_twi_hwio = {
	.write = aglink_gyro_twi_write,
	.read = aglink_gyro_twi_read,
};

static void aglink_gyro_config_attribute(struct device_node *np, struct gyro_config *gyroconfig)
{
	enum of_gpio_flags flag;
	u32 num = 0;

	if (of_property_read_u32(np, "reg", &num) == 0) {
		gyroconfig->reg = num;
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "twi reg=%d\n", gyroconfig->reg);
	}

	if (of_property_read_u32(np, "gyro_take_xl", &num) == 0) {
		gyroconfig->xl_en = OF_ATTRIBUTE_GET_U8(num);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_take_xl=%d\n", gyroconfig->xl_en);
	}

	if (of_property_read_u32(np, "gyro_take_gy", &num) == 0) {
		gyroconfig->gy_en = OF_ATTRIBUTE_GET_U8(num);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_take_gy=%d\n", gyroconfig->gy_en);
	}

	if (gyroconfig->gy_en || gyroconfig->xl_en) {
		if (of_property_read_u32(np, "gyro_take_speed", &num) == 0) {
			gyroconfig->speed = OF_ATTRIBUTE_GET_U8(num);
			gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_take_speed=%d\n", gyroconfig->speed);
		}
	}

	if (of_property_read_u32(np, "gyro_take_ts", &num) == 0) {
		gyroconfig->ts_en = OF_ATTRIBUTE_GET_U8(num);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_take_ts=%d\n", gyroconfig->ts_en);
	}

	if (of_property_read_u32(np, "gyro_int_sel", &num) == 0) {
		gyroconfig->int_sel = OF_ATTRIBUTE_GET_U8(num);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_int_sel=%d\n", gyroconfig->int_sel);
	}

	if (of_property_read_u32(np, "addr", &num) == 0) {
		gyroconfig->addr = OF_ATTRIBUTE_GET_U16(num);
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "addr=%d\n", gyroconfig->addr);
	}

	if (of_property_read_u32(np, "gyro_fifo_deep", &gyroconfig->fifo_deep) == 0)
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gyro_fifo_deep=%d\n", gyroconfig->fifo_deep);

	gyroconfig->gpio_num = of_get_named_gpio_flags(np, "gpio", 0, &flag);
	if (gyroconfig->gpio_num < 0) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "do not support gyro int\n");
	} else {
		if (flag & OF_GPIO_ACTIVE_LOW)
			gyroconfig->gpio_active = 0;
		else
			gyroconfig->gpio_active = 1;
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "gpio_num=%d; gpio_active=%d\n",
						gyroconfig->gpio_num, gyroconfig->gpio_active);
	}
}

static int aglink_gyro_hwio_init(struct device_node *np)
{
	struct device_node *hwio_node;
	struct i2c_adapter *adapter;
	s32 ret;

	gyro_data = aglink_gyro_zmalloc(sizeof(*gyro_data));
	if (!gyro_data) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to malloc gyro_data\n");
		return -1;
	}

	hwio_node = of_parse_phandle(np, "hwio", 0);
	if (!hwio_node) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Missing hwio specification\n");
		ret = -1;
		goto failed;
	}

	if (!strncmp("twi", hwio_node->name, sizeof("twi")-1)) {
		adapter = of_find_i2c_adapter_by_node(hwio_node);
		of_node_put(hwio_node);
		if (!adapter) {
			gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to find I2C adapter\n");
			ret = -1;
			goto failed;
		}
		gyro_data->twi_adapter = adapter;
		gyro_data->gyro_hwio = &gyro_twi_hwio;
	} else {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "The gyrocope device has no twi or spi node selected\n");
		ret = -1;
		goto failed;
	}
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "The gyrocope device select %s\n", hwio_node->name);

	aglink_gyro_config_attribute(np, &gyro_data->gyroconfig);

	aglink_gyro_set_ops(gyro_data->gyroconfig.reg);

	ret = aglink_gpro_chip_probe(gyro_data->gyro_hwio, gyro_data->gyroconfig);
	if (ret) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "aglink_gpro_chip_probe failed!\n");
		goto failed;
	}

	return 0;
failed:
	aglink_gyro_free(gyro_data);
	gyro_data = NULL;
	return ret;
}

static int aglink_gyro_hwio_deinit(void)
{
	if (!gyro_data)
		goto out;
	aglink_gpro_chip_remove(gyro_data->gyroconfig);
	aglink_gyro_clear_ops();
	aglink_gyro_free(gyro_data);
	gyro_data = NULL;
out:
	return 0;
}

static int aglink_gyro_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	int mode;
	int ret;

	mode = fast_ipc_get_mode();
	if ((mode != AG_MODE_VIDEO) && (mode != AG_MODE_VIDEO_VERTICAL))
		return 0;

	gyro_printk(AGLINK_GYRO_DBG_ALWY, "aglink gyro version: %s\n", AGLINK_GYRO_VERSION);

	for_each_available_child_of_node(np, child) {
		if (child) {
			gyro_printk(AGLINK_GYRO_DBG_ALWY, "device[%s] select node[%s]\n", np->name, child->name);
			ret = aglink_gyro_hwio_init(child);
			if (ret) {
				gyro_printk(AGLINK_GYRO_DBG_ERROR, "aglink gyro hwio init failed\n");
				return ret;
			}

			ret = aglink_gyro_sample_kick_start(gyro_data->gyroconfig);
			if (ret) {
				gyro_printk(AGLINK_GYRO_DBG_ERROR, "aglink gyro start sample failed\n");
				return ret;
			}
			aglink_video_subdirfs_init();
			return 0;
		}
	}
	gyro_printk(AGLINK_GYRO_DBG_ERROR, "device[%s] has not node\n", np->name);
	return -1;
}

static int aglink_gyro_remove(struct platform_device *pdev)
{
	int mode;
	mode = fast_ipc_get_mode();
	if (mode != AG_MODE_VIDEO)
		return 0;

	aglink_gyro_sample_stop();
	aglink_video_subdirfs_deinit();
	return aglink_gyro_hwio_deinit();
}

static const struct of_device_id aglink_gyro_of_match[] = {
	{
		.compatible = "allwinner,gyro-aglink",
	},
};

MODULE_DEVICE_TABLE(of, aglink_gyro_of_match);

static struct platform_driver aglink_gyro_driver = {
	.driver = {
		.name = "aglink_gyro",
		.of_match_table = aglink_gyro_of_match,
	},
	.probe = aglink_gyro_probe,
	.remove = aglink_gyro_remove,
};

int __init aglink_gyro_init(void)
{
	return platform_driver_register(&aglink_gyro_driver);
}

void __exit aglink_gyro_exit(void)
{
	platform_driver_unregister(&aglink_gyro_driver);
}

arch_initcall(aglink_gyro_init);
//postcore_initcall(aglink_gyro_init);
module_exit(aglink_gyro_exit);

MODULE_AUTHOR("Allwinner");
MODULE_DESCRIPTION("Allwinner AGLINK gyro driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(AGLINK_GYRO_VERSION);
MODULE_ALIAS("aglink gyro");
