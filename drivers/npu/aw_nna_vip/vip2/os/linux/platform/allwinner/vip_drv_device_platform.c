// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * (C) Copyright 2017-2023
 * Reuuimlla Technology Co., Ltd. <www.allwinnertech.com>
 * Miujiu <zhangjunjie@allwinnertech.com>
 ****************************************************************************
 *
 *	The MIT License (MIT)
 *
 *	Copyright (c) 2017 - 2023 Vivante Corporation
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 */

#include "vip_drv_device_driver.h"
#include "vip_drv_device_platform_config.h"
#include "vip_drv_device_platform.h"
#include <sunxi-sid.h>
#include <sunxi-smc.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0))
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/reset.h>

#if NPU_USER_IOMMU
#include <linux/dma-mapping.h>
#include <asm/dma-iommu.h>
#endif

typedef enum _clk_status {
	CLK_ON  = 0x0,
	CLK_OFF = 0x1,
} clk_status_e;

aw_driver_t aw_driver = {
	.rst = NULL,
	.mclk = NULL,
	.pclk = NULL,

	/*sun8iw21 use*/
	.bus = NULL,
	.mbus = NULL,

	/* sun60iw2 */
	.mbus_gate = NULL,
	.ahb_gate = NULL,

	/*sun55iw6 use*/
	.tzma = NULL,

	.aclk = NULL,
	.hclk = NULL,
	.arst = NULL,
	.hrst = NULL,
	.regulator = NULL,
	.vol = 0,
	.clk_freq = 0,
	.max_freq = 0,
	.default_freq = 0,
	.vf_index = 0,
	.set_vol = 0,
	.freqs = {0},
	.enable_pm = false,
};

#define M2HZ(x) ((x) * (1000000))

#define NPU_LEVEL_0	0b000000
#define NPU_LEVEL_1	0b000001
#define NPU_LEVEL_2	0b000100
#define NPU_LEVEL_3	0b000101
#define NPU_LEVEL_4	0b010000
#define NPU_LEVEL_5	0b010001

#if !IS_ENABLED(CONFIG_ARCH_SUN8IW21)
/*
 * @brief regulator the VIP vol.
 */
extern u32 determine_dcxo_clk_source(void);

static vip_status_e npu_regulator_enable(struct device_node *node)
{
	int err;
	if (!aw_driver.regulator)
		return 0;

	/* set output voltage to the dts config */
	/* npu-regulator para : NPU Regulator Control.
	 * if 1, use npu set vol; if 0, uboot set npu vol.
	 */
	err = of_property_read_u32_index(node, "npu-regulator", 0, &aw_driver.set_vol);
	if (err != 0)
		PRINTK("Get NPU Regulator Control FAIL!\n");

	if (aw_driver.vol) {
		if (aw_driver.set_vol != 0)
			regulator_set_voltage(aw_driver.regulator, aw_driver.vol, aw_driver.vol);
	}

	if (regulator_enable(aw_driver.regulator)) {
		printk("enable regulator failed!\n");
		return -1;
	}

	return 0;
}

static vip_status_e npu_regulator_disable(void)
{
	if (!aw_driver.regulator)
		return 0;

	if (regulator_is_enabled(aw_driver.regulator))
		regulator_disable(aw_driver.regulator);

	return 0;
}
#endif

static vip_status_e set_clk_rate(void)
{
	unsigned long rate = 0, real_rate = 0;

	if (aw_driver.clk_freq && !IS_ERR_OR_NULL(aw_driver.pclk)) {		// parent clk
		rate = clk_round_rate(aw_driver.pclk, aw_driver.clk_freq);
		if (clk_set_rate(aw_driver.pclk, rate)) {
			pr_err("clk_set_rate:%ld  clk_freq:%d failed\n", rate, aw_driver.clk_freq);
			return -1;
		}

		real_rate = clk_get_rate(aw_driver.pclk);
		PRINTK("Want set pclk rate(%ld) support(%ld) real(%ld)\n", aw_driver.clk_freq, rate, real_rate);
	}

	if (!IS_ERR_OR_NULL(aw_driver.mclk) && !IS_ERR_OR_NULL(aw_driver.pclk)) {		// parent clk
		if (clk_set_parent(aw_driver.mclk, aw_driver.pclk)) {
			pr_err("clk_set_parent() failed! return\n");
			return -1;
		}
	}

	if (aw_driver.clk_freq && !IS_ERR_OR_NULL(aw_driver.mclk)) {	// npu_clk
		rate = clk_round_rate(aw_driver.mclk, aw_driver.clk_freq);
		if (clk_set_rate(aw_driver.mclk, rate)) {
			pr_err("clk_set_rate:%ld  clk:%d failed\n", rate, aw_driver.clk_freq);
			return -1;
		}
		real_rate = clk_get_rate(aw_driver.mclk);
		PRINTK("Want set mclk rate(%d) support(%ld) real(%ld)\n", aw_driver.clk_freq, rate, real_rate);
	}

	return 0;
}

/*
* @brief turn on the VIP clock.
* */
static vip_status_e npu_clk_init(void)
{
	/* 1. deassert reset */
	if (!IS_ERR_OR_NULL(aw_driver.hrst)) {
		if (reset_control_deassert(aw_driver.hrst)) {
			PRINTK("vipcore: Couldn't deassert AHB RST\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.arst)) {
		if (reset_control_deassert(aw_driver.arst)) {
			PRINTK("vipcore: Couldn't deassert AXI RST\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.rst)) {
		if (reset_control_deassert(aw_driver.rst)) {
			PRINTK("vipcore: Couldn't deassert NPU RST\n");
			return -EBUSY;
		}
	}

	/* sun60iw2 */
	if (!IS_ERR_OR_NULL(aw_driver.ahb_gate)) {
		if (clk_prepare_enable(aw_driver.ahb_gate)) {
			PRINTK("vipcore: Couldn't enable AHB GATE clock\n");
			return -EBUSY;
		}
	}

	/* 2. set rate */
	if (set_clk_rate()) {
		return -EBUSY;
	}

	/* sun60iw2 */
	if (!IS_ERR_OR_NULL(aw_driver.mbus_gate)) {
		if (clk_prepare_enable(aw_driver.mbus_gate)) {
			PRINTK("vipcore: Couldn't enable MBUS GATE clock\n");
			return -EBUSY;
		}
	}


	if (!IS_ERR_OR_NULL(aw_driver.bus)) {
		if (clk_prepare_enable(aw_driver.bus)) {
			PRINTK("vipcore: Couldn't enable AHB clock\n");
			return -EBUSY;
		}
	}
	/*V85X*/
	if (!IS_ERR_OR_NULL(aw_driver.mbus)) {
		if (clk_prepare_enable(aw_driver.mbus)) {
			PRINTK("vipcore: Couldn't enable AXI clock\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.hclk)) {
		if (clk_prepare_enable(aw_driver.hclk)) {
			PRINTK("vipcore: Couldn't enable AHB clock\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.aclk)) {
		if (clk_prepare_enable(aw_driver.aclk)) {
			PRINTK("vipcore: Couldn't enable AXI clock\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.tzma)) {
		if (clk_prepare_enable(aw_driver.tzma)) {
			PRINTK("vipcore: Couldn't enable TZMA clock\n");
			return -EBUSY;
		}
	}

	if (aw_driver.mclk) {
		if (clk_prepare_enable(aw_driver.mclk)) {
			PRINTK("Couldn't enable module clock\n");
			return -EBUSY;
		}
	}

	return VIP_SUCCESS;
}

/*
* @brief turn off the VIP clock.
* */
static vip_status_e npu_clk_uninit(void)
{
	/* diable clk */
	if (!IS_ERR_OR_NULL(aw_driver.mclk))
		clk_disable_unprepare(aw_driver.mclk);

	if (!IS_ERR_OR_NULL(aw_driver.tzma))
		clk_disable_unprepare(aw_driver.tzma);

	if (!IS_ERR_OR_NULL(aw_driver.bus))
		clk_disable_unprepare(aw_driver.bus);

	/*V85X*/
	if (!IS_ERR_OR_NULL(aw_driver.mbus))
		clk_disable_unprepare(aw_driver.mbus);

	if (!IS_ERR_OR_NULL(aw_driver.hclk))
		clk_disable_unprepare(aw_driver.hclk);
	if (!IS_ERR_OR_NULL(aw_driver.aclk))
		clk_disable_unprepare(aw_driver.aclk);

	if (!IS_ERR_OR_NULL(aw_driver.mbus_gate)) {
		clk_disable_unprepare(aw_driver.mbus_gate);
	}

	if (!IS_ERR_OR_NULL(aw_driver.ahb_gate)) {
		clk_disable_unprepare(aw_driver.ahb_gate);
	}

	if (!IS_ERR_OR_NULL(aw_driver.hrst)) {
		if (reset_control_assert(aw_driver.hrst)) {
			PRINTK("vipcore: Couldn't assert AHB RST\n");
			return -EBUSY;
		}
	}
	if (!IS_ERR_OR_NULL(aw_driver.arst)) {
		if (reset_control_assert(aw_driver.arst)) {
			PRINTK("vipcore: Couldn't assert AXI RST\n");
			return -EBUSY;
		}
	}

	if (!IS_ERR_OR_NULL(aw_driver.rst)) {
		if (reset_control_assert(aw_driver.rst)) {
			PRINTK("vipcore: Couldn't assert NPU RST\n");
			return -EBUSY;
		}
	}
	return VIP_SUCCESS;
}

/*
* @brief configure the power supply and clock of the VIP.
* @param kdriver, vip device object.
* */
static vip_status_e set_vip_power_clk(vipdrv_driver_t *kdriver, uint32_t status)
{
	__attribute__((unused)) struct device *dev = &(kdriver->pdev->dev);
	switch (status) {
	case CLK_ON:
		if (aw_driver.enable_pm) {
			if (VIP_SUCCESS != npu_clk_init())
				pr_err("Failed to init npu clk!\n");

			if (pm_runtime_get_sync(dev) < 0)
				pr_err("Cannot get pm runtime\n");
			PRINTK_I("pm on\n");
		}
		break;
	case CLK_OFF:
		if (aw_driver.enable_pm) {
			pm_runtime_put(dev);
			npu_clk_uninit();
			PRINTK_I("pm off\n");
		}
		break;
	default:
		printk("Unsupport clk status");
		break;
	}
	return VIP_SUCCESS;
}

/*
* @brief convert CPU physical to VIP physical.
* @param cpu_physical. the physical address of CPU domain.
* */

vip_address_t vipdrv_drv_get_vipphysical(
	vip_address_t cpu_physical
	)
{
	vip_address_t vip_hysical = cpu_physical;
	return vip_hysical;
}

/*
 * @brief convert VIP physical to CPU physical.
 * @param vip_physical. the physical address of VIP domain.
 * */

vip_address_t vipdrv_drv_get_cpuphysical(
	vip_address_t vip_physical
	)
{
	vip_address_t cpu_hysical = vip_physical;
	return cpu_hysical;
}

#if !IS_ENABLED(CONFIG_ARCH_SUN8IW21)
/* Get SID DVFS */
static int match_vf_table(u32 combi, u32 *index)
{
	struct device_node *np = NULL;
	int nsels, ret, i;
	u32 tmp;

	np = of_find_node_by_name(NULL, "vf_mapping_table");
	if (!np) {
		pr_err("Unable to find node\n");
		return -EINVAL;
	}

	if (!of_get_property(np, "table", &nsels))
		return -EINVAL;

	nsels /= sizeof(u32);
	if (!nsels) {
		pr_err("invalid table property size\n");
		return -EINVAL;
	}

	for (i = 0; i < nsels / 2; i++) {
		ret = of_property_read_u32_index(np, "table", i * 2, &tmp);
		if (ret) {
			pr_err("could not retrieve table property: %d\n", ret);
			return ret;
		}

		if (tmp == combi) {
			ret = of_property_read_u32_index(np, "table", i * 2 + 1, &tmp);
			if (ret) {
				pr_err("could not retrieve table property: %d\n", ret);
				return ret;
			}
			*index = tmp;
			break;
		}
	}
	if (i == nsels / 2)
		pr_notice("%s %d, could not match vf table, i:%d", __func__, __LINE__, i);

	return 0;
}

#if IS_ENABLED(CONFIG_ARCH_SUN55IW3)
#define SUN55IW3_MARKETID_EFUSE_OFF (0x00)
#define SUN55IW3_DVFS_EFUSE_OFF     (0x48)
static int sun55iw3_nvmem_xlate(void)
{
	u32 marketid, bak_dvfs, dvfs, combi;
	u32 index = 0x0100;

	sunxi_get_module_param_from_sid(&marketid, SUN55IW3_MARKETID_EFUSE_OFF, 4);
	marketid &= 0xffff;
	sunxi_get_module_param_from_sid(&dvfs, SUN55IW3_DVFS_EFUSE_OFF, 4);
	bak_dvfs = (dvfs >> 12) & 0xff;
	if (bak_dvfs)
		combi = bak_dvfs;
	else
		combi = (dvfs >> 4) & 0xff;

	if (marketid == 0x5200 && combi == 0x00)
		index = 0x0000;
	else
		match_vf_table(combi, &index);

	/* printk("NPU Use VF%u, dvfs: 0x%x\n", index, dvfs); */

	return index;
}
#elif IS_ENABLED(CONFIG_ARCH_SUN55IW6)
#define SUN55IW6_DVFS_EFUSE_OFF     (0x20)
static int sun55iw6_nvmem_xlate(void)
{
	u32 bak_dvfs, dvfs, combi;
	u32 index = 0x0100;

	sunxi_get_module_param_from_sid(&dvfs, SUN55IW6_DVFS_EFUSE_OFF, 4);

	bak_dvfs = (dvfs >> 24) & 0xff;
	if (bak_dvfs)
		combi = bak_dvfs;
	else
		combi = (dvfs >> 16) & 0xff;

	match_vf_table(combi, &index);

	return index;
}
#elif IS_ENABLED(CONFIG_ARCH_SUN60IW2)
#define SUN60IW2_DVFS_EFUSE_OFF     (0x4C)
static int sun60iw2_nvmem_xlate(void)
{
	u32 bak_dvfs, dvfs, combi;
	u32 index = 0x0000;

	sunxi_get_module_param_from_sid(&dvfs, SUN60IW2_DVFS_EFUSE_OFF, 4);
	bak_dvfs = (dvfs >> 24) & 0xff;
	if (bak_dvfs)
		combi = bak_dvfs;
	else
		combi = (dvfs >> 16) & 0xff;

	match_vf_table(combi, &index);

	return index;
}
#endif

static void get_vf_index(void)
{
#if IS_ENABLED(CONFIG_ARCH_SUN55IW3)
	aw_driver.vf_index = sun55iw3_nvmem_xlate();
#elif IS_ENABLED(CONFIG_ARCH_SUN55IW6)
	aw_driver.vf_index = sun55iw6_nvmem_xlate();
#elif IS_ENABLED(CONFIG_ARCH_SUN60IW2)
	aw_driver.vf_index = sun60iw2_nvmem_xlate();
#else
	PRINTK("No vf table now, use index 0x0 instead\n");
	aw_driver.vf_index = 0x0;
#endif

	PRINTK("current vf index: %x\n", aw_driver.vf_index);
}

#define DCXO_CLK_26M    (26000000)
#define MAX_NAME_LEN	64
/* Get NPU CLK and Vol */
static int get_npu_clk_vol(unsigned int vf_index, unsigned int npu_vf, int *npu_vol)
{
	struct device_node *npu_table_node;
	struct device_node *opp_node;
	int opp_vol = 0;
	int err;
	char microvolt[MAX_NAME_LEN] =  {0};
	char opp_freq[MAX_NAME_LEN] = {0};
	u32 clock_rate = 24000000;

	if (vf_index > 0xFFFF)
		vf_index = 0;

	PRINTK("NPU Use VF%04x, use freq %d MHz\n", vf_index, npu_vf);

	npu_table_node = of_find_node_by_name(NULL, "npu-opp-table");

#if IS_ENABLED(CONFIG_ARCH_SUN60IW2)
	clock_rate = determine_dcxo_clk_source();
#endif
	PRINTK("clock_rate: %d\n", clock_rate);
	if (clock_rate == DCXO_CLK_26M)
		snprintf(microvolt, MAX_NAME_LEN, "opp-microvolt-26m-vf%04x", vf_index);
	else
		snprintf(microvolt, MAX_NAME_LEN, "opp-microvolt-vf%04x", vf_index);

	snprintf(opp_freq, MAX_NAME_LEN, "opp-%d", npu_vf);

	opp_node = of_find_node_by_name(npu_table_node, opp_freq);

	err = of_property_read_u32_index(opp_node, microvolt, 0, &opp_vol);
	if (err != 0) {
		PRINTK("Get NPU VOL FAIL!\n");
		return -1;
	}

	if (!IS_ERR_OR_NULL(npu_vol))
		*npu_vol = opp_vol;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_ARCH_SUN55IW6)
void check_smc_set_freq(void)
{
	u32 data = 0;
	u32 key = 0;
	int ret = 0;

	aw_driver.max_freq = M2HZ(1000);
	aw_driver.default_freq = M2HZ(1000);

	ret = sunxi_smc_read_extra("rgn0", &data, 4);
	if (ret < 0) {
		PRINTK("failed to read extra\n");
		goto out;
	}

	data &= 0xFFFF;

	if (0xFF5A == data || 0xF25A == data) {
		aw_driver.default_freq = M2HZ(492);
	} else if (0xF05A == data || 0xF45A == data) {
		aw_driver.default_freq = M2HZ(1000);
	}

	ret = sunxi_sid_sram_read32("npu", &key);
	if (ret) {
		PRINTK("failed to gset npu sid key\n");
		goto out;
	}

	switch (key) {
	case NPU_LEVEL_0:
		aw_driver.max_freq = M2HZ(1000);
		break;
	case NPU_LEVEL_1:
		aw_driver.max_freq = 0;
		aw_driver.default_freq = 0;
		break;
	case NPU_LEVEL_2:
		aw_driver.max_freq = M2HZ(400);
		break;
	case NPU_LEVEL_3:
		aw_driver.max_freq = M2HZ(500);
		break;
	case NPU_LEVEL_4:
		aw_driver.max_freq = M2HZ(700);
		break;
	case NPU_LEVEL_5:
		aw_driver.max_freq = M2HZ(1000);
		break;
	default:
		aw_driver.max_freq = M2HZ(1000);
		break;
	}

out:
	aw_driver.clk_freq = aw_driver.default_freq;
	PRINTK("EXTRA: 0x%04x, SID: 0x%x, max_freq: %u, default_freq: %u\n",
			data, key, aw_driver.max_freq, aw_driver.default_freq);
}
#elif IS_ENABLED(CONFIG_ARCH_SUN60IW2)
void check_smc_set_freq(void)
{
	u32 data = 0;
	u32 key = 0;

	aw_driver.max_freq = M2HZ(1500);
	aw_driver.default_freq = M2HZ(900);

//out:
	aw_driver.clk_freq = aw_driver.default_freq;
	PRINTK("EXTRA: 0x%04x, SID: 0x%x, max_freq: %u, default_freq: %u\n",
			data, key, aw_driver.max_freq, aw_driver.default_freq);
}
#else
void check_smc_set_freq(void)
{
	u32 data = 0;
	u32 key = 0;

	aw_driver.max_freq = M2HZ(1000);
	aw_driver.default_freq = M2HZ(1000);

//out:
	aw_driver.clk_freq = aw_driver.default_freq;
	PRINTK("EXTRA: 0x%04x, SID: 0x%x, max_freq: %u, default_freq: %u\n",
			data, key, aw_driver.max_freq, aw_driver.default_freq);
}
#endif

void check_freq_available(void)
{
	int i = 0;
	u32 rate = 0;

	if (IS_ERR_OR_NULL(aw_driver.mclk)) {
		PRINTK("failed to get npu clk, check freq failed\n");
		return;
	}

	for (i = (MAX_FREQ_POINTS - 1); i >= 0; i--) {
		if (aw_driver.freqs[i] == 0
				|| aw_driver.freqs[i] > aw_driver.clk_freq)
			continue;

		rate = clk_round_rate(aw_driver.mclk, aw_driver.freqs[i]);
		if (rate == aw_driver.freqs[i]) {
			aw_driver.clk_freq = aw_driver.freqs[i];

			if (get_npu_clk_vol(aw_driver.vf_index, (aw_driver.clk_freq / 1000000), &aw_driver.vol)
					&& aw_driver.set_vol != 0) {
				PRINTK("Failed to get vol with freq: %u\n", aw_driver.clk_freq);
				return;
			}
			return;
		}
	}

	// no available freq
	PRINTK("no available freq info\n");
	aw_driver.clk_freq = 0;
}

int vipdrv_drv_get_supported_frequency(u32 *max_freq, u32 *npu_vol, u32 freqs[], u32 size)
{
	struct device_node *tbl_node;
	struct device_node *child;
	int err;
	u32 supported_max_freq = 0;
	u32 max_freq_vol = 0;
	u32 freq = 0, vol = 0;

	char microvolt[32] = {0};
	int count = 0;

	snprintf(microvolt, 32, "opp-microvolt-vf%04x", aw_driver.vf_index);

	tbl_node = of_find_node_by_name(NULL, "npu-opp-table");

	PRINTK("Supported vf list: \n");
	for_each_child_of_node(tbl_node, child) {
		vol = 0;
		freq = 0;

		err = of_property_read_u32(child, microvolt, &vol);
		if (err)
			continue;

		err = of_property_read_u32(child, "opp-hz", &freq);
		if (err)
			continue;

		if (freq > (aw_driver.max_freq)) {
			continue;
		}

		PRINTK("    freq: %u, vol: %u\n", freq, vol);
		if ((count < size) && !IS_ERR_OR_NULL(freqs)) {
			freqs[count] = freq;
			count++;
		}

		if (freq > supported_max_freq) {
			supported_max_freq = freq;
			max_freq_vol = vol;
		}
	}

	if (!IS_ERR_OR_NULL(max_freq))
		*max_freq = supported_max_freq;

	if (!IS_ERR_OR_NULL(npu_vol))
		*npu_vol = max_freq_vol;

	return count;
}

vip_int32_t vipdrv_drv_update_clk_freq(const u32 freq)
{

#if !IS_ENABLED(CONFIG_AW_PM_DOMAINS)
	int ret = 0;
#endif
	if (aw_driver.clk_freq == freq) {
		PRINTK("freq is same alrealy set to %u, no need to update\n", freq);
		return 0;
	}

	aw_driver.clk_freq = freq;
	check_freq_available();

#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
	if (aw_driver.vol && !IS_ERR_OR_NULL(aw_driver.regulator) && aw_driver.set_vol != 0) {
		regulator_set_voltage(aw_driver.regulator, aw_driver.vol, aw_driver.vol);
		PRINTK("set vol to %u\n", regulator_get_voltage(aw_driver.regulator));
	} else {
		PRINTK("don't update voltage\n");
	}

	if (set_clk_rate()) {
		PRINTK("Failed to set clk freq\n");
	} else {
		PRINTK("Update freq successful\n");
	}
#else // disable pm
	ret = npu_clk_uninit();
	if (ret) {
		PRINTK("Failed to deinit npu clk\n");
		return -1;
	}

	if (aw_driver.vol && !IS_ERR_OR_NULL(aw_driver.regulator) && aw_driver.set_vol != 0) {
		regulator_set_voltage(aw_driver.regulator, aw_driver.vol, aw_driver.vol);
		PRINTK("set vol to %u\n", regulator_get_voltage(aw_driver.regulator));
	} else {
		PRINTK("don't update voltage\n");
	}

	ret = npu_clk_init();
	if (ret) {
		PRINTK("Failed to init npu clk\n");
		return -1;
	}
#endif
	return 0;
}

vip_uint64_t vipdrv_drv_get_clk_freq(void)
{
	return aw_driver.clk_freq;
}

static const struct of_device_id vipcore_dev_match[] = {
	{
		.compatible = "allwinner,npu"
	},
	{ },
};

/*
@brief platfrom(vendor) initialize. prepare environmnet for running VIP hardware.
@param pdrv vip drvier device object.
*/
vip_int32_t vipdrv_drv_platform_init(
	vipdrv_driver_t *kdriver
	)
{
	kdriver->pdrv->driver.of_match_table = vipcore_dev_match;

	return 0;
}

#if !IS_ENABLED(CONFIG_ARCH_SUN8IW21)
	MODULE_DEVICE_TABLE(of, vipcore_dev_match);
#endif

/*
@brief 1. platfrom(vendor) un-initialize.
	   2. uninitialzie linux platform device.
	   have removed device/driver, so kdriver->pdev can't be used in this function.
@param pdrv vip drvier device object.
*/
vip_int32_t vipdrv_drv_platform_uninit(
	vipdrv_driver_t *kdriver
	)
{
	struct device *dev = &(kdriver->pdev->dev);
	if (dev != NULL) {
		printk("%s %d SUCCESS\n", __func__, __LINE__);
		if (aw_driver.enable_pm)
			pm_runtime_disable(dev);

	} else {
		printk("%s %d ERR\n", __func__, __LINE__);
	}
	return 0;
}

/*
@brief adjust parameters of vip devices. such as irq, SRAM, video memory heap.
	 you can overwrite insmod command line in vipdrv_drv_adjust_param() function.
@param kdriver, vip device object.
*/
vip_int32_t vipdrv_drv_adjust_param(
	vipdrv_driver_t *kdriver
	)
{
	struct platform_device *pdev = kdriver->pdev;
	struct device *dev = &(kdriver->pdev->dev);
	int ret, count;
	struct resource *res;
#if NPU_USER_IOMMU
	unsigned int mod_clk = 0;
	struct dma_iommu_mapping *mapping;
#else
	int err, vol;
#endif

	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}

#if vpmdENABLE_VIDEO_MEMORY_HEAP
	if (0 == kdriver->cpu_physical[0]) {
		kdriver->cpu_physical[0] = VIDEO_MEMORY_HEAP_BASE_ADDRESS;
	}
	if (0 == kdriver->heap_infos[0].vip_memsize) {
		kdriver->heap_infos[0].vip_memsize = VIDEO_MEMORY_HEAP_SIZE;
	}
	if (0 == vipdrv_os_strcmp("", kdriver->heap_infos[0].name)) {
		kdriver->heap_infos[0].name = ALLOCATOR_NAME_HEAP_RESERVED;
	}
#endif
	kdriver->irq_line[0] = platform_get_irq(pdev, 0);
	PRINTK("vipcore irq number is %d.\n", kdriver->irq_line[0]);

	/* IRQ line */
	if (0 == kdriver->irq_line[0]) {
		kdriver->irq_line[0] = IRQ_LINE_NUMBER[0];
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		PRINTK("no resource for registers\n");
		ret = -ENOENT;
		return -1;
	}

#if IS_ENABLED(CONFIG_AW_PM_DOMAINS)
	aw_driver.enable_pm = of_property_read_bool(pdev->dev.of_node, "power-domains");
#endif
	PRINTK("npu power domains status: %d\n", aw_driver.enable_pm);

	/* VIP AHB register memory base physical address */
	if (0 == kdriver->vip_reg_phy[0]) {
		kdriver->vip_reg_phy[0] = res->start;
	}

	/* VIP AHB register memory size */
	if (0 == kdriver->vip_reg_size[0]) {
		kdriver->vip_reg_size[0] = resource_size(res);
	}
	if (0 == kdriver->axi_sram_base) {
		kdriver->axi_sram_base = AXI_SRAM_BASE_ADDRESS;
	}
	if (0 == kdriver->axi_sram_size[0]) {
		kdriver->axi_sram_size[0] = AXI_SRAM_SIZE;
	}

	kdriver->device_core_number[0] = 1;
	kdriver->core_count = 1;

	kdriver->sys_heap_size = SYSTEM_MEMORY_HEAP_SIZE;
	if (dev != NULL) {
		printk("%s %d SUCCESS\n", __func__, __LINE__);

#if NPU_USER_IOMMU
		/* iommu_alloc */
		mapping = arm_iommu_create_mapping(&platform_bus_type, SZ_1G, SZ_2G);
		if (IS_ERR(mapping)) {
			PRINTK("failed to create ARM IOMMU mapping\n");
		}

		/* Attach the ARM VA mapping to the device. */
		ret = arm_iommu_attach_device(dev, mapping);
		if (ret < 0) {
			PRINTK("failed to attach device to VA mapping\n");
		}
		PRINTK("iommu_attach_device SUCCESS\n");
#endif

		aw_driver.mclk = of_clk_get_by_name(pdev->dev.of_node, "clk_npu");
		if (IS_ERR_OR_NULL(aw_driver.mclk)) {
			pr_err("failed to get NPU model clk, return %ld\n", PTR_ERR(aw_driver.mclk));
			return -1;
		}
		aw_driver.pclk = of_clk_get_by_name(pdev->dev.of_node, "clk_parent");
		if (IS_ERR_OR_NULL(aw_driver.pclk)) {
			pr_err("failed to get NPU parent clk\n");
			return -1;
		}

#if IS_ENABLED(CONFIG_ARCH_SUN8IW21)
		/*V85X Get bus.*/
		aw_driver.bus = of_clk_get_by_name(pdev->dev.of_node, "clk_bus");
		if (IS_ERR_OR_NULL(aw_driver.bus)) {
			pr_err("failed to get NPU parent clk\n");
			return -1;
		}
		aw_driver.mbus = of_clk_get_by_name(pdev->dev.of_node, "clk_mbus");
		if (IS_ERR_OR_NULL(aw_driver.mbus)) {
			pr_err("failed to get NPU parent clk\n");
			return -1;
		}
		ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &mod_clk);
#else

#if IS_ENABLED(CONFIG_ARCH_SUN60IW2)
		aw_driver.mbus_gate = of_clk_get_by_name(pdev->dev.of_node, "clk_mbus_gate");
		if (IS_ERR_OR_NULL(aw_driver.mbus_gate))
			pr_notice("NPU MBUS GATE NULL\n");

		aw_driver.ahb_gate = of_clk_get_by_name(pdev->dev.of_node, "clk_ahb_gate");
		if (IS_ERR_OR_NULL(aw_driver.ahb_gate))
			pr_notice("NPU AHB GATE NULL\n");
#endif
		aw_driver.aclk = of_clk_get_by_name(pdev->dev.of_node, "npu-aclk");
		if (IS_ERR_OR_NULL(aw_driver.aclk))
			pr_notice("NPU AXI CLK NULL\n");
		aw_driver.hclk = of_clk_get_by_name(pdev->dev.of_node, "npu-hclk");
		if (IS_ERR_OR_NULL(aw_driver.aclk))
			pr_notice("NPU AHB CLK NULL\n");

		/*sun55iw6 use*/
		aw_driver.tzma = of_clk_get_by_name(pdev->dev.of_node, "clk_tzma");
		if (IS_ERR_OR_NULL(aw_driver.tzma))
			pr_notice("NPU TZMA CLK NULL\n");
		aw_driver.bus = of_clk_get_by_name(pdev->dev.of_node, "clk_bus");
		if (IS_ERR_OR_NULL(aw_driver.bus))
			pr_notice("NPU BUS CLK NULL\n");

		aw_driver.arst = devm_reset_control_get(&pdev->dev, "npu_axi_rst");
		if (IS_ERR_OR_NULL(aw_driver.arst))
			pr_notice("NPU AXI RST NULL\n");
		aw_driver.hrst = devm_reset_control_get(&pdev->dev, "npu_ahb_rst");
		if (IS_ERR_OR_NULL(aw_driver.hrst))
			pr_notice("NPU AHB RST NULL\n");
#endif
		aw_driver.rst = devm_reset_control_get(&pdev->dev, "npu_rst");
		if (IS_ERR_OR_NULL(aw_driver.rst)) {
			pr_err("failed to get NPU reset handle\n");
			return -1;
		}
		if (reset_control_deassert(aw_driver.rst)) {
			pr_err("vipcore: Couldn't deassert NPU RST\n");
			return -EBUSY;
		}

#if !IS_ENABLED(CONFIG_ARCH_SUN8IW21)
	check_smc_set_freq();
	get_vf_index();

	count = vipdrv_drv_get_supported_frequency(NULL, NULL,
											   aw_driver.freqs, MAX_FREQ_POINTS);

	check_freq_available();
	if (set_clk_rate()) {
		PRINTK("Failed to set clk freq\n");
	}

	aw_driver.regulator = regulator_get(dev, "npu");
	if (IS_ERR_OR_NULL(aw_driver.regulator))
		PRINTK("Don`t Set NPU regulator!\n");

	/* Set NPU Vol */
	if (aw_driver.vol && !IS_ERR_OR_NULL(aw_driver.regulator)) {
		err = npu_regulator_enable(pdev->dev.of_node);
		if (err) {
			PRINTK("enable regulator failed!\n");
			return -1;
		}
		vol = regulator_get_voltage(aw_driver.regulator);
		PRINTK("Want set npu vol(%d) now vol(%d)", aw_driver.vol, vol);
	}
#endif

		if (aw_driver.enable_pm) {
			pm_runtime_enable(dev);
		} else if (VIP_SUCCESS != npu_clk_init()) {
			pr_err("Failed to init npu clk!\n");
			return -EBUSY;
		}
	} else {
		printk("%s %d ERR\n", __func__, __LINE__);
	}

	return 0;
}

/*
@brief release resources created in vipdrv_drv_adjust_param if neeed.
this function called before remove device/driver, so kdriver->pdev can be used.
@param kdriver, vip device object.
*/
vip_int32_t vipdrv_drv_unadjust_param(
	vipdrv_driver_t *kdriver
	)
{
#if !IS_ENABLED(CONFIG_ARCH_SUN8IW21)
	if (aw_driver.vol) {
		npu_regulator_disable();
		if (aw_driver.regulator) {
			regulator_put(aw_driver.regulator);
			aw_driver.regulator = NULL;
		}
	}
#endif
	return 0;
}

/*
@brief customer SOC initialize. power on and rise up clock for VIP hardware
@param kdriver, vip device object.
*/
vip_int32_t vipdrv_drv_device_init(
	vipdrv_driver_t *kdriver,
	vip_uint32_t core
	)
{
	vip_int32_t ret = 0;
	struct platform_device *pdev = kdriver->pdev;
	/* PRINTK("vipcore, device init..\n"); */
	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}

	/* power on VIP and make sure power stable before this function return */
	ret = set_vip_power_clk(kdriver, CLK_ON);
	vipdrv_os_delay(2);

	return ret;
}

/*
@brief customer SOC un-initialize. power off
@param kdriver, vip device object.
*/
vip_int32_t vipdrv_drv_device_uninit(
	vipdrv_driver_t *kdriver,
	vip_uint32_t core
	)
{
	struct platform_device *pdev = kdriver->pdev;
	vip_int32_t ret = -1;
	/* PRINTK("vipcore, device un-init..\n"); */
	if (VIP_NULL == pdev) {
		PRINTK("platform device is NULL \n");
		return -1;
	}
	/* power off VIP and make sure power stable before this function return */
	ret = 0;
	ret = set_vip_power_clk(kdriver, CLK_OFF);

	return ret;
}

MODULE_VERSION("1.13.0");
MODULE_AUTHOR("Miujiu <zhangjunjie@allwinnertech.com>");
