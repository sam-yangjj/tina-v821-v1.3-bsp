// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * logo.c
 *
 * Copyright (c) 2007-2020 Allwinnertech Co., Ltd.
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
#include "logo.h"
#include <linux/memblock.h>
#include <linux/kernel.h>
#include <linux/version.h>

#if IS_ENABLED(CONFIG_AW_LCD_FB_DECOMPRESS_LZMA)
#include <linux/decompress/unlzma.h>
#endif

static u32 disp_reserve_size;
static u32 disp_reserve_base;

#ifndef MODULE
static int __init early_lcdfb_reserve(char *str)
{
	u32 temp[3] = { 0 };

	if (!str)
		return -EINVAL;

	get_options(str, 3, temp);

	disp_reserve_size = temp[1];
	disp_reserve_base = temp[2];
	if (temp[0] != 2 || disp_reserve_size <= 0)
		return -EINVAL;

	/* arch_mem_end = memblock_end_of_DRAM(); */
	memblock_reserve(disp_reserve_base, disp_reserve_size);
	pr_info("lcdfb reserve base 0x%x ,size 0x%x\n", disp_reserve_base, disp_reserve_size);
	return 0;
}
early_param("lcdfb_reserve", early_lcdfb_reserve);
#endif

static void *sunxi_lcd_fb_fb_map_kernel(unsigned long phys_addr, unsigned long size)
{
	int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct page *cur_page = phys_to_page(phys_addr);
	pgprot_t pgprot;
	void *vaddr = NULL;
	int i;

	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++)
		*(tmp++) = cur_page++;

	pgprot = pgprot_noncached(PAGE_KERNEL);
	vaddr = vmap(pages, npages, VM_MAP, pgprot);

	vfree(pages);
	return vaddr;
}

static void sunxi_lcd_fb_unmap_kernel(void *vaddr)
{
	vunmap(vaddr);
}

static int sunxi_lcd_fb_bmp_to_fb(const void *psrc, struct bmp_header *bmp_header,
				  struct fb_info *info, void *pdst)
{
	int srclinesize, dstlinesize, w, h, bmp_offset_x, bmp_offset_y;
	const unsigned char *psrcline = NULL, *psrcdot = NULL;
	unsigned char *pdstline = NULL, *pdstdot = NULL;
	int i = 0, j = 0, zero_num = 0, bmp_real_height = 0;
	int BppBmp = 3;

	/* Check for invalid parameters */
	if (!psrc || !pdst || !bmp_header || !info) {
		LCDFB_WRN("sunxi_lcd_fb_bmp_to_fb invalid parameter, null ptr\n");
		return -1;
	}

	/* If the bitmap's bit depth is 24, calculate padding to align rows to 4-byte boundary */
	if (bmp_header->bit_count == 24)
		zero_num = (4 - ((3 * bmp_header->width) % 4)) & 3;

	/* Set the width and height based on the screen resolution and image dimensions */
	w = (info->var.xres < bmp_header->width) ? info->var.xres : bmp_header->width;
	bmp_real_height =
		(bmp_header->height & 0x80000000) ? (-bmp_header->height) : (bmp_header->height);
	h = (info->var.yres < bmp_real_height) ? info->var.yres : bmp_real_height;

	BppBmp = bmp_header->bit_count >> 3; /* Bits per pixel in the BMP image */
	srclinesize = bmp_header->width * BppBmp + zero_num; /* Source line size (with padding) */
	dstlinesize = info->var.xres *
		      (info->var.bits_per_pixel >> 3); /* Destination line size (screen) */
	psrcline = (const unsigned char *)psrc; /* Pointer to the source image data */
	pdstline = (unsigned char *)pdst; /* Pointer to the destination framebuffer */

	/* Calculate horizontal and vertical offsets to center the image on the screen */
	bmp_offset_x = abs(info->var.xres - w) / 2; /* Horizontal offset */
	bmp_offset_y = abs(info->var.yres - h) / 2; /* Vertical offset */

	/* If the BMP image height is negative, it means the image is stored bottom-up */
	if (bmp_header->height & 0x80000000) {
		/* Copy from bottom to top */
		for (i = 0; i < h; ++i) {
			pdstdot = pdstline + bmp_offset_x * (info->var.bits_per_pixel >> 3) +
				  bmp_offset_y * dstlinesize;
			psrcdot = psrcline;
			if (info->var.bits_per_pixel == bmp_header->bit_count) {
				/* Directly copy the entire row if bit depth matches */
				memcpy(pdstdot, psrcline, srclinesize);
			} else {
				/* If bit depths don't match, process pixel by pixel */
				for (j = 0; j < w; ++j) {
					if (info->var.bits_per_pixel == 32 &&
					    bmp_header->bit_count == 24) {
						/* Convert 24-bit BMP to 32-bit framebuffer (RGBA) */
						*pdstdot++ = psrcdot[j * BppBmp]; // Blue channel
						*pdstdot++ =
							psrcdot[j * BppBmp + 1]; // Green channel
						*pdstdot++ = psrcdot[j * BppBmp + 2]; // Red channel
						*pdstdot++ = 0xff; // Alpha (opaque)
					} else if (info->var.bits_per_pixel == 18 &&
						   bmp_header->bit_count >= 24) {
						/* Convert 24-bit BMP to 18-bit framebuffer (RGB666) */
						/* RGB666 format: 6 bits for Green, 6 bits for Red, 6 bits for Blue */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 2) & 0xC0) +
							((psrcdot[j * BppBmp] >> 2) & 0x3F);
						*pdstdot++ =
							((psrcdot[j * BppBmp + 2] >> 2) & 0x3F) +
							((psrcdot[j * BppBmp + 1] >> 4) & 0x3F);
					} else if (info->var.bits_per_pixel == 16 &&
						   bmp_header->bit_count >= 24) {
						/* Convert 24-bit BMP to 16-bit framebuffer (RGB565) */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 3) & 0xe0) +
							((psrcdot[j * BppBmp] >> 3) & 0x1f);
						*pdstdot++ =
							(psrcdot[j * BppBmp + 2] & 0xf8) +
							((psrcdot[j * BppBmp + 1] >> 5) & 0x07);
					}
				}
			}
			psrcline += srclinesize; /* Move to the next row in the BMP */
			pdstline += dstlinesize; /* Move to the next row in the framebuffer */
		}
	} else {
		/* Copy from top to bottom */
		psrcline =
			(const unsigned char *)psrc +
			(bmp_real_height -
			 1) * srclinesize; /* Start from the bottom row if image is not bottom-up */
		for (i = h - 1; i >= 0; --i) {
			pdstdot = pdstline + bmp_offset_x * (info->var.bits_per_pixel >> 3) +
				  bmp_offset_y * dstlinesize;
			psrcdot = psrcline;

			if (info->var.bits_per_pixel == bmp_header->bit_count) {
				/* Directly copy the entire row if bit depth matches */
				memcpy(pdstdot, psrcline, srclinesize);
			} else {
				/* If bit depths don't match, process pixel by pixel */
				for (j = 0; j < w; ++j) {
					if (info->var.bits_per_pixel == 32 &&
					    bmp_header->bit_count == 24) {
						/* Convert 24-bit BMP to 32-bit framebuffer (RGBA) */
						*pdstdot++ = psrcdot[j * BppBmp]; // Blue channel
						*pdstdot++ =
							psrcdot[j * BppBmp + 1]; // Green channel
						*pdstdot++ = psrcdot[j * BppBmp + 2]; // Red channel
						*pdstdot++ = 0xff; // Alpha (opaque)
					} else if (info->var.bits_per_pixel == 18 &&
						   bmp_header->bit_count >= 24) {
						/* Convert 24-bit BMP to 18-bit framebuffer (RGB666) */
						/* RGB666 format: 6 bits for Green, 6 bits for Red, 6 bits for Blue */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 2) & 0xC0) +
							((psrcdot[j * BppBmp] >> 2) & 0x3F);
						*pdstdot++ =
							((psrcdot[j * BppBmp + 2] >> 2) & 0x3F) +
							((psrcdot[j * BppBmp + 1] >> 4) & 0x3F);
					} else if (info->var.bits_per_pixel == 16 &&
						   bmp_header->bit_count >= 24) {
						/* Convert 24-bit BMP to 16-bit framebuffer (RGB565) */
						*pdstdot++ =
							((psrcdot[j * BppBmp + 1] << 3) & 0xe0) +
							((psrcdot[j * BppBmp] >> 3) & 0x1f);
						*pdstdot++ =
							(psrcdot[j * BppBmp + 2] & 0xf8) +
							((psrcdot[j * BppBmp + 1] >> 5) & 0x07);
					}
				}
			}
			psrcline -= srclinesize; /* Move to the previous row in the BMP */
			pdstline += dstlinesize; /* Move to the next row in the framebuffer */
		}
	}

	return 0;
}

static int sunxi_lcd_fb_show_bootlogo_bmp(u8 *bmp_data, struct bmp_header *bmp_header,
					  struct fb_info *info)
{
	int ret = -1;
	u32 bmp_bpix;
	void *screen_offset = NULL, *image_offset = NULL;

	if (bmp_data == NULL || bmp_header == NULL || info == NULL) {
		LCDFB_WRN("one or more input parameters are NULL\n");
		return -1;
	}

	bmp_bpix = bmp_header->bit_count / 8;

	if ((bmp_bpix != 3) && (bmp_bpix != 4)) {
		LCDFB_WRN("not support bmp format:%d only support 24bit or 32bit\n", bmp_bpix);
		return -1;
	}

	image_offset = (void *)bmp_data;
	screen_offset = (void *)((void *__force)info->screen_base);

	sunxi_lcd_fb_bmp_to_fb(image_offset, bmp_header, info, screen_offset);

	return ret;
}

#if IS_ENABLED(CONFIG_AW_LCD_FB_DECOMPRESS_LZMA)
int sunxi_lcd_fb_lzma_decode(uintptr_t paddr, struct fb_info *info)
{
	void *vaddr = NULL;
	long pos = 0;
	u8 *out = NULL;
	int ret = -1;
	struct lzma_header lzma_head;
	struct bmp_header bmp_header;
	char *bmp_data = NULL;

	if (!paddr || !info) {
		LCDFB_WRN("get null pointer with paddr or info\n");
		goto OUT;
	}

	vaddr = (void *)sunxi_lcd_fb_fb_map_kernel(paddr, sizeof(struct lzma_header));
	if (vaddr == NULL) {
		LCDFB_WRN("fb_map_kernel failed, paddr=0x%p,size=0x%x\n", (void *)paddr,
			  (unsigned int)sizeof(struct lzma_header));
		goto OUT;
	}

	memcpy(&lzma_head.signature[0], vaddr, sizeof(struct lzma_header));

	if ((lzma_head.signature[0] != 'L') || (lzma_head.signature[1] != 'Z') ||
	    (lzma_head.signature[2] != 'M') || (lzma_head.signature[3] != 'A')) {
		LCDFB_WRN("this is not a LZMA file\n");
		sunxi_lcd_fb_unmap_kernel(vaddr);
		goto OUT;
	}

	sunxi_lcd_fb_unmap_kernel(vaddr);

	out = kmalloc(lzma_head.original_file_size, GFP_KERNEL | __GFP_ZERO);
	if (!out) {
		LCDFB_WRN("kmalloc outbuffer fail\n");
		goto OUT;
	}

	vaddr = (void *)sunxi_lcd_fb_fb_map_kernel(paddr, lzma_head.file_size +
								  sizeof(struct lzma_header));
	if (vaddr == NULL) {
		LCDFB_WRN("fb_map_kernel failed, paddr=0x%p,size=0x%x\n", (void *)paddr,
			  (unsigned int)(lzma_head.file_size + sizeof(struct lzma_header)));
		goto FREE_OUT;
	}

	LCDFB_NOTE("map bootlogo paddr=0x%x, size=0x%x\n", (u32)paddr,
		   (unsigned int)(lzma_head.file_size + sizeof(struct lzma_header)));

	ret = unlzma((u8 *)(vaddr + sizeof(struct lzma_header)), lzma_head.file_size, NULL, NULL,
		     out, &pos, NULL);
	if (ret) {
		LCDFB_WRN("unlzma fail:%d\n", ret);
		goto UMAPKERNEL;
	}

	memcpy(&bmp_header, out, sizeof(struct bmp_header));

	if ((bmp_header.signature[0] != 'B') || (bmp_header.signature[1] != 'M')) {
		LCDFB_WRN("this is not a bmp picture\n");
		goto UMAPKERNEL;
	}

	bmp_data = (char *)(out + bmp_header.data_offset);

	ret = sunxi_lcd_fb_show_bootlogo_bmp(bmp_data, &bmp_header, info);

UMAPKERNEL:
	sunxi_lcd_fb_unmap_kernel(vaddr);
FREE_OUT:
	kfree(out);
OUT:
	return ret;
}
#endif

int sunxi_lcd_fb_logo_parse(struct fb_info *info)
{
	void *vaddr = NULL;
	int ret = 0;
	uintptr_t paddr = 0;
	char *bmp_data = NULL;
	struct bmp_pad_header bmp_pad_header;
	struct bmp_header *bmp_header;
	unsigned int x, y, bmp_bpix;
	uintptr_t offset;

	paddr = disp_reserve_base;
	if (!paddr || !disp_reserve_size || !info || !info->screen_base) {
		LCDFB_INF("sunxi_lcd_fb_map_kernel_logo failed! skip mapping.");
		return -1;
	}

	/* parser bmp header */
	offset = paddr & ~PAGE_MASK;
	vaddr = (void *)sunxi_lcd_fb_fb_map_kernel(paddr, sizeof(struct bmp_header));
	if (vaddr == NULL) {
		LCDFB_WRN("fb_map_kernel failed, paddr=0x%p,size=0x%x\n", (void *)paddr,
			  (unsigned int)sizeof(struct bmp_header));
		ret = -1;
		goto exit;
	}

	memcpy(&bmp_pad_header.signature[0], vaddr + offset, sizeof(struct bmp_header));
	bmp_header = (struct bmp_header *)&bmp_pad_header.signature[0];
	if ((bmp_header->signature[0] != 'B') || (bmp_header->signature[1] != 'M')) {
#if IS_ENABLED(CONFIG_AW_LCD_FB_DECOMPRESS_LZMA)
		sunxi_lcd_fb_unmap_kernel(vaddr);
		LCDFB_NOTE("disp reserve is not a bmp picture, try to lzma decompress.");
		return sunxi_lcd_fb_lzma_decode(paddr, info);
#else
		LCDFB_WRN("this is not a bmp picture.\n");
		ret = -1;
		goto exit;
#endif
	}

	if (bmp_header->file_size > disp_reserve_size) {
		LCDFB_WRN("logo bmp size %d is bigger then reserve_size %d, refuse to map logo\n",
			  bmp_header->file_size, disp_reserve_size);
		ret = -1;
		goto exit;
	}

	x = bmp_header->width;
	y = (bmp_header->height & 0x80000000) ? (-bmp_header->height) : (bmp_header->height);
	if (x != info->var.xres || y != info->var.yres) {
		LCDFB_INF("Bmp  [%u x %u] is not equal to fb[%d x %d], set to center\n", x, y,
			  info->var.xres, info->var.yres);
	}

	if ((paddr <= 0) || x <= 1 || y <= 1) {
		LCDFB_WRN("kernel logo para error!\n");
		ret = -1;
		goto exit;
	}

	/* map the total bmp buffer */
	vaddr = (void *)sunxi_lcd_fb_fb_map_kernel(paddr, disp_reserve_size);
	if (vaddr == NULL) {
		LCDFB_WRN("fb_map_kernel failed, paddr=0x%p,size=0x%x\n", (void *)paddr,
			  (unsigned int)(x * y * bmp_bpix + sizeof(struct bmp_header)));
		ret = -1;
		goto exit;
	}

	bmp_data = (char *)(vaddr + bmp_header->data_offset + offset);

	sunxi_lcd_fb_show_bootlogo_bmp(bmp_data, bmp_header, info);
exit:
	sunxi_lcd_fb_unmap_kernel(vaddr);
	return ret;
}

int sunxi_lcd_fb_logo_free_reserve(void)
{
#if !defined(MODULE)
	if (!disp_reserve_size || !disp_reserve_base)
		return -EINVAL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) && !defined(MODULE)
	memblock_free(__va(disp_reserve_base), disp_reserve_size);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0) && defined(CONFIG_ARCH_KEEP_MEMBLOCK)
	memblock_free(disp_reserve_base, disp_reserve_size);
#endif
#endif
	return 0;
}
