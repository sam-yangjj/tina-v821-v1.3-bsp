// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2022 liujuan1@allwinnertech.com
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <dt-bindings/clock/sun65iw1-displl-ccu.h>

#include "ccu_common.h"
#include "ccu_reset.h"

#include "ccu_nm.h"
#include "ccu_nkmp.h"

#define SUN65IW1_PLL_CPU0_REG	0x0000
#define SUN65IW1_PLL_CPU1_REG	0x0100
#define SUN65IW1_PLL_CPU2_REG   0x0200

#define SUN65IW1_PLL_CPU0_SSC_REG   0x0014
#define SUN65IW1_PLL_CPU1_SSC_REG   0x0114
#define SUN65IW1_PLL_CPU2_SSC_REG   0x0214

static struct ccu_mult pll_cpu0_clk = {
	.enable		= BIT(27) | BIT(29) | BIT(30) | BIT(31),
	.lock		= BIT(28),
	.mult		= _SUNXI_CCU_MULT_MIN(8, 8, 12),
	.common		= {
		.reg		= 0x0000,
		.hw.init	= CLK_HW_INIT("pll-cpu0", "dcxo24M",
				&ccu_mult_ops,
				CLK_SET_RATE_UNGATE | CLK_IS_CRITICAL),
	},
};

static SUNXI_CCU_M(pll_cpu0_div_clk, "pll-cpu0-div",
		"pll-cpu0", 0x0020, 16, 7, CLK_DIVIDER_POWER_OF_TWO);

static const char * const cpu0_parents[] = { "dcxo24M", "osc32k", "iosc", "pll-cpu0-div", "pll-peri0-2x", "pll-peri0-600m", "pll-peri1-2x" };
static SUNXI_CCU_MUX(cpu0_clk, "cpu0", cpu0_parents,
		0x0018, 24, 3, CLK_SET_RATE_PARENT | CLK_IS_CRITICAL);

/* M or N only support 1 or 3 */
static SUNXI_CCU_M(pll_cpu0_apb_div_clk, "pll-cpu0-apb-div",
		"cpu0", 0x0020, 2, 2, 0);
static SUNXI_CCU_M(pll_cpu0_axi_div_clk, "pll-cpu0-axi-div",
		"cpu0", 0x0020, 0, 2, 0);

static struct ccu_mult pll_cpu1_clk = {
	.enable		= BIT(27) | BIT(29) | BIT(30) | BIT(31),
	.lock		= BIT(28),
	.mult		= _SUNXI_CCU_MULT_MIN(8, 8, 12),
	.common		= {
		.reg		= 0x0100,
		.hw.init	= CLK_HW_INIT("pll-cpu1", "dcxo24M",
				&ccu_mult_ops,
				CLK_SET_RATE_UNGATE | CLK_IS_CRITICAL),
	},
};

static SUNXI_CCU_M(pll_cpu1_div_clk, "pll-cpu1-div",
		"pll-cpu1", 0x0120, 16, 7, CLK_DIVIDER_POWER_OF_TWO);

static const char * const cpu1_parents[] = { "dcxo24M", "osc32k", "iosc", "pll-cpu1-div", "pll-peri0-2x", "pll-peri0-600m", "pll-peri1-2x" };
static SUNXI_CCU_MUX(cpu1_clk, "cpu1", cpu1_parents,
		0x0118, 24, 3, CLK_SET_RATE_PARENT | CLK_IS_CRITICAL);

/* M or N only support 1 or 3 */
static SUNXI_CCU_M(pll_cpu1_apb_div_clk, "pll-cpu1-apb-div",
		"cpu1", 0x0120, 2, 2, 0);
static SUNXI_CCU_M(pll_cpu1_axi_div_clk, "pll-cpu1-axi-div",
		"cpu1", 0x0120, 0, 2, 0);

static struct ccu_mult pll_cpu2_clk = {
	.enable		= BIT(27) | BIT(29) | BIT(30) | BIT(31),
	.lock		= BIT(28),
	.mult		= _SUNXI_CCU_MULT_MIN(8, 8, 12),
	.common		= {
		.reg		= 0x0200,
		.hw.init	= CLK_HW_INIT("pll-cpu2", "dcxo24M",
				&ccu_mult_ops,
				CLK_SET_RATE_UNGATE | CLK_IS_CRITICAL),
	},
};

static SUNXI_CCU_M(pll_cpu2_div_clk, "pll-cpu2-div",
		"pll-cpu2", 0x0220, 16, 7, CLK_DIVIDER_POWER_OF_TWO);

static const char * const cpu2_parents[] = { "dcxo24M", "osc32k", "iosc", "pll-cpu2-div", "pll-peri0-2x", "pll-peri0-600m", "pll-peri1-2x" };
static SUNXI_CCU_MUX(cpu2_clk, "cpu2", cpu2_parents,
		0x0218, 24, 3, CLK_SET_RATE_PARENT | CLK_IS_CRITICAL);

/* M or N only support 1 or 3 */
static SUNXI_CCU_M(pll_cpu2_apb_div_clk, "pll-cpu2-apb-div",
		"cpu2", 0x0220, 2, 2, 0);
static SUNXI_CCU_M(pll_cpu2_axi_div_clk, "pll-cpu2-axi-div",
		"cpu2", 0x0220, 0, 2, 0);

static struct ccu_common *sunxi_pll_cpu_clks[] = {
	&pll_cpu0_clk.common,
	&pll_cpu0_div_clk.common,
	&cpu0_clk.common,
	&pll_cpu0_apb_div_clk.common,
	&pll_cpu0_axi_div_clk.common,
	&pll_cpu1_clk.common,
	&pll_cpu1_div_clk.common,
	&cpu1_clk.common,
	&pll_cpu1_apb_div_clk.common,
	&pll_cpu1_axi_div_clk.common,
	&pll_cpu2_clk.common,
	&pll_cpu2_div_clk.common,
	&cpu2_clk.common,
	&pll_cpu2_apb_div_clk.common,
	&pll_cpu2_axi_div_clk.common,

};

static struct clk_hw_onecell_data sunxi_cpupll_hw_clks = {
	.hws	= {
		[CLK_PLL_CPU0]		= &pll_cpu0_clk.common.hw,
		[CLK_PLL_CPU0_DIV]	= &pll_cpu0_div_clk.common.hw,
		[CLK_CPU0]		= &cpu0_clk.common.hw,
		[CLK_CPU0_APB_DIV]	= &pll_cpu0_apb_div_clk.common.hw,
		[CLK_CPU0_AXI_DIV]	= &pll_cpu0_axi_div_clk.common.hw,
		[CLK_PLL_CPU1]		= &pll_cpu1_clk.common.hw,
		[CLK_PLL_CPU1_DIV]	= &pll_cpu1_div_clk.common.hw,
		[CLK_CPU1]		= &cpu1_clk.common.hw,
		[CLK_CPU1_APB_DIV]	= &pll_cpu1_apb_div_clk.common.hw,
		[CLK_CPU1_AXI_DIV]	= &pll_cpu1_axi_div_clk.common.hw,
		[CLK_PLL_CPU2]		= &pll_cpu2_clk.common.hw,
		[CLK_PLL_CPU2_DIV]	= &pll_cpu2_div_clk.common.hw,
		[CLK_CPU2]		= &cpu2_clk.common.hw,
		[CLK_CPU2_APB_DIV]	= &pll_cpu2_apb_div_clk.common.hw,
		[CLK_CPU2_AXI_DIV]	= &pll_cpu2_axi_div_clk.common.hw,
	},
	.num = CLK_DISPLL_MAX_NO,
};

static const struct sunxi_ccu_desc cpupll_desc = {
	.ccu_clks	= sunxi_pll_cpu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sunxi_pll_cpu_clks),
	.hw_clks	= &sunxi_cpupll_hw_clks,
	.resets		= NULL,
	.num_resets	= 0,
};

static inline unsigned int calc_pll_ssc(unsigned int step, unsigned int scale,
		unsigned int ssc)
{
	return (unsigned int)(((1 << 17) * ssc) - (scale * (1 << step))) / scale;
}

static const u32 sun65iw1_pll_cpu_regs[] = {
	SUN65IW1_PLL_CPU0_REG,
	SUN65IW1_PLL_CPU1_REG,
	SUN65IW1_PLL_CPU2_REG,
};

static const u32 sun65iw1_pll_cpu_ssc_regs[] = {
	SUN65IW1_PLL_CPU0_SSC_REG,
	SUN65IW1_PLL_CPU1_SSC_REG,
	SUN65IW1_PLL_CPU2_SSC_REG,
};

#if (defined CONFIG_AW_FPGA_S4) || (defined CONFIG_AW_FPGA_V7)
static void ccupll_helper_wait_for_lock(void __iomem *addr, u32 lock)
{
}

static void cpupll_helper_wait_for_clear(void __iomem *addr, u32 clear)
{
}
#else
static void ccupll_helper_wait_for_lock(void __iomem *addr, u32 lock)
{
	u32 reg;

	WARN_ON(readl_relaxed_poll_timeout(addr, reg, reg & lock, 100, 70000));
}

static void cpupll_helper_wait_for_clear(void __iomem *addr, u32 clear)
{
	u32 reg;

	reg = readl(addr);
	writel(reg | clear, addr);

	WARN_ON(readl_relaxed_poll_timeout_atomic(addr, reg, !(reg & clear), 100, 10000));
}
#endif

static int cpupll_notifier_cb(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct ccu_pll_nb *pll = to_ccu_pll_nb(nb);
	int ret = 0;

	if (event == PRE_RATE_CHANGE) {
		/* Enable ssc function */
		set_reg(pll->common->base + pll->common->ssc_reg, 1, 1, pll->enable);
	} else if (event == POST_RATE_CHANGE) {
		/* Disable ssc function */
		set_reg(pll->common->base + pll->common->ssc_reg, 0, 1, pll->enable);
	}

	ccu_helper_wait_for_clear(pll->common, pll->common->clear);

	return notifier_from_errno(ret);
}

static struct ccu_pll_nb cpupll0_nb = {
	.common = &pll_cpu0_clk.common,
	.enable = BIT(31), /* switch ssc mode */
	.clk_nb = {
		.notifier_call = cpupll_notifier_cb,
	},
};

static struct ccu_pll_nb cpupll1_nb = {
	.common = &pll_cpu1_clk.common,
	.enable = BIT(31), /* switch ssc mode */
	.clk_nb = {
		.notifier_call = cpupll_notifier_cb,
	},
};

static struct ccu_pll_nb cpupll2_nb = {
	.common = &pll_cpu2_clk.common,
	.enable = BIT(31),
	.clk_nb = {
		.notifier_call = cpupll_notifier_cb,
	},
};

static int sun65iw1_cpupll_probe(struct platform_device *pdev)
{
	void __iomem *reg;
	u32 val;
	int i;
	unsigned int step = 0, scale = 0, ssc = 0;
	struct device_node *np = pdev->dev.of_node;

	reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	if (of_property_read_u32(np, "pll_step", &step))
		step = 0x9;

	if (of_property_read_u32(np, "pll_ssc_scale", &scale))
		scale = 0xa;

	if (of_property_read_u32(np, "pll_ssc", &ssc))
		ssc = 0x1;
	/* TODO: assume boot use the cpupll */
	for (i = 0; i < ARRAY_SIZE(sun65iw1_pll_cpu_ssc_regs); i++) {
		/*
		 * 1. Config n,m1,m0,p: default:480M
		 * 2. Enable pll_en pll_ldo_en lock_en pll_output
		 * 3. wait for update and lock
		 */
		val = readl(reg + sun65iw1_pll_cpu_regs[i]);
		val |= BIT(27) | BIT(29) | BIT(30) | BIT(31);
		set_reg(reg + sun65iw1_pll_cpu_regs[i], val, 32, 0);

		cpupll_helper_wait_for_clear(reg + sun65iw1_pll_cpu_regs[i], BIT(26));
		ccupll_helper_wait_for_lock(reg + sun65iw1_pll_cpu_regs[i], BIT(28));

		/*
		 * set ssc/step in ssc reg
		 */
		val = 0;
		ssc = calc_pll_ssc(step, scale, ssc);
		val = (ssc << 12 | step << 0);
		set_reg(reg + sun65iw1_pll_cpu_ssc_regs[i], val, 29, 0);

		cpupll_helper_wait_for_clear(reg + sun65iw1_pll_cpu_regs[i], BIT(26));
	}

	sunxi_ccu_probe(pdev->dev.of_node, reg, &cpupll_desc);

	ccu_pll_notifier_register(&cpupll0_nb);
	ccu_pll_notifier_register(&cpupll1_nb);
	ccu_pll_notifier_register(&cpupll2_nb);

	dev_info(&pdev->dev, "Sunxi pll_cpu clk init OK\n");

	return 0;
}

static const struct of_device_id sun65iw1_cpupll_ids[] = {
	{ .compatible = "allwinner,sun65iw1-cpupll" },
	{ }
};

static struct platform_driver sun65iw1_cpupll_driver = {
	.probe	= sun65iw1_cpupll_probe,
	.driver	= {
		.name	= "sun65iw1-cpupll",
		.of_match_table	= sun65iw1_cpupll_ids,
	},
};

static int __init sunxi_ccu_cpupll_init(void)
{
	int ret;

	ret = platform_driver_register(&sun65iw1_cpupll_driver);
	if (ret)
		pr_err("register ccu sun65iw1 cpupll failed\n");

	return ret;
}
core_initcall(sunxi_ccu_cpupll_init);

static void __exit sunxi_ccu_cpupll_exit(void)
{
	return platform_driver_unregister(&sun65iw1_cpupll_driver);
}
module_exit(sunxi_ccu_cpupll_exit);

MODULE_DESCRIPTION("Allwinner sun65iw1 cpupll clk driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.4");
MODULE_AUTHOR("rengaomin<rengaomin@allwinnertech.com>");
