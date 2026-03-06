# SPDX-License-Identifier: GPL-2.0
KASAN_SANITIZE := n
UBSAN_SANITIZE_generic.o := n
UBSAN_SANITIZE_report.o := n
KCOV_INSTRUMENT := n

CFLAGS_REMOVE_generic.o = $(CC_FLAGS_FTRACE)

CFLAGS_generic.o := $(call cc-option, -fno-conserve-stack -fno-stack-protector) -DDISABLE_BRANCH_PROFILING

obj-$(CONFIG_AW_ANSC_KASAN) += generic.o report.o
