#ifndef SC7U22_REGS_H
#define SC7U22_REGS_H

#include "aglink_gyro.h"
#include <linux/math64.h>

//传感器调试打印功能是否打开
#define SL_SENSOR_ALGO_RELEASE_ENABLE  0x01

/***使用驱动前请根据实际接线情况配置******/
/**SC7U22的SDO 脚接地：  0****************/
/**SC7U22的SDO 脚接电源：1****************/
#define SL_SC7U22_SDO_VDD_GND            1
/*****************************************/
/***使用驱动前请根据实际IIC情况进行配置***/
/**SC7U22的IIC 接口地址类型 7bits：  0****/
/**SC7U22的IIC 接口地址类型 8bits：  1****/
#define SL_SC7U22_IIC_7BITS_8BITS        0
/*****************************************/
#if SL_SC7U22_SDO_VDD_GND == 0
#define SL_SC7U22_IIC_7BITS_ADDR        0x18
#define SL_SC7U22_IIC_8BITS_WRITE_ADDR  0x30
#define SL_SC7U22_IIC_8BITS_READ_ADDR   0x31
#else
#define SL_SC7U22_IIC_7BITS_ADDR        0x19
#define SL_SC7U22_IIC_8BITS_WRITE_ADDR  0x32
#define SL_SC7U22_IIC_8BITS_READ_ADDR   0x33
#endif
#if SL_SC7U22_IIC_7BITS_8BITS == 0
#define SL_SC7U22_IIC_ADDRESS        SL_SC7U22_IIC_7BITS_ADDR
#else
#define SL_SC7U22_IIC_WRITE_ADDRESS  SL_SC7U22_IIC_8BITS_WRITE_ADDR
#define SL_SC7U22_IIC_READ_ADDRESS   SL_SC7U22_IIC_8BITS_READ_ADDR
#endif

void sl_sc7u22_get_hwio_ops(struct aglink_gyro_hwio_ops *gyro_hwio);

/*************驱动初始化函数*******************/
unsigned char sl_sc7u22_config(struct gyro_config config);
/*************返回数据情况如下*****************/
/**return : 1    IIC通讯正常IC在线*************/
/**return : 0;   IIC通讯异常或IC不在线*********/

/*************SC7U22 Sensor Time**************/
u64 sl_sc7u22_timestamp_read(void);
/*************返回数据情况如下*****************/
/**return : Internal Sensor Time***************/

/******定时读取数据寄存器的FIFO数据******/
struct gyro_pkt *sl_sc7u22_fifo_read(s16 *dat_cnt);

/*********进入传感器关闭模式*************/
unsigned char sl_sc7u22_power_down(void);
/**0: 进入模式失败***********************/
/**1: 进入模式成功***********************/

/*********SC7U22 RESET***************/
unsigned char sl_sc7u22_soft_reset(void);
/**0: OK*****************************/
/**1: ERROR**************************/

/*************GSensor and GyroSensor打开和关闭函数*********/
unsigned char sl_sc7u22_open_close_set(unsigned char acc_enable, unsigned char gyro_enable);
/**acc_enable:  0=关闭ACC Sensor; 1=打开ACC Sensor*********/
/**gyro_enable: 0=关闭GYRO Sensor; 1=打开GYRO Sensor*******/
/**return: 0=操作失败；1=操作成功**************************/

/*********进入休眠模式，开中断函数*************/
unsigned char sl_sc7u22_in_sleep_set(unsigned char acc_odr, unsigned char vth, unsigned char tth, unsigned char int_io);
/**acc_odr:  12/25/50**************************************/
/**vth:  运动检测，幅度阈值设置****************************/
/**vth:  运动检测，满足幅度阈值后最小持续时长设置**********/
/**int_io:  1=INT1；2=INT2*********************************/
/**return: 0=操作失败；1=操作成功**************************/

/*********进入工作模式，配置传感器，关闭中断函数***********/
unsigned char sl_sc7u22_wakeup_set(unsigned char odr_mode, unsigned char acc_range, unsigned char acc_hp_en, unsigned short gyro_range, unsigned char gyro_hp_en);
/**odr_mode:  25HZ/50Hz/100Hz/200Hz ACC+GYRO***************/
/**acc_range: ±2G/±4G/±8G/±16G*****************************/
/**acc_hp_en: 0=disable high performance mode;1=enable*****/
/**gyro_range: ±125dps/±250dps/±500dps/±1000dps/±2000dps***/
/**gyro_hp_en: 0=disable hp mode;1=enable hp mode; ********/
/**return: 0=操作失败；1=操作成功**************************/

/**reg map*******************************/
#define SC7U22_WHO_AM_I         0x01

#endif
