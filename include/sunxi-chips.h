/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
#ifndef __SUNXI_CHIPS_H__
#define __SUNXI_CHIPS_H__

#define SUNXI_CHIP_ALTER_VERSION_DEFAULT	0

#define SUNXI_CHIP_ALTER_VERSION_V821   	1
#define SUNXI_CHIP_ALTER_VERSION_V821B  	2

#if IS_ENABLED(CONFIG_AW_CHIPS)

#if IS_ENABLED(CONFIG_CHIP_V821)
#define sunxi_chip_alter_version() SUNXI_CHIP_ALTER_VERSION_V821
#define SUNXI_CHIP_ALTER_USE_DEFINE
#elif IS_ENABLED(CONFIG_CHIP_V821B)
#define sunxi_chip_alter_version() SUNXI_CHIP_ALTER_VERSION_V821B
#define SUNXI_CHIP_ALTER_USE_DEFINE
#else
int sunxi_chip_alter_version(void);
#endif /* CONFIG_CHIP_V821 */

#else

__weak int sunxi_chip_alter_version(void) { return SUNXI_CHIP_ALTER_VERSION_DEFAULT; }

#endif  /* CONFIG_AW_CHIPS */

#endif  /* __SUNXI_CHIPS_H__ */
