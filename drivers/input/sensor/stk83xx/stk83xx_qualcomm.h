/*
 *
 * $Id: stk83xx_qualcomm.h
 *
 * Copyright (C) 2019 STK, sensortek Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef __STK83XX_QUALCOMM_H__
#define __STK83XX_QUALCOMM_H__

#include <stk83xx.h>
#include <common_define.h>

typedef struct stk83xx_wrapper {
	struct i2c_manager i2c_mgr;
	struct stk_data stk;
	u64 fifo_start_ns;
	struct input_dev *input_dev_accel; /* accel data */
	ktime_t timestamp;
#ifdef STK_AMD
	struct input_dev *input_dev_amd; /* any motion data */
#endif /* STK_AMD */
} stk83xx_wrapper;

int stk_i2c_probe(struct i2c_client *client, struct common_function *common_fn);
int stk_i2c_remove(struct i2c_client *client);
int stk83xx_suspend(struct device *dev);
int stk83xx_resume(struct device *dev);

#endif // __STK3A8X_QUALCOMM_H__