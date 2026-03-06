// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */

#ifndef _MM_SIZE_POOL_H_
#define _MM_SIZE_POOL_H_

#include <linux/types.h>

int size_pool_release_early_boot_buf_name(const char *name);

int size_pool_release_early_boot_buf(uint32_t addr);

#endif //_MM_SIZE_POOL_H_