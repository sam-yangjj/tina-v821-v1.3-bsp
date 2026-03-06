#include "sc7u22_reg.h"
#include "debug.h"

static struct aglink_gyro_hwio_ops *hwio;
static struct gyro_config gyro_attri;
static u64 time_base;

//I2C SPI选择
//#define SL_SC7U22_SPI_EN_I2C_DISABLE   0x00 //必须和SL_SPI_IIC_INTERFACE相反；
#define SL_SPI_IIC_INTERFACE           0x01 //必须和SL_SC7A22H_SPI_EN_I2C_DISABLE 相反；
//原始数据高通输出使能控制宏定义
#define SL_SC7U22_RAWDATA_HPF_ENABLE   0x00
//中断脚默认输出电平控制宏定义
#define SL_SC7U22_INT_DEFAULT_LEVEL    0x01
//SDO  上拉电阻控制
#define SL_SC7U22_SDO_PULLUP_ENABLE    0x01
//AOI唤醒中断检测使能
#define SL_SC7U22_AOI_WAKE_UP_ENABLE   0x00

//FIFO_STREAM使能//FIFO_WTM使能
//#define SL_SC7U22_FIFO_STREAM_WTM  	   0x01//0X00=STREAM MODE 0X01=FIFO MODE

static s64 sl7u22_from_fs2000_to_uradps(s16 lsb)
{
	return lsb * 1065; // 61 * pai(3.1415926535) / 180 * 1000 = urad/s
}

unsigned char sl_sc7u22_i2c_spi_write(unsigned char sl_spi_iic, unsigned char reg, unsigned char dat)
{
	u8 write_dat = dat;
	if (hwio->write(SL_SC7U22_IIC_7BITS_ADDR, reg, &write_dat, 1))
		return 0;
	else
		return 1;
}

unsigned char sl_sc7u22_i2c_spi_read(unsigned char sl_spi_iic, unsigned char reg, unsigned short len, unsigned char *buf)
{
	if (hwio->read(SL_SC7U22_IIC_7BITS_ADDR, reg, buf, len))
		return 0;
	else
		return 1;
}

static void delay_nms(u32 ms)
{
	msleep(ms);
}

static void sl_delay(unsigned char sl_i)
{
	unsigned int sl_j = 10;

	sl_j = sl_j * 1000 * sl_i;
	while (sl_j--)
		;
}

void sl_sc7u22_get_hwio_ops(struct aglink_gyro_hwio_ops *gyro_hwio)
{
	hwio = gyro_hwio;
}

static unsigned char sl_sc7u22_check(void)
{
	unsigned char reg_value = 0;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, SC7U22_WHO_AM_I, 1, &reg_value);
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "0x%x=0x%x\r\n", SC7U22_WHO_AM_I, reg_value);
#endif
	if (reg_value == 0x6A)
		return 0x01;//SC7U22
	else
		return 0x00;//其他芯片
}

static s64 sl_sc7u22_basetime_reset2zero(void)
{
	u64 sc7u22_time = sl_sc7u22_timestamp_read();
	u64 sys_time = aglink_gyro_get_base_time();
	return sys_time - sc7u22_time;
}

unsigned char sl_sc7u22_config(struct gyro_config config)
{
	unsigned char check_flag = 0;
	unsigned char reg_value = 0;
	unsigned char pwr_ctrl = 0;
	unsigned char gyr_conf = 0;
	unsigned char fifo_deep_l = 0;
	unsigned char fifo_deep_h = 0;
	unsigned char fifo_conf = 0;

	memset(&gyro_attri, 0x00, sizeof(gyro_attri));
	memcpy(&gyro_attri, &config, sizeof(gyro_attri));

#if SL_SPI_IIC_INTERFACE == 0x00 //SPI
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x90
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x4A, 0x66);
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x83);//goto 0x6F
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x6F, 0x04);//I2C disable
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x6F
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x4A, 0x00);
	sl_delay(1);
#endif
	check_flag = sl_sc7u22_check();
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_sc7u22_check=0x%x\r\n", check_flag);
#endif
	if (check_flag == 1)
		check_flag = sl_sc7u22_power_down();
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_sc7u22_power_down=0x%x\r\n", check_flag);
#endif
	if (check_flag == 1)
		check_flag = sl_sc7u22_soft_reset();
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_sc7u22_soft_reset=0x%x\r\n", check_flag);
#endif
	if (check_flag == 1) {
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		delay_nms(10);//10ms
		pwr_ctrl = (gyro_attri.xl_en << 2) |
					(gyro_attri.gy_en << 1);
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, pwr_ctrl);//PWR_CTRL ENABLE ACC+GYR+TEMP
		delay_nms(10);//10ms

		if (gyro_attri.xl_en) {
			sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x40, 0x88);//ACC_CONF 0x88=100Hz
			sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x41, 0x01);//ACC_RANGE  ±8G
		}
		if (gyro_attri.gy_en) {
			gyr_conf = (0xc0 | gyro_attri.speed);
			sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x42, gyr_conf);//GYR_CONF 0xC8=100Hz
			sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x43, 0x00);//GYR_RANGE 2000dps
			sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x43, 0x00);//GYR_RANGE 2000dps
		}
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x04, 0x50);//COM_CFG

#if SL_SC7U22_RAWDATA_HPF_ENABLE == 0x01
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x83);//goto 0x83
		sl_delay(1);
		sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x26, 1, &reg_value);
		reg_value = reg_value | 0xA0;
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x26, reg_value);//HPF_CFG  rawdata hpf
#endif

#if SL_SC7U22_AOI_WAKE_UP_ENABLE == 0x01
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x30, 0x2A);//XYZ-ENABLE
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x32, 0x01);//VTH
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x33, 0x01);//TTH
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x3F, 0x30);//HPF FOR AOI1&AOI2
#endif

		if (gyro_attri.fifo_deep > 2047)
			gyro_attri.fifo_deep = 2047;
		fifo_deep_l = gyro_attri.fifo_deep & 0xff;
		fifo_deep_h = (gyro_attri.fifo_deep >> 8) & 0x07;
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1E, fifo_deep_l);//
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1D, 0x00);//
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1D, 0x20 | fifo_deep_h);//
		fifo_conf = gyro_attri.gy_en << 1 |
					gyro_attri.xl_en << 2 ;
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1C, 0x31 | fifo_conf);//

#if SL_SC7U22_SDO_PULLUP_ENABLE == 0x01
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x8C);//goto 0x8C
		sl_delay(1);
		sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x30, 1, &reg_value);
		reg_value = reg_value & 0xFE;//CS PullUP_enable
		reg_value = reg_value & 0xFD;//SDO PullUP_enable
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x30, reg_value);

		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		delay_nms(1);
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		delay_nms(1);
#else
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x8C);//goto 0x8C
		sl_delay(1);
		sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x30, 1, &reg_value);
		reg_value = reg_value & 0xFE;//CS PullUP_enable
		reg_value = reg_value | 0x02;//SDO PullUP_disable
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x30, reg_value);

		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		sl_delay(1);
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		sl_delay(1);
#endif
		time_base = sl_sc7u22_basetime_reset2zero();
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "time_base : %lld ns\n", time_base);
#endif

		return 1;
	} else
		return 0;
}

u64 sl_sc7u22_timestamp_read(void)
{
	unsigned char  time_data[3];
	unsigned int time_stamp;

	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x18, 1, &time_data[0]);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x19, 1, &time_data[1]);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x1A, 1, &time_data[2]);

	time_stamp = (unsigned int)(time_data[0] << 16 | time_data[1] << 8 | time_data[2]);

	return time_stamp * 39061;
}

static unsigned short sl_sc7u22_get_num(void)
{
	unsigned char  fifo_num1 = 0;
	unsigned char  fifo_num2 = 0;
	unsigned short fifo_num = 0;
	//sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7C, 0x00);
	//delay_nms(1);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x1F, 1, &fifo_num1);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x20, 1, &fifo_num2);
	if ((fifo_num1 & 0x10) == 0x10) {
		fifo_num = 2048;
	} else {
		fifo_num = (fifo_num1 & 0x0F) * 256 + fifo_num2;
	}
	return fifo_num;
}

static u8 sl_sc7u22_data_unit_len(void)
{
	return 2 + (gyro_attri.ts_en ? 4 : 0)
			+ (gyro_attri.xl_en ? 6 : 0)
			+ (gyro_attri.gy_en ? 6 : 0);
}

struct gyro_pkt *sl_sc7u22_fifo_read(s16 *dat_cnt)
{
	unsigned char header[2];
	unsigned short i = 0;
	s16 gyro_dat_tmp[3];
	s16 fifo_len = 0;
	struct gyro_pkt *gyro_dat = NULL;
	u8 *sc7u22_fifo = NULL;
	s16 fifo_num = 0;
	s16 sc7u22_fifo_len;
	s16 gyro_dat_len;

	fifo_num = sl_sc7u22_get_num();
	if (fifo_num == 0) {
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "fifo is null\n");
		return NULL;
	}

	sc7u22_fifo_len = fifo_num * 2;
	sc7u22_fifo = (u8 *)aglink_gyro_zmalloc(sc7u22_fifo_len);
	if (!sc7u22_fifo)
		return NULL;

	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x21, sc7u22_fifo_len, sc7u22_fifo);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1D, 0x00);//BY PASS MODE
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x1D, 0x20);//Stream MODE
	//sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7C, 0x01);//

	gyro_dat_len = sc7u22_fifo_len / sl_sc7u22_data_unit_len();
	gyro_dat = (struct gyro_pkt *)aglink_gyro_zmalloc(gyro_dat_len * sizeof(struct gyro_pkt));
	if (!gyro_dat) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "malloc gyro buffer failed\n");
		goto out;
	}

	while (i < sc7u22_fifo_len) {

		if (fifo_len >= gyro_dat_len) {
			fifo_len = gyro_dat_len;
			break;
		}

		//header process 1
		header[0] = sc7u22_fifo[i + 0];
		header[1] = sc7u22_fifo[i + 1];
		i = i + 2;

		//timestamp process 2
		if (header[1] & 0x80) {
			gyro_dat[fifo_len].time = sc7u22_fifo[i + 3] |
								(sc7u22_fifo[i + 2] << 8) |
								(sc7u22_fifo[i + 1] << 16) |
								(sc7u22_fifo[i + 0] << 24);
			gyro_dat[fifo_len].time *= 39061;
			gyro_dat[fifo_len].time += time_base;
			i = i + 4;//every frame  include the timestamp, 4 bytes
		}
		//acc process 3
		if (header[0] & 0x04) {
			i = i + 6;
		}
		//gyro process 3
		if (header[0] & 0x02) {
			gyro_dat_tmp[0] = ((s16)(sc7u22_fifo[i + 0] * 256 + sc7u22_fifo[i + 1])) ;
			gyro_dat_tmp[1] = ((s16)(sc7u22_fifo[i + 2] * 256 + sc7u22_fifo[i + 3])) ;
			gyro_dat_tmp[2] = ((s16)(sc7u22_fifo[i + 4] * 256 + sc7u22_fifo[i + 5])) ;
			gyro_dat[fifo_len].gyr_dat.x =
						sl7u22_from_fs2000_to_uradps(gyro_dat_tmp[0]);
			gyro_dat[fifo_len].gyr_dat.y =
						sl7u22_from_fs2000_to_uradps(gyro_dat_tmp[1]);
			gyro_dat[fifo_len].gyr_dat.z =
						sl7u22_from_fs2000_to_uradps(gyro_dat_tmp[2]);
			i = i + 6;
		}

		//temperature process 1
		if (header[0] & 0x01) {
			i = i + 2;
		}
		fifo_len++;
	}

	if (fifo_len != gyro_dat_len)
		gyro_printk(AGLINK_GYRO_DBG_WARN, "Missing sample num: %d\n", gyro_dat_len - fifo_len);

out:
	aglink_gyro_free(sc7u22_fifo);
	*dat_cnt = fifo_len;
	return gyro_dat;
}

unsigned char sl_sc7u22_power_down(void)
{
	unsigned char sl_read_reg  = 0xff;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(20);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, 0x00);//POWER DOWN
	sl_delay(200);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x7D, 1, &sl_read_reg);
	if (sl_read_reg == 0x00)
		return  1;
	else
		return  0;
}


unsigned char sl_sc7u22_soft_reset(void)
{
	unsigned char sl_read_reg = 0xff;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	delay_nms(1);
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x04, 1, &sl_read_reg);
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "SL_SC7U22_SOFT_RESET1 0x04 = 0x%x\r\n", sl_read_reg);
	sl_read_reg = 0xff;
#endif
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x04, 0x10);//BOOT
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x4A, 0xA5);//SOFT_RESET
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x4A, 0xA5);//SOFT_RESET
	delay_nms(10);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x04, 1, &sl_read_reg);
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "SL_SC7U22_SOFT_RESET2 0x08=0x%x\r\n", sl_read_reg);
#endif
	if (sl_read_reg == 0x50)
		return 1;
	else
		return 0;

}


/****acc_enable ==0 close acc;acc_enable ==1 open acc******/
/****gyro_enable==0 close acc;gyro_enable==1 open acc******/
unsigned char sl_sc7u22_open_close_set(unsigned char acc_enable, unsigned char gyro_enable)
{
	unsigned char sl_read_reg = 0xff;
	unsigned char sl_read_check = 0xff;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x7D, 1, &sl_read_reg);

	if (acc_enable == 0) {
		sl_read_reg = sl_read_reg & 0xFB;//Bit.ACC_EN=0
	} else if (acc_enable == 1) {
		sl_read_reg = sl_read_reg | 0x04;//Bit.ACC_EN=1
	}
	if (gyro_enable == 0) {
		sl_read_reg = sl_read_reg & 0xFD;//Bit.GYR_EN=0
	} else if (gyro_enable == 1) {
		sl_read_reg = sl_read_reg | 0x02;//Bit.GYR_EN=1
	}
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, sl_read_reg);//PWR_CTRL ENABLE ACC+GYR+TEMP
	sl_delay(5);//5ms
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, sl_read_reg);//PWR_CTRL ENABLE ACC+GYR+TEMP
	sl_delay(20);//10ms
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x7D, 1, &sl_read_check);
	if (sl_read_reg != sl_read_check) {
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_read_reg=0x%x  sl_read_check=0x%x\r\n", sl_read_reg, sl_read_check);
#endif
		return 0;
	}
	return 1;
}


/*******open INT******/
unsigned char sl_sc7u22_in_sleep_set(unsigned char acc_odr, unsigned char vth, unsigned char tth, unsigned char int_io)
{
	unsigned char sl_read_reg = 0xff;
	unsigned char sl_acc_odr_reg = 0xff;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	if (int_io == 1) {
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x06, 0x02);//AOI1-INT1
	} else if (int_io == 2) {
		sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x08, 0x02);//AOI1-INT2
	}

	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x04, 1, &sl_read_reg);
#if SL_SC7U22_INT_DEFAULT_LEVEL == 0x01
	sl_read_reg = sl_read_reg | 0x04;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x04, sl_read_reg);//defalut high level&& push-pull
#else
	reg_value = reg_value & 0xDF;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x06, sl_read_reg);//defalut low  level&& push-pull
#endif
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x30, 0x2A);//AIO1-Enable
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x32, vth);//VTH
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x33, tth);//TTH

	if (acc_odr == 12) {
		sl_acc_odr_reg = 0x05;
	} else if (acc_odr == 25) {
		sl_acc_odr_reg = 0x06;
	} else if (acc_odr == 50) {
		sl_acc_odr_reg = 0x07;
	}
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x40, sl_acc_odr_reg);//ACC_CONF
	delay_nms(5);//5ms
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, 0x04);//acc open and gyro close
	delay_nms(5);//5ms
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, 0x04);//acc open and gyro close
	sl_delay(200);
	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x7D, 1, &sl_read_reg);

	if (sl_read_reg != 0x04) {
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_read_reg=0x%x 0x04\r\n", sl_read_reg);
#endif
		return 0;
	}
	return 1;
}

/*******ODR SET:25 50 100 200******************/
/*******acc range:2 4 8 16*********************/
/*******gyro range:125 250 500 1000 2000*******/
/*******acc_hp_en: 0=disable 1=enable**********/
/*******gyro_hp_en:0=disable 1=enable**********/
unsigned char sl_sc7u22_wakeup_set(unsigned char odr_mode, unsigned char acc_range, unsigned char acc_hp_en, unsigned short gyro_range, unsigned char gyro_hp_en)
{
	unsigned char sl_odr_reg        = 0x00;
	unsigned char sl_acc_mode_reg   = 0x00;
	unsigned char sl_gyro_mode_reg  = 0x00;
	unsigned char sl_acc_range_reg  = 0x00;
	unsigned char sl_gyro_range_reg = 0x00;
	unsigned char sl_read_check     = 0xff;

	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, 0x06);//PWR_CTRL ENABLE ACC+GYR
	sl_delay(5);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x7D, 0x06);//PWR_CTRL ENABLE ACC+GYR
	sl_delay(200);
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x30, 0x00);//AIO1-disable
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x32, 0xff);//vth
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x33, 0xff);//tth

	if (odr_mode == 25) {
		sl_odr_reg = 0x06;
	} else if (odr_mode == 50) {
		sl_odr_reg = 0x07;
	} else if (odr_mode == 100) {
		sl_odr_reg = 0x08;
	} else if (odr_mode == 200) {
		sl_odr_reg = 0x09;
	}
	if (acc_hp_en == 1)
		sl_acc_mode_reg = 0x80;

	sl_acc_mode_reg = sl_acc_mode_reg|sl_odr_reg;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x40, sl_acc_mode_reg);//ACC_CONF

	if (gyro_hp_en == 1)
		sl_gyro_mode_reg = 0x40;
	else if (gyro_hp_en == 2)
		sl_gyro_mode_reg = 0x80;
	else if (gyro_hp_en == 3)
		sl_gyro_mode_reg = 0xC0;

	sl_gyro_mode_reg = sl_gyro_mode_reg | sl_odr_reg;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x42, sl_gyro_mode_reg);//GYR_CONF

	if (acc_range == 2)
		sl_acc_range_reg = 0x00;
	else if (acc_range == 4)
		sl_acc_range_reg = 0x01;
	else if (acc_range == 8)
		sl_acc_range_reg = 0x02;
	else if (acc_range == 16)
		sl_acc_range_reg = 0x03;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x41, sl_acc_range_reg);//ACC_RANGE

	if (gyro_range == 2000)
		sl_gyro_range_reg = 0x00;
	else if (gyro_range == 1000)
		sl_gyro_range_reg = 0x01;
	else if (gyro_range == 500)
		sl_gyro_range_reg = 0x02;
	else if (gyro_range == 250)
		sl_gyro_range_reg = 0x03;
	else if (gyro_range == 125)
		sl_gyro_range_reg = 0x04;
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x43, sl_gyro_range_reg);//GYR_RANGE 2000dps
	sl_sc7u22_i2c_spi_write(SL_SPI_IIC_INTERFACE, 0x43, sl_gyro_range_reg);//GYR_RANGE 2000dps
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
//	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x40, 1, &sl_read_check);
//	gyro_printk(AGLINK_GYRO_DBG_ALWY, "RawData:0x40=%x\r\n",sl_read_check);
//	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x41, 1, &sl_read_check);
//	gyro_printk(AGLINK_GYRO_DBG_ALWY, "RawData:0x41=%x\r\n",sl_read_check);
//	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x42, 1, &sl_read_check);
//	gyro_printk(AGLINK_GYRO_DBG_ALWY, "RawData:0x42=%x\r\n",sl_read_check);
//	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x43, 1, &sl_read_check);
//	gyro_printk(AGLINK_GYRO_DBG_ALWY, "RawData:0x43=%x\r\n",sl_read_check);
#endif

	sl_sc7u22_i2c_spi_read(SL_SPI_IIC_INTERFACE, 0x43, 1, &sl_read_check);
	if (sl_read_check != sl_gyro_range_reg) {
#if SL_SENSOR_ALGO_RELEASE_ENABLE == 0x00
		gyro_printk(AGLINK_GYRO_DBG_ALWY, "sl_read_check=0x%x sl_gyro_range_reg=0x%x\r\n", sl_read_check, sl_gyro_range_reg);
#endif
		return 0;
	}
	return 1;
}
