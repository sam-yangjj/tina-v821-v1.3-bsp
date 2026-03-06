ccflags-y += -DSTK_QUALCOMM -Wno-unused-variable -Wno-enum-conversion

obj-$(CONFIG_SENSOR_STK83XX) += stk.o

stk-src += stk83xx.c stk83xx_drv_i2c.c stk83xx_qualcomm.c common_i2c.c common_timer.c common_gpio_qualcomm.c common_storage.c stk_axes_sim.c
stk-objs := $(stk-src:.c=.o)
