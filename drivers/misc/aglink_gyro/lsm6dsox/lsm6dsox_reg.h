/**
  ******************************************************************************
  * @file    lsm6dsox_reg.h
  * @author  Sensors Software Solution Team
  * @brief   This file contains all the functions prototypes for the
  *          lsm6dsox_reg.c driver.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LSM6DSOX_REGS_H
#define LSM6DSOX_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <linux/types.h>
#include <stddef.h>
#include <linux/math64.h>
#define float_t float

#define PROPERTY_DISABLE                (0U)
#define PROPERTY_ENABLE                 (1U)

#ifndef DRV_BYTE_ORDER
#ifndef __BYTE_ORDER__

#define DRV_LITTLE_ENDIAN 1234
#define DRV_BIG_ENDIAN    4321

/** if _BYTE_ORDER is not defined, choose the endianness of your architecture
  * by uncommenting the define which fits your platform endianness
  */
//#define DRV_BYTE_ORDER    DRV_BIG_ENDIAN
#define DRV_BYTE_ORDER    DRV_LITTLE_ENDIAN

#else /* defined __BYTE_ORDER__ */

#define DRV_LITTLE_ENDIAN  __ORDER_LITTLE_ENDIAN__
#define DRV_BIG_ENDIAN     __ORDER_BIG_ENDIAN__
#define DRV_BYTE_ORDER     __BYTE_ORDER__

#endif /* __BYTE_ORDER__*/
#endif /* DRV_BYTE_ORDER */

typedef int (*stmdev_write_ptr)(void *, u8, const u8 *, u16);
typedef int (*stmdev_read_ptr)(void *, u8, u8 *, u16);
typedef void (*stmdev_mdelay_ptr)(u32 millisec);

typedef struct {
	/** Component mandatory fields **/
	stmdev_write_ptr  write_reg;
	stmdev_read_ptr   read_reg;
	/** Component optional fields **/
	stmdev_mdelay_ptr   mdelay;
	/** Customizable optional pointer **/
	void *handle;
} stmdev_ctx_t;

/** I2C Device Address 8 bit format  if SA0=0 -> D5 if SA0=1 -> D7 **/
#define LSM6DSOX_I2C_ADD_L                    0xD5U
#define LSM6DSOX_I2C_ADD_H                    0xD7U

/** Device Identification (Who am I) **/
#define LSM6DSOX_ID                           0x6CU

#define LSM6DSOX_FUNC_CFG_ACCESS              0x01U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 ois_ctrl_from_ui         : 1;
	u8 not_used_01              : 5;
	u8 reg_access               : 2; /* shub_reg_access + func_cfg_access */
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 reg_access               : 2; /* shub_reg_access + func_cfg_access */
	u8 not_used_01              : 5;
	u8 ois_ctrl_from_ui         : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_func_cfg_access_t;

#define LSM6DSOX_EMB_FUNC_EN_A                0x04U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 3;
	u8 pedo_en                  : 1;
	u8 tilt_en                  : 1;
	u8 sign_motion_en           : 1;
	u8 not_used_02              : 2;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02              : 2;
	u8 sign_motion_en           : 1;
	u8 tilt_en                  : 1;
	u8 pedo_en                  : 1;
	u8 not_used_01              : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_en_a_t;

#define LSM6DSOX_EMB_FUNC_EN_B                0x05U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 fsm_en                   : 1;
	u8 not_used_01              : 2;
	u8 fifo_compr_en            : 1;
	u8 mlc_en                   : 1;
	u8 not_used_02              : 3;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02              : 3;
	u8 mlc_en                   : 1;
	u8 fifo_compr_en            : 1;
	u8 not_used_01              : 2;
	u8 fsm_en                   : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_en_b_t;

#define LSM6DSOX_FIFO_CTRL1                   0x07U
typedef struct {
	u8 wtm                      : 8;
} lsm6dsox_fifo_ctrl1_t;

#define LSM6DSOX_FIFO_CTRL2                   0x08U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 wtm                      : 1;
	u8 uncoptr_rate             : 2;
	u8 not_used_01              : 1;
	u8 odrchg_en                : 1;
	u8 not_used_02              : 1;
	u8 fifo_compr_rt_en         : 1;
	u8 stop_on_wtm              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 stop_on_wtm              : 1;
	u8 fifo_compr_rt_en         : 1;
	u8 not_used_02              : 1;
	u8 odrchg_en                : 1;
	u8 not_used_01              : 1;
	u8 uncoptr_rate             : 2;
	u8 wtm                      : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fifo_ctrl2_t;

#define LSM6DSOX_FIFO_CTRL3                   0x09U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 bdr_xl                   : 4;
	u8 bdr_gy                   : 4;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 bdr_gy                   : 4;
	u8 bdr_xl                   : 4;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fifo_ctrl3_t;

#define LSM6DSOX_FIFO_CTRL4                   0x0AU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 fifo_mode                : 3;
	u8 not_used_01              : 1;
	u8 odr_t_batch              : 2;
	u8 odr_ts_batch             : 2;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 odr_ts_batch             : 2;
	u8 odr_t_batch              : 2;
	u8 not_used_01              : 1;
	u8 fifo_mode                : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fifo_ctrl4_t;

#define LSM6DSOX_EMB_FUNC_INT1                0x0AU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 3;
	u8 int1_step_detector       : 1;
	u8 int1_tilt                : 1;
	u8 int1_sig_mot             : 1;
	u8 not_used_02              : 1;
	u8 int1_fsm_lc              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int1_fsm_lc              : 1;
	u8 not_used_02              : 1;
	u8 int1_sig_mot             : 1;
	u8 int1_tilt                : 1;
	u8 int1_step_detector       : 1;
	u8 not_used_01              : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_int1_t;

#define LSM6DSOX_COUNTER_BDR_REG1             0x0BU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 cnt_bdr_th               : 3;
	u8 not_used_01              : 2;
	u8 trig_counter_bdr         : 1;
	u8 rst_counter_bdr          : 1;
	u8 dataready_pulsed         : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 dataready_pulsed         : 1;
	u8 rst_counter_bdr          : 1;
	u8 trig_counter_bdr         : 1;
	u8 not_used_01              : 2;
	u8 cnt_bdr_th               : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_counter_bdr_reg1_t;

#define LSM6DSOX_FSM_INT1_A                   0x0BU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int1_fsm1                : 1;
	u8 int1_fsm2                : 1;
	u8 int1_fsm3                : 1;
	u8 int1_fsm4                : 1;
	u8 int1_fsm5                : 1;
	u8 int1_fsm6                : 1;
	u8 int1_fsm7                : 1;
	u8 int1_fsm8                : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int1_fsm8                : 1;
	u8 int1_fsm7                : 1;
	u8 int1_fsm6                : 1;
	u8 int1_fsm5                : 1;
	u8 int1_fsm4                : 1;
	u8 int1_fsm3                : 1;
	u8 int1_fsm2                : 1;
	u8 int1_fsm1                : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_int1_a_t;

#define LSM6DSOX_FSM_INT1_B                   0x0CU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int1_fsm16               : 1;
	u8 int1_fsm15               : 1;
	u8 int1_fsm14               : 1;
	u8 int1_fsm13               : 1;
	u8 int1_fsm12               : 1;
	u8 int1_fsm11               : 1;
	u8 int1_fsm10               : 1;
	u8 int1_fsm9                : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_int1_b_t;

#define LSM6DSOX_MLC_INT1                     0x0DU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int1_mlc1                : 1;
	u8 int1_mlc2                : 1;
	u8 int1_mlc3                : 1;
	u8 int1_mlc4                : 1;
	u8 int1_mlc5                : 1;
	u8 int1_mlc6                : 1;
	u8 int1_mlc7                : 1;
	u8 int1_mlc8                : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int1_mlc8                : 1;
	u8 int1_mlc7                : 1;
	u8 int1_mlc6                : 1;
	u8 int1_mlc5                : 1;
	u8 int1_mlc4                : 1;
	u8 int1_mlc3                : 1;
	u8 int1_mlc2                : 1;
	u8 int1_mlc1                : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_mlc_int1_t;

#define LSM6DSOX_INT2_CTRL                    0x0EU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int2_drdy_xl             : 1;
	u8 int2_drdy_g              : 1;
	u8 int2_drdy_temp           : 1;
	u8 int2_fifo_th             : 1;
	u8 int2_fifo_ovr            : 1;
	u8 int2_fifo_full           : 1;
	u8 int2_cnt_bdr             : 1;
	u8 not_used_01              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_01              : 1;
	u8 int2_cnt_bdr             : 1;
	u8 int2_fifo_full           : 1;
	u8 int2_fifo_ovr            : 1;
	u8 int2_fifo_th             : 1;
	u8 int2_drdy_temp           : 1;
	u8 int2_drdy_g              : 1;
	u8 int2_drdy_xl             : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_int2_ctrl_t;

#define LSM6DSOX_INT1_CTRL                    0x0D
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int1_drdy_xl             : 1;
	u8 int1_drdy_g              : 1;
	u8 int1_boot                : 1;
	u8 int1_fifo_th             : 1;
	u8 int1_fifo_ovr            : 1;
	u8 int1_fifo_full           : 1;
	u8 int1_cnt_bdr             : 1;
	u8 den_drdy_flag            : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 den_drdy_flag            : 1;
	u8 int1_cnt_bdr             : 1;
	u8 int1_fifo_full           : 1;
	u8 int1_fifo_ovr            : 1;
	u8 int1_fifo_th             : 1;
	u8 int1_boot                : 1;
	u8 int1_drdy_g              : 1;
	u8 int1_drdy_xl             : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_int1_ctrl_t;

#define LSM6DSOX_EMB_FUNC_INT2                0x0EU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 3;
	u8 int2_step_detector       : 1;
	u8 int2_tilt                : 1;
	u8 int2_sig_mot             : 1;
	u8 not_used_02              : 1;
	u8 int2_fsm_lc              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_fsm_lc              : 1;
	u8 not_used_02              : 1;
	u8 int2_sig_mot             : 1;
	u8 int2_tilt                : 1;
	u8 int2_step_detector       : 1;
	u8 not_used_01              : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_int2_t;

#define LSM6DSOX_FSM_INT2_A                   0x0FU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int2_fsm1                : 1;
	u8 int2_fsm2                : 1;
	u8 int2_fsm3                : 1;
	u8 int2_fsm4                : 1;
	u8 int2_fsm5                : 1;
	u8 int2_fsm6                : 1;
	u8 int2_fsm7                : 1;
	u8 int2_fsm8                : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_fsm8                : 1;
	u8 int2_fsm7                : 1;
	u8 int2_fsm6                : 1;
	u8 int2_fsm5                : 1;
	u8 int2_fsm4                : 1;
	u8 int2_fsm3                : 1;
	u8 int2_fsm2                : 1;
	u8 int2_fsm1                : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_int2_a_t;

#define LSM6DSOX_WHO_AM_I                     0x0FU

#define LSM6DSOX_CTRL1_XL                     0x10U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 1;
	u8 lpf2_xl_en               : 1;
	u8 fs_xl                    : 2;
	u8 odr_xl                   : 4;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 odr_xl                   : 4;
	u8 fs_xl                    : 2;
	u8 lpf2_xl_en               : 1;
	u8 not_used_01              : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl1_xl_t;

#define LSM6DSOX_FSM_INT2_B                   0x10U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int2_fsm9                : 1;
	u8 int2_fsm10               : 1;
	u8 int2_fsm11               : 1;
	u8 int2_fsm12               : 1;
	u8 int2_fsm13               : 1;
	u8 int2_fsm14               : 1;
	u8 int2_fsm15               : 1;
	u8 int2_fsm16               : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_fsm16               : 1;
	u8 int2_fsm15               : 1;
	u8 int2_fsm14               : 1;
	u8 int2_fsm13               : 1;
	u8 int2_fsm12               : 1;
	u8 int2_fsm11               : 1;
	u8 int2_fsm10               : 1;
	u8 int2_fsm9                : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_int2_b_t;

#define LSM6DSOX_CTRL2_G                      0x11U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 1;
	u8 fs_g                     : 3; /* fs_125 + fs_g */
	u8 odr_g                    : 4;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 odr_g                    : 4;
	u8 fs_g                     : 3; /* fs_125 + fs_g */
	u8 not_used_01              : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl2_g_t;

#define LSM6DSOX_MLC_INT2                     0x11U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int2_mlc1             : 1;
	u8 int2_mlc2             : 1;
	u8 int2_mlc3             : 1;
	u8 int2_mlc4             : 1;
	u8 int2_mlc5             : 1;
	u8 int2_mlc6             : 1;
	u8 int2_mlc7             : 1;
	u8 int2_mlc8             : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_mlc8             : 1;
	u8 int2_mlc7             : 1;
	u8 int2_mlc6             : 1;
	u8 int2_mlc5             : 1;
	u8 int2_mlc4             : 1;
	u8 int2_mlc3             : 1;
	u8 int2_mlc2             : 1;
	u8 int2_mlc1             : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_mlc_int2_t;

#define LSM6DSOX_CTRL3_C                      0x12U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 sw_reset                 : 1;
	u8 not_used_01              : 1;
	u8 if_inc                   : 1;
	u8 sim                      : 1;
	u8 pp_od                    : 1;
	u8 h_lactive                : 1;
	u8 bdu                      : 1;
	u8 boot                     : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 boot                     : 1;
	u8 bdu                      : 1;
	u8 h_lactive                : 1;
	u8 pp_od                    : 1;
	u8 sim                      : 1;
	u8 if_inc                   : 1;
	u8 not_used_01              : 1;
	u8 sw_reset                 : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl3_c_t;

#define LSM6DSOX_CTRL4_C                      0x13U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 1;
	u8 lpf1_sel_g               : 1;
	u8 i2c_disable              : 1;
	u8 drdy_mask                : 1;
	u8 not_used_02              : 1;
	u8 int2_on_int1             : 1;
	u8 sleep_g                  : 1;
	u8 not_used_03              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_03              : 1;
	u8 sleep_g                  : 1;
	u8 int2_on_int1             : 1;
	u8 not_used_02              : 1;
	u8 drdy_mask                : 1;
	u8 i2c_disable              : 1;
	u8 lpf1_sel_g               : 1;
	u8 not_used_01              : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl4_c_t;

#define LSM6DSOX_CTRL9_XL                     0x18U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 1;
	u8 i3c_disable              : 1;
	u8 den_lh                   : 1;
	u8 den_xl_g                 : 2;   /* den_xl_en + den_xl_g */
	u8 den_z                    : 1;
	u8 den_y                    : 1;
	u8 den_x                    : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 den_x                    : 1;
	u8 den_y                    : 1;
	u8 den_z                    : 1;
	u8 den_xl_g                 : 2;   /* den_xl_en + den_xl_g */
	u8 den_lh                   : 1;
	u8 i3c_disable              : 1;
	u8 not_used_01              : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl9_xl_t;

#define LSM6DSOX_CTRL10_C                     0x19U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 5;
	u8 timestamp_en             : 1;
	u8 not_used_02              : 2;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02              : 2;
	u8 timestamp_en             : 1;
	u8 not_used_01              : 5;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_ctrl10_c_t;

#define LSM6DSOX_STATUS_REG                   0x1EU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 xlda                     : 1;
	u8 gda                      : 1;
	u8 tda                      : 1;
	u8 not_used_01              : 5;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_01              : 5;
	u8 tda                      : 1;
	u8 gda                      : 1;
	u8 xlda                     : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_status_reg_t;

#define LSM6DSOX_OUT_TEMP_L                   0x20U
#define LSM6DSOX_OUT_TEMP_H                   0x21U
#define LSM6DSOX_OUTX_L_G                     0x22U
#define LSM6DSOX_OUTX_H_G                     0x23U
#define LSM6DSOX_OUTY_L_G                     0x24U
#define LSM6DSOX_OUTY_H_G                     0x25U
#define LSM6DSOX_OUTZ_L_G                     0x26U
#define LSM6DSOX_OUTZ_H_G                     0x27U
#define LSM6DSOX_OUTX_L_A                     0x28U
#define LSM6DSOX_OUTX_H_A                     0x29U
#define LSM6DSOX_OUTY_L_A                     0x2AU
#define LSM6DSOX_OUTY_H_A                     0x2BU
#define LSM6DSOX_OUTZ_L_A                     0x2CU
#define LSM6DSOX_OUTZ_H_A                     0x2DU

#define LSM6DSOX_FIFO_STATUS1                 0x3AU
typedef struct {
	u8 diff_fifo                : 8;
} lsm6dsox_fifo_status1_t;

#define LSM6DSOX_FIFO_STATUS2                 0x3B
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 diff_fifo                : 2;
	u8 not_used_01              : 1;
	u8 over_run_latched         : 1;
	u8 counter_bdr_ia           : 1;
	u8 fifo_full_ia             : 1;
	u8 fifo_ovr_ia              : 1;
	u8 fifo_wtm_ia              : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 fifo_wtm_ia              : 1;
	u8 fifo_ovr_ia              : 1;
	u8 fifo_full_ia             : 1;
	u8 counter_bdr_ia           : 1;
	u8 over_run_latched         : 1;
	u8 not_used_01              : 1;
	u8 diff_fifo                : 2;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fifo_status2_t;

#define LSM6DSOX_FSM_ENABLE_A                 0x46U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 fsm1_en                  : 1;
	u8 fsm2_en                  : 1;
	u8 fsm3_en                  : 1;
	u8 fsm4_en                  : 1;
	u8 fsm5_en                  : 1;
	u8 fsm6_en                  : 1;
	u8 fsm7_en                  : 1;
	u8 fsm8_en                  : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 fsm8_en                  : 1;
	u8 fsm7_en                  : 1;
	u8 fsm6_en                  : 1;
	u8 fsm5_en                  : 1;
	u8 fsm4_en                  : 1;
	u8 fsm3_en                  : 1;
	u8 fsm2_en                  : 1;
	u8 fsm1_en                  : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_enable_a_t;

#define LSM6DSOX_FSM_ENABLE_B                 0x47U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 fsm9_en                  : 1;
	u8 fsm10_en                 : 1;
	u8 fsm11_en                 : 1;
	u8 fsm12_en                 : 1;
	u8 fsm13_en                 : 1;
	u8 fsm14_en                 : 1;
	u8 fsm15_en                 : 1;
	u8 fsm16_en                 : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 fsm16_en                 : 1;
	u8 fsm15_en                 : 1;
	u8 fsm14_en                 : 1;
	u8 fsm13_en                 : 1;
	u8 fsm12_en                 : 1;
	u8 fsm11_en                 : 1;
	u8 fsm10_en                 : 1;
	u8 fsm9_en                  : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fsm_enable_b_t;

#define LSM6DSOX_TAP_CFG2                     0x58U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 tap_ths_y                : 5;
	u8 inact_en                 : 2;
	u8 interrupts_enable        : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 interrupts_enable        : 1;
	u8 inact_en                 : 2;
	u8 tap_ths_y                : 5;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_tap_cfg2_t;

#define LSM6DSOX_MD1_CFG                      0x5EU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int1_shub                : 1;
	u8 int1_emb_func            : 1;
	u8 int1_6d                  : 1;
	u8 int1_double_tap          : 1;
	u8 int1_ff                  : 1;
	u8 int1_wu                  : 1;
	u8 int1_single_tap          : 1;
	u8 int1_sleep_change        : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int1_sleep_change        : 1;
	u8 int1_single_tap          : 1;
	u8 int1_wu                  : 1;
	u8 int1_ff                  : 1;
	u8 int1_double_tap          : 1;
	u8 int1_6d                  : 1;
	u8 int1_emb_func            : 1;
	u8 int1_shub                : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_md1_cfg_t;

#define LSM6DSOX_MD2_CFG                      0x5FU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 int2_timestamp           : 1;
	u8 int2_emb_func            : 1;
	u8 int2_6d                  : 1;
	u8 int2_double_tap          : 1;
	u8 int2_ff                  : 1;
	u8 int2_wu                  : 1;
	u8 int2_single_tap          : 1;
	u8 int2_sleep_change        : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_sleep_change        : 1;
	u8 int2_single_tap          : 1;
	u8 int2_wu                  : 1;
	u8 int2_ff                  : 1;
	u8 int2_double_tap          : 1;
	u8 int2_6d                  : 1;
	u8 int2_emb_func            : 1;
	u8 int2_timestamp           : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_md2_cfg_t;

#define LSM6DSOX_EMB_FUNC_ODR_CFG_B           0x5FU
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01              : 3;
	u8 fsm_odr                  : 2;
	u8 not_used_02              : 3;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02              : 3;
	u8 fsm_odr                  : 2;
	u8 not_used_01              : 3;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_odr_cfg_b_t;

#define LSM6DSOX_EMB_FUNC_ODR_CFG_C           0x60U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 not_used_01             : 4;
	u8 mlc_odr                 : 2;
	u8 not_used_02             : 2;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02             : 2;
	u8 mlc_odr                 : 2;
	u8 not_used_01             : 4;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_emb_func_odr_cfg_c_t;

#define LSM6DSOX_I3C_BUS_AVB                  0x62U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 pd_dis_int1              : 1;
	u8 not_used_01              : 2;
	u8 i3c_bus_avb_sel          : 2;
	u8 not_used_02              : 3;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 not_used_02              : 3;
	u8 i3c_bus_avb_sel          : 2;
	u8 not_used_01              : 2;
	u8 pd_dis_int1              : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_i3c_bus_avb_t;

#define LSM6DSOX_SPI2_INT_OIS                 0x6F
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 st_xl_ois                : 2;
	u8 not_used_01              : 3;
	u8 den_lh_ois               : 1;
	u8 lvl2_ois                 : 1;
	u8 int2_drdy_ois            : 1;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 int2_drdy_ois            : 1;
	u8 lvl2_ois                 : 1;
	u8 den_lh_ois               : 1;
	u8 not_used_01              : 3;
	u8 st_xl_ois                : 2;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_spi2_int_ois_t;

#define LSM6DSOX_FIFO_DATA_OUT_TAG            0x78U
typedef struct {
#if DRV_BYTE_ORDER == DRV_LITTLE_ENDIAN
	u8 tag_parity               : 1;
	u8 tag_cnt                  : 2;
	u8 tag_sensor               : 5;
#elif DRV_BYTE_ORDER == DRV_BIG_ENDIAN
	u8 tag_sensor               : 5;
	u8 tag_cnt                  : 2;
	u8 tag_parity               : 1;
#endif /* DRV_BYTE_ORDER */
} lsm6dsox_fifo_data_out_tag_t;

#define LSM6DSOX_FIFO_DATA_OUT_X_L            0x79
#define LSM6DSOX_FIFO_DATA_OUT_X_H            0x7A
#define LSM6DSOX_FIFO_DATA_OUT_Y_L            0x7B
#define LSM6DSOX_FIFO_DATA_OUT_Y_H            0x7C
#define LSM6DSOX_FIFO_DATA_OUT_Z_L            0x7D
#define LSM6DSOX_FIFO_DATA_OUT_Z_H            0x7E

#ifndef __weak
#define __weak __attribute__((weak))
#endif /* __weak */

int lsm6dsox_read_reg(const stmdev_ctx_t *ctx, u8 reg,
						u8 *data,
						u16 len);
int lsm6dsox_write_reg(const stmdev_ctx_t *ctx, u8 reg,
						u8 *data,
						u16 len);

float_t lsm6dsox_from_fs2_to_mg(s16 lsb);
float_t lsm6dsox_from_fs4_to_mg(s16 lsb);
float_t lsm6dsox_from_fs8_to_mg(s16 lsb);
float_t lsm6dsox_from_fs16_to_mg(s16 lsb);

float_t lsm6dsox_from_fs125_to_mdps(s16 lsb);
float_t lsm6dsox_from_fs500_to_mdps(s16 lsb);
float_t lsm6dsox_from_fs250_to_mdps(s16 lsb);
float_t lsm6dsox_from_fs1000_to_mdps(s16 lsb);
float_t lsm6dsox_from_fs2000_to_mdps(s16 lsb);

s64 lsm6dsox_from_fs2000_to_uradps(s16 lsb);

int lsm6dsox_fifo_out_raw_get(const stmdev_ctx_t *ctx, u8 *buff);

int lsm6dsox_xl_flag_data_ready_get(const stmdev_ctx_t *ctx,
									u8 *val);

int lsm6dsox_gy_flag_data_ready_get(const stmdev_ctx_t *ctx,
									u8 *val);

int lsm6dsox_angular_rate_raw_get(const stmdev_ctx_t *ctx,
									s16 *val);

int lsm6dsox_acceleration_raw_get(const stmdev_ctx_t *ctx,
									s16 *val);

int lsm6dsox_fifo_data_level_get(const stmdev_ctx_t *ctx,
									u16 *val);

int lsm6dsox_fifo_wtm_flag_get(const stmdev_ctx_t *ctx, u8 *val);

int lsm6dsox_timestamp_set(const stmdev_ctx_t *ctx, u8 val);

int lsm6dsox_fifo_watermark_set(const stmdev_ctx_t *ctx, u16 val);

int lsm6dsox_device_id_get(const stmdev_ctx_t *ctx, u8 *buff);
int lsm6dsox_reset_set(const stmdev_ctx_t *ctx, u8 val);
int lsm6dsox_reset_get(const stmdev_ctx_t *ctx, u8 *val);

typedef enum {
	LSM6DSOX_I3C_DISABLE         = 0x80,
	LSM6DSOX_I3C_ENABLE_T_50us   = 0x00,
	LSM6DSOX_I3C_ENABLE_T_2us    = 0x01,
	LSM6DSOX_I3C_ENABLE_T_1ms    = 0x02,
	LSM6DSOX_I3C_ENABLE_T_25ms   = 0x03,
} lsm6dsox_i3c_disable_t;
int lsm6dsox_i3c_disable_set(const stmdev_ctx_t *ctx,
							lsm6dsox_i3c_disable_t val);
//int lsm6dsox_i3c_disable_get(const stmdev_ctx_t *ctx,
//                                 lsm6dsox_i3c_disable_t *val);

typedef enum {
	LSM6DSOX_DRDY_LATCHED = 0,
	LSM6DSOX_DRDY_PULSED  = 1,
} lsm6dsox_dataready_pulsed_t;
int lsm6dsox_data_ready_mode_set(const stmdev_ctx_t *ctx,
								lsm6dsox_dataready_pulsed_t val);
//int lsm6dsox_data_ready_mode_get(const stmdev_ctx_t *ctx,
//                                     lsm6dsox_dataready_pulsed_t *val);

typedef enum {
	LSM6DSOX_ACTIVE_HIGH = 0,
	LSM6DSOX_ACTIVE_LOW  = 1,
} lsm6dsox_h_lactive_t;
int lsm6dsox_pin_polarity_set(const stmdev_ctx_t *ctx,
								lsm6dsox_h_lactive_t val);
//int lsm6dsox_pin_polarity_get(const stmdev_ctx_t *ctx,
//                                  lsm6dsox_h_lactive_t *val);

typedef enum {
	LSM6DSOX_BYPASS_MODE             = 0,
	LSM6DSOX_FIFO_MODE               = 1,
	LSM6DSOX_STREAM_TO_FIFO_MODE     = 3,
	LSM6DSOX_BYPASS_TO_STREAM_MODE   = 4,
	LSM6DSOX_STREAM_MODE             = 6,
	LSM6DSOX_BYPASS_TO_FIFO_MODE     = 7,
} lsm6dsox_fifo_mode_t;
int lsm6dsox_fifo_mode_set(const stmdev_ctx_t *ctx,
							lsm6dsox_fifo_mode_t val);
//int lsm6dsox_fifo_mode_get(const stmdev_ctx_t *ctx,
//                               lsm6dsox_fifo_mode_t *val);

typedef enum {
	LSM6DSOX_USER_BANK           = 0,
	LSM6DSOX_SENSOR_HUB_BANK     = 1,
	LSM6DSOX_EMBEDDED_FUNC_BANK  = 2,
} lsm6dsox_reg_access_t;
int lsm6dsox_mem_bank_set(const stmdev_ctx_t *ctx,
							lsm6dsox_reg_access_t val);
//int lsm6dsox_mem_bank_get(const stmdev_ctx_t *ctx,
//                              lsm6dsox_reg_access_t *val);

typedef enum {
	LSM6DSOX_ODR_PRGS_12Hz5 = 0,
	LSM6DSOX_ODR_PRGS_26Hz  = 1,
	LSM6DSOX_ODR_PRGS_52Hz  = 2,
	LSM6DSOX_ODR_PRGS_104Hz = 3,
} lsm6dsox_mlc_odr_t;
//int lsm6dsox_mlc_data_rate_set(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_mlc_odr_t val);
int lsm6dsox_mlc_data_rate_get(const stmdev_ctx_t *ctx,
								lsm6dsox_mlc_odr_t *val);

typedef enum {
	LSM6DSOX_ODR_FSM_12Hz5 = 0,
	LSM6DSOX_ODR_FSM_26Hz  = 1,
	LSM6DSOX_ODR_FSM_52Hz  = 2,
	LSM6DSOX_ODR_FSM_104Hz = 3,
} lsm6dsox_fsm_odr_t;
int lsm6dsox_fsm_data_rate_get(const stmdev_ctx_t *ctx,
								lsm6dsox_fsm_odr_t *val);

typedef struct {
	lsm6dsox_fsm_enable_a_t          fsm_enable_a;
	lsm6dsox_fsm_enable_b_t          fsm_enable_b;
} lsm6dsox_emb_fsm_enable_t;
int lsm6dsox_fsm_enable_get(const stmdev_ctx_t *ctx,
							lsm6dsox_emb_fsm_enable_t *val);

typedef enum {
	LSM6DSOX_NO_DECIMATION = 0,
	LSM6DSOX_DEC_1         = 1,
	LSM6DSOX_DEC_8         = 2,
	LSM6DSOX_DEC_32        = 3,
} lsm6dsox_odr_ts_batch_t;
int lsm6dsox_fifo_timestamp_decimation_set(const stmdev_ctx_t *ctx,
											lsm6dsox_odr_ts_batch_t val);

typedef enum {
	LSM6DSOX_2g   = 0,
	LSM6DSOX_16g  = 1, /* if XL_FS_MODE = ‘1’ -> LSM6DSOX_2g */
	LSM6DSOX_4g   = 2,
	LSM6DSOX_8g   = 3,
} lsm6dsox_fs_xl_t;
int lsm6dsox_xl_full_scale_set(const stmdev_ctx_t *ctx,
								lsm6dsox_fs_xl_t val);
//int lsm6dsox_xl_full_scale_get(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_fs_xl_t *val);

typedef enum {
	LSM6DSOX_XL_ODR_OFF    = 0,
	LSM6DSOX_XL_ODR_12Hz5  = 1,
	LSM6DSOX_XL_ODR_26Hz   = 2,
	LSM6DSOX_XL_ODR_52Hz   = 3,
	LSM6DSOX_XL_ODR_104Hz  = 4,
	LSM6DSOX_XL_ODR_208Hz  = 5,
	LSM6DSOX_XL_ODR_417Hz  = 6,
	LSM6DSOX_XL_ODR_833Hz  = 7,
	LSM6DSOX_XL_ODR_1667Hz = 8,
	LSM6DSOX_XL_ODR_3333Hz = 9,
	LSM6DSOX_XL_ODR_6667Hz = 10,
	LSM6DSOX_XL_ODR_1Hz6   = 11, /* (low power only) */
} lsm6dsox_odr_xl_t;
int lsm6dsox_xl_data_rate_set(const stmdev_ctx_t *ctx,
								lsm6dsox_odr_xl_t val);
//int lsm6dsox_xl_data_rate_get(const stmdev_ctx_t *ctx,
//                                  lsm6dsox_odr_xl_t *val);

typedef enum {
	LSM6DSOX_250dps   = 0,
	LSM6DSOX_125dps   = 1,
	LSM6DSOX_500dps   = 2,
	LSM6DSOX_1000dps  = 4,
	LSM6DSOX_2000dps  = 6,
} lsm6dsox_fs_g_t;
int lsm6dsox_gy_full_scale_set(const stmdev_ctx_t *ctx,
								lsm6dsox_fs_g_t val);
//int lsm6dsox_gy_full_scale_get(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_fs_g_t *val);

typedef enum {
	LSM6DSOX_GY_ODR_OFF    = 0,
	LSM6DSOX_GY_ODR_12Hz5  = 1,
	LSM6DSOX_GY_ODR_26Hz   = 2,
	LSM6DSOX_GY_ODR_52Hz   = 3,
	LSM6DSOX_GY_ODR_104Hz  = 4,
	LSM6DSOX_GY_ODR_208Hz  = 5,
	LSM6DSOX_GY_ODR_417Hz  = 6,
	LSM6DSOX_GY_ODR_833Hz  = 7,
	LSM6DSOX_GY_ODR_1667Hz = 8,
	LSM6DSOX_GY_ODR_3333Hz = 9,
	LSM6DSOX_GY_ODR_6667Hz = 10,
} lsm6dsox_odr_g_t;
int lsm6dsox_gy_data_rate_set(const stmdev_ctx_t *ctx,
								lsm6dsox_odr_g_t val);
//int lsm6dsox_gy_data_rate_get(const stmdev_ctx_t *ctx,
//                                  lsm6dsox_odr_g_t *val);

int lsm6dsox_block_data_update_set(const stmdev_ctx_t *ctx,
									u8 val);
//int lsm6dsox_block_data_update_get(const stmdev_ctx_t *ctx,
//                                       u8 *val);

typedef enum {
	LSM6DSOX_XL_NOT_BATCHED       =  0,
	LSM6DSOX_XL_BATCHED_AT_12Hz5   =  1,
	LSM6DSOX_XL_BATCHED_AT_26Hz    =  2,
	LSM6DSOX_XL_BATCHED_AT_52Hz    =  3,
	LSM6DSOX_XL_BATCHED_AT_104Hz   =  4,
	LSM6DSOX_XL_BATCHED_AT_208Hz   =  5,
	LSM6DSOX_XL_BATCHED_AT_417Hz   =  6,
	LSM6DSOX_XL_BATCHED_AT_833Hz   =  7,
	LSM6DSOX_XL_BATCHED_AT_1667Hz  =  8,
	LSM6DSOX_XL_BATCHED_AT_3333Hz  =  9,
	LSM6DSOX_XL_BATCHED_AT_6667Hz  = 10,
	LSM6DSOX_XL_BATCHED_AT_6Hz5    = 11,
} lsm6dsox_bdr_xl_t;
int lsm6dsox_fifo_xl_batch_set(const stmdev_ctx_t *ctx,
								lsm6dsox_bdr_xl_t val);
//int lsm6dsox_fifo_xl_batch_get(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_bdr_xl_t *val);

typedef enum {
	LSM6DSOX_GY_NOT_BATCHED         = 0,
	LSM6DSOX_GY_BATCHED_AT_12Hz5    = 1,
	LSM6DSOX_GY_BATCHED_AT_26Hz     = 2,
	LSM6DSOX_GY_BATCHED_AT_52Hz     = 3,
	LSM6DSOX_GY_BATCHED_AT_104Hz    = 4,
	LSM6DSOX_GY_BATCHED_AT_208Hz    = 5,
	LSM6DSOX_GY_BATCHED_AT_417Hz    = 6,
	LSM6DSOX_GY_BATCHED_AT_833Hz    = 7,
	LSM6DSOX_GY_BATCHED_AT_1667Hz   = 8,
	LSM6DSOX_GY_BATCHED_AT_3333Hz   = 9,
	LSM6DSOX_GY_BATCHED_AT_6667Hz   = 10,
	LSM6DSOX_GY_BATCHED_AT_6Hz5     = 11,
} lsm6dsox_bdr_gy_t;
int lsm6dsox_fifo_gy_batch_set(const stmdev_ctx_t *ctx,
								lsm6dsox_bdr_gy_t val);
//int lsm6dsox_fifo_gy_batch_get(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_bdr_gy_t *val);

typedef struct {
	u8 sig_mot      : 1; /* significant motion */
	u8 tilt         : 1; /* tilt detection  */
	u8 step         : 1; /* step counter/detector */
	u8 mlc          : 1; /* machine learning core */
	u8 fsm          : 1; /* finite state machine */
	u8 fifo_compr   : 1; /* mlc 8 interrupt event */
} lsm6dsox_emb_sens_t;
//int lsm6dsox_embedded_sens_set(const stmdev_ctx_t *ctx,
//                                   lsm6dsox_emb_sens_t *emb_sens);
int lsm6dsox_embedded_sens_get(const stmdev_ctx_t *ctx,
								lsm6dsox_emb_sens_t *emb_sens);

typedef enum {
	LSM6DSOX_GYRO_NC_TAG    = 1,
	LSM6DSOX_XL_NC_TAG,
	LSM6DSOX_TEMPERATURE_TAG,
	LSM6DSOX_TIMESTAMP_TAG,
	LSM6DSOX_CFG_CHANGE_TAG,
	LSM6DSOX_XL_NC_T_2_TAG,
	LSM6DSOX_XL_NC_T_1_TAG,
	LSM6DSOX_XL_2XC_TAG,
	LSM6DSOX_XL_3XC_TAG,
	LSM6DSOX_GYRO_NC_T_2_TAG,
	LSM6DSOX_GYRO_NC_T_1_TAG,
	LSM6DSOX_GYRO_2XC_TAG,
	LSM6DSOX_GYRO_3XC_TAG,
	LSM6DSOX_SENSORHUB_SLAVE0_TAG,
	LSM6DSOX_SENSORHUB_SLAVE1_TAG,
	LSM6DSOX_SENSORHUB_SLAVE2_TAG,
	LSM6DSOX_SENSORHUB_SLAVE3_TAG,
	LSM6DSOX_STEP_CPUNTER_TAG,
	LSM6DSOX_GAME_ROTATION_TAG,
	LSM6DSOX_GEOMAG_ROTATION_TAG,
	LSM6DSOX_ROTATION_TAG,
	LSM6DSOX_SENSORHUB_NACK_TAG  = 0x19,
} lsm6dsox_fifo_tag_t;

int lsm6dsox_fifo_sensor_tag_get(const stmdev_ctx_t *ctx,
									lsm6dsox_fifo_tag_t *val);

typedef struct {
	u8 drdy_xl       : 1; /* Accelerometer data ready */
	u8 drdy_g        : 1; /* Gyroscope data ready */
	u8 drdy_temp     : 1; /* Temperature data ready (1 = int2 pin disable) */
	u8 boot          : 1; /* Restoring calibration parameters */
	u8 fifo_th       : 1; /* FIFO threshold reached */
	u8 fifo_ovr      : 1; /* FIFO overrun */
	u8 fifo_full     : 1; /* FIFO full */
	u8 fifo_bdr      : 1; /* FIFO Batch counter threshold reached */
	u8 den_flag      : 1; /* external trigger level recognition (DEN) */
	u8 sh_endop      : 1; /* sensor hub end operation */
	u8 timestamp     : 1; /* timestamp overflow (1 = int2 pin disable) */
	u8 six_d         : 1; /* orientation change (6D/4D detection) */
	u8 double_tap    : 1; /* double-tap event */
	u8 free_fall     : 1; /* free fall event */
	u8 wake_up       : 1; /* wake up event */
	u8 single_tap    : 1; /* single-tap event */
	u8 sleep_change  : 1; /* Act/Inact (or Vice-versa) status changed */
	u8 step_detector : 1; /* Step detected */
	u8 tilt          : 1; /* Relative tilt event detected */
	u8 sig_mot       : 1; /* "significant motion" event detected */
	u8 fsm_lc        : 1; /* fsm long counter timeout interrupt event */
	u8 fsm1          : 1; /* fsm 1 interrupt event */
	u8 fsm2          : 1; /* fsm 2 interrupt event */
	u8 fsm3          : 1; /* fsm 3 interrupt event */
	u8 fsm4          : 1; /* fsm 4 interrupt event */
	u8 fsm5          : 1; /* fsm 5 interrupt event */
	u8 fsm6          : 1; /* fsm 6 interrupt event */
	u8 fsm7          : 1; /* fsm 7 interrupt event */
	u8 fsm8          : 1; /* fsm 8 interrupt event */
	u8 fsm9          : 1; /* fsm 9 interrupt event */
	u8 fsm10         : 1; /* fsm 10 interrupt event */
	u8 fsm11         : 1; /* fsm 11 interrupt event */
	u8 fsm12         : 1; /* fsm 12 interrupt event */
	u8 fsm13         : 1; /* fsm 13 interrupt event */
	u8 fsm14         : 1; /* fsm 14 interrupt event */
	u8 fsm15         : 1; /* fsm 15 interrupt event */
	u8 fsm16         : 1; /* fsm 16 interrupt event */
	u8 mlc1          : 1; /* mlc 1 interrupt event */
	u8 mlc2          : 1; /* mlc 2 interrupt event */
	u8 mlc3          : 1; /* mlc 3 interrupt event */
	u8 mlc4          : 1; /* mlc 4 interrupt event */
	u8 mlc5          : 1; /* mlc 5 interrupt event */
	u8 mlc6          : 1; /* mlc 6 interrupt event */
	u8 mlc7          : 1; /* mlc 7 interrupt event */
	u8 mlc8          : 1; /* mlc 8 interrupt event */
} lsm6dsox_pin_int1_route_t;
int lsm6dsox_pin_int1_route_set(const stmdev_ctx_t *ctx,
								lsm6dsox_pin_int1_route_t val);
int lsm6dsox_pin_int1_route_get(const stmdev_ctx_t *ctx,
								lsm6dsox_pin_int1_route_t *val);

typedef struct {
	u8 drdy_ois      : 1; /* OIS chain data ready */
	u8 drdy_xl       : 1; /* Accelerometer data ready */
	u8 drdy_g        : 1; /* Gyroscope data ready */
	u8 drdy_temp     : 1; /* Temperature data ready */
	u8 fifo_th       : 1; /* FIFO threshold reached */
	u8 fifo_ovr      : 1; /* FIFO overrun */
	u8 fifo_full     : 1; /* FIFO full */
	u8 fifo_bdr      : 1; /* FIFO Batch counter threshold reached */
	u8 timestamp     : 1; /* timestamp overflow */
	u8 six_d         : 1; /* orientation change (6D/4D detection) */
	u8 double_tap    : 1; /* double-tap event */
	u8 free_fall     : 1; /* free fall event */
	u8 wake_up       : 1; /* wake up event */
	u8 single_tap    : 1; /* single-tap event */
	u8 sleep_change  : 1; /* Act/Inact (or Vice-versa) status changed */
	u8 step_detector : 1; /* Step detected */
	u8 tilt          : 1; /* Relative tilt event detected */
	u8 sig_mot       : 1; /* "significant motion" event detected */
	u8 fsm_lc        : 1; /* fsm long counter timeout interrupt event */
	u8 fsm1          : 1; /* fsm 1 interrupt event */
	u8 fsm2          : 1; /* fsm 2 interrupt event */
	u8 fsm3          : 1; /* fsm 3 interrupt event */
	u8 fsm4          : 1; /* fsm 4 interrupt event */
	u8 fsm5          : 1; /* fsm 5 interrupt event */
	u8 fsm6          : 1; /* fsm 6 interrupt event */
	u8 fsm7          : 1; /* fsm 7 interrupt event */
	u8 fsm8          : 1; /* fsm 8 interrupt event */
	u8 fsm9          : 1; /* fsm 9 interrupt event */
	u8 fsm10         : 1; /* fsm 10 interrupt event */
	u8 fsm11         : 1; /* fsm 11 interrupt event */
	u8 fsm12         : 1; /* fsm 12 interrupt event */
	u8 fsm13         : 1; /* fsm 13 interrupt event */
	u8 fsm14         : 1; /* fsm 14 interrupt event */
	u8 fsm15         : 1; /* fsm 15 interrupt event */
	u8 fsm16         : 1; /* fsm 16 interrupt event */
	u8 mlc1          : 1; /* mlc 1 interrupt event */
	u8 mlc2          : 1; /* mlc 2 interrupt event */
	u8 mlc3          : 1; /* mlc 3 interrupt event */
	u8 mlc4          : 1; /* mlc 4 interrupt event */
	u8 mlc5          : 1; /* mlc 5 interrupt event */
	u8 mlc6          : 1; /* mlc 6 interrupt event */
	u8 mlc7          : 1; /* mlc 7 interrupt event */
	u8 mlc8          : 1; /* mlc 8 interrupt event */
} lsm6dsox_pin_int2_route_t;
int lsm6dsox_pin_int2_route_set(const stmdev_ctx_t *ctx,
								stmdev_ctx_t *aux_ctx,
								lsm6dsox_pin_int2_route_t val);
int lsm6dsox_pin_int2_route_get(const stmdev_ctx_t *ctx,
								stmdev_ctx_t *aux_ctx,
								lsm6dsox_pin_int2_route_t *val);

#ifdef __cplusplus
}
#endif

#endif /*LSM6DSOX_DRIVER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
