#ifndef __AI_GLASS_LED_H
#define __AI_GLASS_LED_H

enum ag_led_mode {
	AG_LED_SHORT_ON,
	AG_LED_ALWAYS_ON,
	AG_LED_OFF,
	AG_LED_BREATHING,
};

typedef int (*ag_led_ctrl_cb)(enum ag_led_mode mode);
#endif
