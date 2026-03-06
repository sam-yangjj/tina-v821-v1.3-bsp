// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * dev_fb/dev_fb.c
 *
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
#include "dev_fb.h"
#include "disp_display.h"
#include "lcd_fb_ion_mem.h"
#include "logo.h"

#include <uapi/video/uapi_lcd_fb.h>

#ifndef dma_mmap_writecombine
#define dma_mmap_writecombine dma_mmap_wc
#endif

struct lcd_fb_panel_data {
	bool fb_enable;
	struct fb_info *fbinfo;
	struct sunxi_lcd_fb_disp_panel_para panel_para;
	int blank;
	int fb_index;
	u32 pseudo_palette[16];
#if defined(USE_LCDFB_MEM_FOR_FB)
	struct lcdfb_ion_mem *ion_mem;
#endif
};

struct lcd_fb_info {
	u32 fb_count;
	struct device *dev;
	struct lcd_fb_panel_data *panel;
};

static struct lcd_fb_info g_fbi;

static int sunxi_lcd_fb_open(struct fb_info *info, int user)
{
	return 0;
}

static int sunxi_lcd_fb_release(struct fb_info *info, int user)
{
	return 0;
}

static ssize_t sunxi_lcd_fb_write(struct fb_info *info, const char __user *buf, size_t count,
				  loff_t *ppos)
{
	ssize_t res;

	LCDFB_DBG("count=%zd, ppos=%llu\n", count, *ppos);
	res = fb_sys_write(info, buf, count, ppos);

	return res;
}

static int sunxi_lcd_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	u32 sel = 0;

	LCDFB_DBG("LCD Pan Display: fb_var_screeninfo details:\n");
	LCDFB_DBG("  xres: %d, yres: %d, bpp: %d, width: %d, height: %d\n", var->xres, var->yres,
		  var->bits_per_pixel, var->width, var->height);
	LCDFB_DBG("  xres_virtual: %d, yres_virtual: %d, xoffset: %d, yoffset: %d\n",
		  var->xres_virtual, var->yres_virtual, var->xoffset, var->yoffset);
	LCDFB_DBG("  left_margin: %d, right_margin: %d, upper_margin: %d, lower_margin: %d\n",
		  var->left_margin, var->right_margin, var->upper_margin, var->lower_margin);
	LCDFB_DBG("  sync: %d, vmode: %d\n", var->sync, var->vmode);

	LCDFB_DBG("fb_info details:\n");
	LCDFB_DBG("  node: %d, fb_index: %d\n", info->node, g_fbi.panel[info->node].fb_index);
	LCDFB_DBG("  fix: line_length: %d\n", info->fix.line_length);

	for (sel = 0; sel < g_drv_info.lcd_panel_count; sel++) {
		if (sel == g_fbi.panel[info->node].fb_index) {
			memcpy(g_fbi.panel[sel].fbinfo, info, sizeof(struct fb_info));
			memcpy(&g_fbi.panel[sel].fbinfo->var, var,
			       sizeof(struct fb_var_screeninfo));
			sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);
			if (!g_fbi.panel[sel].panel_para.lcd_vsync_send_frame) {
				sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
				sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);
			}
		}
	}
	return 0;
}

static int sunxi_lcd_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned int offset = vma->vm_pgoff << PAGE_SHIFT;

	if (offset < info->fix.smem_len) {
#if defined(USE_LCDFB_MEM_FOR_FB)
		return g_fbi.panel[info->node].ion_mem->p_item->dmabuf->ops->mmap(
			g_fbi.panel[info->node].ion_mem->p_item->dmabuf, vma);
#else
		return dma_mmap_writecombine(g_fbi.dev, vma, info->screen_base,
					     info->fix.smem_start, info->fix.smem_len);
#endif
	}

	return -EINVAL;
}

static __maybe_unused struct dma_buf *sunxi_lcd_fb_get_dmabuf(struct fb_info *info)
{
#if defined(USE_LCDFB_MEM_FOR_FB)
	return g_fbi.panel[info->node].ion_mem->p_item->dmabuf;
#endif
	return NULL;
}

static int sunxi_lcd_fb_set_par(struct fb_info *info)
{
	return sunxi_lcd_fb_bsp_disp_lcd_set_var(info->node, info);
}

static int sunxi_lcd_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	return 0;
}

static int sunxi_lcd_fb_blank(int blank_mode, struct fb_info *info)
{
	if (blank_mode == FB_BLANK_POWERDOWN)
		return sunxi_lcd_fb_bsp_disp_lcd_blank(info->node, 1);
	else
		return sunxi_lcd_fb_bsp_disp_lcd_blank(info->node, 0);
}

static inline u32 sunxi_lcd_fb_convert_bitfield(int val, struct fb_bitfield *bf)
{
	u32 mask = ((1 << bf->length) - 1) << bf->offset;

	return (val << bf->offset) & mask;
}

static int sunxi_lcd_fb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
				  unsigned transp, struct fb_info *info)
{
	u32 val;
	u32 ret = 0;

	switch (info->fix.visual) {
	case FB_VISUAL_PSEUDOCOLOR:
		ret = -EINVAL;
		break;
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			val = sunxi_lcd_fb_convert_bitfield(transp, &info->var.transp) |
			      sunxi_lcd_fb_convert_bitfield(red, &info->var.red) |
			      sunxi_lcd_fb_convert_bitfield(green, &info->var.green) |
			      sunxi_lcd_fb_convert_bitfield(blue, &info->var.blue);
			((u32 *)info->pseudo_palette)[regno] = val;
		} else {
			ret = 0;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sunxi_lcd_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	unsigned int j, r = 0;
	unsigned char hred, hgreen, hblue, htransp = 0xff;
	unsigned short *red, *green, *blue, *transp;

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;

	for (j = 0; j < cmap->len; j++) {
		hred = *red++;
		hgreen = *green++;
		hblue = *blue++;
		if (transp)
			htransp = (*transp++) & 0xff;
		else
			htransp = 0xff;

		r = sunxi_lcd_fb_setcolreg(cmap->start + j, hred, hgreen, hblue, htransp, info);
		if (r)
			return r;
	}

	return 0;
}

static int sunxi_lcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __maybe_unused __user *argp = (void __user *)arg;

	switch (cmd) {
	case LCDFB_IO_GET_DMABUF_FD: {
#if defined(USE_LCDFB_MEM_FOR_FB)
		struct dma_buf *dmabuf;
		struct lcd_fb_dmabuf_export k_ret = { -1, 0 };

		ret = -1;
		dmabuf = sunxi_lcd_fb_get_dmabuf(info);

		/* add reference for current dmabuf */
		get_file(dmabuf->file);

		if (IS_ERR_OR_NULL(dmabuf))
			return PTR_ERR(dmabuf);

		k_ret.fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (k_ret.fd < 0) {
			dma_buf_put(dmabuf);
			break;
		}

		if (copy_to_user(argp, &k_ret, sizeof(struct lcd_fb_dmabuf_export))) {
			LCDFB_WRN("LCDFB_IO_GET_DMABUF_FD copy to user err\n");
		}
		ret = 0;
#else
		ret = -EINVAL;
#endif /* USE_LCDFB_MEM_FOR_FB */
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static struct fb_ops lcdfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = sunxi_lcd_fb_open,
	.fb_release = sunxi_lcd_fb_release,
	.fb_write = sunxi_lcd_fb_write,
	.fb_pan_display = sunxi_lcd_fb_pan_display,
#if IS_ENABLED(CONFIG_COMPAT)
	.fb_compat_ioctl = sunxi_lcd_fb_ioctl,
#endif
	.fb_ioctl = sunxi_lcd_fb_ioctl,
	.fb_check_var = sunxi_lcd_fb_check_var,
	.fb_set_par = sunxi_lcd_fb_set_par,
	.fb_blank = sunxi_lcd_fb_blank,
	/* .fb_cursor = lcd_fb_cursor, */
	.fb_mmap = sunxi_lcd_fb_mmap,
#if IS_ENABLED(CONFIG_AW_FB_CONSOLE)
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
#endif
	.fb_setcmap = sunxi_lcd_fb_setcmap,
	.fb_setcolreg = sunxi_lcd_fb_setcolreg,
};

static s32 sunxi_lcd_fb_pixel_format_to_var(enum sunxi_lcd_fb_pixel_format format,
					    struct fb_var_screeninfo *var)
{
	switch (format) {
	case LCDFB_FORMAT_ARGB_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;
		break;
	case LCDFB_FORMAT_ABGR_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;
		break;
	case LCDFB_FORMAT_RGBA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		break;
	case LCDFB_FORMAT_BGRA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		break;
	case LCDFB_FORMAT_RGB_888:
		var->bits_per_pixel = 24;
		var->transp.length = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case LCDFB_FORMAT_BGR_888:
		var->bits_per_pixel = 24;
		var->transp.length = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case LCDFB_FORMAT_RGB_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case LCDFB_FORMAT_BGR_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	default:
		LCDFB_WRN("[FB]not support format %d\n", format);
	}

	LCDFB_INF("fmt%d para: %dbpp, a(%d,%d),r(%d,%d),g(%d,%d),b(%d,%d)\n", (int)format,
		  (int)var->bits_per_pixel, (int)var->transp.offset, (int)var->transp.length,
		  (int)var->red.offset, (int)var->red.length, (int)var->green.offset,
		  (int)var->green.length, (int)var->blue.offset, (int)var->blue.length);

	return 0;
}

static int sunxi_lcd_fb_map_video_memory(struct fb_info *info)
{
	unsigned long long phy_addr = 0;
#if defined(USE_LCDFB_MEM_FOR_FB)
	g_fbi.panel[info->node].ion_mem = sunxi_lcd_fb_ion_malloc(info->fix.smem_len, &phy_addr);
	if (g_fbi.panel[info->node].ion_mem)
		info->screen_base = (char __iomem *)g_fbi.panel[info->node].ion_mem->vaddr;
#else
	info->screen_base =
		(char __iomem *)sunxi_lcd_fb_dma_malloc(info->fix.smem_len, (u32 *)(&phy_addr));
#endif
	info->fix.smem_start = (unsigned long)phy_addr;

	if (info->screen_base) {
		LCDFB_INF("%s(reserve),va=0x%08x, pa=0x%08x size:0x%08x\n", __func__,
			  (u32)info->screen_base, (u32)info->fix.smem_start,
			  (unsigned int)info->fix.smem_len);
		memset((void *__force)info->screen_base, 0x0, info->fix.smem_len);
		return 0;
	}

	LCDFB_WRN("%s fail!\n", __func__);
	return -ENOMEM;
}

static void sunxi_lcd_fb_unmap_video_memory(struct fb_info *info)
{
	if (!info->screen_base) {
		LCDFB_WRN("%s: screen_base is null\n", __func__);
		return;
	}
	LCDFB_INF("%s: screen_base=0x%p, smem=0x%p, len=0x%x\n", __func__,
		  (void *)info->screen_base, (void *)info->fix.smem_start, info->fix.smem_len);

#if defined(USE_LCDFB_MEM_FOR_FB)
	sunxi_lcd_fb_ion_free((void *__force)info->screen_base, (void *)info->fix.smem_start,
			      info->fix.smem_len);
#else
	sunxi_lcd_fb_dma_free((void *__force)info->screen_base, (void *)info->fix.smem_start,
			      info->fix.smem_len);
#endif
	info->screen_base = 0;
	info->fix.smem_start = 0;
}

#if IS_ENABLED(CONFIG_FB_DEFERRED_IO)
static void sunxi_lcd_fb_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
	u32 sel = 0;
	for (sel = 0; sel < g_drv_info.lcd_panel_count; sel++) {
		if (sel == g_fbi.panel[info->node].fb_index) {
			sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
		}
	}
}
#endif

void sunxi_lcd_fb_sync_frame(u32 sel)
{
	if (sel >= g_drv_info.lcd_panel_count) {
		LCDFB_WRN("Exceed max fb:%d >= %d\n", sel, g_drv_info.lcd_panel_count);
		return;
	}

	sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
}

void sunxi_lcd_fb_black_screen(u32 sel)
{
	sunxi_lcd_fb_sync_frame(sel);
}

void sunxi_lcd_fb_draw_black_screen(u32 sel)
{
	if (sel >= g_drv_info.lcd_panel_count) {
		LCDFB_WRN("Exceed max fb:%d >= %d\n", sel, g_drv_info.lcd_panel_count);
		return;
	}

	if (g_fbi.panel[sel].fbinfo->screen_base) {
		memset((void *__force)g_fbi.panel[sel].fbinfo->screen_base, 0x0,
		       g_fbi.panel[sel].fbinfo->fix.smem_len);
	}

	/* reset to 0,0 for flush disp */
	g_fbi.panel[sel].fbinfo->var.xoffset = 0;
	g_fbi.panel[sel].fbinfo->var.yoffset = 0;
	sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
	sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);
}

void sunxi_lcd_fb_draw_test_screen(u32 sel)
{
	u32 i = 0;

	if (sel >= g_drv_info.lcd_panel_count) {
		LCDFB_WRN("Exceed max fb:%d >= %d\n", sel, g_drv_info.lcd_panel_count);
		return;
	}

	for (i = 0; i < 500; i++) {
		if (g_fbi.panel[sel].fbinfo->screen_base) {
			memset((void *__force)g_fbi.panel[sel].fbinfo->screen_base, 0x0,
			       g_fbi.panel[sel].fbinfo->fix.smem_len);
		}

		/* reset to 0,0 for flush disp */
		g_fbi.panel[sel].fbinfo->var.xoffset = 0;
		g_fbi.panel[sel].fbinfo->var.yoffset = 0;
		sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
		sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);

		if (g_fbi.panel[sel].fbinfo->screen_base) {
			memset((void *__force)g_fbi.panel[sel].fbinfo->screen_base, 0xFF,
			       g_fbi.panel[sel].fbinfo->fix.smem_len);
		}

		/* reset to 0,0 for flush disp */
		g_fbi.panel[sel].fbinfo->var.xoffset = 0;
		g_fbi.panel[sel].fbinfo->var.yoffset = 0;
		sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
		sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);
	}
}

int sunxi_lcd_fb_lcd_draw_colorbar(u32 sel)
{
	u32 i, y, x;

	u32 colors[] = {
		0xFF0000, // Red
		0x00FF00, // Green
		0x0000FF, // Blue
		0xFFFF00, // Yellow
		0xFF00FF, // Magenta
		0x00FFFF, // Cyan
		0xFFFFFF // White
	};

	u32 num_colors = sizeof(colors) / sizeof(colors[0]);
	u32 width = g_fbi.panel[sel].fbinfo->var.xres;
	u32 height = g_fbi.panel[sel].fbinfo->var.yres;
	u32 bar_height = height / num_colors;
	void *screen_offset = (void *)((void *__force)g_fbi.panel[sel].fbinfo->screen_base);
	u32 *pdest = (u32 *)screen_offset;

	if (sel > g_drv_info.lcd_panel_count) {
		LCDFB_WRN("Only %d fb devices are supported\n", g_drv_info.lcd_panel_count);
		return -1;
	}

	/* reset to 0,0 for flush disp */
	g_fbi.panel[sel].fbinfo->var.xoffset = 0;
	g_fbi.panel[sel].fbinfo->var.yoffset = 0;
	for (i = 0; i < num_colors; i++) {
		for (y = i * bar_height; y < (i + 1) * bar_height; y++) {
			for (x = 0; x < width; x++) {
				pdest[y * width + x] = colors[i];
			}
		}
	}

	sunxi_lcd_fb_bsp_disp_lcd_set_layer(sel, g_fbi.panel[sel].fbinfo);
	sunxi_lcd_fb_bsp_disp_lcd_wait_for_vsync(sel);

	return 0;
}

int sunxi_lcd_fb_init(struct sunxi_lcd_fb_dev_lcd_fb_t *p_info)
{
	int i = 0, buffer_num = 2, pixel_format = 0;
#if IS_ENABLED(CONFIG_FB_DEFERRED_IO)
	struct fb_deferred_io *fbdefio = NULL;
#endif

	g_fbi.dev = p_info->device;

	if (p_info->lcd_fb_num > g_drv_info.lcd_panel_count) {
		LCDFB_WRN("Only %d fb devices are supported\n", g_drv_info.lcd_panel_count);
		return -1;
	}

	g_fbi.panel = kmalloc(sizeof(struct lcd_fb_panel_data) * p_info->lcd_fb_num, GFP_KERNEL);
	if (!g_fbi.panel) {
		return -ENOMEM;
	}

	for (i = 0; i < p_info->lcd_fb_num; i++) {
		g_fbi.panel[i].fbinfo = framebuffer_alloc(0, g_fbi.dev);
		if (!g_fbi.panel[i].fbinfo) {
			goto err_exit;
		}

		g_fbi.panel[i].fbinfo->fbops = &lcdfb_ops;
		g_fbi.panel[i].fbinfo->flags = 0;
		g_fbi.panel[i].fbinfo->device = g_fbi.dev;
		g_fbi.panel[i].fbinfo->par = &g_fbi;
		g_fbi.panel[i].fbinfo->var.xoffset = 0;
		g_fbi.panel[i].fbinfo->var.yoffset = 0;
		g_fbi.panel[i].fbinfo->var.xres = 800;
		g_fbi.panel[i].fbinfo->var.yres = 480;
		g_fbi.panel[i].fbinfo->var.xres_virtual = 800;
		g_fbi.panel[i].fbinfo->var.yres_virtual = 480 * 2;
		g_fbi.panel[i].fbinfo->var.nonstd = 0;
		g_fbi.panel[i].fbinfo->var.bits_per_pixel = 32;
		g_fbi.panel[i].fbinfo->var.transp.length = 8;
		g_fbi.panel[i].fbinfo->var.red.length = 8;
		g_fbi.panel[i].fbinfo->var.green.length = 8;
		g_fbi.panel[i].fbinfo->var.blue.length = 8;
		g_fbi.panel[i].fbinfo->var.transp.offset = 24;
		g_fbi.panel[i].fbinfo->var.red.offset = 16;
		g_fbi.panel[i].fbinfo->var.green.offset = 8;
		g_fbi.panel[i].fbinfo->var.blue.offset = 0;
		g_fbi.panel[i].fbinfo->var.activate = FB_ACTIVATE_FORCE;
		g_fbi.panel[i].fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
		g_fbi.panel[i].fbinfo->fix.type_aux = 0;
		g_fbi.panel[i].fbinfo->fix.visual = FB_VISUAL_TRUECOLOR;
		g_fbi.panel[i].fbinfo->fix.xpanstep = 1;
		g_fbi.panel[i].fbinfo->fix.ypanstep = 1;
		g_fbi.panel[i].fbinfo->fix.ywrapstep = 0;
		g_fbi.panel[i].fbinfo->fix.accel = FB_ACCEL_NONE;
		g_fbi.panel[i].fbinfo->fix.line_length =
			g_fbi.panel[i].fbinfo->var.xres_virtual * 4;
		g_fbi.panel[i].fbinfo->fix.smem_len = g_fbi.panel[i].fbinfo->fix.line_length *
						      g_fbi.panel[i].fbinfo->var.yres_virtual * 2;
		g_fbi.panel[i].fbinfo->screen_base = NULL;
		g_fbi.panel[i].fbinfo->pseudo_palette = g_fbi.panel[i].pseudo_palette;
		g_fbi.panel[i].fbinfo->fix.smem_start = 0x0;
		g_fbi.panel[i].fbinfo->fix.mmio_start = 0;
		g_fbi.panel[i].fbinfo->fix.mmio_len = 0;

		if (fb_alloc_cmap(&g_fbi.panel[i].fbinfo->cmap, 256, 1) < 0)
			goto err_exit;

		buffer_num = 2;
		pixel_format = LCDFB_FORMAT_ARGB_8888;
		memset(&g_fbi.panel[i].panel_para, 0, sizeof(g_fbi.panel[i].panel_para));
		if (!sunxi_lcd_fb_bsp_disp_get_panel_info(i, &g_fbi.panel[i].panel_para)) {
			buffer_num = g_fbi.panel[i].panel_para.fb_buffer_num;
			pixel_format = g_fbi.panel[i].panel_para.lcd_pixel_fmt;
		}

#if IS_ENABLED(CONFIG_FB_DEFERRED_IO)
		fbdefio = NULL;
		fbdefio = devm_kzalloc(g_fbi.dev, sizeof(*fbdefio), GFP_KERNEL);
		if (!fbdefio)
			goto err_exit;

		g_fbi.panel[i].fbinfo->fbdefio = fbdefio;
		fbdefio->delay = HZ / info.lcd_fps;
		fbdefio->deferred_io = sunxi_lcd_fb_deferred_io;
		fb_deferred_io_init(g_fbi.panel[i].fbinfo);
#endif

		sunxi_lcd_fb_pixel_format_to_var(pixel_format, &(g_fbi.panel[i].fbinfo->var));

		g_fbi.panel[i].fbinfo->var.xoffset = 0;
		g_fbi.panel[i].fbinfo->var.yoffset = 0;
		g_fbi.panel[i].fbinfo->var.xres = sunxi_lcd_fb_bsp_disp_get_screen_width(i);
		g_fbi.panel[i].fbinfo->var.yres = sunxi_lcd_fb_bsp_disp_get_screen_height(i);
		g_fbi.panel[i].fbinfo->var.xres_virtual = sunxi_lcd_fb_bsp_disp_get_screen_width(i);
		g_fbi.panel[i].fbinfo->fix.line_length =
			(g_fbi.panel[i].fbinfo->var.xres *
			 g_fbi.panel[i].fbinfo->var.bits_per_pixel) >>
			3;

		g_fbi.panel[i].fbinfo->fix.smem_len = g_fbi.panel[i].fbinfo->fix.line_length *
						      g_fbi.panel[i].fbinfo->var.yres * buffer_num;

		if (g_fbi.panel[i].fbinfo->fix.line_length != 0)
			g_fbi.panel[i].fbinfo->var.yres_virtual =
				g_fbi.panel[i].fbinfo->fix.smem_len /
				g_fbi.panel[i].fbinfo->fix.line_length;
		sunxi_lcd_fb_map_video_memory(g_fbi.panel[i].fbinfo);

		/* set fb timing */
		g_fbi.panel[i].fbinfo->var.width =
			sunxi_lcd_fb_bsp_disp_get_screen_physical_width(i);
		g_fbi.panel[i].fbinfo->var.height =
			sunxi_lcd_fb_bsp_disp_get_screen_physical_height(i);

		/* enable display */
		g_fbi.panel[i].fb_enable = 1;
		g_fbi.panel[i].fb_index = i;
		register_framebuffer(g_fbi.panel[i].fbinfo);
		sunxi_lcd_fb_logo_parse(g_fbi.panel[i].fbinfo);
	}

	sunxi_lcd_fb_logo_free_reserve();

	return 0;

err_exit:
	if (g_fbi.panel) {
		for (i = 0; i < p_info->lcd_fb_num; i++) {
			if (g_fbi.panel[i].fbinfo) {
				unregister_framebuffer(g_fbi.panel[i].fbinfo);
				framebuffer_release(g_fbi.panel[i].fbinfo);
			}
		}
		kfree(g_fbi.panel);
		g_fbi.panel = NULL;
	}

#if IS_ENABLED(CONFIG_FB_DEFERRED_IO)
	if (fbdefio) {
		devm_kfree(g_fbi.dev, fbdefio);
	}
#endif

	return -ENOMEM;
}

int sunxi_lcd_fb_exit(void)
{
	unsigned int fb_id = 0;

	for (fb_id = 0; fb_id < g_drv_info.lcd_panel_count; fb_id++) {
		if (g_fbi.panel[fb_id].fbinfo) {
#if IS_ENABLED(CONFIG_FB_DEFERRED_IO)
			fb_deferred_io_cleanup(g_fbi.panel[fb_id].fbinfo);
#endif
			fb_dealloc_cmap(&g_fbi.panel[fb_id].fbinfo->cmap);
			sunxi_lcd_fb_unmap_video_memory(g_fbi.panel[fb_id].fbinfo);
			unregister_framebuffer(g_fbi.panel[fb_id].fbinfo);
			framebuffer_release(g_fbi.panel[fb_id].fbinfo);
			g_fbi.panel[fb_id].fbinfo = NULL;
		}
	}

	if (g_fbi.panel) {
		kfree(g_fbi.panel);
		g_fbi.panel = NULL;
	}

	return 0;
}
