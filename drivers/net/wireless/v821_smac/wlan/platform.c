/*
 * platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013, XRadio
 * Author: XRadio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/regulator/consumer.h>
//#include <asm/mach-types.h>
//#include <mach/sys_config.h>
#ifdef CONFIG_DRIVER_V821
#include <sunxi-sid.h>
#include <sunxi-smc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include "xradio.h"
#include "platform.h"
#include "sbus.h"
#include "hwio.h"
#include <linux/gpio.h>
#include <sunxi-gpio.h>
#include <linux/types.h>
//#include <linux/power/scenelock.h>
//#include <linux/power/aw_pm.h>
#include <linux/pm_wakeirq.h>

MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xradio_wlan");

#ifndef CONFIG_DRIVER_V821
extern void wlan_set_reset_pin(bool state);
extern void sunxi_wlan_set_power(bool on);
extern int sunxi_wlan_get_bus_index(void);
extern int sunxi_wlan_get_oob_irq(int *, int *);
static int wlan_bus_id;
#ifdef CONFIG_XRADIO_USE_GPIO_IRQ
static u32 gpio_irq_handle;
static int irq_flags, wakeup_enable;
#endif

int xradio_get_syscfg(void)
{
	int wlan_bus_index = 0;
	wlan_bus_index = sunxi_wlan_get_bus_index();
	if (wlan_bus_index < 0)
		return wlan_bus_index;
	else
		wlan_bus_id = wlan_bus_index;
	return wlan_bus_index;
}
/*********************Interfaces called by xradio core. *********************/
int xradio_plat_init(void)
{
	int ret = xradio_get_syscfg();
	if (ret < 0)
		return ret;

	xradio_dbg(XRADIO_DBG_ALWY, "Force remove card first\n");
	wlan_set_reset_pin(0);
	MCI_RESCAN_CARD(wlan_bus_id);
	return 0;
}

int xradio_wlan_power(int on)
{
	wlan_set_reset_pin(on);
	mdelay(100);
	return 0;
}


void xradio_plat_deinit(void)
{
;
}

void xradio_sdio_detect(int enable)
{
	MCI_RESCAN_CARD(wlan_bus_id);
	xradio_dbg(XRADIO_DBG_ALWY, "%s SDIO card %d\n",
				enable ? "Detect" : "Remove", wlan_bus_id);
	mdelay(10);
}
#else
int xradio_get_syscfg(void)
{
	return 0;
}

void xradio_plat_deinit(void)
{
;
}

int xradio_plat_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_XRADIO_USE_GPIO_IRQ
static irqreturn_t xradio_gpio_irq_handler(int irq, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return IRQ_HANDLED;
}

int xradio_request_gpio_irq(struct device *dev, void *sbus_priv)
{
	int ret = -1;
	gpio_irq_handle = sunxi_wlan_get_oob_irq(&irq_flags, &wakeup_enable);
	ret = devm_request_irq(dev, gpio_irq_handle,
					(irq_handler_t)xradio_gpio_irq_handler,
					irq_flags, "xradio_irq", sbus_priv);
	if (IS_ERR_VALUE((unsigned long)ret)) {
			gpio_irq_handle = 0;
			xradio_dbg(XRADIO_DBG_ERROR, "%s: request_irq FAIL!ret=%d\n",
					__func__, ret);
	}

	if (wakeup_enable) {
		ret = device_init_wakeup(dev, true);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "device init wakeup failed!\n");
			return ret;
		}

		ret = dev_pm_set_wake_irq(dev, gpio_irq_handle);
		if (ret < 0) {
			xradio_dbg(XRADIO_DBG_ERROR, "can't enable wakeup src!\n");
			return ret;
		}
	}
	return ret;
}

void xradio_free_gpio_irq(struct device *dev, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	if (wakeup_enable) {
		device_init_wakeup(dev, false);
		dev_pm_clear_wake_irq(dev);
	}
	devm_free_irq(dev, gpio_irq_handle, self);
	gpio_irq_handle = 0;
}
#endif

#ifdef CONFIG_DRIVER_V821

//#define XRADIO_EFUSE_DBG

#define EFPG_DCXO_TRIM_FLAG_START       (1216)
#define EFPG_DCXO_TRIM_FLAG_NUM         (2)
#define EFPG_DCXO_TRIM_LEN              (8)
#define EFPG_DCXO_TRIM_START            (352)
#define EFPG_DCXO_TRIM_NUM              (EFPG_DCXO_TRIM_LEN)
#define EFPG_DCXO_TRIM1_START           (1218)
#define EFPG_DCXO_TRIM1_NUM             (EFPG_DCXO_TRIM_LEN)
#define EFPG_DCXO_TRIM2_START           (EFPG_DCXO_TRIM1_START + EFPG_DCXO_TRIM1_NUM)
#define EFPG_DCXO_TRIM2_NUM             (EFPG_DCXO_TRIM_LEN)

#define EFPG_WLAN_POUT_FLAG_START       (1234)
#define EFPG_WLAN_POUT_FLAG_NUM         (2)
#define EFPG_WLAN_POUT_LEN              (7)
#define EFPG_WLAN_POUT1_START           (1236)
#define EFPG_WLAN_POUT1_NUM             (EFPG_WLAN_POUT_LEN * 3)
#define EFPG_WLAN_POUT2_START           (EFPG_WLAN_POUT1_START + EFPG_WLAN_POUT1_NUM)
#define EFPG_WLAN_POUT2_NUM             (EFPG_WLAN_POUT_LEN * 3)

#define EFPG_MAC_WLAN_ADDR_NUM          (2)
#define EFPG_MAC_WLAN_ADDR_FLAG_START   (510)
#define EFPG_MAC_WLAN_ADDR_FLAG_NUM     (EFPG_MAC_WLAN_ADDR_NUM)
#define EFPG_MAC_WLAN_ADDR_LEN          (48)
#if (EFPG_MAC_WLAN_ADDR_NUM >= 1)
#define EFPG_MAC_WLAN_ADDR1_START       (EFPG_MAC_WLAN_ADDR_FLAG_START + EFPG_MAC_WLAN_ADDR_FLAG_NUM)
#define EFPG_MAC_WLAN_ADDR1_NUM         (EFPG_MAC_WLAN_ADDR_LEN)
#endif
#if (EFPG_MAC_WLAN_ADDR_NUM >= 2)
#define EFPG_MAC_WLAN_ADDR2_START       (EFPG_MAC_WLAN_ADDR1_START + EFPG_MAC_WLAN_ADDR1_NUM)
#define EFPG_MAC_WLAN_ADDR2_NUM         (EFPG_MAC_WLAN_ADDR_LEN)
#endif

#define EFPG_WLAN_OEM_START             (1216)
#define EFPG_WLAN_OEM_LEN               (448)

#define EFUSE_WMAC_SEL_FLAG_NAME        "wmac_sel"
#define EFUSE_WMAC_ADDR1_NAME           "wmac_addr1"
#define EFUSE_WMAC_ADDR2_NAME           "wmac_addr2"

#define EFUSE_TEMP_DATA_BYTE  EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_OEM_LEN)
#define EFPG_BITS_TO_BYTE_CNT(bits)        (((bits) + 7) / 8)

typedef struct efpg_region_info {
	u16 flag_start;       /* bit */
	u16 flag_bits;        /* MUST less than 32-bit */
	u16 data_start;       /* bit */
	u16 data_bits;
	u8  *buf;             /* temp buffer for write to save read back data */
	u8  buf_len;          /* byte, MUST equal to ((data_bits + 7) / 8) */
	// key info
	u16 key_start;       /* bit */
	u16 key_len;         /* byte */
	char *key_name;
	u16 key_flag_start;  /* bit */
	u16 key_flag_len;    /* byte */
	char *key_flag_name;
} efpg_region_info_t;

struct xradio_efuse_data {
	sunxi_efuse_key_info_t temp_key;
	u8 temp_data[EFUSE_TEMP_DATA_BYTE];
};

static struct xradio_efuse_data *efuse_data_info;
static void __iomem *efuse_addr;
static u8 efuse_area_in_use;

u8 xradio_get_efuse_area_in_use(void)
{
	return efuse_area_in_use;
}

void xradio_set_efuse_area_in_use(u8 val)
{
	efuse_area_in_use = val;
}

static int xradio_call_efuse_write(char *name, u8 *data, u8 len)
{
	int ret;

	if ((sizeof(name) > sizeof(efuse_data_info->temp_key.name)) || (len > sizeof(efuse_data_info->temp_data)))
		return -EINVAL;

	//strncpy(efuse_data_info->temp_key.name, name, sizeof(efuse_data_info->temp_key.name));
	snprintf(efuse_data_info->temp_key.name, sizeof(efuse_data_info->temp_key.name), "%s", name);
	efuse_data_info->temp_key.len = len;
	efuse_data_info->temp_key.offset = 0;
	memcpy(efuse_data_info->temp_data, data, len);
	efuse_data_info->temp_key.key_data = virt_to_phys((void *)efuse_data_info->temp_data);

#ifdef XRADIO_EFUSE_DBG
	xradio_dbg(XRADIO_DBG_ALWY, "%s %d the virt addr is 0x%x\n", __func__, __LINE__,
		(u32)&efuse_data_info->temp_key);
	xradio_dbg(XRADIO_DBG_ALWY, "%s %d the phy  addr is 0x%lx\n", __func__, __LINE__,
		(unsigned long)virt_to_phys(&efuse_data_info->temp_key));
	xradio_dbg(XRADIO_DBG_ALWY, "key name %s %s len %d\n", name,
		efuse_data_info->temp_key.name, efuse_data_info->temp_key.len);
	print_hex_dump(KERN_ERR, "data: ", DUMP_PREFIX_OFFSET, 16, 1,
		efuse_data_info->temp_data, efuse_data_info->temp_key.len, 0);
#endif

	ret = sbi_efuse_write(virt_to_phys((volatile void *)&efuse_data_info->temp_key));
#ifdef XRADIO_EFUSE_DBG
	xradio_dbg(XRADIO_DBG_ALWY, "%s after write dump all efuse\n", __func__);
	print_hex_dump(KERN_ERR, "efsue: ", DUMP_PREFIX_OFFSET, 16, 1, efuse_addr, 256, 0);
#endif

	return ret;
}

#if 0
static int etf_call_efuse_read(char *name, u8 *buf, u8 len)
{
	return sunxi_efuse_readn(name, buf, len);
}
#endif

static __inline void xradio_efuse_sram_read(u8 index, u32 *pData)
{
	u32 *VALUE = efuse_addr;

	*pData = VALUE[index];
}

static int xradio_efuse_read(u32 start_bit, u32 bit_num, u8 *data)
{
	u32 start_word = start_bit / 32;
	u8 word_shift = start_bit % 32;
	u8 word_cnt = (start_bit + bit_num) / 32 - start_word;
	u32 *sid_data;
	int i;

	if ((start_bit + bit_num) % 32)
		word_cnt++;

	if (!efuse_addr) {
		xradio_dbg(XRADIO_DBG_WARN, "xradio_efuse_read: base addr is 0.\n");
		return -1;
	}

	sid_data = kzalloc(word_cnt * 4, GFP_KERNEL);
	if (sid_data == NULL)
		return -1;

	for (i = 0; i < word_cnt; i++) {
		xradio_efuse_sram_read(start_word + i, sid_data + i);
	}
	if (word_shift) {
		for (i = 0; i < (word_cnt - 1); i++) {
			sid_data[i] = (sid_data[i] >> word_shift) | (sid_data[i + 1] << (32 - word_shift));
		}
		sid_data[i] = (sid_data[i] >> word_shift);
	}
	((u8 *)sid_data)[(bit_num - 1) / 8] &= ((1 << (bit_num % 8 == 0 ? 8 : bit_num % 8)) - 1);

	memcpy(data, (u8 *)sid_data, (bit_num + 7) / 8);
	kfree(sid_data);

	return 0;
}

#if 0
static u16 xradio_efuse_read_region(efpg_region_info_t *info, u8 *data)
{
	u8 idx = 0;
	u8 flag;
	u32 start_bit;

	/* flag */
	if (xradio_efuse_read(info->flag_start, info->flag_bits, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << info->flag_bits) - 1);
	xradio_dbg(XRADIO_DBG_ALWY, "r start %d, bits %d, flag 0x%x\n", info->flag_start, info->flag_bits, flag);
	if ((flag == 0) || (flag > ((1 << info->flag_bits) - 1))) {
		xradio_dbg(XRADIO_DBG_WARN, "%s(), flag (%d, %d) = 0x%x is invalid\n", __func__,
			info->flag_start, info->flag_bits, flag);
		return EFUSE_STATUS_CODE__NODATA_ERR;
	}
	while ((flag & 0x1) != 0) {
		flag = flag >> 1;
		idx++;
	}
	start_bit = info->data_start + (idx - 1) * info->data_bits;

	/* data */
	xradio_dbg(XRADIO_DBG_ALWY, "r data, start %d, bits %d\n", start_bit, info->data_bits);
	if (xradio_efuse_read(start_bit, info->data_bits, data)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}

	return EFUSE_STATUS_CODE__SUCCESS;
}
#endif

static u16 xradio_efuse_write(u32 start_bit, u32 bit_num, u8 *data, char *key_name, u8 key_len)
{
	u8 *p_data = data;
	u32 bit_shift = start_bit & (32 - 1);
	u32 word_idx = start_bit >> 5;

	u64 buf = 0;
	u32 *efuse_word = (u32 *)&buf;
	u32 bit_cnt = bit_num;
	u32 *efuse = (void *)efuse_data_info->temp_data;
	int ret;

	if ((data == NULL) || (bit_num == 0) || (bit_num > EFUSE_TEMP_DATA_BYTE) ||
		(EFPG_BITS_TO_BYTE_CNT(start_bit + bit_num) > EFUSE_TEMP_DATA_BYTE)) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s check fail: start bit %u, bit num %u, data %p\n", __func__,
			start_bit, bit_num, data);
		return EFUSE_STATUS_CODE__RW_ERR;
	}

#ifdef XRADIO_EFUSE_DBG
	xradio_dbg(XRADIO_DBG_ALWY, "pre change key %s len %d:\n", key_name, key_len);
	print_hex_dump(KERN_ERR, "efsue: ", DUMP_PREFIX_OFFSET, 16, 1, efuse, key_len, 0);
#endif

	data[(bit_num - 1) / 8] &= ((1 << (bit_num % 8 == 0 ? 8 : bit_num % 8)) - 1);
	memcpy(&efuse_word[1], p_data, sizeof(efuse_word[1]));
	if (bit_cnt < 32)
		efuse_word[1] &= (1U << bit_cnt) - 1;
	efuse_word[1] = efuse_word[1] << bit_shift;

	efuse[word_idx] |= efuse_word[1];

	word_idx++;
	bit_cnt -= (bit_cnt <= 32 - bit_shift) ? bit_cnt : 32 - bit_shift;

	while (bit_cnt > 0) {
		memcpy(&buf, p_data, sizeof(buf));
		buf = buf << bit_shift;
		if (bit_cnt < 32)
			efuse_word[1] &= (1U << bit_cnt) - 1;

		efuse[word_idx] |= efuse_word[1];

		word_idx++;
		p_data += 4;
		bit_cnt -= (bit_cnt <= 32) ? bit_cnt : 32;
	}

#ifdef XRADIO_EFUSE_DBG
	xradio_dbg(XRADIO_DBG_ALWY, "pre write key %s len %d:\n", key_name, key_len);
	print_hex_dump(KERN_ERR, "key  : ", DUMP_PREFIX_OFFSET, 16, 1, efuse, key_len, 0);
#endif

	ret = xradio_call_efuse_write(key_name, (u8 *)efuse, key_len);
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_call_efuse_write key %s len %d fail %d\n",
			key_name, key_len, ret);
		return EFUSE_STATUS_CODE__RW_ERR;
	}

	return EFUSE_STATUS_CODE__SUCCESS;
}

static u16 xradio_efuse_write_region(efpg_region_info_t *info, u8 *data)
{
	u8 idx = 0;
	u8 flag;
	u32 start_bit;
	u8 w_tmp;

	if ((info->flag_start < info->key_flag_start) ||
		(info->data_start < info->key_start)) {
		xradio_dbg(XRADIO_DBG_ERROR, "check fail: flag:start %u key start %u, data:start %u key start %u\n",
			info->flag_start, info->key_flag_start, info->data_start, info->key_start);
		return EFUSE_STATUS_CODE__INVALID_PARAMS;
	}
	if (info->key_len > sizeof(efuse_data_info->temp_data)) {
		xradio_dbg(XRADIO_DBG_ERROR, "check fail: key len %u, buf size %u\n",
			info->key_len, sizeof(efuse_data_info->temp_data));
		return EFUSE_STATUS_CODE__INVALID_PARAMS;
	}

	/* read flag */
	if (xradio_efuse_read(info->flag_start, info->flag_bits, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << info->flag_bits) - 1);
	xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d, flag 0x%x\n", info->flag_start, info->flag_bits, flag);
	memset(info->buf, 0, info->buf_len);
	memset(efuse_data_info->temp_data, 0, sizeof(efuse_data_info->temp_data));

	if (flag < (1 << info->flag_bits)) {
		if (flag == 0) {
			idx = 0;
			w_tmp = 1 << idx;

			if (xradio_efuse_read(info->key_flag_start, info->key_flag_len * 8, (u8 *)efuse_data_info->temp_data)) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
			if ((xradio_efuse_write(info->flag_start - info->key_flag_start, info->flag_bits,
				&w_tmp, info->key_flag_name, info->key_flag_len))) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
		} else {
			while (flag != 0) {
				flag = flag >> 1;
				idx++;
			}
			idx--;
		}

		xradio_dbg(XRADIO_DBG_NIY, "idx:%d flag_bits:%d\n", idx, info->flag_bits);

		while (idx < info->flag_bits) {
			xradio_dbg(XRADIO_DBG_NIY, "idx:%d\n", idx);
			/* will try compare old data with new */
			start_bit = info->data_start + idx * info->data_bits;
			memset(info->buf, 0, info->buf_len);
			if (xradio_efuse_read(start_bit, info->data_bits, info->buf) == 0) {
				int i;

				if (info->data_bits % 8) {
					info->buf[info->buf_len - 1] &= ((1 << (info->data_bits % 8)) - 1);
				}
				for (i = 0; i < info->buf_len; i++) {
					if (((info->buf[i] ^ data[i]) & info->buf[i]) != 0) {
						xradio_dbg(XRADIO_DBG_NIY, "compare fail i:%d data %x %x\n", i, info->buf[i], data[i]);
						break;
					}
				}
				if (i == info->buf_len) {
					memset(info->buf, 0, info->buf_len);
					xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d\n", start_bit, info->data_bits);
					if (xradio_efuse_read(info->key_start, info->key_len, (u8 *)efuse_data_info->temp_data)) {
						return EFUSE_STATUS_CODE__RW_ERR;
					}
					if ((xradio_efuse_write(start_bit - info->key_start, info->data_bits, data,
						info->key_name, info->key_len) == 0)) {
						return EFUSE_STATUS_CODE__SUCCESS;
					}
				}
			}

			if (++idx > info->flag_bits - 1) {
				xradio_dbg(XRADIO_DBG_WARN, "region has no space\n");
				return EFUSE_STATUS_CODE__DI_ERR;
			}
			/* update flag as next */
			w_tmp = 1 << idx;
			memset(info->buf, 0, info->buf_len);
			xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d, w_tmp 0x%x\n", info->flag_start, info->flag_bits, w_tmp);
			if (xradio_efuse_read(info->key_flag_start, info->key_flag_len, (u8 *)efuse_data_info->temp_data)) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
			if ((xradio_efuse_write(info->flag_start - info->key_flag_start, info->flag_bits,
				&w_tmp, info->key_flag_name, info->key_flag_len))) {
				xradio_dbg(XRADIO_DBG_WARN, "fail to write flag\n");
				return EFUSE_STATUS_CODE__RW_ERR;
			}
		}
	}

	xradio_dbg(XRADIO_DBG_WARN, "flag (%d, %d) = 0x%x is invalid, idx:%d\n", info->flag_start, info->flag_bits, flag, idx);
	return EFUSE_STATUS_CODE__NODATA_ERR;
}

int xradio_rw_efuse_init(void)
{
	int ret = 1;
	struct device_node *dev_node = NULL;
	struct resource efuse_ram_res = {0};

	if (efuse_addr && efuse_data_info) {
		xradio_dbg(XRADIO_DBG_WARN, "xradio_rw_efuse_init already init.\n");
		return 0;
	}
	efuse_addr = NULL;
	efuse_data_info = kzalloc(sizeof(struct xradio_efuse_data), GFP_KERNEL);
	if (!efuse_data_info) {
		xradio_dbg(XRADIO_DBG_ERROR, "no mem.\n");
		return -ENOMEM;
	}

	dev_node = of_find_compatible_node(NULL, NULL, EFUSE_SID_BASE);
	if (IS_ERR_OR_NULL(dev_node)) {
		xradio_dbg(XRADIO_DBG_ERROR, "Failed to find \"%s\" in dts.\n", EFUSE_SID_BASE);
		kfree(efuse_data_info);
		efuse_data_info = NULL;
		return -ENOMEM;
	}
	/* get efuse_ram_reserved from dts for efuse sram read */
	efuse_ram_res.start = 0;
	dev_node = of_parse_phandle(dev_node, "memory-region", 0);
	if (dev_node) {
		ret = of_address_to_resource(dev_node, 0, &efuse_ram_res);
	}
	if (efuse_ram_res.start) {
		void __iomem *start;

		start = ioremap(efuse_ram_res.start, resource_size(&efuse_ram_res));
		if (IS_ERR_OR_NULL(start))
			return -ENOMEM;
		efuse_addr = start;
#ifdef XRADIO_EFUSE_DBG
		xradio_dbg(XRADIO_DBG_ALWY, "%s dump efuse, addr 0x%x\n", __func__, (u32)efuse_ram_res.start);
		print_hex_dump(KERN_ERR, "efsue: ", DUMP_PREFIX_OFFSET, 16, 1, start, 256, 0);
#endif
		return 0;
	}
	xradio_dbg(XRADIO_DBG_ERROR, "Failed to get efuse %d.\n", ret);
	kfree(efuse_data_info);
	efuse_data_info = NULL;

	return ret;
}

void xradio_rw_efuse_deinit(void)
{
	if (efuse_addr) {
		iounmap(efuse_addr);
		efuse_addr = NULL;
	}
	if (efuse_data_info) {
		kfree(efuse_data_info);
		efuse_data_info = NULL;
	}
}

u16 xradio_efuse_read_dcxo(u8 *freq_offset)
{
	u8 flag, data, idx = 0;
	u32 start_bit;

	/* flag */
	if (xradio_efuse_read(EFPG_DCXO_TRIM_FLAG_START, EFPG_DCXO_TRIM_FLAG_NUM, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << EFPG_DCXO_TRIM_FLAG_NUM) - 1);
	xradio_dbg(XRADIO_DBG_NIY, "r start %d, bits %d, flag 0x%x\n", EFPG_DCXO_TRIM_FLAG_START, EFPG_DCXO_TRIM_FLAG_NUM, flag);

	if (flag == 0)
		return EFUSE_STATUS_CODE__NODATA_ERR;
	else if (flag == 1)
		start_bit = EFPG_DCXO_TRIM1_START;
	else if (flag == 3)
		start_bit = EFPG_DCXO_TRIM2_START;
	else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s(), flag (%d, %d) = 0x%x is invalid\n",
			__func__, EFPG_DCXO_TRIM_FLAG_START, EFPG_DCXO_TRIM_FLAG_NUM, flag);
		return EFUSE_STATUS_CODE__NOTMATCH_ERR;
	}

	while (flag) {
		flag = flag >> 1;
		idx++;
	}
	efuse_area_in_use = idx;
	xradio_dbg(XRADIO_DBG_NIY, "flag 0x%x idx:%d\n", flag, idx);

	/* data */
	data = 0;
	xradio_dbg(XRADIO_DBG_NIY, "r data, start %d, bits %d\n", start_bit, EFPG_DCXO_TRIM_LEN);
	if (xradio_efuse_read(start_bit, EFPG_DCXO_TRIM_LEN, &data)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	xradio_dbg(XRADIO_DBG_NIY, "efuse dcxo:%d\n", data);
	if (data == 0)
		return EFUSE_STATUS_CODE__NODATA_ERR;
	*freq_offset = data;

	return EFUSE_STATUS_CODE__SUCCESS;
}

u16 xradio_efuse_write_dcxo(u8 *data)
{
	efpg_region_info_t info;
	u8 buf[EFPG_BITS_TO_BYTE_CNT(EFPG_DCXO_TRIM_LEN)] = {0};
	char key_name[] = {EFUSE_OEM_NAME};

	info.flag_start = EFPG_DCXO_TRIM_FLAG_START;
	info.flag_bits = EFPG_DCXO_TRIM_FLAG_NUM;
	info.data_start = EFPG_DCXO_TRIM1_START;
	info.data_bits = EFPG_DCXO_TRIM1_NUM;
	info.buf = buf;
	info.buf_len = sizeof(buf);

	info.key_start = EFPG_WLAN_OEM_START;
	info.key_len = EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_OEM_LEN);
	info.key_name = key_name;

	info.key_flag_start = EFPG_WLAN_OEM_START;
	info.key_flag_len = EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_OEM_LEN);
	info.key_flag_name = key_name;

	xradio_dbg(XRADIO_DBG_NIY, "%s Flag: key name %s len %d start %d\n", __func__,
		info.key_flag_name, info.key_flag_len, info.key_flag_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: key name %s len %d start %d\n", __func__,
		info.key_name, info.key_len, info.key_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: %d\n", __func__, data[0]);

	return xradio_efuse_write_region(&info, data);
}

u16 xradio_efuse_read_wlan_pout(u8 *pout)
{
	u8 flag, data[3] = {0}, idx = 0;
	u32 start_bit;

	/* flag */
	if (xradio_efuse_read(EFPG_WLAN_POUT_FLAG_START, EFPG_WLAN_POUT_FLAG_NUM, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << EFPG_WLAN_POUT_FLAG_NUM) - 1);
	xradio_dbg(XRADIO_DBG_MSG, "r start %d, bits %d, flag 0x%x\n", EFPG_WLAN_POUT_FLAG_START, EFPG_WLAN_POUT_FLAG_NUM, flag);

	if (flag == 0) {
		return EFUSE_STATUS_CODE__NODATA_ERR;
	} else if (flag == 1) {
		start_bit = EFPG_WLAN_POUT1_START;
	} else if (flag == 3) {
		start_bit = EFPG_WLAN_POUT2_START;
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s(), flag (%d, %d) = 0x%x is invalid\n",
		      __func__, EFPG_WLAN_POUT_FLAG_START, EFPG_WLAN_POUT_FLAG_NUM, flag);
		return EFUSE_STATUS_CODE__NODATA_ERR;
	}

	while (flag) {
		flag = flag >> 1;
		idx++;
	}
	efuse_area_in_use = idx;
	xradio_dbg(XRADIO_DBG_NIY, "flag 0x%x idx:%d\n", flag, idx);

	/* data */
	xradio_dbg(XRADIO_DBG_NIY, "r data, start %d, bits %d\n", start_bit, EFPG_WLAN_POUT1_NUM);
	if (xradio_efuse_read(start_bit, EFPG_WLAN_POUT1_NUM, data)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	xradio_dbg(XRADIO_DBG_NIY, "efuse wlan pout:0x%x 0x%x 0x%x\n", data[0], data[1], data[2]);

	pout[0] = data[0];
	pout[1] = data[1];
	pout[2] = data[2];

	return EFUSE_STATUS_CODE__SUCCESS;
}

u16 xradio_efuse_write_wlan_pout(u8 *data)
{
	efpg_region_info_t info;
	u8 buf[EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_POUT1_NUM)] = {0};
	char key_name[] = {EFUSE_OEM_NAME};

	info.flag_start = EFPG_WLAN_POUT_FLAG_START;
	info.flag_bits = EFPG_WLAN_POUT_FLAG_NUM;
	info.data_start = EFPG_WLAN_POUT1_START;
	info.data_bits = EFPG_WLAN_POUT1_NUM;
	info.buf = buf;
	info.buf_len = sizeof(buf);

	info.key_start = EFPG_WLAN_OEM_START;
	info.key_len = EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_OEM_LEN);
	info.key_name = key_name;

	info.key_flag_start = EFPG_WLAN_OEM_START;
	info.key_flag_len = EFPG_BITS_TO_BYTE_CNT(EFPG_WLAN_OEM_LEN);
	info.key_flag_name = key_name;

	xradio_dbg(XRADIO_DBG_NIY, "%s Flag: key name %s len %d start %d\n", __func__,
		info.key_flag_name, info.key_flag_len, info.key_flag_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: key name %s len %d start %d\n", __func__,
		info.key_name, info.key_len, info.key_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: %x %x %x\n", __func__,
		data[0], data[1], data[2]);

	return xradio_efuse_write_region(&info, data);
}

u16 xradio_efuse_read_wlan_mac(u8 *buf)
{
	u8 flag, wmac1[6] = {0}, idx = 0;
	u32 start_bit;

	/* flag */
	if (xradio_efuse_read(EFPG_MAC_WLAN_ADDR_FLAG_START, EFPG_MAC_WLAN_ADDR_FLAG_NUM, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << EFPG_MAC_WLAN_ADDR_FLAG_NUM) - 1);
	xradio_dbg(XRADIO_DBG_NIY, "r start %d, bits %d, flag 0x%x\n", EFPG_MAC_WLAN_ADDR_FLAG_START, EFPG_MAC_WLAN_ADDR_FLAG_NUM, flag);

	while (flag) {
		flag = flag >> 1;
		idx++;
	}
	efuse_area_in_use = idx;
	xradio_dbg(XRADIO_DBG_NIY, "flag 0x%x idx:%d\n", flag, idx);

	if (flag == 0) {
		u8 wmac2[6] = {0};

		start_bit = EFPG_MAC_WLAN_ADDR1_START;
		/* data */
		xradio_dbg(XRADIO_DBG_NIY, "r data, start %d, bits %d\n", start_bit, EFPG_MAC_WLAN_ADDR1_NUM);
		if (xradio_efuse_read(start_bit, EFPG_MAC_WLAN_ADDR1_NUM, wmac1)) {
			return EFUSE_STATUS_CODE__RW_ERR;
		}

		start_bit = EFPG_MAC_WLAN_ADDR2_START;
		/* data */
		xradio_dbg(XRADIO_DBG_NIY, "r data, start %d, bits %d\n", start_bit, EFPG_MAC_WLAN_ADDR1_NUM);
		if (xradio_efuse_read(start_bit, EFPG_MAC_WLAN_ADDR1_NUM, wmac2)) {
			return EFUSE_STATUS_CODE__RW_ERR;
		}

		if ((wmac2[0] == 0) && (wmac2[1] == 0) && (wmac2[2] == 0) && (wmac2[3] == 0) &&
			(wmac2[4] == 0) && (wmac2[5] == 0)) {

			if ((wmac1[0] == 0) && (wmac1[1] == 0) && (wmac1[2] == 0) && (wmac1[3] == 0) &&
				(wmac1[4] == 0) && (wmac1[5] == 0)) {
				return EFUSE_STATUS_CODE__NODATA_ERR;
			} else {
				// sel MAC addr1
				memcpy(buf, wmac1, sizeof(wmac1));
				xradio_dbg(XRADIO_DBG_NIY, "%02x:%02x:%02x:%02x:%02x:%02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

				return EFUSE_STATUS_CODE__SUCCESS;
			}
		} else {
			// sel MAC addr2
			memcpy(buf, wmac2, sizeof(wmac2));
			xradio_dbg(XRADIO_DBG_NIY, "%02x:%02x:%02x:%02x:%02x:%02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

			return EFUSE_STATUS_CODE__SUCCESS;
		}

	} else if (flag == 1) {
		start_bit = EFPG_MAC_WLAN_ADDR1_START;
	} else if (flag & 0x2) {
		start_bit = EFPG_MAC_WLAN_ADDR2_START;
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s(), flag (%d, %d) = 0x%x is invalid\n",
			__func__, EFPG_MAC_WLAN_ADDR_FLAG_START, EFPG_MAC_WLAN_ADDR_FLAG_NUM, flag);
		return EFUSE_STATUS_CODE__NODATA_ERR;
	}

	/* data */
	xradio_dbg(XRADIO_DBG_NIY, "r data, start %d, bits %d\n", start_bit, EFPG_MAC_WLAN_ADDR1_NUM);
	if (xradio_efuse_read(start_bit, EFPG_MAC_WLAN_ADDR1_NUM, wmac1)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}

	if ((wmac1[0] == 0) && (wmac1[1] == 0) && (wmac1[2] == 0) && (wmac1[3] == 0) &&
		(wmac1[4] == 0) && (wmac1[5] == 0)) {
		return EFUSE_STATUS_CODE__NODATA_ERR;
	} else {
		memcpy(buf, wmac1, sizeof(wmac1));
		xradio_dbg(XRADIO_DBG_NIY, "%02x:%02x:%02x:%02x:%02x:%02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
		return EFUSE_STATUS_CODE__SUCCESS;
	}
}

static u16 xradio_efuse_write_mac_region(efpg_region_info_t *info, u8 *data)
{
	u8 idx = 0;
	u8 flag;
	u32 start_bit;
	u8 w_tmp;
	int ret;

	if ((info->flag_start < info->key_flag_start) ||
		(info->data_start < info->key_start)) {
		xradio_dbg(XRADIO_DBG_ERROR, "flag:start %u key start %u, data:start %u key start %u\n",
			info->flag_start, info->key_flag_start, info->data_start, info->key_start);
		return EFUSE_STATUS_CODE__INVALID_PARAMS;
	}
	if (info->key_len > sizeof(efuse_data_info->temp_data)) {
		xradio_dbg(XRADIO_DBG_ERROR, "key len %u, buf size %u\n",
			info->key_len, sizeof(efuse_data_info->temp_data));
		return EFUSE_STATUS_CODE__INVALID_PARAMS;
	}

	/* read flag */
	if (xradio_efuse_read(info->flag_start, info->flag_bits, (u8 *)&flag)) {
		return EFUSE_STATUS_CODE__RW_ERR;
	}
	flag &= ((1 << info->flag_bits) - 1);
	xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d, flag 0x%x\n", info->flag_start, info->flag_bits, flag);
	memset(info->buf, 0, info->buf_len);
	memset(efuse_data_info->temp_data, 0, sizeof(efuse_data_info->temp_data));

	if (flag < (1 << info->flag_bits)) {
		if (flag == 0) {
			idx = 0;
			w_tmp = 1 << idx;

			if (xradio_efuse_read(info->key_flag_start, info->key_flag_len * 8, (u8 *)efuse_data_info->temp_data)) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
			if ((xradio_efuse_write(info->flag_start - info->key_flag_start, info->flag_bits,
				&w_tmp, info->key_flag_name, info->key_flag_len))) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
		} else {
			while (flag != 0) { //while ((flag & 0x1) != 0) {
				flag = flag >> 1;
				idx++;
			}
			idx--;
		}

		xradio_dbg(XRADIO_DBG_NIY, "idx:%d flag_bits:%d\n", idx, info->flag_bits);

		while (idx < info->flag_bits) {
			xradio_dbg(XRADIO_DBG_NIY, "idx:%d\n", idx);
			/* will try compare old data with new */
			start_bit = info->data_start + idx * info->data_bits;
			memset(info->buf, 0, info->buf_len);
			ret = xradio_efuse_read(start_bit, info->data_bits, info->buf);
			if (ret == 0) {
				int i;

				if (info->data_bits % 8) {
					info->buf[info->buf_len - 1] &= ((1 << (info->data_bits % 8)) - 1);
				}
				for (i = 0; i < info->buf_len; i++) {
					if (((info->buf[i] ^ data[i]) & info->buf[i]) != 0) {
						xradio_dbg(XRADIO_DBG_NIY, "compare fail i:%d data %x %x\n", i, info->buf[i], data[i]);
						break;
					}
				}
				if (i == info->buf_len) {
					memset(info->buf, 0, info->buf_len);
					if (idx == 1) {
						char key_name_2[] = {EFUSE_WMAC_ADDR2_NAME};

						// update to MAC addr 2 key
						info->key_start = EFPG_MAC_WLAN_ADDR2_START;
						info->key_len = EFPG_BITS_TO_BYTE_CNT(EFPG_MAC_WLAN_ADDR2_NUM);
						info->key_name = key_name_2;
						xradio_dbg(XRADIO_DBG_NIY, "MAC region update to addr 2, now idx %d\n", idx);
					} else if (idx > 1) {
						xradio_dbg(XRADIO_DBG_ERROR, "MAC max region is 2, now idx %d\n", idx);
						return EFUSE_STATUS_CODE__DI_ERR;
					}

					xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d\n", start_bit, info->data_bits);
					if (xradio_efuse_read(info->key_start, info->key_len, (u8 *)efuse_data_info->temp_data)) {
						return EFUSE_STATUS_CODE__RW_ERR;
					}
					if ((xradio_efuse_write(start_bit - info->key_start, info->data_bits,
						data, info->key_name, info->key_len) == 0)) {
						return EFUSE_STATUS_CODE__SUCCESS;
					}
				}
			} else {
				return EFUSE_STATUS_CODE__RW_ERR;
			}

			if (++idx > info->flag_bits - 1) {
				xradio_dbg(XRADIO_DBG_WARN, "region has no space\n");
				return EFUSE_STATUS_CODE__DI_ERR;
			}
			/* update flag as next */
			w_tmp = 1 << idx;
			memset(info->buf, 0, info->buf_len);
			xradio_dbg(XRADIO_DBG_NIY, "w start %d, bits %d, w_tmp 0x%x\n", info->flag_start, info->flag_bits, w_tmp);
			if (xradio_efuse_read(info->key_flag_start, info->key_flag_len, (u8 *)efuse_data_info->temp_data)) {
				return EFUSE_STATUS_CODE__RW_ERR;
			}
			if ((xradio_efuse_write(info->flag_start - info->key_flag_start, info->flag_bits,
				&w_tmp, info->key_flag_name, info->key_flag_len))) {
				xradio_dbg(XRADIO_DBG_WARN, "fail to write flag\n");
				return EFUSE_STATUS_CODE__RW_ERR;
			}
		}
	}

	xradio_dbg(XRADIO_DBG_WARN, "flag (%d, %d) = 0x%x is invalid, idx:%d\n", info->flag_start, info->flag_bits, flag, idx);
	return EFUSE_STATUS_CODE__NODATA_ERR;
}

u16 xradio_efuse_write_wlan_mac(u8 *data)
{
	efpg_region_info_t info;
	int ret;
	u8 buf[EFPG_BITS_TO_BYTE_CNT(EFPG_MAC_WLAN_ADDR1_NUM)] = {0};
	char key_name_1[] = {EFUSE_WMAC_ADDR1_NAME};
	char key_flag_name[] = {EFUSE_WMAC_SEL_FLAG_NAME};

	info.flag_start = EFPG_MAC_WLAN_ADDR_FLAG_START;
	info.flag_bits = EFPG_MAC_WLAN_ADDR_FLAG_NUM;
	info.data_start = EFPG_MAC_WLAN_ADDR1_START;
	info.data_bits = EFPG_MAC_WLAN_ADDR1_NUM;
	info.buf = buf;
	info.buf_len = sizeof(buf);
	info.key_start = EFPG_MAC_WLAN_ADDR1_START;
	info.key_len = EFPG_BITS_TO_BYTE_CNT(EFPG_MAC_WLAN_ADDR1_NUM);
	info.key_name = key_name_1;

	info.key_flag_start = EFPG_MAC_WLAN_ADDR_FLAG_START / 8 * 8;
	info.key_flag_len = EFPG_BITS_TO_BYTE_CNT(EFPG_MAC_WLAN_ADDR_FLAG_NUM);
	info.key_flag_name = key_flag_name;

	xradio_dbg(XRADIO_DBG_NIY, "%s Flag: key name %s len %d start %d\n", __func__,
		info.key_flag_name, info.key_flag_len, info.key_flag_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: key name %s len %d start %d\n", __func__,
		info.key_name, info.key_len, info.key_start);
	xradio_dbg(XRADIO_DBG_NIY, "%s Data: %x:%x:%x:%x:%x:%x\n", __func__,
		data[0], data[1], data[2], data[3], data[4], data[5]);

	ret = xradio_efuse_write_mac_region(&info, data);

	return ret;
}

int xradio_efuse_info(void *data)
{
	if (xradio_rw_efuse_init()) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s rw efuse init fail!\n", __func__);
		return -1;
	}

	memcpy(data, efuse_addr, EFUSE_AREA_SIZE);
	xradio_rw_efuse_deinit();

	return 0;
}

#define RCOSC_CLOCK         (37500U)        /* 31K ~ 63K */
#define RCOSC_CLOCK_PPM     (350)           /* ppm/deg */
void xradio_get_oshwinfo(struct wsm_sethardware_info *info)
{
	int ret = -1;

#ifdef CONFIG_DRIVER_V821
	info->bg_settling_time = 0;
	info->pfmtopwm_settling_time = 0;
#endif
	info->osc_freq = RCOSC_CLOCK;
	info->osc_type = 0; /* always external */
	info->osc_ppm = RCOSC_CLOCK_PPM;

#ifdef CONFIG_DRIVER_V821
	ret = xradio_efuse_read(0, EFUSE_AREA_SIZE * 8, info->efuse);
#endif
	if (ret != 0) {
		xradio_dbg(XRADIO_DBG_WARN, "xradio_get_oshwinfo:wlan get efuse faild!\n");
		memset(info->efuse, 0, sizeof(info->efuse));
	}
#if defined(XRADIO_DEBUG) && (CONFIG_EFUSE_DUMP_EN)
	print_hex_dump(KERN_ERR, "efsue: ", DUMP_PREFIX_OFFSET, 16, 1, info->efuse, 256, 0);
#endif
	/* use host cfg or not */
	info->use_host_pwr_up_cfg = 0x0;

	/* time unit is us */
	info->ldo_dcxo_set_time = 122;
	info->dcxo_set_time = 1000;

	/* time unit is 1/52MHz, 10us@52Mhz = 10 * 52 */
	info->ldo1_sel_set_time = 10 * 52;
	info->ldo2_sel_set_time = 10 * 52;
	info->ldo2_set_time = 10 * 52;
	info->dpll_set_time = 100 * 52;
	info->rfdig_set_time = 20 * 52;
	info->ldo1_set_time = 10 * 52;
	info->vdd_to_sleep_time = 20;
}


void HAL_CCM_ForceSys3Reset(void)
{
	u32 val = REG_READ_U32(CCM_MOD_RST_CTRL) & ~(CCM_WLAN_RST_BIT);
	REG_WRITE_U32(CCM_MOD_RST_CTRL, val);
}

void HAL_CCM_ReleaseSys3Reset(void)
{
	u32 val = REG_READ_U32(CCM_MOD_RST_CTRL) | CCM_WLAN_RST_BIT;
	REG_WRITE_U32(CCM_MOD_RST_CTRL, val);
}

int HAL_CCM_IsSys3Release(void)
{
	u32 val = REG_READ_U32(CCM_MOD_RST_CTRL) & (CCM_WLAN_RST_BIT);
	return !!val;
}

int HAL_PMU_IsSys3Alive(void)
{
	return !!(REG_READ_U32(HIF_WLAN_STATE) & WLAN_STATE_ACTIVE);
}


int xradio_wlan_power(int on)
{
	if (on) {
#ifdef CONFIG_DRIVER_R128
		/* set wlan sram default state to work mode from retention mode. */
		REG_SET_BIT(WLAN_SRAM_CTRL_REG, WLAN_SRAM_CTRL_WORK_BIT);
		HAL_CCM_EnableCPUWClk(1);
		msleep(1);
#endif
		HAL_CCM_ForceSys3Reset();
		xradio_dbg(XRADIO_DBG_ALWY, "xradio_wlan_power %d!\n", on);
		while (1) {
			if (HAL_PMU_IsSys3Alive())
				xradio_dbg(XRADIO_DBG_WARN, "%d wlan steal active\n", __LINE__);
			else
				break;
		}
		HAL_CCM_ReleaseSys3Reset();
		msleep(20);
	} else {
		HAL_CCM_ForceSys3Reset();
		while (1) {
			msleep(5);
			if (HAL_PMU_IsSys3Alive())
				xradio_dbg(XRADIO_DBG_WARN, "%d wlan steal active\n", __LINE__);
			else
				break;
		}
		msleep(20);
#ifdef CONFIG_DRIVER_R128
		HAL_CCM_EnableCPUWClk(0);
		msleep(5);
		/* set wlan sram default state to retention mode from work mode. */
		REG_CLR_BIT(WLAN_SRAM_CTRL_REG, WLAN_SRAM_CTRL_WORK_BIT);
#endif
	}
	return 0;
}

int xradio_get_freq_offset_from_efuse(u8 *freq_offset)
{
	return xradio_efuse_read_dcxo(freq_offset);;
}

int xradio_set_freq_offset(struct xradio_common *hw_priv, u8 freq_offset)
{
	struct wsm_config_freq_offset arg;
	int ret = 0;
	u32 val = REG_READ_U32(CCU_AON_DCXO_CFG);

	if (val & CCU_AON_DCXO_FLAG_BIT) {
		u8 enhance_rf_clk = 0x1;

		val &= ~(CCU_AON_DCXO_DIE_FREQ_OFFSET_MASK | CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_MASK);
		val |= (freq_offset << CCU_AON_DCXO_DIE_FREQ_OFFSET_SHIFT) & CCU_AON_DCXO_DIE_FREQ_OFFSET_MASK;
		val |= (enhance_rf_clk << CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_SHIFT) & CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_MASK;
		REG_WRITE_U32(CCU_AON_DCXO_CFG, val);
		arg.dcxo_from_a_die = 0;
		xradio_dbg(XRADIO_DBG_ALWY, "dcxo from D die, set freq offset=%d\n", freq_offset);
	} else {
		arg.dcxo_from_a_die = 1;
		xradio_dbg(XRADIO_DBG_ALWY, "dcxo from A die, set freq offset=%d\n", freq_offset);
	}
	arg.freq_offset = freq_offset;
	ret = wsm_set_freq_offset(hw_priv, &arg, 0);
	if (ret)
		xradio_dbg(XRADIO_DBG_ERROR, "Set dcxo freq offset err:%d\n", ret);

	return ret;
}

int xradio_get_freq_offset(struct xradio_common *hw_priv, u8 *freq_offset, u8 *dcxo_from_a_die)
{
	int ret = 0;
	u32 val = REG_READ_U32(CCU_AON_DCXO_CFG);

	if (val & CCU_AON_DCXO_FLAG_BIT) {
		if (freq_offset)
			*freq_offset = (val & CCU_AON_DCXO_DIE_FREQ_OFFSET_MASK) >> CCU_AON_DCXO_DIE_FREQ_OFFSET_SHIFT;
		if (dcxo_from_a_die)
			*dcxo_from_a_die = 0;
		xradio_dbg(XRADIO_DBG_MSG, "dcxo from D die, freq offset=%d\n", *freq_offset);
	} else {
		struct wsm_config_freq_offset arg = {0};

		ret = wsm_get_freq_offset(hw_priv, &arg);
		if (ret) {
			xradio_dbg(XRADIO_DBG_ERROR, "Get dcxo freq offset err:%d\n", ret);
			return ret;
		}

		if (freq_offset)
			*freq_offset = arg.freq_offset;
		if (dcxo_from_a_die)
			*dcxo_from_a_die = 1;
		xradio_dbg(XRADIO_DBG_MSG, "dcxo from A die, freq offset=%d\n", *freq_offset);
	}

	return ret;
}

int xradio_get_channel_fec_from_efuse(signed short *ch_1, signed short *ch_7, signed short *ch_13)
{
	int ret;
	u8 pout[3];

#if CONFIG_WLAN_GET_POUT_FROM_EFUSE_EN
	ret = xradio_efuse_read_wlan_pout(pout);
	if (ret) {
		xradio_dbg(XRADIO_DBG_MSG, "efuse pout read failed\n");
		return -1;
	}
#else
	ret = -1;
	xradio_dbg(XRADIO_DBG_MSG, "efuse pout read failed\n");
	return ret;
#endif

	xradio_dbg(XRADIO_DBG_NIY, "wlan pout: 0x%02X 0x%02X 0x%02X\n", pout[0], pout[1], pout[2]);
	if (pout[0] & 0x40)
		*ch_1 = -1 * (pout[0] & 0x3f) * 2;
	else
		*ch_1 = (pout[0] & 0x3f) * 2;

	if (pout[1] & 0x20)
		*ch_7 = -1 * (((pout[0] & 0x80) >> 7) | ((pout[1] & 0x1f) << 1)) * 2;
	else
		*ch_7 = ((pout[0] & 0x80) >> 7) | ((pout[1] & 0x1f) << 1) * 2;

	if (pout[2] & 0x10)
		*ch_13 = -1 * (((pout[1] & 0xc0) >> 6) | ((pout[2] & 0x0f) << 2)) * 2;
	else
		*ch_13 = ((pout[1] & 0xc0) >> 6) | ((pout[2] & 0x0f) << 2) * 2;

	return 0;
}

#if XRADIO_SDD_DIFF_HOSC_FREQ_FILE_MERGE
#include <linux/clk.h>

int xradio_hosc_frequency_get(struct xradio_common *hw_priv)
{
	struct clk *bus_clk_hosc = NULL;

	hw_priv->hosc_freq = 0;

	bus_clk_hosc = devm_clk_get(hw_priv->pdev, "hosc");
	if (IS_ERR(bus_clk_hosc)) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio get HOSC clock failed\n");
		return -EINVAL;
	}
	hw_priv->hosc_freq = clk_get_rate(bus_clk_hosc);

	xradio_dbg(XRADIO_DBG_NIY, "xradio get hosc:%d Hz\n", hw_priv->hosc_freq);

	return 0;
}
#endif

#endif
