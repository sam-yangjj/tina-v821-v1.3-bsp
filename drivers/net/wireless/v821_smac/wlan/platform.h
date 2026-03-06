/*
 * platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013, XRadio
 * Author: XRadio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef XRADIO_PLAT_H_INCLUDED
#define XRADIO_PLAT_H_INCLUDED

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mmc/host.h>
#if IS_ENABLED(CONFIG_ARCH_SUN300IW1)
#include <sunxi-chips.h>
#endif

enum xr_chip_type {
	XR_CHIP_TYPE_NONE = 0,
	XR_CHIP_TYPE_V821,
	XR_CHIP_TYPE_V821B,
};

/* Select hardware platform.*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))

extern void sunxi_mmc_rescan_card(unsigned ids);
extern int sunxi_mmc_check_r1_ready(struct mmc_host *mmc, unsigned ms);

#define MCI_RESCAN_CARD(id)  sunxi_mmc_rescan_card(id)
#define MCI_CHECK_READY(h, t)     sunxi_mmc_check_r1_ready(h, t)

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
#define PLAT_ALLWINNER_SUNXI
#define MCI_RESCAN_CARD(id, ins)  sunxi_mci_rescan_card(id, ins)
#define MCI_CHECK_READY(h, t)     sunxi_mci_check_r1_ready(h, t)

extern void sunxi_mci_rescan_card(unsigned id, unsigned insert);
extern int sunxi_mci_check_r1_ready(struct mmc_host *mmc, unsigned ms);

#else
#define PLAT_ALLWINNER_SUN6I
#define MCI_RESCAN_CARD(id, ins)  sw_mci_rescan_card(id, ins)
#define MCI_CHECK_READY(h, t)     sw_mci_check_r1_ready(h, t)

extern void sw_mci_rescan_card(unsigned id, unsigned insert);
extern int sw_mci_check_r1_ready(struct mmc_host *mmc, unsigned ms);
#endif

int xradio_get_syscfg(void);

/* platform interfaces */
int  xradio_plat_init(void);
void xradio_plat_deinit(void);
void  xradio_sdio_detect(int enable);
int  xradio_request_gpio_irq(struct device *dev, void *sbus_priv);
void xradio_free_gpio_irq(struct device *dev, void *sbus_priv);
int  xradio_wlan_power(int on);

#ifdef CONFIG_DRIVER_V821
/* used for the same SDD file for different hosc frequencies(24M/40M...) */
#ifdef CONFIG_DRIVER_V821
#define XRADIO_SDD_DIFF_HOSC_FREQ_FILE_MERGE     1
#define XRADIO_FIX_P2P_RX_SAME_NGO_REQ           1
#else
#define XRADIO_SDD_DIFF_HOSC_FREQ_FILE_MERGE     0
#define XRADIO_FIX_P2P_RX_SAME_NGO_REQ           0
#endif

#define POWER_CTRL_AON_BASE             0x4A011000
#define GPRCM_CTRL_BASE                 0x4A000000
#define HIF_WLAN_STATE_REL             (POWER_CTRL_AON_BASE + 0x68)
#define HIF_OV_CTRL_REL                (GPRCM_CTRL_BASE + 0x18C)

#ifdef USE_ADDRESS_REMAP_IN_LINUX
extern volatile void *HIF_WLAN_STATE_VIR;
extern volatile void *HIF_OV_CTRL_VIR;
extern volatile void *HIF_OV_MISC_REG_VIR;
extern volatile void *WLAN_APB_PWR_CTRL_BASE_VIR;
extern volatile void *PAS_SHARE_RAM_ADDR_BASE;
extern volatile void *AHB_SHARE_RAM_ADDR_BASE;
extern volatile void *CCM_MOD_RST_CTRL_VIR;
extern volatile void *CCU_AON_DCXO_CFG_VIR;
#endif

#ifdef USE_ADDRESS_REMAP_IN_LINUX
#define HIF_WLAN_STATE					HIF_WLAN_STATE_VIR
#else
#define HIF_WLAN_STATE					HIF_WLAN_STATE_REL
#endif

#ifdef USE_ADDRESS_REMAP_IN_LINUX
#define HIF_OV_CTRL             HIF_OV_CTRL_VIR
#else
#define HIF_OV_CTRL				HIF_OV_CTRL_REL
#endif


#define CCM_MOD_RST_CTRL_REL (0x4A010000 + 0x518)
#ifdef USE_ADDRESS_REMAP_IN_LINUX
#define CCM_MOD_RST_CTRL CCM_MOD_RST_CTRL_VIR
#else
#define CCM_MOD_RST_CTRL CCM_MOD_RST_CTRL_REL
#endif

#define CCU_AON_DCXO_CFG_REL (0x4A010000 + 0x570)
#define CCU_AON_DCXO_CFG    CCU_AON_DCXO_CFG_VIR
#define CCM_WLAN_RST_BIT BIT(0)
#define CCU_AON_DCXO_FLAG_BIT BIT(0) /* 1: Ddie 0:Adie */
#define CCU_AON_DCXO_DIE_FREQ_OFFSET_SHIFT 11 /* R/W */
#define CCU_AON_DCXO_DIE_FREQ_OFFSET_MASK  (0x7f << CCU_AON_DCXO_DIE_FREQ_OFFSET_SHIFT)
#define CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_SHIFT 20 /* R/W */
#define CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_MASK  (0x3 << CCU_AON_DCXO_ENHANCE_RFCLK_OUTV9_SHIFT)


#define CONFIG_HIF_BY_PASS              1  /* no sdio */

#define CONFIG_NO_WLAN_ROM              0

#define CONFIG_ACCESS_WIFI_DIRECT       1       /* this must be enabled if HIF bypass defined.*/
#define CONFIG_HIF_BYPASS_USE_DMA       0
#define HIF_BYPASS_USE_DMA_SIZE         512     /* if size smaller than the threshold, just use memcpy*/
#define CONFIG_WLAN_FUNC_CALL_SUPPORT   1

#define RESTORE_EFUSE_MAPPING_REG       0
#define CONFIG_FIRMWARE_USE_FLASH       0
#define CONFIG_FIRMWARE_USE_SRAM        0 //1
#if CONFIG_FIRMWARE_USE_SRAM
#define CONFIG_LOAD_FW_SIZE_NOT_CARE    1
#endif
#define CONFIG_VFY_TEST                 0
#if CONFIG_VFY_TEST
#define CONFIG_VFY_TEST_IN_AHB_RAM      0  /* Verify code in AHB RAM */
#define CONFIG_VFY_TEST_IN_PAS_RAM      1  /* Verify code in PAS RAM */
#define CONFIG_VFY_TEST_IN_APP_RAM      0  /* Verify code in APP RAM */

#define CONFIG_HIF_TEST_SDIO            0
#define CONFIG_WLAN_RAM_TEST            1
#define CONFIG_WLAN_FW_ROM_TEST         1  /*this test case may cause fw setup fail.*/
#define CONFIG_AHB_CONFLICT_TEST        0
#define CONFIG_WLAN_FW_RAM_TEST         0  /*this test case may cause fw setup fail.*/
#define CONFIG_HIF_BY_PASS_TEST         1
#define CONFIG_WLAN_SLEEP_TEST          0
#define CONFIG_HOST_SLEEP_TEST          0
#define CONFIG_SPIN_LOCK_TEST           1
#define CONFIG_RFAS_SEMP_TEST           1
#define CONFIG_APP_BUS_STRESS_TEST      1

#define VERIFY_FPGA_CONFIG              1

#if (CONFIG_WLAN_SLEEP_TEST)
#define CONFIG_WLAN_SELF_WAKEUP          1
#define CONFIG_WLAN_HOST_WAKEUP          1
#endif
#endif //CONFIG_VFY_TEST


#ifdef CONFIG_ETF_CLI
#elif defined CONFIG_ETF
/* firmware stored in buffer (eg. code/data segment) */
#define CONFIG_FIRMWARE_USE_BUFF        1

/* use reserved buffer for update firmware by uart */
#define CONFIG_FIRMWARE_USE_RESERVE_BUFF 1
#endif /* CONFIG_ETF */

/* enable it to dump efuse value which send to fw */
#define CONFIG_EFUSE_DUMP_EN                     0
#define CONFIG_WLAN_GET_POUT_FROM_EFUSE_EN       1

#define EFUSE_AREA_SIZE (256)
typedef enum _RW_EFUSE_STATUS_CODE {
	EFUSE_STATUS_CODE__SUCCESS		= 0x0,
	EFUSE_STATUS_CODE__DEFAULT		= 0x1,
	EFUSE_STATUS_CODE__CS_ERR		= 0x2,
	EFUSE_STATUS_CODE__PARSE_ERR	= 0x3,
	EFUSE_STATUS_CODE__RW_ERR		= 0x4,
	EFUSE_STATUS_CODE__DI_ERR		= 0x5,
	EFUSE_STATUS_CODE__NODATA_ERR		= 0x6,
	EFUSE_STATUS_CODE__HAVEWRITE_ERR	= 0x7,
	EFUSE_STATUS_CODE__NOTMATCH_ERR		= 0x8,
	EFUSE_STATUS_CODE__NOTSUPPORTNOW	= 0x9,
	EFUSE_STATUS_CODE__INVALID_PARAMS	= 0xA
} RW_EFUSE_STATUS_CODE;

struct wsm_sethardware_info;
struct xradio_common;

u8 xradio_get_efuse_area_in_use(void);
void xradio_set_efuse_area_in_use(u8 val);
u16 xradio_efuse_read_dcxo(u8 *freq_offset);
u16 xradio_efuse_write_dcxo(u8 *data);
u16 xradio_efuse_read_wlan_pout(u8 *pout);
u16 xradio_efuse_write_wlan_pout(u8 *data);
u16 xradio_efuse_read_wlan_mac(u8 *buf);
u16 xradio_efuse_write_wlan_mac(u8 *data);
int xradio_efuse_info(void *data);
int xradio_rw_efuse_init(void);
void xradio_rw_efuse_deinit(void);

void xradio_get_oshwinfo(struct wsm_sethardware_info *info);
int xradio_get_channel_fec_from_efuse(signed short *ch_1, signed short *ch_7, signed short *ch_13);

#if XRADIO_SDD_DIFF_HOSC_FREQ_FILE_MERGE
int xradio_sdd_update_frequency_ie(struct xradio_common *hw_priv, u8 *sdd_data, int sdd_len);
int xradio_hosc_frequency_get(struct xradio_common *hw_priv);
#endif
enum xr_chip_type xr_get_chip_type(void);

//#include "prcm/prcm-sun20iw2/prcm.h"
//PRCM_LFClkSrc XR_PRCM_GetWlanLFClockType(void);
//uint32_t XR_PRCM_GetWlanLFClock(void);
/////////
#else
#define CONFIG_HIF_BY_PASS              0  /* sdio */
#define CONFIG_WLAN_FUNC_CALL_SUPPORT   0
#endif
#endif /* XRADIO_PLAT_H_INCLUDED */
