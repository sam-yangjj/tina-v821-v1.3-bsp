CONFIG_MODULE_NAME_GYRO:=aglink_gyro

ifeq ($(CONFIG_AG_GYRO_USE), m)
aglink_gyro-m := \
	aglink_gyro_hwio.o \
	aglink_take_gyro.o \
	aglink_gyro.o \
	aglink_gyro_osintf.o
endif
ifeq ($(CONFIG_GYRO_LSM6DSOX), m)
aglink_gyro-m += \
	lsm6dsox/lsm6dsox.o \
	lsm6dsox/lsm6dsox_reg.o
else ifeq ($(CONFIG_GYRO_SC7U22), m)
aglink_gyro-m += \
	sc7u22/sc7u22.o \
	sc7u22/sc7u22_reg.o
else ifeq ($(CONFIG_GYRO_ICM42607), m)
aglink_gyro-m += \
	icm42607/icm42607.o \
	icm42607/icm42607_driver.o \
	icm42607/icm42607_transport.o \
	icm42607/icm42607_ids.o
endif

ifeq ($(CONFIG_AG_GYRO_USE), y)
aglink_gyro-y := \
	aglink_gyro_hwio.o \
	aglink_take_gyro.o \
	aglink_gyro.o \
	aglink_gyro_osintf.o
endif
ifeq ($(CONFIG_GYRO_LSM6DSOX), y)
aglink_gyro-y += \
	lsm6dsox/lsm6dsox.o \
	lsm6dsox/lsm6dsox_reg.o
else ifeq ($(CONFIG_GYRO_SC7U22), y)
aglink_gyro-y += \
	sc7u22/sc7u22.o \
	sc7u22/sc7u22_reg.o
else ifeq ($(CONFIG_GYRO_ICM42607), y)
aglink_gyro-y += \
	icm42607/icm42607.o \
	icm42607/icm42607_driver.o \
	icm42607/icm42607_transport.o \
	icm42607/icm42607_ids.o
endif

EXTRA_CFLAGS += -I$(srctree)/$(src)/include

obj-$(CONFIG_AG_GYRO_USE) += $(CONFIG_MODULE_NAME_GYRO).o
