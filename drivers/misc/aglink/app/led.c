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
#include "ag_txrx.h"
#include "ag_led.h"

#define CONFIG_PWM_PERIOD 1000000
#define DELAY_US 10
#define STEP 10000

typedef int (*ag_led_ctrl_cb)(enum ag_led_mode mode);

enum aglink_led_drive_id {
	AG_LED_PWM,
	AG_LED_GPIO,
	AG_LED_MAX,
};

struct aglink_led_ops {
	int (*on)(void);
	void (*off)(void);
};

struct aglink_led_pwm_data {
	int pwm_ch;
	struct pwm_device *pwm_dev;
};

struct aglink_led_gpio_data {
	int gpio_num;
};

struct aglink_led_data {
	int reg;
	int ledon_time;
	int active;
	struct aglink_led_pwm_data pwm;
	struct aglink_led_gpio_data gpio;
	struct aglink_led_ops ag_led_ops;

	struct task_struct *breath_thread;
	int thread_stop;
	int short_on_runing;

	struct platform_device *pdev;
};

static struct aglink_led_data *ag_led_data;

void request_aglink_led_pwm(void)
{
	int ret;
	struct pwm_state state;
	struct aglink_led_pwm_data *pwm_data = &ag_led_data->pwm;

	pwm_data->pwm_dev = pwm_request(pwm_data->pwm_ch, "aglink_pwm");
	if (pwm_data->pwm_dev) {
		ret = pwm_config(pwm_data->pwm_dev, 1000000, 1000000);

		memset(&state, 0, sizeof(struct pwm_state));
		pwm_get_state(pwm_data->pwm_dev, &state);
		state.polarity = PWM_POLARITY_NORMAL;
		ret = pwm_apply_state(pwm_data->pwm_dev, &state);
		pwm_enable(pwm_data->pwm_dev);
	} else {
		dev_err(&ag_led_data->pdev->dev, "get pwm dev fail\n");
	}
}

static int aglink_set_led_on_by_pwm(void)
{
	int ret;
	struct pwm_state state;
	struct aglink_led_pwm_data *pwm_data = &ag_led_data->pwm;

	pwm_data->pwm_dev = pwm_request(pwm_data->pwm_ch, "aglink_pwm");
	if (pwm_data->pwm_dev) {
		ret = pwm_config(pwm_data->pwm_dev, 1000000, 1000000);

		memset(&state, 0, sizeof(struct pwm_state));
		pwm_get_state(pwm_data->pwm_dev, &state);
		state.polarity = PWM_POLARITY_NORMAL;
		ret = pwm_apply_state(pwm_data->pwm_dev, &state);
		pwm_enable(pwm_data->pwm_dev);
	} else {
		dev_err(&ag_led_data->pdev->dev, "get pwm dev fail\n");
		return -1;
	}
	return 0;
}

static void aglink_set_led_off_by_pwm(void)
{
	struct aglink_led_pwm_data *pwm_data = &ag_led_data->pwm;

	if (ag_led_data->breath_thread != NULL) {
		ag_led_data->thread_stop = 1;
		kthread_stop(ag_led_data->breath_thread);
	}

	if (pwm_data->pwm_dev) {
		pwm_disable(pwm_data->pwm_dev);
		pwm_free(pwm_data->pwm_dev);
		pwm_data->pwm_dev = NULL;
	} else {
		dev_err(&ag_led_data->pdev->dev, "get pwm dev fail\n");
	}
}

static int aglink_set_led_on_by_gpio(void)
{
	gpio_set_value(ag_led_data->gpio.gpio_num,
				ag_led_data->active ? 1 : 0);
	return 0;
}

static void aglink_set_led_off_by_gpio(void)
{
	gpio_set_value(ag_led_data->gpio.gpio_num,
				ag_led_data->active ? 0 : 1);
}
static int aglink_set_led_on(void)
{
	return ag_led_data->ag_led_ops.on();
}

static void aglink_set_led_off(void)
{
	ag_led_data->ag_led_ops.off();
}

static int aglink_set_led_breath(void *data)
{
	int ret;
	struct pwm_state state;
	int duty_cycle;
	struct aglink_led_pwm_data *pwm_data = &ag_led_data->pwm;

	pwm_data->pwm_dev = pwm_request(pwm_data->pwm_ch, "aglink_pwm");
	if (pwm_data->pwm_dev) {
		ret = pwm_config(pwm_data->pwm_dev, 0, 1000000);

		memset(&state, 0, sizeof(struct pwm_state));
		pwm_get_state(pwm_data->pwm_dev, &state);
		state.polarity = PWM_POLARITY_NORMAL;
		ret = pwm_apply_state(pwm_data->pwm_dev, &state);
		pwm_enable(pwm_data->pwm_dev);
	} else {
		dev_err(&ag_led_data->pdev->dev, "get pwm dev fail\n");
		return 0;
	}
	while (!kthread_should_stop() && !ag_led_data->thread_stop) {
		for (duty_cycle = 0; duty_cycle <= CONFIG_PWM_PERIOD; duty_cycle += STEP) {
			memset(&state, 0, sizeof(struct pwm_state));
			pwm_get_state(pwm_data->pwm_dev, &state);
			state.duty_cycle = duty_cycle;
			pwm_apply_state(pwm_data->pwm_dev, &state);
			msleep(DELAY_US);
		}
		for (duty_cycle = CONFIG_PWM_PERIOD; duty_cycle >= 0; duty_cycle -= STEP) {
			memset(&state, 0, sizeof(struct pwm_state));
			pwm_get_state(pwm_data->pwm_dev, &state);
			state.duty_cycle = duty_cycle;
			pwm_apply_state(pwm_data->pwm_dev, &state);
			msleep(DELAY_US);
		}
	}
	pwm_disable(pwm_data->pwm_dev);
	pwm_free(pwm_data->pwm_dev);
	pwm_data->pwm_dev = NULL;
	return 0;
}

static void led_off_callback(struct timer_list *t)
{
	aglink_set_led_off();
	ag_led_data->short_on_runing = 0;
	dev_info(&ag_led_data->pdev->dev, "Timer expired, calling aglink_set_led_mode(0)\n");
}

static int aglink_set_led_mode(enum ag_led_mode mode)
{
	static struct timer_list mtimer;

	switch (mode) {
	case AG_LED_SHORT_ON:{
		if (ag_led_data->breath_thread != NULL) {
			dev_err(&ag_led_data->pdev->dev, "led in breath mode\n");
			return 0;
		}
		if (!ag_led_data->short_on_runing) {
			ag_led_data->short_on_runing = 1;
			aglink_set_led_on();
			timer_setup(&mtimer, led_off_callback, 0);
			mod_timer(&mtimer, jiffies + msecs_to_jiffies(ag_led_data->ledon_time));
		} else {
			dev_err(&ag_led_data->pdev->dev, "led mode already AG_LED_SHORT_ON\n");
		}
		break;
	}
	case AG_LED_ALWAYS_ON:
		if (ag_led_data->breath_thread != NULL) {
			dev_err(&ag_led_data->pdev->dev, "led in breath mode\n");
			return 0;
		}
		aglink_set_led_on();
		break;
	case AG_LED_OFF:
		if (ag_led_data->breath_thread != NULL) {
			ag_led_data->thread_stop = 1;
			ag_led_data->breath_thread = NULL;
		} else {
			aglink_set_led_off();
		}
		break;
	case AG_LED_BREATHING:{
		if (ag_led_data->reg == AG_LED_GPIO) {
			dev_err(&ag_led_data->pdev->dev,
					"do not support breath when use gpio drive led\n");
			return -1;
		}
		if (ag_led_data->breath_thread != NULL) {
			dev_err(&ag_led_data->pdev->dev, "Kernel thread is already running.\n");
			return 0;
		}
		ag_led_data->thread_stop = 0;
		ag_led_data->breath_thread = kthread_run(aglink_set_led_breath,
			NULL, "breath_led_thread");
		if (IS_ERR(ag_led_data->breath_thread)) {
			dev_err(&ag_led_data->pdev->dev, "Failed to create kernel thread\n");
			return PTR_ERR(ag_led_data->breath_thread);
		}
	}
	break;
	}
	return 0;
}

extern void aglinnk_register_led_ctrl(ag_led_ctrl_cb cb);
extern void aglinnk_unregister_led_ctrl(void);
extern int aglink_app_return_mode(void);

static int aglink_led_probe(struct platform_device *pdev)
{
	int mode = -1;
	struct device_node *np = pdev->dev.of_node;
	int ret;
	struct device *dev = &pdev->dev;
	struct aglink_led_data *data;
	struct device_node *child;

	if (!dev)
		return -ENOMEM;

	if (!np)
		return 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	data->pdev = pdev;

	for_each_available_child_of_node(np, child) {
		if (child) {
			dev_info(&pdev->dev, "device[%s] select [%s] to drive led\n", np->name, child->name);
			break;
		}
	}

	if (!child) {
		dev_err(&pdev->dev, "device[%s] has no vaild driver interface\n", np->name);
		return -EAGAIN;
	}

	ret = of_property_read_u32(np, "ledon_time", &data->ledon_time);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read ledon time: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "active", &data->active);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read active: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(child, "reg", &data->reg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read reg: %d\n", ret);
		return ret;
	}

	switch (data->reg) {
	case AG_LED_PWM:
		ret = of_property_read_u32(child, "pwm_led_ch", &data->pwm.pwm_ch);
		if (ret) {
			dev_err(&pdev->dev, "Failed to read pwm_led_channel property: %d\n", ret);
			return ret;
		}
		data->ag_led_ops.on = aglink_set_led_on_by_pwm;
		data->ag_led_ops.off = aglink_set_led_off_by_pwm;
		break;
	case AG_LED_GPIO:
		data->gpio.gpio_num = of_get_named_gpio_flags(child, "gpio", 0, NULL);
		if (data->gpio.gpio_num < 0) {
			dev_err(&pdev->dev, "Failed to read gpio num: %d\n", ret);
			return data->gpio.gpio_num;
		}

		ret = devm_gpio_request_one(dev, data->gpio.gpio_num,
									GPIOF_DIR_OUT, "ag_leg_gpio");
		if (ret < 0) {
			dev_err(&pdev->dev, "could not request ab-pin-yellow gpio\n");
			return -ENOMEM;
		}
		gpio_set_value(data->gpio.gpio_num,
					data->active ? 0 : 1);
		data->ag_led_ops.on = aglink_set_led_on_by_gpio;
		data->ag_led_ops.off = aglink_set_led_off_by_gpio;
		break;
	default:
		return -EAGAIN;
	}

	ag_led_data = data;

	aglinnk_register_led_ctrl(aglink_set_led_mode);
	mode = aglink_app_return_mode();
	switch (mode) {
	case AG_MODE_PHOTO:
	case AG_MODE_AI:
		aglink_set_led_mode(AG_LED_SHORT_ON);
		break;
	case AG_MODE_VIDEO:
	case AG_MODE_LIVESTREAM:
		aglink_set_led_mode(AG_LED_ALWAYS_ON);
		break;
	case AG_MODE_RECORD_AUDIO:
		aglink_set_led_mode(AG_LED_BREATHING);
		break;
	}
	return 0;
}

static int aglink_led_remove(struct platform_device *pdev)
{
	aglinnk_unregister_led_ctrl();
	return 0;
}

static const struct of_device_id aglink_led_ids[] = {
	{ .compatible = "allwinner,led-aglink" },
	{}
};

static struct platform_driver aglink_led_driver = {
	.probe	= aglink_led_probe,
	.remove	= aglink_led_remove,
	.driver	= {
		.owner = THIS_MODULE,
		.name = "aglink_led",
		.of_match_table = aglink_led_ids,
	},
};

static int __init aglink_led_ctrl_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&aglink_led_driver);
	if (ret) {
		dev_err(&ag_led_data->pdev->dev, "platform driver register aglink led fail\n");
		return ret;
	}
	dev_info(&ag_led_data->pdev->dev, "aglink led init\n");
	return 0;
}

static void __exit aglink_led_ctrl_exit(void)
{
	platform_driver_unregister(&aglink_led_driver);
	return;
}

subsys_initcall_sync(aglink_led_ctrl_init);
module_exit(aglink_led_ctrl_exit);

MODULE_AUTHOR("Allwinner");
MODULE_DESCRIPTION("Allwinner AGLINK led ctrl driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("aglink led");
