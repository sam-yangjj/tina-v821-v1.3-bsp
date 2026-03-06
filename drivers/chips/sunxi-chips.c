/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
//#define DEBUG
#include <linux/module.h>
#include <sunxi-chips.h>
#include <sunxi-log.h>
#include <asm/io.h>

extern int console_printk[];
#define console_loglevel (console_printk[0])
static void sunxi_chip_alter_print_error(const char *img_name, const char *real_chip)
{
	int i;

	console_loglevel = CONSOLE_LOGLEVEL_DEBUG;

	for (i = 0; i < 5; i++) {
		printk(KERN_EMERG "ERROR: The detected chip model is %s, not %s. Please do not use "
			"the %s firmware.\n",
			real_chip, img_name, img_name);
	}
	panic("The detected chip model is %s, not %s. Please do not use "
			"the %s firmware.\n",
			real_chip, img_name, img_name);
}

#if !defined(SUNXI_CHIP_ALTER_USE_DEFINE)
int sunxi_chip_alter_version(void)
{
	return SUNXI_CHIP_ALTER_VERSION_DEFAULT;
}
EXPORT_SYMBOL_GPL(sunxi_chip_alter_version);
#endif /* !defined(SUNXI_CHIP_ALTER_USE_DEFINE) */

static int __init sunxi_chips_init(void)
{
#if IS_ENABLED(CONFIG_ARCH_SUN300IW1)
	#define PRCM_ROM_REG		(0x4A000040)
	u32 val;
	static int ver = -1;
	void __iomem *rom_reg;

	if (ver == -1) {
		rom_reg = ioremap(PRCM_ROM_REG, 0x4);
		val = readl(rom_reg);
		ver = ((val == 0x2) ? SUNXI_CHIP_ALTER_VERSION_V821 : SUNXI_CHIP_ALTER_VERSION_V821B);
		iounmap(rom_reg);
	}

#if IS_ENABLED(CONFIG_CHIP_V821)
	if (ver != SUNXI_CHIP_ALTER_VERSION_V821)
		sunxi_chip_alter_print_error("V821", "V821B");
#endif /* CONFIG_CHIP_V821 */

#if IS_ENABLED(CONFIG_CHIP_V821B)
	if (ver != SUNXI_CHIP_ALTER_VERSION_V821B)
		sunxi_chip_alter_print_error("V821B", "V821");
#endif /* CONFIG_CHIP_V821B */

#endif /* CONFIG_ARCH_SUN300IW1 */

	return 0;
}

static void __exit sunxi_chips_exit(void)
{
}

#if !defined(SUNXI_CHIP_ALTER_USE_DEFINE)
early_initcall(sunxi_chips_init);
#else
late_initcall(sunxi_chips_init);
#endif /* !defined(SUNXI_CHIP_ALTER_USE_DEFINE) */
module_exit(sunxi_chips_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Martin <wuyan@allwinnertech.com>");
MODULE_DESCRIPTION("sunxi chip level APIs");
MODULE_VERSION("0.0.2");
