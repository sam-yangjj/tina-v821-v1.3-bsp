// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2023 - 2025 Allwinner Technology Co.,Ltd. All rights reserved.
* Copyright (c) 2025 zhangyuanjing@allwinnertech.com
*/

#include <linux/kmsg_dump.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/io.h>

#include <linux/amba/serial.h>
#include <linux/serial_reg.h>

#ifdef CONFIG_FIX_EARLYCON_MEM
#include <asm/fixmap.h>
#endif

#define DEFAULT_UART_MAP_SIZE 0x400

#define UART_USR 31 /* I/O: uart status Register */
#define UART_USR_NF 0x02 /* Tansmit fifo not full */

static void __iomem *early_base;

static int __init setup_early_printk_address(char *buf)
{
	phys_addr_t paddr = 0;
	char *start_of_address;
	char *e;

	if (!buf) {
		pr_warn("No earlyprintk arguments passed.\n");
		return 0;
	}

	start_of_address = strstr(buf, ",0x");

	if (start_of_address) {
		start_of_address += 3;
		paddr = simple_strtoul(start_of_address, &e, 16);
		if (*e != '\0') {
			pr_warn("Invalid earlyprintk address: %s\n", buf);
			return -EINVAL;
		}
	}

	if (paddr) {
#if IS_ENABLED(CONFIG_FIX_EARLYCON_MEM)
		set_fixmap_io(FIX_EARLYCON_MEM_BASE, paddr & PAGE_MASK);
		early_base =
			(void __iomem *)__fix_to_virt(FIX_EARLYCON_MEM_BASE);
		early_base += paddr & ~PAGE_MASK;
#else
		early_base = ioremap(paddr, DEFAULT_UART_MAP_SIZE);
#endif /* CONFIG_FIX_EARLYCON_MEM */
	}

	if (!early_base)
		pr_err("%s: Couldn't map %pa\n", __func__, &paddr);

	return 0;
}
early_param("earlyprintk", setup_early_printk_address);

static void sunxi_serial_putc(char ch)
{
	while (!(readl_relaxed(early_base + (UART_USR << 2)) & UART_USR_NF))
		;

	writel_relaxed(ch, early_base + (UART_TX << 2));
}

/* Transmit a character over the UART */
static void uart_putchar(int c)
{
	if (c == '\n') {
		/* If the character is a newline, transmit a carriage return before newline */
		sunxi_serial_putc('\r');
	}
	/* Transmit the character */
	sunxi_serial_putc(c);
}

static void uart_puts(const char *s)
{
	const char *c = s;
	/* Iterate through the characters in the string */
	while (*c != '\0') {
		/* Transmit each character */
		uart_putchar(*c);
		c++;
	}
}

static void kmsg_dumper_stdout(struct kmsg_dumper *dumper,
			       enum kmsg_dump_reason reason)
{
	static char line[1024];
	size_t len = 0;

	if (early_base) {
		uart_puts("Fastboot panic dumper force dump kmsg:\n");
		uart_puts("---[ FASTBOOT PANIC DUMPER START ]---\n");
		while (kmsg_dump_get_line(dumper, true, line, sizeof(line),
					  &len)) {
			line[len] = '\0';
			uart_puts(line);
		}
		uart_puts("---[ FASTBOOT PANIC DUMPER ENDS ]---\n");
	} else {
		pr_err("earlyprintk not register, skip dump info\n");
	}
}

static struct kmsg_dumper kmsg_dumper = { .dump = kmsg_dumper_stdout };

static int __init kmsg_dumper_stdout_init(void)
{
	pr_info("fastboot kmsg dumper registered\n");
	return kmsg_dump_register(&kmsg_dumper);
}

static void __exit kmsg_dumper_stdout_deinit(void)
{
	pr_info("fastboot kmsg dumper unregistered\n");
	kmsg_dump_unregister(&kmsg_dumper);
}

postcore_initcall(kmsg_dumper_stdout_init);
module_exit(kmsg_dumper_stdout_deinit);

MODULE_DESCRIPTION("Allwinner fastboot kmsg dumper driver");
MODULE_AUTHOR("zhangyuanjing<zhangyuanjing@allwinnertech.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.1");
