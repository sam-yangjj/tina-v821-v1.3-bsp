#include <linux/init.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <uapi/aglink/ag_proto.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#include "ag_txrx.h"

#define AOV_GPIO_MODE_INPUT 0
#define AOV_GPIO_MODE_OUTPUT 1
#define AOV_GPIO_MODE AOV_GPIO_MODE_INPUT //0 input 1 output

#define AG_AOV_IOC_MAGIC 'V'
#define AG_AOV_SET_ON  _IO(AG_AOV_IOC_MAGIC, 0)
#define AG_AOV_SET_OFF _IO(AG_AOV_IOC_MAGIC, 1)
#define AG_AOV_GET_LEVEL _IOR(AG_AOV_IOC_MAGIC, 2, int)
#define AG_AOV_IOC_MAXNR 3

#define DEVICE_NAME "aov_gpio"
#define CLASS_NAME  "aov_gpio_ctrl"

static DEFINE_MUTEX(ag_aov_lock);
dev_t aov_devt;
struct cdev aov_cdev;
struct class *aov_class;
struct device *aov_device;

struct aglink_aov_data {
	int gpio_v821_aov;
	int gpio_v821_aov_assert;
	struct platform_device *pdev;
};

static struct aglink_aov_data *ag_aov_data;

#if (AOV_GPIO_MODE == AOV_GPIO_MODE_OUTPUT)
int aglink_aov_on(bool on_off)
{
	if (!ag_aov_data) {
		pr_err("ag_aov_data is not init!\n");
		return -1;
	}

	struct aglink_aov_data *data = ag_aov_data;

	struct device *dev = &data->pdev->dev;
	if (gpio_is_valid(data->gpio_v821_aov)) {
		dev_info(dev, "gpio_v821_aov set %d\n", on_off);
		gpio_set_value(data->gpio_v821_aov, on_off == true ? data->gpio_v821_aov_assert : !data->gpio_v821_aov_assert);
	} else {
		dev_err(dev, "gpio_v821_aov is error\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(aglink_aov_on);

#endif

static int aglink_aov_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct aglink_aov_data *data;
	enum of_gpio_flags config;

	if (!dev)
		return -ENOMEM;

	if (!np)
		return 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	data->pdev = pdev;

	data->gpio_v821_aov = of_get_named_gpio_flags(np, "gpio_v821_aov", 0, &config);
	if (data->gpio_v821_aov < 0) {
		dev_err(dev, "Failed to get gpio_v821_aov: %d\n", data->gpio_v821_aov);
		return data->gpio_v821_aov;
	}

	if (!gpio_is_valid(data->gpio_v821_aov)) {
		dev_err(dev, "Invalid gpio_v821_aov\n");
	} else {
		data->gpio_v821_aov_assert = (config == OF_GPIO_ACTIVE_LOW) ? 0 : 1;
		dev_info(dev, "gpio_v821_aov gpio=%d assert=%d\n", data->gpio_v821_aov, data->gpio_v821_aov_assert);
		ret = devm_gpio_request(dev, data->gpio_v821_aov,
			"gpio_v821_aov");
		if (ret < 0) {
			dev_err(dev, "can't request gpio_v821_aov gpio %d\n",
				data->gpio_v821_aov);
			return ret;
		}
		ret = gpio_direction_input(data->gpio_v821_aov);
		if (ret < 0) {
			dev_err(dev, "can't request input direction gpio_v821_aov gpio %d\n",
				data->gpio_v821_aov);
			return ret;
		}
	}
	ag_aov_data = data;
	return ret;
}

static int aglink_aov_remove(struct platform_device *pdev)
{
	ag_aov_data = NULL;
	return 0;
}

static int ag_aov_open(struct inode *inode, struct file *file)
{
	pr_info("ag_aov_open \n");
	return 0;
}

static ssize_t ag_aov_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	pr_info("ag_aov_read \n");
	char kbuf[4];
	size_t len;
	int gpio_val;
	ssize_t ret;

	if (*ppos != 0)
		return 0;
#if (AOV_GPIO_MODE == AOV_GPIO_MODE_INPUT)
	mutex_lock(&ag_aov_lock);
	if (!ag_aov_data) {
		mutex_unlock(&ag_aov_lock);
		pr_err("ag_aov_data is not initialized\n");
		return -ENODEV;
	}
	gpio_val = gpio_get_value(ag_aov_data->gpio_v821_aov);
	pr_info("ag_aov_read: gpio_val = %d\n", gpio_val);
	mutex_unlock(&ag_aov_lock);
	len = snprintf(kbuf, sizeof(kbuf), "%d\n", gpio_val);
	if (count > len)
		count = len;
	if (copy_to_user(buf, kbuf, count))
		return -EFAULT;
	*ppos += count;
	pr_info("ag_aov_read: return %zu bytes\n", count);
#else
	pr_warn("Now AOV gpio is output mode\n");
#endif
	return count;
}

static ssize_t ag_aov_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	char kbuf[16];
	int ret;
	long val;

	if (count >= sizeof(kbuf))
		return -EINVAL;

	ret = copy_from_user(kbuf, buf, count);
	if (ret)
		return -EFAULT;

	kbuf[count] = '\0';

	ret = kstrtol(kbuf, 10, &val);
	if (ret != 0)
		return -EINVAL;
#if (AOV_GPIO_MODE == AOV_GPIO_MODE_OUTPUT)
	if (val == 0) {
		pr_info("LED OFF\n");
		aglink_aov_on(0);
	} else {
		pr_info("LED ON\n");
		aglink_aov_on(1);
	}
#else
	pr_warn("Now AOV gpio is input mode\n");
#endif
	return count;
}

static long ag_aov_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int gpio_val;
	int __user *user_ptr = (int __user *)arg;

	if (_IOC_TYPE(cmd) != AG_AOV_IOC_MAGIC) {
		pr_err("Invalid ioctl type\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > AG_AOV_IOC_MAXNR) {
		pr_err("Invalid ioctl command\n");
		return -ENOTTY;
	}

	switch (cmd) {
#if (AOV_GPIO_MODE == AOV_GPIO_MODE_OUTPUT)
	case AG_AOV_SET_ON:
		pr_info("AG_AOV_SET_ON via ioctl\n");
		aglink_aov_on(1);
		break;

	case AG_AOV_SET_OFF:
		pr_info("AG_AOV_SET_OFF via ioctl\n");
		aglink_aov_on(0);
		break;
#endif
	case AG_AOV_GET_LEVEL:
		pr_info("via ioctl: AG_AOV_GET_LEVEL\n");
		mutex_lock(&ag_aov_lock);
		if (!ag_aov_data) {
			pr_err("ag_aov_data is not initialized\n");
			mutex_unlock(&ag_aov_lock);
			return -ENODEV;
		}
		gpio_val = gpio_get_value(ag_aov_data->gpio_v821_aov);
		pr_info("aov gpio level read: %d\n", gpio_val);
		mutex_unlock(&ag_aov_lock);
		if (copy_to_user(user_ptr, &gpio_val, sizeof(gpio_val))) {
			pr_err("Failed to copy data to user\n");
			return -EFAULT;
		}
		break;
	default:
		pr_err("Unknown ioctl command: 0x%x\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations aov_fops = {
	.owner = THIS_MODULE,
	.open = ag_aov_open,
	.read = ag_aov_read,
	.write = ag_aov_write,
	.unlocked_ioctl = ag_aov_ioctl,
	.compat_ioctl = ag_aov_ioctl,
};

static const struct of_device_id aglink_aov_ids[] = {
	{.compatible = "allwinner,aov-aglink" },
	{}
};

static struct platform_driver aglink_aov_driver = {
	.probe = aglink_aov_probe,
	.remove = aglink_aov_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "aglink_aov",
		.of_match_table = aglink_aov_ids,
	},
};

static int __init aglink_aov_ctrl_init(void)
{
	int ret = 0;

	pr_info("aglink aov init now\n");
	ret = alloc_chrdev_region(&aov_devt, 0, 1, DEVICE_NAME);
	if (ret) {
		pr_err("Failed to alloc chrdev region\n");
		goto out;
	}

	cdev_init(&aov_cdev, &aov_fops);
	aov_cdev.owner = THIS_MODULE;
	ret = cdev_add(&aov_cdev, aov_devt, 1);
	if (ret) {
		pr_err("Failed to add cdev\n");
		goto unregister_region;
	}

	aov_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(aov_class)) {
		pr_err("Failed to create class\n");
		ret = PTR_ERR(aov_class);
		goto del_cdev;
	}

	aov_device = device_create(aov_class, NULL, aov_devt, NULL, DEVICE_NAME);
	if (IS_ERR(aov_device)) {
		pr_err("Failed to create device\n");
		ret = PTR_ERR(aov_device);
		goto destroy_class;
	}

	ret = platform_driver_register(&aglink_aov_driver);
	if (ret) {
		pr_err("platform driver register aglink aov fail\n");
		goto destroy_device;
	}
	pr_info("aglink aov init ok\n");
	return 0;

destroy_device:
	device_destroy(aov_class, aov_devt);
destroy_class:
	class_destroy(aov_class);
del_cdev:
	cdev_del(&aov_cdev);
unregister_region:
	unregister_chrdev_region(aov_devt, 1);
out:
	return ret;
}

static void __exit aglink_aov_ctrl_exit(void)
{
	platform_driver_unregister(&aglink_aov_driver);
	device_destroy(aov_class, aov_devt);
	class_destroy(aov_class);
	cdev_del(&aov_cdev);
	unregister_chrdev_region(aov_devt, 1);
	pr_info("aglink aov exit\n");
}

subsys_initcall_sync(aglink_aov_ctrl_init);
module_exit(aglink_aov_ctrl_exit);

MODULE_AUTHOR("Allwinner");
MODULE_DESCRIPTION("Allwinner AGLINK aov ctrl driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("aglink_aov");
