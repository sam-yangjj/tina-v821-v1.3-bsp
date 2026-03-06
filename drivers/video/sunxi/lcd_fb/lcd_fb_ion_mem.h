// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LCD_FB_ION_MEM__
#define __LCD_FB_ION_MEM__
#include <linux/version.h>
#include "include.h"

#if defined(USE_LCDFB_MEM_FOR_FB)

#include <linux/version.h>
#include <linux/dma-mapping.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/dma-heap.h>
#endif
#if IS_ENABLED(CONFIG_AW_IOMMU)
#include "sunxi-iommu.h"
#endif

#include <linux/ion.h>
#include <uapi/linux/ion.h>

struct dmabuf_item {
	struct list_head list;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	unsigned long long id;
};

struct lcdfb_ion_mgr {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	struct ion_client *client;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	struct ion_handle *handle;
#else
	struct dma_buf *dma_buf;
#endif
	struct mutex mlock;
	struct list_head ion_list;
};

struct lcdfb_ion_mem {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	struct ion_handle *handle;
#endif
	void *vaddr;
	size_t size;
	struct dmabuf_item *p_item;
};

struct lcdfb_ion_list_node {
	struct list_head node;
	struct lcdfb_ion_mem mem;
};

/**
 * @brief Allocates memory from ION for the LCD framebuffer.
 *
 * This function allocates a block of memory from ION for use by the LCD framebuffer.
 * It returns a pointer to the allocated memory and associates it with a physical address.
 *
 * @param num_bytes The number of bytes to allocate.
 * @param phys_addr A pointer to store the physical address of the allocated memory.
 *
 * @return A pointer to a structure holding the ION memory information, or NULL on failure.
 */
struct lcdfb_ion_mem *sunxi_lcd_fb_ion_malloc(u32 num_bytes, void *phys_addr);

/**
 * @brief Frees memory allocated from ION for the LCD framebuffer.
 *
 * This function frees a block of memory that was previously allocated using
 * `sunxi_lcd_fb_ion_malloc`. The virtual and physical addresses are used to
 * correctly release the resources.
 *
 * @param virt_addr The virtual address of the memory to free.
 * @param phys_addr The physical address of the memory to free.
 * @param num_bytes The number of bytes to free.
 */
void sunxi_lcd_fb_ion_free(void *virt_addr, void *phys_addr, u32 num_bytes);

/**
 * @brief Gets the file descriptor of the ION memory.
 *
 * This function retrieves the file descriptor associated with the ION memory,
 * which can be used for further memory operations or management.
 *
 * @param mem A pointer to the `lcdfb_ion_mem` structure holding the memory information.
 *
 * @return The file descriptor associated with the ION memory, or a negative value on failure.
 */
int sunxi_lcd_fb_get_ion_fd(struct lcdfb_ion_mem *mem);

/**
 * @brief Gets the physical address of the ION memory.
 *
 * This function returns the physical address associated with the given ION memory structure.
 *
 * @param mem A pointer to the `lcdfb_ion_mem` structure holding the memory information.
 *
 * @return A pointer to the physical address of the ION memory.
 */
void *sunxi_lcd_fb_get_phy_addr(struct lcdfb_ion_mem *mem);

/**
 * @brief Initializes the ION memory manager.
 *
 * This function initializes the ION memory manager, preparing it to allocate and manage
 * memory for the LCD framebuffer.
 *
 * @param ion_mgr A pointer to the `lcdfb_ion_mgr` structure that will manage ION resources.
 *
 * @return 0 on success, a negative value on failure.
 */
int sunxi_lcd_fb_init_ion_mgr(struct lcdfb_ion_mgr *ion_mgr);

/**
 * @brief Deinitializes the ION memory manager.
 *
 * This function deinitializes the ION memory manager, releasing any resources it had
 * allocated and performing necessary cleanup.
 *
 * @param ion_mgr A pointer to the `lcdfb_ion_mgr` structure to be deinitialized.
 */
void sunxi_lcd_fb_deinit_ion_mgr(struct lcdfb_ion_mgr *ion_mgr);

#endif /* USE_LCDFB_MEM_FOR_FB */

#endif /* __LCD_FB_ION_MEM__ */