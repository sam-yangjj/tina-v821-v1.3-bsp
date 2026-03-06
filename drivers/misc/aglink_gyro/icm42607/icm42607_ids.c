#include "icm42607.h"

#define INV_IMU_STRING_ID_C         "ICM42607C"
#define INV_IMU_STRING_ID_P         "ICM42607P"
#define INV_IMU_STRING_ID_L         "ICM42607L"

#define INFO(_name, _whoami) \
	.name = _name, \
	.ids = _whoami,

const struct icm_info icm42607_ids[] = {
	{INFO(INV_IMU_STRING_ID_C, 0x61)},
	{INFO(INV_IMU_STRING_ID_P, 0x67)},
	{INFO(INV_IMU_STRING_ID_L, 0x63)},
};

const size_t icm42607_ids_arrsize = ARRAY_SIZE(icm42607_ids);
