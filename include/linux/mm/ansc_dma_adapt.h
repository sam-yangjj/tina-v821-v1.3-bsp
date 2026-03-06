// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2023 - 2025 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * port dma ops to andes platform for memory access
 */

#ifndef __ANSC_DMA_ADAPT_H__
#define __ANSC_DMA_ADAPT_H__

#include <linux/dma-direct.h>

#if IS_ENABLED(CONFIG_AW_ANSC_DMA_OPS)
#define AW_ANSC_DMA_ATTR_CACHED (1 << 12)

int sunxi_ansc_dma_mmap(struct device *dev, struct vm_area_struct *vma,
			  void *cpu_addr, dma_addr_t dma_addr, size_t size,
			  unsigned long attrs);
#endif
#endif