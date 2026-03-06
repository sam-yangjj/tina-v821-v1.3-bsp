// SPDX-License-Identifier: GPL-2.0
/*
 * ksc.h
 *
 * Copyright (c) 2007-2024 Allwinnertech Co., Ltd.
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
#ifndef _KSC_H
#define _KSC_H
#include <linux/list.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <video/sunxi_ksc.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "ksc_reg.h"
#include "ksc_macro_def.h"

#define MAX_CLK_NUM 4

struct ksc_drv_data {
	int version;
	bool support_offline_ksc;
	bool support_offline_up_scaler;
	bool support_offline_arbitrary_angel_rotation;
	bool support_offline_img_crop;
	bool support_offline_img_flip;
};



/**
 * description here
 */
struct ksc_freeing_buffers {
	void *mem0_virt;
	dma_addr_t mem0_phy_addr;
	void *mem1_virt;
	dma_addr_t mem1_phy_addr;
	unsigned int mem_size;
};

struct ksc_buffers {
	bool alloced;
	struct device *p_device;

	void *mem0_virt;
	dma_addr_t mem0_phy_addr;
	void *mem1_virt;
	dma_addr_t mem1_phy_addr;
	unsigned int mem_size;
	unsigned int chm_offset;

	void *lut_virt;
	dma_addr_t lut_phy_addr;
	unsigned int lut_size;

	void *lut_mem0_virt;
	dma_addr_t lut_mem0_phy_addr;
	void *lut_mem1_virt;
	dma_addr_t lut_mem1_phy_addr;

	struct dmabuf_item *src_item;
	struct dmabuf_item *dst_item;

	struct ksc_freeing_buffers buf_to_be_free;
};


/**
 * ksc_para
 */
struct ksc_para {
	struct sunxi_ksc_para para;
	struct ksc_buffers buf;
	bool is_first_frame;
};

/**
 * ksc device
 */
struct ksc_device {
	bool enabled;
	struct mutex dev_lock;
	struct device *p_device;
	struct device attr_device;
	struct attribute_group *p_attr_grp;

	struct ksc_regs *p_reg;
	unsigned int irq_no;
	struct clk *clks[MAX_CLK_NUM];
	struct reset_control *rsts[MAX_CLK_NUM];
	wait_queue_head_t event_wait;
	const struct ksc_drv_data *drv_data;
	unsigned int clk_freq[MAX_CLK_NUM];
	bool offline_finish_flag;
	int be_err_cnt;
	int fe_err_cnt;
	int irq_cnt;
	struct delayed_work free_buffer_work;

	struct ksc_para ksc_para;

	int (*enable)(struct ksc_device *p_ksc, bool enable);
	int (*set_ksc_para)(struct ksc_device *p_ksc, struct sunxi_ksc_para *para);
	struct ksc_buffers *(*get_ksc_buffers)(struct ksc_device *p_ksc);
	struct sunxi_ksc_para *(*get_ksc_para)(struct ksc_device *p_ksc);
};

struct ksc_device *ksc_dev_init(struct device *dev);

#endif /*End of file*/
