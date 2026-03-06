/*
 ******************************************************************************
 * @file    lsm6dsox_reg.c
 * @author  Sensors Software Solution Team
 * @brief   LSM6DSOX driver file
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#include "lsm6dsox_reg.h"

float_t lsm6dsox_from_fs2_to_mg(s16 lsb)
{
	return ((float_t)lsb) * 0.061f;
}

float_t lsm6dsox_from_fs4_to_mg(s16 lsb)
{
	return ((float_t)lsb) * 0.122f;
}

float_t lsm6dsox_from_fs8_to_mg(s16 lsb)
{
	return ((float_t)lsb) * 0.244f;
}

float_t lsm6dsox_from_fs16_to_mg(s16 lsb)
{
	return ((float_t)lsb) * 0.488f;
}

float_t lsm6dsox_from_fs125_to_mdps(s16 lsb)
{
	return ((float_t)lsb) * 4.375f;
}

float_t lsm6dsox_from_fs500_to_mdps(s16 lsb)
{
	return ((float_t)lsb) * 17.50f;
}

float_t lsm6dsox_from_fs250_to_mdps(s16 lsb)
{
	return ((float_t)lsb) * 8.750f;
}

float_t lsm6dsox_from_fs1000_to_mdps(s16 lsb)
{
	return ((float_t)lsb) * 35.0f;
}

float_t lsm6dsox_from_fs2000_to_mdps(s16 lsb)
{
	return ((float_t)lsb) * 70.0f;
}

s64 lsm6dsox_from_fs2000_to_uradps(s16 lsb)
{
	return lsb * 1222; // 70 * pai(3.1415926535) / 180 * 1000 = urad/s
}

float_t lsm6dsox_from_lsb_to_celsius(s16 lsb)
{
	return (((float_t)lsb / 256.0f) + 25.0f);
}

float_t lsm6dsox_from_lsb_to_nsec(s16 lsb)
{
	return ((float_t)lsb * 25000.0f);
}

/*
 * @brief  Read generic device register
 *
 * @param  ctx   communication interface handler.(ptr)
 * @param  reg   first register address to read.
 * @param  data  buffer for data read.(ptr)
 * @param  len   number of consecutive register to read.
 * @retval       interface status (MANDATORY: return 0 -> no Error)
 *
 */
int __weak lsm6dsox_read_reg(const stmdev_ctx_t *ctx, u8 reg,
							u8 *data,
							u16 len)
{
	int ret;

	if (ctx == NULL)
		return -1;

	ret = ctx->read_reg(ctx->handle, reg, data, len);

	return ret;
}

/*
 * @brief  Write generic device register
 *
 * @param  ctx   communication interface handler.(ptr)
 * @param  reg   first register address to write.
 * @param  data  the buffer contains data to be written.(ptr)
 * @param  len   number of consecutive register to write.
 * @retval       interface status (MANDATORY: return 0 -> no Error)
 *
 */
int __weak lsm6dsox_write_reg(const stmdev_ctx_t *ctx, u8 reg,
							 u8 *data,
							 u16 len)
{
	int ret;

	if (ctx == NULL)
		return -1;

	ret = ctx->write_reg(ctx->handle, reg, data, len);

	return ret;
}

/*
 * @brief  Device "Who am I".[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  buff     buffer that stores data read
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_device_id_get(const stmdev_ctx_t *ctx, u8 *buff)
{
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_WHO_AM_I, buff, 1);

	return ret;
}

/*
 * @brief  Software reset. Restore the default values
 *         in user registers[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of sw_reset in reg CTRL3_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_reset_set(const stmdev_ctx_t *ctx, u8 val)
{
	lsm6dsox_ctrl3_c_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.sw_reset = val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Software reset. Restore the default values in user registers.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of sw_reset in reg CTRL3_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_reset_get(const stmdev_ctx_t *ctx, u8 *val)
{
	lsm6dsox_ctrl3_c_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);
	*val = reg.sw_reset;

	return ret;
}

/*
 * @brief  I3C Enable/Disable communication protocol[.set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of i3c_disable
 *                                    in reg CTRL9_XL
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_i3c_disable_set(const stmdev_ctx_t *ctx,
							lsm6dsox_i3c_disable_t val)
{
	lsm6dsox_i3c_bus_avb_t i3c_bus_avb;
	lsm6dsox_ctrl9_xl_t ctrl9_xl;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL9_XL, (u8 *)&ctrl9_xl, 1);

	if (ret == 0) {
		ctrl9_xl.i3c_disable = ((u8)val & 0x80U) >> 7;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL9_XL, (u8 *)&ctrl9_xl, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_I3C_BUS_AVB,
							   (u8 *)&i3c_bus_avb, 1);
	}

	if (ret == 0) {
		i3c_bus_avb.i3c_bus_avb_sel = (u8)val & 0x03U;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_I3C_BUS_AVB,
								(u8 *)&i3c_bus_avb, 1);
	}

	return ret;
}

/*
 * @brief  Block data update.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of bdu in reg CTRL3_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_block_data_update_set(const stmdev_ctx_t *ctx, u8 val)
{
	lsm6dsox_ctrl3_c_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.bdu = val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Accelerometer full-scale selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of fs_xl in reg CTRL1_XL
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_xl_full_scale_set(const stmdev_ctx_t *ctx,
							  lsm6dsox_fs_xl_t val)
{
	lsm6dsox_ctrl1_xl_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL1_XL, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.fs_xl = (u8) val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL1_XL, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Gyroscope UI chain full-scale selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of fs_g in reg CTRL2_G
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_gy_full_scale_set(const stmdev_ctx_t *ctx,
							  lsm6dsox_fs_g_t val)
{
	lsm6dsox_ctrl2_g_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL2_G, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.fs_g = (u8) val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL2_G, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  FIFO watermark level selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of wtm in reg FIFO_CTRL1
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_watermark_set(const stmdev_ctx_t *ctx, u16 val)
{
	lsm6dsox_fifo_ctrl1_t fifo_ctrl1;
	lsm6dsox_fifo_ctrl2_t fifo_ctrl2;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_CTRL2,
							(u8 *)&fifo_ctrl2, 1);

	if (ret == 0) {
		fifo_ctrl1.wtm = 0x00FFU & (u8)val;
		fifo_ctrl2.wtm = (u8)((0x0100U & val) >> 8);
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL1,
								(u8 *)&fifo_ctrl1, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL2,
								(u8 *)&fifo_ctrl2, 1);
	}

	return ret;
}

/*
 * @brief  FIFO mode selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of fifo_mode in reg FIFO_CTRL4
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_mode_set(const stmdev_ctx_t *ctx,
							lsm6dsox_fifo_mode_t val)
{
	lsm6dsox_fifo_ctrl4_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_CTRL4, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.fifo_mode = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL4, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Data-ready pulsed / letched mode.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of
 *                                     dataready_pulsed in
 *                                     reg COUNTER_BDR_REG1
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_data_ready_mode_set(const stmdev_ctx_t *ctx,
								lsm6dsox_dataready_pulsed_t val)
{
	lsm6dsox_counter_bdr_reg1_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_COUNTER_BDR_REG1,
							(u8 *)&reg, 1);

	if (ret == 0) {
		reg.dataready_pulsed = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_COUNTER_BDR_REG1,
							(u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Interrupt active-high/low.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of h_lactive in reg CTRL3_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_pin_polarity_set(const stmdev_ctx_t *ctx,
								lsm6dsox_h_lactive_t val)
{
	lsm6dsox_ctrl3_c_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.h_lactive = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL3_C, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Enable access to the embedded functions/sensor
 *         hub configuration registers.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of reg_access in
 *                               reg FUNC_CFG_ACCESS
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_mem_bank_set(const stmdev_ctx_t *ctx,
							lsm6dsox_reg_access_t val)
{
	lsm6dsox_func_cfg_access_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FUNC_CFG_ACCESS,
							(u8 *)&reg, 1);

	if (ret == 0) {
		reg.reg_access = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FUNC_CFG_ACCESS,
								(u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Route interrupt signals on int1 pin.[get]
 *
 * @param  ctx          communication interface handler.(ptr)
 * @param  val          the signals that are routed on int1 pin.(ptr)
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_pin_int1_route_get(const stmdev_ctx_t *ctx,
								lsm6dsox_pin_int1_route_t *val)
{
	lsm6dsox_emb_func_int1_t   emb_func_int1;
	lsm6dsox_fsm_int1_a_t      fsm_int1_a;
	lsm6dsox_fsm_int1_b_t      fsm_int1_b;
	lsm6dsox_int1_ctrl_t       int1_ctrl;
	lsm6dsox_int2_ctrl_t       int2_ctrl;
	lsm6dsox_mlc_int1_t        mlc_int1;
	lsm6dsox_md2_cfg_t         md2_cfg;
	lsm6dsox_md1_cfg_t         md1_cfg;
	lsm6dsox_ctrl4_c_t         ctrl4_c;
	int                    ret;
	ctrl4_c.int2_on_int1 = 0;

	ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MLC_INT1,
								(u8 *)&mlc_int1, 1);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_INT1,
								(u8 *)&emb_func_int1, 1);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FSM_INT1_A,
								(u8 *)&fsm_int1_a, 1);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FSM_INT1_B,
								(u8 *)&fsm_int1_b, 1);

	if (ret == 0)
		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_INT1_CTRL,
								(u8 *)&int1_ctrl, 1);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MD1_CFG, (u8 *)&md1_cfg, 1);

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);

	if (ctrl4_c.int2_on_int1 == PROPERTY_ENABLE) {
		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_INT2_CTRL,
									(u8 *)&int2_ctrl, 1);
			val->drdy_temp = int2_ctrl.int2_drdy_temp;
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MD2_CFG, (u8 *)&md2_cfg, 1);
			val->timestamp = md2_cfg.int2_timestamp;
		}
	} else {
		val->drdy_temp = PROPERTY_DISABLE;
		val->timestamp = PROPERTY_DISABLE;
	}

	val->drdy_xl   = int1_ctrl.int1_drdy_xl;
	val->drdy_g    = int1_ctrl.int1_drdy_g;
	val->boot      = int1_ctrl.int1_boot;
	val->fifo_th   = int1_ctrl.int1_fifo_th;
	val->fifo_ovr  = int1_ctrl.int1_fifo_ovr;
	val->fifo_full = int1_ctrl.int1_fifo_full;
	val->fifo_bdr  = int1_ctrl.int1_cnt_bdr;
	val->den_flag  = int1_ctrl.den_drdy_flag;
	val->sh_endop     = md1_cfg.int1_shub;
	val->six_d        = md1_cfg.int1_6d;
	val->double_tap   = md1_cfg.int1_double_tap;
	val->free_fall    = md1_cfg.int1_ff;
	val->wake_up      = md1_cfg.int1_wu;
	val->single_tap   = md1_cfg.int1_single_tap;
	val->sleep_change = md1_cfg.int1_sleep_change;
	val->step_detector = emb_func_int1.int1_step_detector;
	val->tilt          = emb_func_int1.int1_tilt;
	val->sig_mot       = emb_func_int1.int1_sig_mot;
	val->fsm_lc        = emb_func_int1.int1_fsm_lc;
	val->fsm1 = fsm_int1_a.int1_fsm1;
	val->fsm2 = fsm_int1_a.int1_fsm2;
	val->fsm3 = fsm_int1_a.int1_fsm3;
	val->fsm4 = fsm_int1_a.int1_fsm4;
	val->fsm5 = fsm_int1_a.int1_fsm5;
	val->fsm6 = fsm_int1_a.int1_fsm6;
	val->fsm7 = fsm_int1_a.int1_fsm7;
	val->fsm8 = fsm_int1_a.int1_fsm8;
	val->fsm9  = fsm_int1_b.int1_fsm9;
	val->fsm10 = fsm_int1_b.int1_fsm10;
	val->fsm11 = fsm_int1_b.int1_fsm11;
	val->fsm12 = fsm_int1_b.int1_fsm12;
	val->fsm13 = fsm_int1_b.int1_fsm13;
	val->fsm14 = fsm_int1_b.int1_fsm14;
	val->fsm15 = fsm_int1_b.int1_fsm15;
	val->fsm16 = fsm_int1_b.int1_fsm16;
	val->mlc1 = mlc_int1.int1_mlc1;
	val->mlc2 = mlc_int1.int1_mlc2;
	val->mlc3 = mlc_int1.int1_mlc3;
	val->mlc4 = mlc_int1.int1_mlc4;
	val->mlc5 = mlc_int1.int1_mlc5;
	val->mlc6 = mlc_int1.int1_mlc6;
	val->mlc7 = mlc_int1.int1_mlc7;
	val->mlc8 = mlc_int1.int1_mlc8;

	return ret;
}

/*
 * @brief  Route interrupt signals on int1 pin.[set]
 *
 * @param  ctx          communication interface handler.(ptr)
 * @param  val          the signals to route on int1 pin.
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_pin_int1_route_set(const stmdev_ctx_t *ctx,
								lsm6dsox_pin_int1_route_t val)
{
	lsm6dsox_pin_int2_route_t  pin_int2_route;
	lsm6dsox_emb_func_int1_t   emb_func_int1 = {0};
	lsm6dsox_fsm_int1_a_t      fsm_int1_a = {0};
	lsm6dsox_fsm_int1_b_t      fsm_int1_b = {0};
	lsm6dsox_int1_ctrl_t       int1_ctrl = {0};
	lsm6dsox_int2_ctrl_t       int2_ctrl;
	lsm6dsox_mlc_int1_t        mlc_int1 = {0};
	lsm6dsox_tap_cfg2_t        tap_cfg2;
	lsm6dsox_md2_cfg_t         md2_cfg;
	lsm6dsox_md1_cfg_t         md1_cfg = {0};
	lsm6dsox_ctrl4_c_t         ctrl4_c;
	int                    ret;

	int1_ctrl.int1_drdy_xl   = val.drdy_xl;
	int1_ctrl.int1_drdy_g    = val.drdy_g;
	int1_ctrl.int1_boot      = val.boot;
	int1_ctrl.int1_fifo_th   = val.fifo_th;
	int1_ctrl.int1_fifo_ovr  = val.fifo_ovr;
	int1_ctrl.int1_fifo_full = val.fifo_full;
	int1_ctrl.int1_cnt_bdr   = val.fifo_bdr;
	int1_ctrl.den_drdy_flag  = val.den_flag;
	md1_cfg.int1_shub         = val.sh_endop;
	md1_cfg.int1_6d           = val.six_d;
	md1_cfg.int1_double_tap   = val.double_tap;
	md1_cfg.int1_ff           = val.free_fall;
	md1_cfg.int1_wu           = val.wake_up;
	md1_cfg.int1_single_tap   = val.single_tap;
	md1_cfg.int1_sleep_change = val.sleep_change;
	emb_func_int1.not_used_01 = 0;
	emb_func_int1.int1_step_detector = val.step_detector;
	emb_func_int1.int1_tilt          = val.tilt;
	emb_func_int1.int1_sig_mot       = val.sig_mot;
	emb_func_int1.not_used_02 = 0;
	emb_func_int1.int1_fsm_lc        = val.fsm_lc;
	fsm_int1_a.int1_fsm1 = val.fsm1;
	fsm_int1_a.int1_fsm2 = val.fsm2;
	fsm_int1_a.int1_fsm3 = val.fsm3;
	fsm_int1_a.int1_fsm4 = val.fsm4;
	fsm_int1_a.int1_fsm5 = val.fsm5;
	fsm_int1_a.int1_fsm6 = val.fsm6;
	fsm_int1_a.int1_fsm7 = val.fsm7;
	fsm_int1_a.int1_fsm8 = val.fsm8;
	fsm_int1_b.int1_fsm9  = val.fsm9 ;
	fsm_int1_b.int1_fsm10 = val.fsm10;
	fsm_int1_b.int1_fsm11 = val.fsm11;
	fsm_int1_b.int1_fsm12 = val.fsm12;
	fsm_int1_b.int1_fsm13 = val.fsm13;
	fsm_int1_b.int1_fsm14 = val.fsm14;
	fsm_int1_b.int1_fsm15 = val.fsm15;
	fsm_int1_b.int1_fsm16 = val.fsm16;
	mlc_int1.int1_mlc1 = val.mlc1;
	mlc_int1.int1_mlc2 = val.mlc2;
	mlc_int1.int1_mlc3 = val.mlc3;
	mlc_int1.int1_mlc4 = val.mlc4;
	mlc_int1.int1_mlc5 = val.mlc5;
	mlc_int1.int1_mlc6 = val.mlc6;
	mlc_int1.int1_mlc7 = val.mlc7;
	mlc_int1.int1_mlc8 = val.mlc8;
	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);

	if (ret == 0) {
		if ((val.drdy_temp | val.timestamp) != PROPERTY_DISABLE) {
			ctrl4_c.int2_on_int1 = PROPERTY_ENABLE;
		} else {
			ctrl4_c.int2_on_int1 = PROPERTY_DISABLE;
		}

		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_MLC_INT1,
								(u8 *)&mlc_int1, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_EMB_FUNC_INT1,
								(u8 *)&emb_func_int1, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FSM_INT1_A,
								(u8 *)&fsm_int1_a, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FSM_INT1_B,
								(u8 *)&fsm_int1_b, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
	}

	if (ret == 0) {
		if ((emb_func_int1.int1_fsm_lc
			| emb_func_int1.int1_sig_mot
			| emb_func_int1.int1_step_detector
			| emb_func_int1.int1_tilt
			| fsm_int1_a.int1_fsm1
			| fsm_int1_a.int1_fsm2
			| fsm_int1_a.int1_fsm3
			| fsm_int1_a.int1_fsm4
			| fsm_int1_a.int1_fsm5
			| fsm_int1_a.int1_fsm6
			| fsm_int1_a.int1_fsm7
			| fsm_int1_a.int1_fsm8
			| fsm_int1_b.int1_fsm9
			| fsm_int1_b.int1_fsm10
			| fsm_int1_b.int1_fsm11
			| fsm_int1_b.int1_fsm12
			| fsm_int1_b.int1_fsm13
			| fsm_int1_b.int1_fsm14
			| fsm_int1_b.int1_fsm15
			| fsm_int1_b.int1_fsm16
			| mlc_int1.int1_mlc1
			| mlc_int1.int1_mlc2
			| mlc_int1.int1_mlc3
			| mlc_int1.int1_mlc4
			| mlc_int1.int1_mlc5
			| mlc_int1.int1_mlc6
			| mlc_int1.int1_mlc7
			| mlc_int1.int1_mlc8) != PROPERTY_DISABLE) {
			md1_cfg.int1_emb_func = PROPERTY_ENABLE;
		} else {
			md1_cfg.int1_emb_func = PROPERTY_DISABLE;
		}

		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_INT1_CTRL,
								(u8 *)&int1_ctrl, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_MD1_CFG, (u8 *)&md1_cfg, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_INT2_CTRL,
								(u8 *)&int2_ctrl, 1);
	}

	if (ret == 0) {
		int2_ctrl.int2_drdy_temp = val.drdy_temp;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_INT2_CTRL,
								(u8 *)&int2_ctrl, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MD2_CFG, (u8 *)&md2_cfg, 1);
	}

	if (ret == 0) {
		md2_cfg.int2_timestamp = val.timestamp;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_MD2_CFG, (u8 *)&md2_cfg, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_TAP_CFG2, (u8 *) &tap_cfg2, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_pin_int2_route_get(ctx, NULL, &pin_int2_route);
	}

	if (ret == 0) {
		if ((pin_int2_route.fifo_bdr
			| pin_int2_route.drdy_g
			| pin_int2_route.drdy_temp
			| pin_int2_route.drdy_xl
			| pin_int2_route.fifo_full
			| pin_int2_route.fifo_ovr
			| pin_int2_route.fifo_th
			| pin_int2_route.six_d
			| pin_int2_route.double_tap
			| pin_int2_route.free_fall
			| pin_int2_route.wake_up
			| pin_int2_route.single_tap
			| pin_int2_route.sleep_change
			| int1_ctrl.den_drdy_flag
			| int1_ctrl.int1_boot
			| int1_ctrl.int1_cnt_bdr
			| int1_ctrl.int1_drdy_g
			| int1_ctrl.int1_drdy_xl
			| int1_ctrl.int1_fifo_full
			| int1_ctrl.int1_fifo_ovr
			| int1_ctrl.int1_fifo_th
			| md1_cfg.int1_shub
			| md1_cfg.int1_6d
			| md1_cfg.int1_double_tap
			| md1_cfg.int1_ff
			| md1_cfg.int1_wu
			| md1_cfg.int1_single_tap
			| md1_cfg.int1_sleep_change) != PROPERTY_DISABLE) {
			tap_cfg2.interrupts_enable = PROPERTY_ENABLE;
		} else {
			tap_cfg2.interrupts_enable = PROPERTY_DISABLE;
		}

		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_TAP_CFG2,
								(u8 *) &tap_cfg2, 1);
	}

	return ret;
}

/*
 * @brief  Route interrupt signals on int2 pin.[get]
 *
 * @param  ctx          communication interface handler. Use NULL to ignore
 *                      this interface.(ptr)
 * @param  aux_ctx      auxiliary communication interface handler. Use NULL
 *                      to ignore this interface.(ptr)
 * @param  val          the signals that are routed on int2 pin.(ptr)
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_pin_int2_route_get(const stmdev_ctx_t *ctx,
								stmdev_ctx_t *aux_ctx,
								lsm6dsox_pin_int2_route_t *val)
{
	lsm6dsox_emb_func_int2_t  emb_func_int2;
	lsm6dsox_spi2_int_ois_t   spi2_int_ois;
	lsm6dsox_fsm_int2_a_t     fsm_int2_a;
	lsm6dsox_fsm_int2_b_t     fsm_int2_b;
	lsm6dsox_int2_ctrl_t      int2_ctrl;
	lsm6dsox_mlc_int2_t       mlc_int2;
	lsm6dsox_md2_cfg_t        md2_cfg;
	lsm6dsox_ctrl4_c_t        ctrl4_c;
	int                   ret;
	ret = 0;
	ctrl4_c.int2_on_int1 = 0;

	if (aux_ctx != NULL) {
		ret = lsm6dsox_read_reg(aux_ctx, LSM6DSOX_SPI2_INT_OIS,
								(u8 *)&spi2_int_ois, 1);
		val->drdy_ois = spi2_int_ois.int2_drdy_ois;
	}

	if (ctx != NULL) {
		if (ret == 0) {
			ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MLC_INT2,
									(u8 *)&mlc_int2, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_INT2,
									(u8 *)&emb_func_int2, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FSM_INT2_A,
									(u8 *)&fsm_int2_a, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FSM_INT2_B,
									(u8 *)&fsm_int2_b, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_INT2_CTRL,
									(u8 *)&int2_ctrl, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MD2_CFG,
									(u8 *)&md2_cfg, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);
		}

		if (ctrl4_c.int2_on_int1 == PROPERTY_DISABLE) {
			if (ret == 0) {
				ret = lsm6dsox_read_reg(ctx, LSM6DSOX_INT2_CTRL,
										(u8 *)&int2_ctrl, 1);
				val->drdy_temp = int2_ctrl.int2_drdy_temp;
			}

			if (ret == 0) {
				ret = lsm6dsox_read_reg(ctx, LSM6DSOX_MD2_CFG, (u8 *)&md2_cfg, 1);
				val->timestamp = md2_cfg.int2_timestamp;
			}
		} else {
			val->drdy_temp = PROPERTY_DISABLE;
			val->timestamp = PROPERTY_DISABLE;
		}

		val->drdy_xl   = int2_ctrl.int2_drdy_xl;
		val->drdy_g    = int2_ctrl.int2_drdy_g;
		val->drdy_temp = int2_ctrl.int2_drdy_temp;
		val->fifo_th   = int2_ctrl.int2_fifo_th;
		val->fifo_ovr  = int2_ctrl.int2_fifo_ovr;
		val->fifo_full = int2_ctrl.int2_fifo_full;
		val->fifo_bdr   = int2_ctrl.int2_cnt_bdr;
		val->timestamp    = md2_cfg.int2_timestamp;
		val->six_d        = md2_cfg.int2_6d;
		val->double_tap   = md2_cfg.int2_double_tap;
		val->free_fall    = md2_cfg.int2_ff;
		val->wake_up      = md2_cfg.int2_wu;
		val->single_tap   = md2_cfg.int2_single_tap;
		val->sleep_change = md2_cfg.int2_sleep_change;
		val->step_detector = emb_func_int2. int2_step_detector;
		val->tilt          = emb_func_int2.int2_tilt;
		val->fsm_lc        = emb_func_int2.int2_fsm_lc;
		val->fsm1 = fsm_int2_a.int2_fsm1;
		val->fsm2 = fsm_int2_a.int2_fsm2;
		val->fsm3 = fsm_int2_a.int2_fsm3;
		val->fsm4 = fsm_int2_a.int2_fsm4;
		val->fsm5 = fsm_int2_a.int2_fsm5;
		val->fsm6 = fsm_int2_a.int2_fsm6;
		val->fsm7 = fsm_int2_a.int2_fsm7;
		val->fsm8 = fsm_int2_a.int2_fsm8;
		val->fsm9  = fsm_int2_b.int2_fsm9;
		val->fsm10 = fsm_int2_b.int2_fsm10;
		val->fsm11 = fsm_int2_b.int2_fsm11;
		val->fsm12 = fsm_int2_b.int2_fsm12;
		val->fsm13 = fsm_int2_b.int2_fsm13;
		val->fsm14 = fsm_int2_b.int2_fsm14;
		val->fsm15 = fsm_int2_b.int2_fsm15;
		val->fsm16 = fsm_int2_b.int2_fsm16;
		val->mlc1 = mlc_int2.int2_mlc1;
		val->mlc2 = mlc_int2.int2_mlc2;
		val->mlc3 = mlc_int2.int2_mlc3;
		val->mlc4 = mlc_int2.int2_mlc4;
		val->mlc5 = mlc_int2.int2_mlc5;
		val->mlc6 = mlc_int2.int2_mlc6;
		val->mlc7 = mlc_int2.int2_mlc7;
		val->mlc8 = mlc_int2.int2_mlc8;
	}

	return ret;
}

/*
 * @brief  Route interrupt signals on int2 pin.[set]
 *
 * @param  ctx          communication interface handler. Use NULL to ignore
 *                      this interface.(ptr)
 * @param  aux_ctx      auxiliary communication interface handler. Use NULL
 *                      to ignore this interface.(ptr)
 * @param  val          the signals to route on int2 pin.
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_pin_int2_route_set(const stmdev_ctx_t *ctx,
								stmdev_ctx_t *aux_ctx,
								lsm6dsox_pin_int2_route_t val)
{
	lsm6dsox_pin_int1_route_t pin_int1_route;
	lsm6dsox_emb_func_int2_t  emb_func_int2;
	lsm6dsox_spi2_int_ois_t   spi2_int_ois;
	lsm6dsox_fsm_int2_a_t     fsm_int2_a;
	lsm6dsox_fsm_int2_b_t     fsm_int2_b;
	lsm6dsox_int2_ctrl_t      int2_ctrl;
	lsm6dsox_mlc_int2_t       mlc_int2;
	lsm6dsox_tap_cfg2_t       tap_cfg2;
	lsm6dsox_md2_cfg_t        md2_cfg;
	lsm6dsox_ctrl4_c_t        ctrl4_c;
	int                   ret;
	ret = 0;

	if (aux_ctx != NULL) {
		ret = lsm6dsox_read_reg(aux_ctx, LSM6DSOX_SPI2_INT_OIS,
								(u8 *)&spi2_int_ois, 1);

		if (ret == 0) {
			spi2_int_ois.int2_drdy_ois = val.drdy_ois;
			ret = lsm6dsox_write_reg(aux_ctx, LSM6DSOX_SPI2_INT_OIS,
									(u8 *)&spi2_int_ois, 1);
		}
	}

	if (ctx != NULL) {
		int2_ctrl.int2_drdy_xl   = val.drdy_xl;
		int2_ctrl.int2_drdy_g    = val.drdy_g;
		int2_ctrl.int2_drdy_temp = val.drdy_temp;
		int2_ctrl.int2_fifo_th   = val.fifo_th;
		int2_ctrl.int2_fifo_ovr  = val.fifo_ovr;
		int2_ctrl.int2_fifo_full = val.fifo_full;
		int2_ctrl.int2_cnt_bdr   = val.fifo_bdr;
		int2_ctrl.not_used_01    = 0;
		md2_cfg.int2_timestamp    = val.timestamp;
		md2_cfg.int2_6d           = val.six_d;
		md2_cfg.int2_double_tap   = val.double_tap;
		md2_cfg.int2_ff           = val.free_fall;
		md2_cfg.int2_wu           = val.wake_up;
		md2_cfg.int2_single_tap   = val.single_tap;
		md2_cfg.int2_sleep_change = val.sleep_change;
		emb_func_int2.not_used_01 = 0;
		emb_func_int2. int2_step_detector = val.step_detector;
		emb_func_int2.int2_tilt           = val.tilt;
		emb_func_int2.int2_sig_mot        = val.sig_mot;
		emb_func_int2.not_used_02 = 0;
		emb_func_int2.int2_fsm_lc         = val.fsm_lc;
		fsm_int2_a.int2_fsm1 = val.fsm1;
		fsm_int2_a.int2_fsm2 = val.fsm2;
		fsm_int2_a.int2_fsm3 = val.fsm3;
		fsm_int2_a.int2_fsm4 = val.fsm4;
		fsm_int2_a.int2_fsm5 = val.fsm5;
		fsm_int2_a.int2_fsm6 = val.fsm6;
		fsm_int2_a.int2_fsm7 = val.fsm7;
		fsm_int2_a.int2_fsm8 = val.fsm8;
		fsm_int2_b.int2_fsm9  = val.fsm9 ;
		fsm_int2_b.int2_fsm10 = val.fsm10;
		fsm_int2_b.int2_fsm11 = val.fsm11;
		fsm_int2_b.int2_fsm12 = val.fsm12;
		fsm_int2_b.int2_fsm13 = val.fsm13;
		fsm_int2_b.int2_fsm14 = val.fsm14;
		fsm_int2_b.int2_fsm15 = val.fsm15;
		fsm_int2_b.int2_fsm16 = val.fsm16;
		mlc_int2.int2_mlc1 = val.mlc1;
		mlc_int2.int2_mlc2 = val.mlc2;
		mlc_int2.int2_mlc3 = val.mlc3;
		mlc_int2.int2_mlc4 = val.mlc4;
		mlc_int2.int2_mlc5 = val.mlc5;
		mlc_int2.int2_mlc6 = val.mlc6;
		mlc_int2.int2_mlc7 = val.mlc7;
		mlc_int2.int2_mlc8 = val.mlc8;

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);

			if (ret == 0) {
				if ((val.drdy_temp | val.timestamp) != PROPERTY_DISABLE) {
					ctrl4_c.int2_on_int1 = PROPERTY_DISABLE;
				}

				ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL4_C, (u8 *)&ctrl4_c, 1);
			}
		}

		if (ret == 0) {
			ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);
		}

		if (ret == 0) {
			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_MLC_INT2,
									(u8 *)&mlc_int2, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_EMB_FUNC_INT2,
									(u8 *)&emb_func_int2, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FSM_INT2_A,
									(u8 *)&fsm_int2_a, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FSM_INT2_B,
									(u8 *)&fsm_int2_b, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
		}

		if (ret == 0) {
			if ((emb_func_int2.int2_fsm_lc
				| emb_func_int2.int2_sig_mot
				| emb_func_int2.int2_step_detector
				| emb_func_int2.int2_tilt
				| fsm_int2_a.int2_fsm1
				| fsm_int2_a.int2_fsm2
				| fsm_int2_a.int2_fsm3
				| fsm_int2_a.int2_fsm4
				| fsm_int2_a.int2_fsm5
				| fsm_int2_a.int2_fsm6
				| fsm_int2_a.int2_fsm7
				| fsm_int2_a.int2_fsm8
				| fsm_int2_b.int2_fsm9
				| fsm_int2_b.int2_fsm10
				| fsm_int2_b.int2_fsm11
				| fsm_int2_b.int2_fsm12
				| fsm_int2_b.int2_fsm13
				| fsm_int2_b.int2_fsm14
				| fsm_int2_b.int2_fsm15
				| fsm_int2_b.int2_fsm16
				| mlc_int2.int2_mlc1
				| mlc_int2.int2_mlc2
				| mlc_int2.int2_mlc3
				| mlc_int2.int2_mlc4
				| mlc_int2.int2_mlc5
				| mlc_int2.int2_mlc6
				| mlc_int2.int2_mlc7
				| mlc_int2.int2_mlc8) != PROPERTY_DISABLE) {
				md2_cfg.int2_emb_func = PROPERTY_ENABLE;
			} else {
				md2_cfg.int2_emb_func = PROPERTY_DISABLE;
			}

			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_INT2_CTRL,
									(u8 *)&int2_ctrl, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_MD2_CFG, (u8 *)&md2_cfg, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_read_reg(ctx, LSM6DSOX_TAP_CFG2, (u8 *) &tap_cfg2, 1);
		}

		if (ret == 0) {
			ret = lsm6dsox_pin_int1_route_get(ctx, &pin_int1_route);
		}

		if (ret == 0) {
			if ((val.fifo_bdr
				| val.drdy_g
				| val.drdy_temp
				| val.drdy_xl
				| val.fifo_full
				| val.fifo_ovr
				| val.fifo_th
				| val.six_d
				| val.double_tap
				| val.free_fall
				| val.wake_up
				| val.single_tap
				| val.sleep_change
				| pin_int1_route.den_flag
				| pin_int1_route.boot
				| pin_int1_route.fifo_bdr
				| pin_int1_route.drdy_g
				| pin_int1_route.drdy_xl
				| pin_int1_route.fifo_full
				| pin_int1_route.fifo_ovr
				| pin_int1_route.fifo_th
				| pin_int1_route.six_d
				| pin_int1_route.double_tap
				| pin_int1_route.free_fall
				| pin_int1_route.wake_up
				| pin_int1_route.single_tap
				| pin_int1_route.sleep_change) != PROPERTY_DISABLE) {
				tap_cfg2.interrupts_enable = PROPERTY_ENABLE;
			} else {
				tap_cfg2.interrupts_enable = PROPERTY_DISABLE;
			}

			ret = lsm6dsox_write_reg(ctx, LSM6DSOX_TAP_CFG2,
									(u8 *) &tap_cfg2, 1);
		}
	}

	return ret;
}

/**
  * @brief  Selects Batching Data Rate (writing frequency in FIFO)
  *         for accelerometer data.[set]
  *
  * @param  ctx      read / write interface definitions
  * @param  val      change the values of bdr_xl in reg FIFO_CTRL3
  * @retval             interface status (MANDATORY: return 0 -> no Error)
  *
  */
int lsm6dsox_fifo_xl_batch_set(const stmdev_ctx_t *ctx,
								lsm6dsox_bdr_xl_t val)
{
	lsm6dsox_fifo_ctrl3_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_CTRL3, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.bdr_xl = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL3, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Finite State Machine enable.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      union of registers from FSM_ENABLE_A to FSM_ENABLE_B
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fsm_enable_get(const stmdev_ctx_t *ctx,
							lsm6dsox_emb_fsm_enable_t *val)
{
	int ret;

	ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FSM_ENABLE_A, (u8 *) val, 2);
	}

	if (ret == 0) {
		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
	}

	return ret;
}

/*
 * @brief  Embedded functions.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      get the values of registers
 *                  EMB_FUNC_EN_A e EMB_FUNC_EN_B.
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_embedded_sens_get(const stmdev_ctx_t *ctx,
								lsm6dsox_emb_sens_t *emb_sens)
{
	lsm6dsox_emb_func_en_a_t emb_func_en_a;
	lsm6dsox_emb_func_en_b_t emb_func_en_b;
	int ret;

	ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_EN_A,
								(u8 *)&emb_func_en_a, 1);
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_EN_B,
								(u8 *)&emb_func_en_b, 1);
		emb_sens->mlc = emb_func_en_b.mlc_en;
		emb_sens->fsm = emb_func_en_b.fsm_en;
		emb_sens->tilt = emb_func_en_a.tilt_en;
		emb_sens->step = emb_func_en_a.pedo_en;
		emb_sens->sig_mot = emb_func_en_a.sign_motion_en;
		emb_sens->fifo_compr = emb_func_en_b.fifo_compr_en;
	}

	if (ret == 0) {
		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
	}

	return ret;
}

/*
 * @brief  Machine Learning Core data rate selection.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of mlc_odr in
 *                  reg EMB_FUNC_ODR_CFG_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_mlc_data_rate_get(const stmdev_ctx_t *ctx,
								lsm6dsox_mlc_odr_t *val)
{
	lsm6dsox_emb_func_odr_cfg_c_t reg;
	int ret;

	ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_ODR_CFG_C,
								(u8 *)&reg, 1);
	}

	if (ret == 0) {
		switch (reg.mlc_odr) {
		case LSM6DSOX_ODR_PRGS_12Hz5:
			*val = LSM6DSOX_ODR_PRGS_12Hz5;
			break;

		case LSM6DSOX_ODR_PRGS_26Hz:
			*val = LSM6DSOX_ODR_PRGS_26Hz;
			break;

		case LSM6DSOX_ODR_PRGS_52Hz:
			*val = LSM6DSOX_ODR_PRGS_52Hz;
			break;

		case LSM6DSOX_ODR_PRGS_104Hz:
			*val = LSM6DSOX_ODR_PRGS_104Hz;
			break;

		default:
			*val = LSM6DSOX_ODR_PRGS_12Hz5;
			break;
		}

		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
	}

	return ret;
}

/*
 * @brief  Finite State Machine ODR configuration.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      Get the values of fsm_odr in reg EMB_FUNC_ODR_CFG_B
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fsm_data_rate_get(const stmdev_ctx_t *ctx,
								lsm6dsox_fsm_odr_t *val)
{
	lsm6dsox_emb_func_odr_cfg_b_t reg;
	int ret;

	ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_EMBEDDED_FUNC_BANK);

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_EMB_FUNC_ODR_CFG_B,
								(u8 *)&reg, 1);
	}

	if (ret == 0) {
		switch (reg.fsm_odr) {
		case LSM6DSOX_ODR_FSM_12Hz5:
			*val = LSM6DSOX_ODR_FSM_12Hz5;
			break;

		case LSM6DSOX_ODR_FSM_26Hz:
			*val = LSM6DSOX_ODR_FSM_26Hz;
			break;

		case LSM6DSOX_ODR_FSM_52Hz:
			*val = LSM6DSOX_ODR_FSM_52Hz;
			break;

		case LSM6DSOX_ODR_FSM_104Hz:
			*val = LSM6DSOX_ODR_FSM_104Hz;
			break;

		default:
			*val = LSM6DSOX_ODR_FSM_12Hz5;
			break;
		}

		ret = lsm6dsox_mem_bank_set(ctx, LSM6DSOX_USER_BANK);
	}

	return ret;
}

/*
 * @brief  Accelerometer UI data rate selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of odr_xl in reg CTRL1_XL
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_xl_data_rate_set(const stmdev_ctx_t *ctx,
								lsm6dsox_odr_xl_t val)
{
	lsm6dsox_odr_xl_t odr_xl =  val;
	lsm6dsox_emb_fsm_enable_t fsm_enable;
	lsm6dsox_fsm_odr_t fsm_odr;
	lsm6dsox_emb_sens_t emb_sens;
	lsm6dsox_mlc_odr_t mlc_odr;
	lsm6dsox_ctrl1_xl_t reg;
	int ret;

	/* Check the Finite State Machine data rate constraints */
	ret =  lsm6dsox_fsm_enable_get(ctx, &fsm_enable);

	if (ret == 0) {
		if ((fsm_enable.fsm_enable_a.fsm1_en  |
			fsm_enable.fsm_enable_a.fsm2_en  |
			fsm_enable.fsm_enable_a.fsm3_en  |
			fsm_enable.fsm_enable_a.fsm4_en  |
			fsm_enable.fsm_enable_a.fsm5_en  |
			fsm_enable.fsm_enable_a.fsm6_en  |
			fsm_enable.fsm_enable_a.fsm7_en  |
			fsm_enable.fsm_enable_a.fsm8_en  |
			fsm_enable.fsm_enable_b.fsm9_en  |
			fsm_enable.fsm_enable_b.fsm10_en |
			fsm_enable.fsm_enable_b.fsm11_en |
			fsm_enable.fsm_enable_b.fsm12_en |
			fsm_enable.fsm_enable_b.fsm13_en |
			fsm_enable.fsm_enable_b.fsm14_en |
			fsm_enable.fsm_enable_b.fsm15_en |
			fsm_enable.fsm_enable_b.fsm16_en) == PROPERTY_ENABLE) {
			ret =  lsm6dsox_fsm_data_rate_get(ctx, &fsm_odr);

			if (ret == 0) {
				switch (fsm_odr) {
				case LSM6DSOX_ODR_FSM_12Hz5:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_12Hz5;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_FSM_26Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_26Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_26Hz;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_FSM_52Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else if (val == LSM6DSOX_XL_ODR_26Hz)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_FSM_104Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_26Hz)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_52Hz)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else
						odr_xl = val;

					break;

				default:
					odr_xl = val;
					break;
				}
			}
		}
	}

	/* Check the Machine Learning Core data rate constraints */
	emb_sens.mlc = PROPERTY_DISABLE;

	if (ret == 0) {
		lsm6dsox_embedded_sens_get(ctx, &emb_sens);

		if (emb_sens.mlc == PROPERTY_ENABLE) {
			ret =  lsm6dsox_mlc_data_rate_get(ctx, &mlc_odr);

			if (ret == 0) {
				switch (mlc_odr) {
				case LSM6DSOX_ODR_PRGS_12Hz5:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_12Hz5;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_PRGS_26Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_26Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_26Hz;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_PRGS_52Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else if (val == LSM6DSOX_XL_ODR_26Hz)
						odr_xl = LSM6DSOX_XL_ODR_52Hz;
					else
						odr_xl = val;

					break;

				case LSM6DSOX_ODR_PRGS_104Hz:
					if (val == LSM6DSOX_XL_ODR_OFF)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_12Hz5)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_26Hz)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else if (val == LSM6DSOX_XL_ODR_52Hz)
						odr_xl = LSM6DSOX_XL_ODR_104Hz;
					else
						odr_xl = val;

					break;

				default:
					odr_xl = val;
					break;
				}
			}
		}
	}

	if (ret == 0) {
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL1_XL, (u8 *)&reg, 1);
	}

	if (ret == 0) {
		reg.odr_xl = (u8) odr_xl;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL1_XL, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Selects Batching Data Rate (writing frequency in FIFO)
 *         for gyroscope data.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of bdr_gy in reg FIFO_CTRL3
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_gy_batch_set(const stmdev_ctx_t *ctx,
								lsm6dsox_bdr_gy_t val)
{
	lsm6dsox_fifo_ctrl3_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_CTRL3, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.bdr_gy = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL3, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Gyroscope UI data rate selection.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of odr_g in reg CTRL2_G
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_gy_data_rate_set(const stmdev_ctx_t *ctx,
								lsm6dsox_odr_g_t val)
{
	lsm6dsox_odr_g_t odr_gy =  val;
	lsm6dsox_emb_fsm_enable_t fsm_enable;
	lsm6dsox_fsm_odr_t fsm_odr;
	lsm6dsox_emb_sens_t emb_sens;
	lsm6dsox_mlc_odr_t mlc_odr;
	lsm6dsox_ctrl2_g_t reg;
	int ret;

  /* Check the Finite State Machine data rate constraints */
	ret =  lsm6dsox_fsm_enable_get(ctx, &fsm_enable);

	if (ret == 0) {
		if ((fsm_enable.fsm_enable_a.fsm1_en  |
			fsm_enable.fsm_enable_a.fsm2_en  |
			fsm_enable.fsm_enable_a.fsm3_en  |
			fsm_enable.fsm_enable_a.fsm4_en  |
			fsm_enable.fsm_enable_a.fsm5_en  |
			fsm_enable.fsm_enable_a.fsm6_en  |
			fsm_enable.fsm_enable_a.fsm7_en  |
			fsm_enable.fsm_enable_a.fsm8_en  |
			fsm_enable.fsm_enable_b.fsm9_en  |
			fsm_enable.fsm_enable_b.fsm10_en |
			fsm_enable.fsm_enable_b.fsm11_en |
			fsm_enable.fsm_enable_b.fsm12_en |
			fsm_enable.fsm_enable_b.fsm13_en |
			fsm_enable.fsm_enable_b.fsm14_en |
			fsm_enable.fsm_enable_b.fsm15_en |
			fsm_enable.fsm_enable_b.fsm16_en) == PROPERTY_ENABLE) {
			ret =  lsm6dsox_fsm_data_rate_get(ctx, &fsm_odr);

			if (ret == 0) {
				switch (fsm_odr) {
				case LSM6DSOX_ODR_FSM_12Hz5:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_12Hz5;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_FSM_26Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_26Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_26Hz;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_FSM_52Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else if (val == LSM6DSOX_GY_ODR_26Hz)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_FSM_104Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_26Hz)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_52Hz)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else
						odr_gy = val;

					break;

				default:
					odr_gy = val;
					break;
				}
			}
		}
	}

  /* Check the Machine Learning Core data rate constraints */
	emb_sens.mlc = PROPERTY_DISABLE;

	if (ret == 0) {
		ret =  lsm6dsox_embedded_sens_get(ctx, &emb_sens);

		if (emb_sens.mlc == PROPERTY_ENABLE) {
			ret =  lsm6dsox_mlc_data_rate_get(ctx, &mlc_odr);

			if (ret == 0) {
				switch (mlc_odr) {
				case LSM6DSOX_ODR_PRGS_12Hz5:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_12Hz5;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_PRGS_26Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_26Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_26Hz;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_PRGS_52Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else if (val == LSM6DSOX_GY_ODR_26Hz)
						odr_gy = LSM6DSOX_GY_ODR_52Hz;
					else
						odr_gy = val;

					break;

				case LSM6DSOX_ODR_PRGS_104Hz:
					if (val == LSM6DSOX_GY_ODR_OFF)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_12Hz5)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_26Hz)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else if (val == LSM6DSOX_GY_ODR_52Hz)
						odr_gy = LSM6DSOX_GY_ODR_104Hz;
					else
						odr_gy = val;

					break;

				default:
					odr_gy = val;
					break;
				}
			}
		}
	}

	if (ret == 0)
		ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL2_G, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.odr_g = (u8) odr_gy;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL2_G, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Selects decimation for timestamp batching in FIFO.
 *         Writing rate will be the maximum rate between XL and
 *         GYRO BDR divided by decimation decoder.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of odr_ts_batch in reg FIFO_CTRL4
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_timestamp_decimation_set(const stmdev_ctx_t *ctx,
											lsm6dsox_odr_ts_batch_t val)
{
	lsm6dsox_fifo_ctrl4_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_CTRL4, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.odr_ts_batch = (u8)val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_FIFO_CTRL4, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  Enables timestamp counter.[set]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of timestamp_en in reg CTRL10_C
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_timestamp_set(const stmdev_ctx_t *ctx, u8 val)
{
	lsm6dsox_ctrl10_c_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_CTRL10_C, (u8 *)&reg, 1);

	if (ret == 0) {
		reg.timestamp_en = val;
		ret = lsm6dsox_write_reg(ctx, LSM6DSOX_CTRL10_C, (u8 *)&reg, 1);
	}

	return ret;
}

/*
 * @brief  FIFO watermark status.[get]
 *
 * @param  ctx    Read / write interface definitions.(ptr)
 * @param  val    Read the values of fifo_wtm_ia in reg FIFO_STATUS2
 * @retval        Interface status (MANDATORY: return 0 -> no Error).
 *
 */
int lsm6dsox_fifo_wtm_flag_get(const stmdev_ctx_t *ctx, u8 *val)
{
	u8 reg[2];
	lsm6dsox_fifo_status2_t *fifo_status2 = (lsm6dsox_fifo_status2_t *)&reg[1];
	int ret;

	/* read both FIFO_STATUS1 + FIFO_STATUS2 regs */
	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_STATUS1, (u8 *)reg, 2);
	if (ret == 0) {
		*val = fifo_status2->fifo_wtm_ia;
	}

	return ret;
}

/*
 * @brief  Number of unread sensor data (TAG + 6 bytes) stored in FIFO.[get]
 *
 * @param  ctx    Read / write interface definitions.(ptr)
 * @param  val    Read the value of diff_fifo in reg FIFO_STATUS1 and FIFO_STATUS2
 * @retval        Interface status (MANDATORY: return 0 -> no Error).
 *
 */
int lsm6dsox_fifo_data_level_get(const stmdev_ctx_t *ctx,
								u16 *val)
{
	u8 reg[2];
	lsm6dsox_fifo_status1_t *fifo_status1 = (lsm6dsox_fifo_status1_t *)&reg[0];
	lsm6dsox_fifo_status2_t *fifo_status2 = (lsm6dsox_fifo_status2_t *)&reg[1];
	int ret;

	/* read both FIFO_STATUS1 + FIFO_STATUS2 regs */
	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_STATUS1, (u8 *)reg, 2);
	if (ret == 0) {
		*val = fifo_status2->diff_fifo;
		*val = (*val * 256U) + fifo_status1->diff_fifo;
	}

	return ret;
}

/*
 * @brief  Identifies the sensor in FIFO_DATA_OUT.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of tag_sensor in reg FIFO_DATA_OUT_TAG
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_sensor_tag_get(const stmdev_ctx_t *ctx,
								lsm6dsox_fifo_tag_t *val)
{
	lsm6dsox_fifo_data_out_tag_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_DATA_OUT_TAG,
							(u8 *)&reg, 1);

	switch (reg.tag_sensor) {
	case LSM6DSOX_GYRO_NC_TAG:
		*val = LSM6DSOX_GYRO_NC_TAG;
		break;

	case LSM6DSOX_XL_NC_TAG:
		*val = LSM6DSOX_XL_NC_TAG;
		break;

	case LSM6DSOX_TEMPERATURE_TAG:
		*val = LSM6DSOX_TEMPERATURE_TAG;
		break;

	case LSM6DSOX_TIMESTAMP_TAG:
		*val = LSM6DSOX_TIMESTAMP_TAG;
		break;

	case LSM6DSOX_CFG_CHANGE_TAG:
		*val = LSM6DSOX_CFG_CHANGE_TAG;
		break;

	case LSM6DSOX_XL_NC_T_2_TAG:
		*val = LSM6DSOX_XL_NC_T_2_TAG;
		break;

	case LSM6DSOX_XL_NC_T_1_TAG:
		*val = LSM6DSOX_XL_NC_T_1_TAG;
		break;

	case LSM6DSOX_XL_2XC_TAG:
		*val = LSM6DSOX_XL_2XC_TAG;
		break;

	case LSM6DSOX_XL_3XC_TAG:
		*val = LSM6DSOX_XL_3XC_TAG;
		break;

	case LSM6DSOX_GYRO_NC_T_2_TAG:
		*val = LSM6DSOX_GYRO_NC_T_2_TAG;
		break;

	case LSM6DSOX_GYRO_NC_T_1_TAG:
		*val = LSM6DSOX_GYRO_NC_T_1_TAG;
		break;

	case LSM6DSOX_GYRO_2XC_TAG:
		*val = LSM6DSOX_GYRO_2XC_TAG;
		break;

	case LSM6DSOX_GYRO_3XC_TAG:
		*val = LSM6DSOX_GYRO_3XC_TAG;
		break;

	case LSM6DSOX_SENSORHUB_SLAVE0_TAG:
		*val = LSM6DSOX_SENSORHUB_SLAVE0_TAG;
		break;

	case LSM6DSOX_SENSORHUB_SLAVE1_TAG:
		*val = LSM6DSOX_SENSORHUB_SLAVE1_TAG;
		break;

	case LSM6DSOX_SENSORHUB_SLAVE2_TAG:
		*val = LSM6DSOX_SENSORHUB_SLAVE2_TAG;
		break;

	case LSM6DSOX_SENSORHUB_SLAVE3_TAG:
		*val = LSM6DSOX_SENSORHUB_SLAVE3_TAG;
		break;

	case LSM6DSOX_STEP_CPUNTER_TAG:
		*val = LSM6DSOX_STEP_CPUNTER_TAG;
		break;

	case LSM6DSOX_GAME_ROTATION_TAG:
		*val = LSM6DSOX_GAME_ROTATION_TAG;
		break;

	case LSM6DSOX_GEOMAG_ROTATION_TAG:
		*val = LSM6DSOX_GEOMAG_ROTATION_TAG;
		break;

	case LSM6DSOX_ROTATION_TAG:
		*val = LSM6DSOX_ROTATION_TAG;
		break;

	case LSM6DSOX_SENSORHUB_NACK_TAG:
		*val = LSM6DSOX_SENSORHUB_NACK_TAG;
		break;

	default:
		*val = LSM6DSOX_GYRO_NC_TAG;
		break;
	}

	return ret;
}

/*
 * @brief  FIFO data output [get]
 *
 * @param  ctx      read / write interface definitions
 * @param  buff     buffer that stores data read
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_fifo_out_raw_get(const stmdev_ctx_t *ctx, u8 *buff)
{
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_FIFO_DATA_OUT_X_L, buff, 6);

	return ret;
}

/*
 * @brief  Accelerometer new data available.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of xlda in reg STATUS_REG
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_xl_flag_data_ready_get(const stmdev_ctx_t *ctx,
									u8 *val)
{
	lsm6dsox_status_reg_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_STATUS_REG, (u8 *)&reg, 1);
	*val = reg.xlda;

	return ret;
}

/*
 * @brief  Gyroscope new data available.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  val      change the values of gda in reg STATUS_REG
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_gy_flag_data_ready_get(const stmdev_ctx_t *ctx,
									u8 *val)
{
	lsm6dsox_status_reg_t reg;
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_STATUS_REG, (u8 *)&reg, 1);
	*val = reg.gda;

	return ret;
}

/*
 * @brief  Linear acceleration output register.
 *         The value is expressed as a 16-bit word in two’s complement.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  buff     buffer that stores data read
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_acceleration_raw_get(const stmdev_ctx_t *ctx, s16 *val)
{
	u8 buff[6];
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_OUTX_L_A, buff, 6);
	val[0] = (s16)buff[1];
	val[0] = (val[0] * 256) + (s16)buff[0];
	val[1] = (s16)buff[3];
	val[1] = (val[1] * 256) + (s16)buff[2];
	val[2] = (s16)buff[5];
	val[2] = (val[2] * 256) + (s16)buff[4];

	return ret;
}

/*
 * @brief  Angular rate sensor. The value is expressed as a 16-bit
 *         word in two’s complement.[get]
 *
 * @param  ctx      read / write interface definitions
 * @param  buff     buffer that stores data read
 * @retval             interface status (MANDATORY: return 0 -> no Error)
 *
 */
int lsm6dsox_angular_rate_raw_get(const stmdev_ctx_t *ctx, s16 *val)
{
	u8 buff[6];
	int ret;

	ret = lsm6dsox_read_reg(ctx, LSM6DSOX_OUTX_L_G, buff, 6);
	val[0] = (s16)buff[1];
	val[0] = (val[0] * 256) + (s16)buff[0];
	val[1] = (s16)buff[3];
	val[1] = (val[1] * 256) + (s16)buff[2];
	val[2] = (s16)buff[5];
	val[2] = (val[2] * 256) + (s16)buff[4];

	return ret;
}
