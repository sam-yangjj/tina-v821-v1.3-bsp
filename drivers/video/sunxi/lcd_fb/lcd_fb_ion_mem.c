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

#include "include.h"
#include "lcd_fb_ion_mem.h"
#include "dev_lcd_fb.h"

#if defined(USE_LCDFB_MEM_FOR_FB)

#define LCDFB_BYTE_ALIGN(x) (((x + (4 * 1024 - 1)) >> 12) << 12)

extern struct sunxi_lcd_fb_dev_lcd_fb_t g_drv_info;

static int sunxi_lcd_fb_dma_map_core(struct dma_buf *dmabuf, struct dmabuf_item *item)
{
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	int ret = -1;

	LCDFB_HERE;

	if (IS_ERR(dmabuf)) {
		LCDFB_WRN("dma_buf_get failed\n");
		goto exit;
	}
	attachment = dma_buf_attach(dmabuf, g_drv_info.device);
	if (IS_ERR(attachment)) {
		LCDFB_WRN("dma_buf_attach failed\n");
		goto err_buf_put;
	}
	sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
	if (IS_ERR_OR_NULL(sgt)) {
		LCDFB_WRN("dma_buf_map_attachment failed\n");
		goto err_buf_detach;
	}

	item->dmabuf = dmabuf;
	item->sgt = sgt;
	item->attachment = attachment;
	item->dma_addr = sg_dma_address(sgt->sgl);
	ret = 0;

	goto exit;

	/* unmap attachment sgt, not sgt_bak, cause it's not alloc yet! */
	dma_buf_unmap_attachment(attachment, sgt, DMA_FROM_DEVICE);
err_buf_detach:
	dma_buf_detach(dmabuf, attachment);
err_buf_put:
	dma_buf_put(dmabuf);
exit:
	return ret;
}

static void sunxi_lcd_fb_dma_ummap_core(struct dmabuf_item *item)
{
	LCDFB_HERE;

	dma_buf_unmap_attachment(item->attachment, item->sgt, DMA_FROM_DEVICE);
	dma_buf_detach(item->dmabuf, item->attachment);
	dma_buf_put(item->dmabuf);
}

static struct dmabuf_item *sunxi_lcd_fb_dma_map(struct dma_buf *dmabuf)
{
	struct dmabuf_item *item = NULL;

	LCDFB_HERE;

	item = kmalloc(sizeof(struct dmabuf_item), GFP_KERNEL | __GFP_ZERO);

	if (item == NULL) {
		LCDFB_WRN("malloc memory of size %d fail!\n",
			  (unsigned int)sizeof(struct dmabuf_item));
		goto exit;
	}

	if (sunxi_lcd_fb_dma_map_core(dmabuf, item) != 0) {
		kfree(item);
		item = NULL;
	}

exit:
	return item;
}

static void sunxi_lcd_fb_dma_unmap(struct dmabuf_item *item)
{
	LCDFB_HERE;

	sunxi_lcd_fb_dma_ummap_core(item);
	kfree(item);
}

int sunxi_lcd_fb_init_ion_mgr(struct lcdfb_ion_mgr *ion_mgr)
{
	LCDFB_HERE;

	if (ion_mgr == NULL) {
		LCDFB_WRN("input param is null\n");
		return -EINVAL;
	}

	mutex_init(&(ion_mgr->mlock));

	mutex_lock(&(ion_mgr->mlock));
	INIT_LIST_HEAD(&(ion_mgr->ion_list));
	mutex_unlock(&(ion_mgr->mlock));

	return 0;
}

static int sunxi_lcd_fb_dma_heap_alloc_coherent(struct lcdfb_ion_mem *mem)
{
#if IS_ENABLED(CONFIG_ION)
	unsigned int flags = 0;
	unsigned int heap_id_mask;
	struct dma_buf *dmabuf;

	LCDFB_HERE;

#if IS_ENABLED(CONFIG_AW_IOMMU)
	heap_id_mask = ION_HEAP_SYSTEM;
#elif IS_ENABLED(CONFIG_ION_SIZE_POOL_HEAP)
	heap_id_mask = ION_HEAP_CUSTOM_START;
#else
	heap_id_mask = ION_HEAP_DMA_START;
#endif /* CONFIG_AW_IOMMU */

	dmabuf = ion_alloc(mem->size, heap_id_mask, flags);
	if (IS_ERR_OR_NULL(dmabuf)) {
		LCDFB_WRN("%s: ion_alloc failed, size=%u dmabuf=0x%p\n", __func__,
			  (unsigned int)mem->size, dmabuf);
		return -2;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	mem->vaddr = dma_buf_vmap(dmabuf);
#else
	mem->vaddr = ion_heap_map_kernel(NULL, dmabuf->priv);
#endif

	if (IS_ERR_OR_NULL(mem->vaddr)) {
		LCDFB_WRN("ion_map_kernel failed!!\n");
		goto err_map_kernel;
	}

	LCDFB_INF("ion map kernel, vaddr=0x%08x\n", (u32)mem->vaddr);
	mem->p_item = kmalloc(sizeof(struct dmabuf_item), GFP_KERNEL);

	mem->p_item = sunxi_lcd_fb_dma_map(dmabuf);
	if (!mem->p_item)
		goto err_phys;

	return 0;

err_phys:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	dma_buf_vunmap(mem->p_item->dmabuf, mem->vaddr);
#else
	ion_heap_unmap_kernel(NULL, mem->p_item->dmabuf->priv);
#endif
err_map_kernel:
	ion_free(mem->p_item->dmabuf->priv);
	return -ENOMEM;

#elif (IS_ENABLED(CONFIG_DMABUF_HEAPS) && IS_ENABLED(CONFIG_AW_KERNEL_AOSP))
	struct dma_buf *dmabuf;
	struct dma_heap *dmaheap;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	int ret;
	struct dma_buf_map map;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	int ret;
	struct iosys_map map;
#endif

	LCDFB_HERE;

#if IS_ENABLED(CONFIG_AW_IOMMU) || IS_ENABLED(CONFIG_ARM_SMMU_V3)
	dmaheap = dma_heap_find("system-uncached");
#else
	dmaheap = dma_heap_find("reserved");
#endif

	if (IS_ERR_OR_NULL(dmaheap)) {
		LCDFB_WRN("failed, size=%u dmaheap=0x%p\n", (unsigned int)mem->size, dmaheap);
		return -2;
	}

	dmabuf = dma_heap_buffer_alloc(dmaheap, mem->size, O_RDWR, 0);

	if (IS_ERR_OR_NULL(dmabuf)) {
		LCDFB_WRN("failed, size=%u dmabuf=0x%p\n", (unsigned int)mem->size, dmabuf);
		return -2;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	ret = dma_buf_vmap(dmabuf, &map);
	mem->vaddr = map.vaddr;
	if (ret) {
		LCDFB_WRN("dma_buf_vmap failed!\n");
		goto err_map_kernel;
	}
#else
	mem->vaddr = dma_buf_vmap(dmabuf);
#endif

	if (IS_ERR_OR_NULL(mem->vaddr)) {
		LCDFB_WRN("ion_map_kernel failed!\n");
		goto err_map_kernel;
	}

	LCDFB_INF("ion map kernel, vaddr=0x%08x\n", (u32)mem->vaddr);
	mem->p_item = kmalloc(sizeof(struct dmabuf_item), GFP_KERNEL);

	attachment = dma_buf_attach(dmabuf, g_drv_info.dev);
	if (IS_ERR(attachment)) {
		LCDFB_WRN("dma_buf_attach failed\n");
		goto err_buf_put;
	}
	sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
	if (IS_ERR_OR_NULL(sgt)) {
		LCDFB_WRN("dma_buf_map_attachment failed\n");
		/* FIXME, wait iommu ready */
		return -1;
		/* goto err_buf_detach; */
	}

	mem->p_item->dmabuf = dmabuf;
	mem->p_item->sgt = sgt;
	mem->p_item->attachment = attachment;
	mem->p_item->dma_addr = sg_dma_address(sgt->sgl);

	return 0;
	/* unmap attachment sgt, not sgt_bak, cause it's not alloc yet! */
	dma_buf_unmap_attachment(attachment, sgt, DMA_FROM_DEVICE);
	/* err_buf_detach: */
	dma_buf_detach(dmabuf, attachment);

err_buf_put:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	map.vaddr = mem->vaddr;
	dma_buf_vunmap(mem->p_item->dmabuf, &map);
#else
	dma_buf_vunmap(mem->p_item->dmabuf, mem->vaddr);
#endif
err_map_kernel:
	dma_heap_buffer_free(mem->p_item->dmabuf);
	return -ENOMEM;
#else
	return 0;
#endif
}

static void sunxi_lcd_fb_ion_free_coherent(struct lcdfb_ion_mem *mem)
{
#if IS_ENABLED(CONFIG_ION)
	struct dmabuf_item item;
	LCDFB_HERE;

	memcpy(&item, mem->p_item, sizeof(struct dmabuf_item));
	sunxi_lcd_fb_dma_unmap(mem->p_item);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	dma_buf_vunmap(item.dmabuf, mem->vaddr);
#else
	ion_heap_unmap_kernel(NULL, item.dmabuf->priv);
#endif
	ion_free(item.dmabuf->priv);

#elif (IS_ENABLED(CONFIG_DMABUF_HEAPS) && IS_ENABLED(CONFIG_AW_KERNEL_AOSP))
	struct dmabuf_item item;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(mem->vaddr);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(mem->vaddr);
#endif
	memcpy(&item, mem->p_item, sizeof(struct dmabuf_item));
	sunxi_lcd_fb_dma_unmap(mem->p_item);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	dma_buf_vunmap(item.dmabuf, &map);
#else
	dma_buf_vunmap(item.dmabuf, mem->vaddr);
#endif
	dma_heap_buffer_free(item.dmabuf);
#endif
	return;
}

struct lcdfb_ion_mem *sunxi_lcd_fb_ion_malloc(u32 num_bytes, void *phys_addr)
{
	struct lcdfb_ion_mgr *ion_mgr = &(g_drv_info.ion_mgr);
	struct lcdfb_ion_list_node *ion_node = NULL;
	struct lcdfb_ion_mem *mem = NULL;
	u32 *paddr = NULL;
	int ret = -1;

	LCDFB_HERE;

	if (ion_mgr == NULL) {
		LCDFB_WRN("disp ion manager has not initial yet\n");
		return NULL;
	}

	/* Dynamically allocate the ion node */
	ion_node = kmalloc(sizeof(struct lcdfb_ion_list_node), GFP_KERNEL);
	if (ion_node == NULL) {
		LCDFB_WRN("fail to alloc ion node, size=%u\n",
			  (unsigned int)sizeof(struct lcdfb_ion_list_node));
		return NULL;
	}

	/* Allocate the memory for the 'mem' structure as well */
	mem = &ion_node->mem;
	if (mem == NULL) {
		LCDFB_WRN("fail to allocate memory for 'mem'\n");
		kfree(ion_node);
		return NULL;
	}

	mem->size = LCDFB_BYTE_ALIGN(num_bytes);

	/* Lock the mutex for synchronization */
	mutex_lock(&(ion_mgr->mlock));

	/* Allocate the memory from the DMA heap */
	ret = sunxi_lcd_fb_dma_heap_alloc_coherent(mem);

	if (ret != 0) {
		LCDFB_WRN("fail to alloc ion, ret=%d\n", ret);
		goto err_hdl;
	}

	/* Set the physical address */
	paddr = (u32 *)phys_addr;
	*paddr = (u32)mem->p_item->dma_addr;

	/* Add the node to the list */
	list_add_tail(&(ion_node->node), &(ion_mgr->ion_list));

	/* Unlock the mutex after the operations */
	mutex_unlock(&(ion_mgr->mlock));
	return mem;

err_hdl:
	/* If error, free the allocated memory */
	kfree(ion_node);
	mutex_unlock(&(ion_mgr->mlock));

	return NULL;
}

int sunxi_lcd_fb_get_ion_fd(struct lcdfb_ion_mem *mem)
{
	LCDFB_HERE;

	return dma_buf_fd(mem->p_item->dmabuf, O_CLOEXEC);
}

void *sunxi_lcd_fb_get_phy_addr(struct lcdfb_ion_mem *mem)
{
	LCDFB_HERE;

	return (void *)(uintptr_t)mem->p_item->dma_addr;
}

void sunxi_lcd_fb_ion_free(void *virt_addr, void *phys_addr, u32 num_bytes)
{
	struct lcdfb_ion_mgr *ion_mgr = &(g_drv_info.ion_mgr);
	struct lcdfb_ion_list_node *ion_node = NULL, *tmp_ion_node = NULL;
	struct lcdfb_ion_mem *mem = NULL;
	bool found = false;

	LCDFB_HERE;

	if (ion_mgr == NULL) {
		LCDFB_WRN("disp ion manager has not initial yet\n");
		return;
	}

	mutex_lock(&(ion_mgr->mlock));
	list_for_each_entry_safe (ion_node, tmp_ion_node, &ion_mgr->ion_list, node) {
		if (ion_node != NULL) {
			mem = &ion_node->mem;
			if ((((unsigned long)mem->p_item->dma_addr) ==
			     ((unsigned long)phys_addr)) &&
			    (((unsigned long)mem->vaddr) == ((unsigned long)virt_addr))) {
				sunxi_lcd_fb_ion_free_coherent(mem);
				__list_del_entry(&(ion_node->node));
				found = true;
				break;
			}
		}
	}
	mutex_unlock(&(ion_mgr->mlock));

	if (false == found) {
		LCDFB_WRN("vaddr=0x%p, paddr=0x%p is not found in ion\n", virt_addr, phys_addr);
	}
}

void sunxi_lcd_fb_deinit_ion_mgr(struct lcdfb_ion_mgr *ion_mgr)
{
	struct lcdfb_ion_list_node *ion_node = NULL, *tmp_ion_node = NULL;
	struct lcdfb_ion_mem *mem = NULL;

	LCDFB_HERE;

	if (ion_mgr == NULL) {
		LCDFB_WRN("input param is null\n");
		return;
	}

	mutex_lock(&(ion_mgr->mlock));
	list_for_each_entry_safe (ion_node, tmp_ion_node, &ion_mgr->ion_list, node) {
		if (ion_node != NULL) {
			/* free all ion node */
			mem = &ion_node->mem;
			sunxi_lcd_fb_ion_free_coherent(mem);
			__list_del_entry(&(ion_node->node));
			kfree(ion_node);
		}
	}
	mutex_unlock(&(ion_mgr->mlock));
}

#endif /* USE_LCDFB_MEM_FOR_FB */