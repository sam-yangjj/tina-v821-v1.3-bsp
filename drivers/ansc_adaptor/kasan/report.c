// SPDX-License-Identifier: GPL-2.0
/*
 * This file contains common generic and tag-based KASAN error reporting code.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Author: Andrey Ryabinin <ryabinin.a.a@gmail.com>
 *
 * Some code borrowed from https://github.com/xairy/kasan-prototype by
 *        Andrey Konovalov <andreyknvl@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bitops.h>
#include <linux/ftrace.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stackdepot.h>
#include <linux/stacktrace.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kasan.h>
#include <linux/module.h>
#include <linux/sched/task_stack.h>

#include <asm/sections.h>

#include "kasan.h"

static unsigned long kasan_flags;

#define KASAN_BIT_REPORTED	0
#define KASAN_BIT_MULTI_SHOT	1

bool kasan_save_enable_multi_shot(void)
{
	return test_and_set_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
}
EXPORT_SYMBOL_GPL(kasan_save_enable_multi_shot);

void kasan_restore_multi_shot(bool enabled)
{
	if (!enabled)
		clear_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
}
EXPORT_SYMBOL_GPL(kasan_restore_multi_shot);

static int __init kasan_set_multi_shot(char *str)
{
	set_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags);
	return 1;
}
__setup("kasan_multi_shot", kasan_set_multi_shot);

static void print_error_description(struct kasan_access_info *info)
{
	pr_err("BUG: KASAN: %pS\n",
		(void *)info->ip);
	pr_err("%s of size %zu at addr %px by task %s/%d\n",
		info->is_write ? "Write" : "Read", info->access_size,
		info->access_addr, current->comm, task_pid_nr(current));
}

static DEFINE_SPINLOCK(report_lock);

static void start_report(unsigned long *flags)
{
	/*
	 * Make sure we don't end up in loop.
	 */
	kasan_disable_current();
	spin_lock_irqsave(&report_lock, *flags);
	pr_err("==================================================================\n");
}

static void end_report(unsigned long *flags)
{
	pr_err("==================================================================\n");
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
	spin_unlock_irqrestore(&report_lock, *flags);
	if (panic_on_warn)
		panic("panic_on_warn set ...\n");
	kasan_enable_current();
}

static bool report_enabled(void)
{
	if (current->kasan_depth)
		return false;
	if (test_bit(KASAN_BIT_MULTI_SHOT, &kasan_flags))
		return true;
	return !test_and_set_bit(KASAN_BIT_REPORTED, &kasan_flags);
}

void __kasan_report(unsigned long addr, size_t size, bool is_write, unsigned long ip)
{
	struct kasan_access_info info;
	void *tagged_addr;
	void *untagged_addr;
	unsigned long flags;

	if (likely(!report_enabled()))
		return;

	disable_trace_on_warning();

	tagged_addr = (void *)addr;
	untagged_addr = reset_tag(tagged_addr);

	info.access_addr = tagged_addr;
	info.access_size = size;
	info.is_write = is_write;
	info.ip = ip;

	start_report(&flags);

	print_error_description(&info);
	dump_stack();

	end_report(&flags);
}
