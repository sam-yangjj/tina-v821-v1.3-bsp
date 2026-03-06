/*
 *
 * $Id: stk83xx_config.h
 *
 * Copyright (C) 2012~2019 STK, sensortek Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef __STK83XX_CONFIG_H__
#define __STK83XX_CONFIG_H__

/*****************************************************************************
 * Global variable
 *****************************************************************************/
#define STK_INTERRUPT_MODE
//#define STK_POLLING_MODE

//#define STK_AMD /* Turn ON any motion */
//#define STK_HW_STEP_COUNTER /* Turn on step counter */
#define STK_CALI /* Turn on sensortek calibration feature */
//#define STK_CALI_FILE /* Write cali data to file */
#define STK_FIR /* low-pass mode */
//#define STK_ZG
/* enable Zero-G simulation.
 * This feature only works when both of STK_FIR and STK_ZG are turn ON. */
//#define STK_6D_DET /* only for STK8331! */
// #define STK_TILT  /* only for STK8331! */
#define STK_AXES_SIM // for checking and simulation axes
//#define STK_AUTO_K

#ifdef STK_QUALCOMM
//#include <linux/sensors.h>
#undef STK_SPREADTRUM
#elif defined STK_MTK
//#undef STK_INTERRUPT_MODE
//#undef STK_POLLING_MODE
#undef STK_AMD
#define STK_CALI
#elif defined STK_SPREADTRUM
#include <linux/limits.h>
#include <linux/version.h>
//#undef STK_INTERRUPT_MODE
//#define STK_POLLING_MODE
#elif defined STK_ALLWINNER
#undef STK_INTERRUPT_MODE
#define STK_POLLING_MODE
#undef STK_AMD
#endif /* STK_QUALCOMM, STK_MTK, STK_SPREADTRUM, or STK_ALLWINNER */

#ifdef STK_AXES_SIM
#include "stk_axes_sim.h"
#endif // STK_AXES_SIM

#endif /* __STK83XX_CONFIG_H__ */
