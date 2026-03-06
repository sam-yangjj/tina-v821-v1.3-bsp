/*
 * Main code of XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*Linux version 3.4.0 compilation*/
#include <generated/uapi/linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <net/mac80211.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

#include "platform.h"
#include "xradio.h"
#include "txrx.h"
#include "sbus.h"
#include "fwio.h"
#include "hwio.h"
#include "bh.h"
#include "sta.h"
#include "ap.h"
#include "scan.h"
#include "pm.h"
#include "xr_version.h"
#if CONFIG_HIF_BY_PASS
#include "hifbypass.h"
#endif
#ifdef USE_RPMSG_IN_LINUX
#include "xr_rpmsg.h"
#endif
#if (CONFIG_WLAN_FUNC_CALL_SUPPORT)
#include "xr_func.h"
#endif

MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver core");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xradio_core");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

#ifdef XRADIO_P2P_SUP
#define P2P_FLAG "_P2P"
#else
#define P2P_FLAG
#endif

char *drv_version   = XRADIO_VERSION P2P_FLAG;
char *drv_buildtime = __DATE__" "__TIME__;

#define XRADIO_MAC_CHARLEN 18
#ifdef XRADIO_MACPARAM_HEX
/* insmod xradio_wlan.ko macaddr=0xDC,0x44,0x6D,0x00,0x00,0x00 */
static u8 xradio_macaddr_param[ETH_ALEN] = { 0x0 };

module_param_array_named(macaddr, xradio_macaddr_param, byte, NULL, S_IRUGO);
#else
/* insmod xradio_wlan.ko macaddr=xx:xx:xx:xx:xx:xx */
static char *xradio_macaddr_param;
module_param_named(macaddr, xradio_macaddr_param, charp, S_IRUGO);
#endif

MODULE_PARM_DESC(macaddr, "First MAC address");

#ifdef HW_RESTART
void xradio_restart_work(struct work_struct *work);
#endif

/* TODO: use rates and channels from the device */
#define RATETAB_ENT(_rate, _rateid, _flags)		\
	{						\
		.bitrate  = (_rate),    \
		.hw_value = (_rateid),  \
		.flags    = (_flags),   \
	}

static struct ieee80211_rate xradio_rates[] = {
	RATETAB_ENT(10,  0,   0),
	RATETAB_ENT(20,  1,   0),
	RATETAB_ENT(55,  2,   0),
	RATETAB_ENT(110, 3,   0),
	RATETAB_ENT(60,  6,  0),
	RATETAB_ENT(90,  7,  0),
	RATETAB_ENT(120, 8,  0),
	RATETAB_ENT(180, 9,  0),
	RATETAB_ENT(240, 10, 0),
	RATETAB_ENT(360, 11, 0),
	RATETAB_ENT(480, 12, 0),
	RATETAB_ENT(540, 13, 0),
};

static struct ieee80211_rate xradio_mcs_rates[] = {
	RATETAB_ENT(65,  14, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(130, 15, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(195, 16, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(260, 17, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(390, 18, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(520, 19, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(585, 20, IEEE80211_TX_RC_MCS),
	RATETAB_ENT(650, 21, IEEE80211_TX_RC_MCS),
};

#define xradio_g_rates      (xradio_rates + 0)
#define xradio_a_rates      (xradio_rates + 4)
#define xradio_n_rates      (xradio_mcs_rates)

#define xradio_g_rates_size (ARRAY_SIZE(xradio_rates))
#define xradio_a_rates_size (ARRAY_SIZE(xradio_rates) - 4)
#define xradio_n_rates_size (ARRAY_SIZE(xradio_mcs_rates))

#define CHAN2G(_channel, _freq, _flags) {   \
	.band             = NL80211_BAND_2GHZ,  \
	.center_freq      = (_freq),              \
	.hw_value         = (_channel),           \
	.flags            = (_flags),             \
	.max_antenna_gain = 0,                    \
	.max_power        = 30,                   \
}

#define CHAN5G(_channel, _flags) {   \
	.band             = NL80211_BAND_5GHZ,     \
	.center_freq      = 5000 + (5 * (_channel)), \
	.hw_value         = (_channel),              \
	.flags            = (_flags),                \
	.max_antenna_gain = 0,                       \
	.max_power        = 30,                      \
}

static struct ieee80211_channel xradio_2ghz_chantable[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

#ifdef CONFIG_XRADIO_5GHZ_SUPPORT
static struct ieee80211_channel xradio_5ghz_chantable[] = {
	CHAN5G(34, 0),		CHAN5G(36, 0),
	CHAN5G(38, 0),		CHAN5G(40, 0),
	CHAN5G(42, 0),		CHAN5G(44, 0),
	CHAN5G(46, 0),		CHAN5G(48, 0),
	CHAN5G(52, 0),		CHAN5G(56, 0),
	CHAN5G(60, 0),		CHAN5G(64, 0),
	CHAN5G(100, 0),		CHAN5G(104, 0),
	CHAN5G(108, 0),		CHAN5G(112, 0),
	CHAN5G(116, 0),		CHAN5G(120, 0),
	CHAN5G(124, 0),		CHAN5G(128, 0),
	CHAN5G(132, 0),		CHAN5G(136, 0),
	CHAN5G(140, 0),		CHAN5G(149, 0),
	CHAN5G(153, 0),		CHAN5G(157, 0),
	CHAN5G(161, 0),		CHAN5G(165, 0),
	CHAN5G(184, 0),		CHAN5G(188, 0),
	CHAN5G(192, 0),		CHAN5G(196, 0),
	CHAN5G(200, 0),		CHAN5G(204, 0),
	CHAN5G(208, 0),		CHAN5G(212, 0),
	CHAN5G(216, 0),
};
#endif /* CONFIG_XRADIO_5GHZ_SUPPORT */

static struct ieee80211_supported_band xradio_band_2ghz = {
	.channels = xradio_2ghz_chantable,
	.n_channels = ARRAY_SIZE(xradio_2ghz_chantable),
	.bitrates = xradio_g_rates,
	.n_bitrates = xradio_g_rates_size,
	.ht_cap = {
		.cap = IEEE80211_HT_CAP_GRN_FLD |
		       (1 << IEEE80211_HT_CAP_RX_STBC_SHIFT),
		.ht_supported  = 1,
		.ampdu_factor  = IEEE80211_HT_MAX_AMPDU_32K,
		.ampdu_density = IEEE80211_HT_MPDU_DENSITY_NONE,
		.mcs = {
			.rx_mask[0] = 0xFF,
			.rx_highest = __cpu_to_le16(0x41),
			.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
		},
	},
};

#ifdef CONFIG_XRADIO_5GHZ_SUPPORT
static struct ieee80211_supported_band xradio_band_5ghz = {
	.channels   = xradio_5ghz_chantable,
	.n_channels = ARRAY_SIZE(xradio_5ghz_chantable),
	.bitrates   = xradio_a_rates,
	.n_bitrates = xradio_a_rates_size,
	.ht_cap = {
		.cap = IEEE80211_HT_CAP_GRN_FLD |
		       (1 << IEEE80211_HT_CAP_RX_STBC_SHIFT),
		.ht_supported  = 1,
		.ampdu_factor  = IEEE80211_HT_MAX_AMPDU_8K,
		.ampdu_density = IEEE80211_HT_MPDU_DENSITY_NONE,
		.mcs = {
			.rx_mask[0] = 0xFF,
			.rx_highest = __cpu_to_le16(0x41),
			.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
		},
	},
};
#endif /* CONFIG_XRADIO_5GHZ_SUPPORT */

static const unsigned long xradio_ttl[] = {
	1 * HZ,	/* VO */
	2 * HZ,	/* VI */
	5 * HZ, /* BE */
	10 * HZ	/* BK */
};

static const struct ieee80211_ops xradio_ops = {
	.start             = xradio_start,
	.stop              = xradio_stop,
	.add_interface     = xradio_add_interface,
	.remove_interface  = xradio_remove_interface,
	.change_interface  = xradio_change_interface,
	.tx                = xradio_tx,
	.hw_scan           = xradio_hw_scan,
#ifdef ROAM_OFFLOAD
	.sched_scan_start  = xradio_hw_sched_scan_start,
	.sched_scan_stop   = xradio_hw_sched_scan_stop,
#endif /*ROAM_OFFLOAD*/
	.set_tim           = xradio_set_tim,
	.sta_notify        = xradio_sta_notify,
	.sta_add           = xradio_sta_add,
	.sta_remove        = xradio_sta_remove,
	.set_key           = xradio_set_key,
	.set_rts_threshold = xradio_set_rts_threshold,
	.config            = xradio_config,
	.bss_info_changed  = xradio_bss_info_changed,
	.prepare_multicast = xradio_prepare_multicast,
	.configure_filter  = xradio_configure_filter,
	.conf_tx           = xradio_conf_tx,
	.get_stats         = xradio_get_stats,
	.ampdu_action      = xradio_ampdu_action,
	.flush             = xradio_flush,
#ifdef SUPPORT_ACS
	.get_survey	   = xradio_get_survey,
#endif
#ifdef CONFIG_PM
	.suspend           = xradio_wow_suspend,
	.resume            = xradio_wow_resume,
#endif /* CONFIG_PM */
	/* Intentionally not offloaded:					*/
	/*.channel_switch	 = xradio_channel_switch,		*/
	.remain_on_channel = xradio_remain_on_channel,
	.cancel_remain_on_channel = xradio_cancel_remain_on_channel,
#ifdef IPV6_FILTERING
	//.set_data_filter   = xradio_set_data_filter,
#endif /*IPV6_FILTERING*/
#ifdef CONFIG_XRADIO_TESTMODE
	.testmode_cmd      = xradio_testmode_cmd,
#endif /* CONFIG_XRADIO_TESTMODE */
	.change_mac	   = xradio_change_mac,
};

struct xradio_common *g_hw_priv;

#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
static const struct wiphy_wowlan_support xradio_wowlan = {
	.flags = WIPHY_WOWLAN_ANY | WIPHY_WOWLAN_DISCONNECT,
};
#endif
#endif
/**************************** functions **********************************/
void xradio_version_show(void)
{
/* Show XRADIO version and compile time */
	xradio_dbg(XRADIO_DBG_ALWY, "Driver Label:%s  %s\n",
		   DRV_VERSION, DRV_BUILDTIME);

/************* Linux Kernel config *************/
#ifdef CONFIG_XRADIO_NON_POWER_OF_TWO_BLOCKSIZES
	xradio_dbg(XRADIO_DBG_NIY,
		  "[CONFIG_XRADIO_NON_POWER_OF_TWO_BLOCKSIZES]\n");
#endif

#ifdef CONFIG_XRADIO_USE_GPIO_IRQ
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_USE_GPIO_IRQ]\n");
#endif

#ifdef CONFIG_XRADIO_5GHZ_SUPPORT
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_5GHZ_SUPPORT]\n");
#endif

#ifdef CONFIG_XRADIO_WAPI_SUPPORT
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_WAPI_SUPPORT]\n");
#endif

#ifdef CONFIG_XRADIO_USE_EXTENSIONS
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_USE_EXTENSIONS]\n");
#endif

#ifdef CONFIG_PM
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_PM]\n");
#endif

#ifdef CONFIG_XRADIO_SDIO
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_SDIO]\n");
#endif

#ifdef CONFIG_XRADIO_DUMP_ON_ERROR
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_DUMP_ON_ERROR]\n");
#endif

#ifdef CONFIG_XRADIO_DEBUGFS
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_DEBUGFS]\n");
#endif

#ifdef CONFIG_XRADIO_ITP
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_ITP]\n");
#endif

#ifdef CONFIG_XRADIO_TESTMODE
	xradio_dbg(XRADIO_DBG_NIY, "[CONFIG_XRADIO_TESTMODE]\n");
#endif

/************ XRADIO Make File config ************/
#ifdef P2P_MULTIVIF
	xradio_dbg(XRADIO_DBG_NIY, "[P2P_MULTIVIF]\n");
#endif

#ifdef MCAST_FWDING
	xradio_dbg(XRADIO_DBG_NIY, "[MCAST_FWDING]\n");
#endif

#ifdef XRADIO_SUSPEND_RESUME_FILTER_ENABLE
	xradio_dbg(XRADIO_DBG_NIY, "[XRADIO_SUSPEND_RESUME_FILTER_ENABLE]\n");
#endif

#ifdef AP_AGGREGATE_FW_FIX
	xradio_dbg(XRADIO_DBG_NIY, "[AP_AGGREGATE_FW_FIX]\n");
#endif

#ifdef AP_HT_CAP_UPDATE
	xradio_dbg(XRADIO_DBG_NIY, "[AP_HT_CAP_UPDATE]\n");
#endif

#ifdef PROBE_RESP_EXTRA_IE
	xradio_dbg(XRADIO_DBG_NIY, "[PROBE_RESP_EXTRA_IE]\n");
#endif

#ifdef IPV6_FILTERING
	xradio_dbg(XRADIO_DBG_NIY, "[IPV6_FILTERING]\n");
#endif

#ifdef ROAM_OFFLOAD
	xradio_dbg(XRADIO_DBG_NIY, "[ROAM_OFFLOAD]\n");
#endif

#ifdef TES_P2P_0002_ROC_RESTART
	xradio_dbg(XRADIO_DBG_NIY, "[TES_P2P_0002_ROC_RESTART]\n");
#endif

#ifdef TES_P2P_000B_EXTEND_INACTIVITY_CNT
	xradio_dbg(XRADIO_DBG_NIY, "[TES_P2P_000B_EXTEND_INACTIVITY_CNT]\n");
#endif

#ifdef TES_P2P_000B_DISABLE_EAPOL_FILTER
	xradio_dbg(XRADIO_DBG_NIY, "[TES_P2P_000B_DISABLE_EAPOL_FILTER]\n");
#endif

#ifdef HAS_PUT_TASK_STRUCT
	xradio_dbg(XRADIO_DBG_NIY, "[HAS_PUT_TASK_STRUCT]\n");
#endif

/************* XRADIO.h config *************/
#ifdef HIDDEN_SSID
	xradio_dbg(XRADIO_DBG_NIY, "[HIDDEN_SSID]\n");
#endif

#ifdef ROC_DEBUG
	xradio_dbg(XRADIO_DBG_NIY, "[ROC_DEBUG]\n");
#endif

#ifdef XRADIO_RRM
	xradio_dbg(XRADIO_DBG_NIY, "[XRADIO_RRM]\n");
#endif
}

/* return 0: failed*/
static inline int xradio_macaddr_val2char(char *c_mac, const u8 *v_mac)
{
	SYS_BUG(!v_mac || !c_mac);
	return sprintf(c_mac, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		       v_mac[0], v_mac[1], v_mac[2],
		       v_mac[3], v_mac[4], v_mac[5]);
}

#ifndef XRADIO_MACPARAM_HEX
static int xradio_macaddr_char2val(u8 *v_mac, const char *c_mac)
{
	int i = 0;
	const char *tmp_char = c_mac;
	SYS_BUG(!v_mac || !c_mac);

	for (i = 0; i < ETH_ALEN; i++) {
		if (*tmp_char != 0) {
			v_mac[i] = simple_strtoul(tmp_char, (char **)&tmp_char, 16);
		} else {
			xradio_dbg(XRADIO_DBG_ERROR, "%s, Len Error\n", __func__);
			return -1;
		}
		if (i < ETH_ALEN - 1 && *tmp_char != ':') {
			xradio_dbg(XRADIO_DBG_ERROR, "%s, Format or Len Error\n",
				   __func__);
			return -1;
		}
		tmp_char++;
	}
	return 0;
}
#endif

#define MACADDR_VAILID(a) ( \
!(a[0] & 0x3) && \
(a[0] != 0 || a[1] != 0 ||  \
 a[2] != 0 || a[3] != 0 ||  \
 a[4] != 0 || a[5] != 0))

#if IS_ENABLED(CONFIG_AW_MACADDR_MGT) || IS_ENABLED(CONFIG_SUNXI_ADDR_MGT)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern int get_custom_mac_address(int fmt, char *name, char *addr);

static int xradio_get_mac_addrs(u8 *macaddr)
{
	int ret = 0;
	SYS_BUG(!macaddr);
	/* Check mac addrs param, if exsist, use it first.*/
#ifdef XRADIO_MACPARAM_HEX
	memcpy(macaddr, xradio_macaddr_param, ETH_ALEN);
#else
	if (xradio_macaddr_param) {
		ret = xradio_macaddr_char2val(macaddr, xradio_macaddr_param);
	}
#endif

	if (ret == 0 && MACADDR_VAILID(macaddr))
		return 0;

	ret = get_custom_mac_address(1, "wifi", macaddr);
	if (ret > 0)
		return 0;

	return ret;
}
#else
static int xradio_get_mac_addrs(u8 *macaddr)
{
	return 0;
}
#endif
#endif

#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
static volatile u8 fw_txrxbuf_bt_present;
static volatile u8 fw_txrxbuf_wifi_stat;
static volatile u8 fw_txrxbuf_switch_disable;

int xradio_device_wakeup(struct xradio_common *hw_priv);
static void xradio_fw_txrxbuf_check_switch(void);

#define FW_TXRXBUF_WF_STAT_WF_ON     (1 << 0)
#define FW_TXRXBUF_WF_STAT_PWR_ON    (1 << 1)
#define FW_TXRXBUF_WF_STAT_FW_RDY    (1 << 2)
#define FW_TXRXBUF_WF_STAT_MASK      (FW_TXRXBUF_WF_STAT_WF_ON | FW_TXRXBUF_WF_STAT_PWR_ON | FW_TXRXBUF_WF_STAT_FW_RDY)

#define FW_TXRXBUF_SET_WF_STAT(stat) do {  \
	fw_txrxbuf_wifi_stat |= stat;          \
	if (stat & FW_TXRXBUF_WF_STAT_FW_RDY)  \
		xradio_fw_txrxbuf_check_switch();  \
	} while (0)
#define FW_TXRXBUF_CLR_WF_STAT(stat) {fw_txrxbuf_wifi_stat &= ~(stat); }


void xradio_fw_txrxbuf_set_disable(u8 disable)
{
	fw_txrxbuf_switch_disable = disable;
}

u8 xradio_fw_txrxbuf_is_enable(void)
{
	return (!fw_txrxbuf_switch_disable);
}

u8 xradio_fw_txrxbuf_get_bt_stat(void)
{
	return fw_txrxbuf_bt_present;
}

static void xradio_fw_txrxbuf_set_bt_stat(u8 stat)
{
	fw_txrxbuf_bt_present = stat;
}

static int xradio_wlan_update_bt_stat(u8 stat)
{
	xradio_fw_txrxbuf_set_bt_stat(stat);
	xradio_fw_txrxbuf_check_switch();

	return 0;
}

int xradio_wlan_switch_epta_stat(uint8_t release_num, uint8_t force_buf)
{
	if (get_etf_mode()) {
		return -1;
	}

	if (force_buf) {
		switch (force_buf) {
		case XR_WF_SW_FW_BUF_FORCE_SMALL_BUF:
			xradio_wlan_update_bt_stat(1);
			break;
		case XR_WF_SW_FW_BUF_FORCE_BIG_BUF:
			xradio_wlan_update_bt_stat(0);
			break;
		case XR_WF_SW_FW_BUF_FORCE_DISABLE:
			xradio_fw_txrxbuf_set_disable(1);
			break;
		case XR_WF_SW_FW_BUF_FORCE_ENABLE:
			xradio_fw_txrxbuf_set_disable(0);
			break;
		default:
			xradio_dbg(XRADIO_DBG_ALWY, "[SW_BUF] force_buf:%d type error\n", force_buf);
		}
	} else {
		if (release_num >= 2)
			xradio_wlan_update_bt_stat(1);
		else
			xradio_wlan_update_bt_stat(0);
	}

	return 0;
}

static void xradio_fw_txrxbuf_check_switch(void)
{
	if (fw_txrxbuf_wifi_stat & FW_TXRXBUF_WF_STAT_FW_RDY) {
		struct xradio_common *hw_priv = g_hw_priv;

		if (hw_priv->workqueue)
			queue_delayed_work(hw_priv->workqueue, &hw_priv->fw_txrxbuf_switch_buf_work, 0);
		// wifi fw should enbale bt in EPTA default, so bt no need wait wifi switch over!
	}
}

static void xradio_fw_txrxbuf_switch_buf_work(struct work_struct *work)
{
	struct xradio_common *hw_priv = container_of(work, struct xradio_common, fw_txrxbuf_switch_buf_work.work);
	u32 big_buf = xradio_fw_txrxbuf_get_bt_stat() ? 0 : 1;
	u32 fw_is_big_buf = 0;
	int ret;
	u8 lock_data_tx, lock_cmd_tx;
	long timeout = WSM_CMD_LOCK_TIMEOUT / 5 * 3;

	if (fw_txrxbuf_switch_disable) {
		xradio_dbg(XRADIO_DBG_ALWY, "[SW_BUF] already disable func\n");
		return;
	}
	if ((hw_priv->fw_txrxbuf_epta_sec_start < FW_TXRXBUF_EPTA_SECTION_RANG_START) ||
		(hw_priv->fw_txrxbuf_epta_sec_start > FW_TXRXBUF_EPTA_SECTION_RANG_END) ||
		(hw_priv->fw_txrxbuf_epta_sec_end < FW_TXRXBUF_EPTA_SECTION_RANG_START) ||
		(hw_priv->fw_txrxbuf_epta_sec_end > FW_TXRXBUF_EPTA_SECTION_RANG_END)) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s, epta range err: start:0x%x end:0x%x\n",
			__func__, hw_priv->fw_txrxbuf_epta_sec_start, hw_priv->fw_txrxbuf_epta_sec_end);
		return;
	}
	if (hw_priv->bh_error)
		return;

	xradio_dbg(XRADIO_DBG_ALWY, "[SW_BUF] switch start\n");

	ret = wsm_get_switch_fw_txrx_buf(hw_priv, &fw_is_big_buf);
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s get fw buf stat fail %d\n", __func__, ret);
		return;
	}
	if (fw_is_big_buf == big_buf) {
		xradio_dbg(XRADIO_DBG_ALWY, "[SW_BUF] want %d, fw:%d, no need switch\n", big_buf, fw_is_big_buf);
		return;
	}

	hw_priv->fw_txrxbuf_switch_buf_status = FW_TXRXBUF_SWITCH_BUF_ST_HOST_SWING;
	wsm_lock_tx_async(hw_priv);
	lock_data_tx = 1;
	xradio_dbg(XRADIO_DBG_MSG, "%s locked tx\n", __func__);
	xradio_dbg(XRADIO_DBG_MSG, "%s set switch fw txrx buf\n", __func__);
	ret = wsm_set_switch_fw_txrx_buf(hw_priv, &big_buf, 0);
	lock_cmd_tx = 1;
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s, fw switch buffer cmd fail %d\n", __func__, ret);
		hw_priv->bh_error = 1;
#ifdef BH_USE_SEMAPHORE
		up(&hw_priv->bh_sem);
#else
		wake_up(&hw_priv->bh_wq);
#endif
		goto unlock_cmd;
	}

	xradio_dbg(XRADIO_DBG_MSG, "%s wait fw switch buffer...\n", __func__);
	if (down_timeout(&hw_priv->fw_txrxbuf_switch_buf_wq, timeout)) {
		xradio_dbg(XRADIO_DBG_WARN, "%s wait fw switch buffer overtime\n", __func__);
	}
	xradio_dbg(XRADIO_DBG_MSG, "%s wait fw switch buffer over\n", __func__);
	if (hw_priv->fw_txrxbuf_switch_buf_status) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s, fw switch buffer fail, status:%d\n",
			__func__, hw_priv->fw_txrxbuf_switch_buf_status);
		hw_priv->bh_error = 1;
#ifdef BH_USE_SEMAPHORE
		up(&hw_priv->bh_sem);
#else
		wake_up(&hw_priv->bh_wq);
#endif
		goto unlock_cmd;
	}

	if (hw_priv->hw_bufs_used > 0) {
		// fw flushed tx, so wait bh process tx confirm
		while (hw_priv->hw_bufs_used > 0) {
			xradio_dbg(XRADIO_DBG_MSG, "%s %d bufs_used:%d\n", __func__, __LINE__, hw_priv->hw_bufs_used);
			if (hw_priv->fw_txrxbuf_switch_buf_work_try_cnt++ < FW_TXRXBUF_SWITCH_BUF_TRY_MAX) {
				msleep(5);
			} else if (hw_priv->hw_bufs_used > 0) {
				xradio_dbg(XRADIO_DBG_ERROR, "%s, tx buf not clear %d try:%d\n", __func__, hw_priv->hw_bufs_used,
					hw_priv->fw_txrxbuf_switch_buf_work_try_cnt);
				hw_priv->bh_error = 1;
#ifdef BH_USE_SEMAPHORE
				up(&hw_priv->bh_sem);
#else
				wake_up(&hw_priv->bh_wq);
#endif
				goto unlock_cmd;
			}
		}
	}

	// load EPTA
	if (!big_buf) {
		u32 epta_en = 1;

		atomic_set(&hw_priv->fw_txrxbuf_switch_buf_lock_cmd_tx, 0);
		lock_cmd_tx = 0;
		//load epta
		xradio_dbg(XRADIO_DBG_MSG, "%s locked cmd tx\n", __func__);
		down(&hw_priv->device_wkup_lock);
		if (hw_priv->device_can_sleep) {
			ret = xradio_device_wakeup(hw_priv);
			if (ret < 0) { /* Error in wakeup */
				up(&hw_priv->device_wkup_lock);
				xradio_dbg(XRADIO_DBG_ERROR, "%s,%d wakeup fw fail, ret:%x\n", __func__, __LINE__, ret);
				hw_priv->bh_error = 1;
#ifdef BH_USE_SEMAPHORE
				up(&hw_priv->bh_sem);
#else
				wake_up(&hw_priv->bh_wq);
#endif
				goto unlock_tx;
			} else if (ret > 0) {
				hw_priv->device_can_sleep = false;
			}
		}
		up(&hw_priv->device_wkup_lock);
		hw_priv->fw_txrxbuf_switch_buf_ps_active = 1;
		ret = xradio_fw_txrxbuf_load_epta_section(hw_priv);
		hw_priv->fw_txrxbuf_switch_buf_ps_active = 0;
		xradio_dbg(XRADIO_DBG_MSG, "%s load epta section over, ret:%d\n", __func__, ret);
		if (!ret)
			ret = wsm_set_epta_config(hw_priv, &epta_en, 0);
		if (ret) {
			xradio_dbg(XRADIO_DBG_ERROR, "%s, fw epta start fail, ret:%d\n", __func__, ret);
			hw_priv->bh_error = 1;
#ifdef BH_USE_SEMAPHORE
			up(&hw_priv->bh_sem);
#else
			wake_up(&hw_priv->bh_wq);
#endif
			goto unlock_tx;
		}
	}

	hw_priv->fw_txrxbuf_switch_buf_work_try_cnt = 0;
	xradio_dbg(XRADIO_DBG_ALWY, "[SW_BUF] switch Over!\n");
unlock_cmd:
	// unlock cmd
	if (lock_cmd_tx)
		atomic_set(&hw_priv->fw_txrxbuf_switch_buf_lock_cmd_tx, 0);
unlock_tx:
	if (lock_data_tx)
		wsm_unlock_tx(hw_priv);
	hw_priv->fw_txrxbuf_switch_buf_status = 0;
}
#else
int xradio_wlan_switch_epta_stat(uint8_t release_num, uint8_t force_buf)
{
	return -1;
}
#endif

static void xradio_set_mac_addrs(struct xradio_common *hw_priv)
{
	int ret;
	u8 *macaddr = (u8 *)&hw_priv->addresses[0];
	u8 *chip_addr = &hw_priv->wsm_caps.efuse_mac[0];

	/*1.First get MAC address from system.*/
	ret = xradio_get_mac_addrs(macaddr);
	if (ret == 0 && MACADDR_VAILID(macaddr)) {
		xradio_dbg(XRADIO_DBG_NIY, "Use System MAC address!\n");
		goto exit;
	}

	/*2.Then get Chip MAC address if system addr invalid */
	if (MACADDR_VAILID(chip_addr)) {
		memcpy(macaddr, chip_addr, ETH_ALEN);
		xradio_dbg(XRADIO_DBG_NIY, "Use Chip MAC address!\n");
		goto exit;
	}

	/*3.Use random MAC address, but don't try to save it in file. */
	get_random_bytes(macaddr, ETH_ALEN);
	macaddr[0] &= 0xFC;
	if (MACADDR_VAILID(macaddr)) {
		xradio_dbg(XRADIO_DBG_NIY, "Use Random MAC address!\n");
	} else {
		xradio_dbg(XRADIO_DBG_WARN, "Use AW OUI MAC address!\n");
		macaddr[0] = 0xDC;
		macaddr[1] = 0x44;
		macaddr[2] = 0x6D;
	}

exit:
	memcpy(hw_priv->addresses[1].addr, hw_priv->addresses[0].addr, ETH_ALEN);
	hw_priv->addresses[1].addr[5] += 0x01;
#ifdef P2P_MULTIVIF
	memcpy(hw_priv->addresses[2].addr, hw_priv->addresses[1].addr, ETH_ALEN);
	hw_priv->addresses[2].addr[4] ^= 0x80;
#endif
	xradio_dbg(XRADIO_DBG_NIY, "MACADDR=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   macaddr[0], macaddr[1], macaddr[2],
		   macaddr[3], macaddr[4], macaddr[5]);
}


static void xradio_set_ifce_comb(struct xradio_common *hw_priv,
				 struct ieee80211_hw *hw)
{
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);
#ifdef P2P_MULTIVIF
	hw_priv->if_limits1[0].max = 2;
#else
	hw_priv->if_limits1[0].max = 1;
#endif

	hw_priv->if_limits1[0].types = BIT(NL80211_IFTYPE_STATION);
	hw_priv->if_limits1[1].max = 1;
	hw_priv->if_limits1[1].types = BIT(NL80211_IFTYPE_AP);

#ifdef P2P_MULTIVIF
	hw_priv->if_limits2[0].max = 3;
#else
	hw_priv->if_limits2[0].max = 2;
#endif
	hw_priv->if_limits2[0].types = BIT(NL80211_IFTYPE_STATION);

#ifdef P2P_MULTIVIF
	hw_priv->if_limits3[0].max = 2;
#else
	hw_priv->if_limits3[0].max = 1;
#endif

	hw_priv->if_limits3[0].types = BIT(NL80211_IFTYPE_STATION);
	hw_priv->if_limits3[1].max = 1;
	hw_priv->if_limits3[1].types = BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO);
	hw_priv->if_limits3[2].max = 1;
	hw_priv->if_limits3[2].types = BIT(NL80211_IFTYPE_P2P_DEVICE);

	/* TODO:COMBO: mac80211 doesn't yet support more than 1
	 * different channel */
	hw_priv->if_combs[0].num_different_channels = 1;
#ifdef P2P_MULTIVIF
	hw_priv->if_combs[0].max_interfaces = 3;
#else
	hw_priv->if_combs[0].max_interfaces = 2;
#endif
	hw_priv->if_combs[0].limits = hw_priv->if_limits1;
	hw_priv->if_combs[0].n_limits = 2;

	hw_priv->if_combs[1].num_different_channels = 1;

#ifdef P2P_MULTIVIF
	hw_priv->if_combs[1].max_interfaces = 3;
#else
	hw_priv->if_combs[1].max_interfaces = 2;
#endif
	hw_priv->if_combs[1].limits = hw_priv->if_limits2;
	hw_priv->if_combs[1].n_limits = 1;

	hw_priv->if_combs[2].num_different_channels = 1;
#ifdef P2P_MULTIVIF
	hw_priv->if_combs[2].max_interfaces = 3;
#else
	hw_priv->if_combs[2].max_interfaces = 2;
#endif
	hw_priv->if_combs[2].limits = hw_priv->if_limits3;
	hw_priv->if_combs[2].n_limits = 3;

	hw->wiphy->iface_combinations = &hw_priv->if_combs[0];
	hw->wiphy->n_iface_combinations = 3;
}

static int xradio_device_init(struct xradio_common *hw_priv)
{
	int ret = 0;
	SYS_BUG(!hw_priv);
	/* Add pm device. */
	hw_priv->plat_device = platform_device_alloc(XRADIO_PLAT_DEVICE, 0);
	if (!hw_priv->plat_device) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:platform_device_alloc failed!\n", __func__);
		return -ENOMEM;
	}
	hw_priv->plat_device->dev.platform_data = hw_priv;
	ret = platform_device_add(hw_priv->plat_device);
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:platform_device_add failed(%d)!\n", __func__, ret);
		kfree(hw_priv->plat_device);
		hw_priv->plat_device = NULL;
		return ret;
	}

	return 0;

}

static void xradio_device_deinit(struct xradio_common *hw_priv)
{
	if (hw_priv->plat_device) {
		hw_priv->plat_device->dev.platform_data = NULL;
		/* kfree is already do in platform_device_unregister.*/
		platform_device_unregister(hw_priv->plat_device);
		hw_priv->plat_device = NULL;
		hw_priv->pdev = NULL;
	} else {
		xradio_dbg(XRADIO_DBG_WARN, "%s:hw_priv->plat_device is NULL!\n",
				__func__);
	}
}

struct ieee80211_hw *xradio_init_common(size_t hw_priv_data_len)
{
	int i;
	struct ieee80211_hw *hw;
	struct xradio_common *hw_priv;
	struct ieee80211_supported_band *sband;
	int band;

	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);

	/* Alloc ieee_802.11 hw and xradio_common struct. */
	hw = mac80211_alloc_hw(hw_priv_data_len, &xradio_ops);
	if (!hw)
		return NULL;
	hw_priv = hw->priv;
	xradio_dbg(XRADIO_DBG_ALWY, "Allocated hw_priv @ %p\n", hw_priv);
	xradio_dbg(XRADIO_DBG_MSG, "xradio_common_size=%zu, vif_data_size=%zu\n",
			sizeof(struct xradio_common), sizeof(struct xradio_vif));
	memset(hw_priv, 0, sizeof(*hw_priv));

	/* Initialize members of hw_priv. */
	hw_priv->hw = hw;
	hw_priv->if_id_slot = 0;
	hw_priv->roc_if_id = -1;
	atomic_set(&hw_priv->num_vifs, 0);
	/* initial rates and channels TODO: fetch from FW */
	hw_priv->rates = xradio_rates;
	hw_priv->mcs_rates = xradio_n_rates;
#ifdef ROAM_OFFLOAD
	hw_priv->auto_scanning = 0;
	hw_priv->frame_rcvd = 0;
	hw_priv->num_scanchannels = 0;
	hw_priv->num_2g_channels = 0;
	hw_priv->num_5g_channels = 0;
#endif /*ROAM_OFFLOAD*/
#ifdef AP_AGGREGATE_FW_FIX
	/* Enable block ACK for 4 TID (BE,VI,VI,VO). */
	hw_priv->ba_tid_mask = 0xB1;  /*due to HW limitations*/
#else
	/* Enable block ACK for every TID but voice. */
	hw_priv->ba_tid_mask = 0x3F;
#endif
	hw_priv->noise = -94;
	/* hw_priv->beacon_req_id = cpu_to_le32(0); */
	hw_priv->short_frame_max_tx_count = 15;

	/* Initialize members of ieee80211_hw, it works in UMAC. */
	hw->sta_data_size = sizeof(struct xradio_sta_priv);
	hw->vif_data_size = sizeof(struct xradio_vif);

	ieee80211_hw_set(hw, SIGNAL_DBM);
	ieee80211_hw_set(hw, SUPPORTS_PS);
	ieee80211_hw_set(hw, SUPPORTS_DYNAMIC_PS);
	ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
	ieee80211_hw_set(hw, CONNECTION_MONITOR);
	ieee80211_hw_set(hw, MFP_CAPABLE);
#if 0
	hw->flags = IEEE80211_HW_SIGNAL_DBM            |
		    IEEE80211_HW_SUPPORTS_PS           |
		    IEEE80211_HW_SUPPORTS_DYNAMIC_PS   |
		    IEEE80211_HW_REPORTS_TX_ACK_STATUS |
		    IEEE80211_HW_SUPPORTS_UAPSD        |
		    IEEE80211_HW_CONNECTION_MONITOR    |
		    IEEE80211_HW_SUPPORTS_CQM_RSSI     |
		    IEEE80211_HW_MFP_CAPABLE		   |
		    /* Aggregation is fully controlled by firmware.
		     * Do not need any support from the mac80211 stack */
		    /* IEEE80211_HW_AMPDU_AGGREGATION  | */
#if defined(CONFIG_XRADIO_USE_EXTENSIONS)
		    IEEE80211_HW_SUPPORTS_P2P_PS          |
		    IEEE80211_HW_SUPPORTS_CQM_BEACON_MISS |
		    IEEE80211_HW_SUPPORTS_CQM_TX_FAIL     |
#endif /* CONFIG_XRADIO_USE_EXTENSIONS */
		    IEEE80211_HW_BEACON_FILTER;
#endif
	hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION)    |
				     BIT(NL80211_IFTYPE_ADHOC)      |
				     BIT(NL80211_IFTYPE_AP)         |
				     BIT(NL80211_IFTYPE_MESH_POINT) |
				     BIT(NL80211_IFTYPE_P2P_CLIENT) |
				     BIT(NL80211_IFTYPE_P2P_GO)     |
				     BIT(NL80211_IFTYPE_P2P_DEVICE);

	/* Support only for limited wowlan functionalities */
#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	hw->wiphy->wowlan = &xradio_wowlan;
#else
	hw->wiphy->wowlan.flags = WIPHY_WOWLAN_ANY | WIPHY_WOWLAN_DISCONNECT;
	hw->wiphy->wowlan.n_patterns = 0;
#endif
#endif

#if defined(CONFIG_XRADIO_USE_EXTENSIONS)
#ifndef XRADIO_P2P_SUP
	/* Comment out this because 821 P2P does not fully support UAPSD.
	 * Enabling it will cause the connection to phones that support UAPSD to fail,
	 * such as OPPO K7.
	 */
	hw->wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
#endif
#endif /* CONFIG_XRADIO_USE_EXTENSIONS */

#if defined(CONFIG_XRADIO_DISABLE_BEACON_HINTS)
	hw->wiphy->flags |= WIPHY_FLAG_DISABLE_BEACON_HINTS;
#endif
	hw->wiphy->n_addresses = XRWL_MAX_VIFS;
	hw->wiphy->addresses   = hw_priv->addresses;
	/*
	 * max_remain_on_channel_duration will bigger than 500 in
	 * wpa_supplicant v2.3, change to 2500 and default value=5000 in umac.
	 */
	hw->wiphy->max_remain_on_channel_duration = 2500;
	//hw->channel_change_time = 500;	/* TODO: find actual value */
	hw->extra_tx_headroom = WSM_TX_EXTRA_HEADROOM +
				8  /* TKIP IV */      +
				12 /* TKIP ICV and MIC */;
	hw->wiphy->bands[NL80211_BAND_2GHZ] = &xradio_band_2ghz;
#ifdef CONFIG_XRADIO_5GHZ_SUPPORT
	hw->wiphy->bands[NL80211_BAND_5GHZ] = &xradio_band_5ghz;
#endif /* CONFIG_XRADIO_5GHZ_SUPPORT */
	hw->queues         = AC_QUEUE_NUM;
	hw->max_rates      = MAX_RATES_STAGE;
	hw->max_rate_tries = MAX_RATES_RETRY;
	/* Channel params have to be cleared before registering wiphy again */
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = hw->wiphy->bands[band];
		if (!sband)
			continue;
		for (i = 0; i < sband->n_channels; i++) {
			sband->channels[i].flags = 0;
			sband->channels[i].max_antenna_gain = 0;
			sband->channels[i].max_power = 30;
		}
	}
	/*
	 * hw_priv->channel init value is the local->oper_channel init value;
	 *  when transplanting,take care.
	 */
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = hw->wiphy->bands[band];
		if (!sband)
			continue;
		if (!hw_priv->channel) {
			hw_priv->channel = &sband->channels[2];
		}
	}
	hw->wiphy->max_scan_ssids = WSM_SCAN_MAX_NUM_OF_SSIDS;
	hw->wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;
	SET_IEEE80211_PERM_ADDR(hw, hw_priv->addresses[0].addr);

	/* Initialize locks. */
	spin_lock_init(&hw_priv->vif_list_lock);
	sema_init(&hw_priv->wsm_cmd_sema, 1);
	sema_init(&hw_priv->conf_lock, 1);
	sema_init(&hw_priv->wsm_oper_lock, 1);
	atomic_set(&hw_priv->tx_lock, 0);
	sema_init(&hw_priv->tx_lock_sem, 1);
	sema_init(&hw_priv->dtor_lock, 1);
	sema_init(&hw_priv->device_wkup_lock, 1);

	hw_priv->workqueue = create_singlethread_workqueue(XRADIO_WORKQUEUE);
	hw_priv->spare_workqueue = create_singlethread_workqueue(XRADIO_SPARE_WORKQUEUE);
	sema_init(&hw_priv->scan.lock, 1);
	sema_init(&hw_priv->scan.status_lock, 1);
	INIT_WORK(&hw_priv->scan.work, xradio_scan_work);
#ifdef ROAM_OFFLOAD
	INIT_WORK(&hw_priv->scan.swork, xradio_sched_scan_work);
#endif /*ROAM_OFFLOAD*/
	INIT_DELAYED_WORK(&hw_priv->scan.probe_work, xradio_probe_work);
	INIT_DELAYED_WORK(&hw_priv->scan.timeout, xradio_scan_timeout);

#ifdef SCAN_FAILED_WORKAROUND_OF_FW_EXCEPTION
	hw_priv->scan.scan_failed_cnt = 0;
#endif

	INIT_DELAYED_WORK(&hw_priv->rem_chan_timeout, xradio_rem_chan_timeout);
	INIT_WORK(&hw_priv->tx_policy_upload_work, tx_policy_upload_work);
	atomic_set(&hw_priv->upload_count, 0);
	for (i = 0; i < (XRWL_MAX_VIFS-1); ++i) {
		hw_priv->scan_delay_status[i] = XRADIO_SCAN_ALLOW;
		hw_priv->scan_delay_time[i] = 0;
	}

#ifdef MONITOR_RX_FIX
	INIT_WORK(&hw_priv->monitor_rx_restart_work, xradio_monitor_rx_restart_work);
#endif

	spin_lock_init(&hw_priv->event_queue_lock);
	INIT_LIST_HEAD(&hw_priv->event_queue);
	INIT_WORK(&hw_priv->event_handler, xradio_event_handler);
	INIT_WORK(&hw_priv->ba_work, xradio_ba_work);
	spin_lock_init(&hw_priv->ba_lock);
	timer_setup(&hw_priv->ba_timer, xradio_ba_timer, 0);

#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	INIT_DELAYED_WORK(&hw_priv->fw_txrxbuf_switch_buf_work, xradio_fw_txrxbuf_switch_buf_work);
	sema_init(&hw_priv->fw_txrxbuf_switch_buf_wq, 0);
	atomic_set(&hw_priv->fw_txrxbuf_switch_buf_lock_cmd_tx, 0);
#endif

	if (unlikely(xradio_queue_stats_init(&hw_priv->tx_queue_stats,
			WLAN_LINK_ID_MAX, xradio_skb_dtor, hw_priv))) {
		mac80211_free_hw(hw);
		return NULL;
	}
	for (i = 0; i < AC_QUEUE_NUM; ++i) {
		if (unlikely(xradio_queue_init(&hw_priv->tx_queue[i],
				     &hw_priv->tx_queue_stats, i,
				     XRWL_MAX_QUEUE_SZ, xradio_ttl[i]))) {
			for (; i > 0; i--)
				xradio_queue_deinit(&hw_priv->tx_queue[i - 1]);
			xradio_queue_stats_deinit(&hw_priv->tx_queue_stats);
			mac80211_free_hw(hw);
			return NULL;
		}
	}

	init_waitqueue_head(&hw_priv->channel_switch_done);
	init_waitqueue_head(&hw_priv->wsm_cmd_wq);
	init_waitqueue_head(&hw_priv->wsm_wakeup_done);
	init_waitqueue_head(&hw_priv->wsm_startup_done);
	init_waitqueue_head(&hw_priv->offchannel_wq);
	hw_priv->wsm_caps.firmwareReady = 0;
	hw_priv->driver_ready = 0;
	hw_priv->offchannel_done = 0;
	wsm_buf_init(&hw_priv->wsm_cmd_buf, xr_sdio_blksize_align(1024));
	spin_lock_init(&hw_priv->wsm_cmd.lock);
	hw_priv->tx_policy_limit = TX_POLICY_CACHE_SIZE;
	xradio_init_resv_skb(hw_priv);

	for (i = 0; i < XRWL_MAX_VIFS; i++)
		hw_priv->hw_bufs_used_vif[i] = 0;

#ifdef SUPPORT_ACS
	init_waitqueue_head(&hw_priv->ms_wq);
	atomic_set(&hw_priv->ms_status, 0);
	mutex_init(&hw_priv->ms_mutex);
#endif

#ifdef MCAST_FWDING
	wsm_init_release_buffer_request(hw_priv);
	hw_priv->buf_released = 0;
#endif
	hw_priv->vif0_throttle = XRWL_HOST_VIF0_11BG_THROTTLE;
	hw_priv->vif1_throttle = XRWL_HOST_VIF1_11BG_THROTTLE;

#if defined(CONFIG_XRADIO_DEBUG)
	hw_priv->wsm_enable_wsm_dumps = 0;
	hw_priv->wsm_dump_max_size = WSM_DUMP_MAX_SIZE;
#endif /* CONFIG_XRADIO_DEBUG */
	hw_priv->query_packetID = 0;
	atomic_set(&hw_priv->query_cnt, 0);
	INIT_WORK(&hw_priv->query_work, wsm_query_work);
#ifdef CONFIG_PM
	atomic_set(&hw_priv->suspend_state, XRADIO_RESUME);
#endif

#ifdef HW_RESTART
	hw_priv->exit_sync  = false;
	hw_priv->hw_restart = false;
	INIT_WORK(&hw_priv->hw_restart_work, xradio_restart_work);
#endif

#ifdef CONFIG_XRADIO_TESTMODE
	hw_priv->test_frame.data = NULL;
	hw_priv->test_frame.len = 0;
	spin_lock_init(&hw_priv->tsm_lock);
	INIT_DELAYED_WORK(&hw_priv->advance_scan_timeout,
			  xradio_advance_scan_timeout);
#endif /*CONFIG_XRADIO_TESTMODE*/

	xradio_set_ifce_comb(hw_priv, hw_priv->hw);
#ifdef MONITOR_MODE
	hw_priv->monitor_if_id = -1;
	hw_priv->monitor_running = false;
#endif
	hw_priv->join_chan = 1;

	if (!g_hw_priv) {
		g_hw_priv = hw_priv;
		return hw;
	} else {		/*error:didn't release hw_priv last time. */
		mac80211_free_hw(hw);
		xradio_dbg(XRADIO_DBG_ERROR, "g_hw_priv is not NULL @ %p!\n", g_hw_priv);
		return NULL;
	}
}

void xradio_free_common(struct ieee80211_hw *dev)
{
	int i;
	struct xradio_common *hw_priv = dev->priv;
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);

#ifdef CONFIG_XRADIO_TESTMODE
	kfree(hw_priv->test_frame.data);
#endif /* CONFIG_XRADIO_TESTMODE */

#ifdef MONITOR_RX_FIX
	cancel_work_sync(&hw_priv->monitor_rx_restart_work);
#endif
	cancel_work_sync(&hw_priv->query_work);
	del_timer_sync(&hw_priv->ba_timer);
	wsm_buf_deinit(&hw_priv->wsm_cmd_buf);
	flush_workqueue(hw_priv->workqueue);
	destroy_workqueue(hw_priv->workqueue);
	hw_priv->workqueue = NULL;
	flush_workqueue(hw_priv->spare_workqueue);
	destroy_workqueue(hw_priv->spare_workqueue);
	hw_priv->spare_workqueue = NULL;

	xradio_deinit_resv_skb(hw_priv);
	if (hw_priv->skb_cache) {
		dev_kfree_skb(hw_priv->skb_cache);
		hw_priv->skb_cache = NULL;
	}

	for (i = 0; i < 4; ++i)
		xradio_queue_deinit(&hw_priv->tx_queue[i]);
	xradio_queue_stats_deinit(&hw_priv->tx_queue_stats);

#ifdef CONFIG_XRADIO_DEBUG
	for (i = 0; i < XRWL_MAX_VIFS; i++) {
		if (hw_priv->vif_list[i])
			xradio_dbg(XRADIO_DBG_ERROR,
			"vif_list[%d]is not NULL @ %p!\n", i, hw_priv->vif_list[i]);
	}
#endif

#ifdef MCAST_FWDING
	wsm_deinit_release_buffer(hw_priv);
#endif
	/* unsigned int i; */
	mac80211_free_hw(dev);
#ifdef SUPPORT_XR_COEX
	/* during process of etf deinit, maybe not clear wakeup bit */
	if (g_hw_priv->device_can_sleep == false)
		xradio_func_call_xr_coex_wakeup(0);
#endif
	g_hw_priv = NULL;
}

int xradio_register_common(struct ieee80211_hw *dev)
{
	int err = 0;
	struct xradio_common *hw_priv = dev->priv;
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);

	/* Set MAC address. */
	xradio_set_mac_addrs(hw_priv);

	if ((hw_priv->hw_revision == XR819_HW_REV1) || (hw_priv->hw_revision == V821_HW_REV0) ||
		(hw_priv->hw_revision == V821B_HW_REV0)) {
		xradio_band_2ghz.ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;
		xradio_band_2ghz.ht_cap.mcs.rx_highest = 0x0;
	}

	SET_IEEE80211_DEV(dev, hw_priv->pdev);
	err = mac80211_register_hw(dev);
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR, "Cannot register device (%d).\n", err);
		return err;
	}
	xradio_dbg(XRADIO_DBG_MSG, "is registered as '%s'\n",
		   wiphy_name(dev->wiphy));
	xradio_debug_init_common(hw_priv);
	hw_priv->driver_ready = 1;
	wake_up(&hw_priv->wsm_startup_done);
	return 0;
}

void xradio_unregister_common(struct ieee80211_hw *dev)
{
	struct xradio_common *hw_priv = dev->priv;
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);
#ifdef CONFIG_XRADIO_ETF
	if (etf_is_connect()) {
		return ;
	}
#endif

	if (wiphy_dev(dev->wiphy)) {
		mac80211_unregister_hw(dev);
		SET_IEEE80211_DEV(dev, NULL);
		xradio_debug_release_common(hw_priv);
	}
	hw_priv->driver_ready = 0;
}

#ifdef HW_RESTART
#ifndef CONFIG_DRIVER_V821
static int xradio_find_rfkill(struct device *dev, void *data)
{
	if (dev_name(dev)[0] == 'r' && dev_name(dev)[1] == 'f')
		return true;
	return false;
}
#endif
int xradio_core_reinit(struct xradio_common *hw_priv)
{
	int ret = 0;
	u16 ctrl_reg;
	int i = 0;
#ifndef CONFIG_DRIVER_V821
	struct device *rfkill;
#endif
	struct xradio_vif *priv = NULL;
	struct wsm_operational_mode mode = {
		.power_mode = wsm_power_mode_quiescent,
		.disableMoreFlagUsage = true,
	};
#ifdef CONFIG_DRIVER_V821
	struct wsm_sethardware_info *hw_info = NULL;
	u8 freq_offset = 0;
#if XRADIO_SUPPORT_AIF_CONFIG
	u32 _aif_en = XRADIO_AIF_DEFAULT_SEL;
	struct wsm_aif_info aif_info;
#endif
#endif

	if (!hw_priv) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s hw_priv is NULL!\n", __func__);
		return -1;
	}

	/* Need some time for restart hardware, don't suspend again.*/
#ifdef CONFIG_PM
	xradio_pm_lock_awake(&hw_priv->pm_state);
#endif

	xradio_dbg(XRADIO_DBG_ALWY, "%s %d!\n", __func__, __LINE__);

	/* Disconnect with AP or STAs. */
	xradio_for_each_vif(hw_priv, priv, i) {
		if (!priv)
			continue;
		if (priv->join_status == XRADIO_JOIN_STATUS_STA) {
			mac80211_connection_loss(priv->vif);
			msleep(200);
		} else if (priv->join_status == XRADIO_JOIN_STATUS_AP) {
			wsm_send_disassoc_to_self(hw_priv, priv);
			msleep(200);
		}
		xradio_remove_interface(hw_priv->hw, priv->vif);
	}
	xradio_stop(hw_priv->hw);
	/* lock queue of network. */
	xradio_tx_queues_lock(hw_priv);

	/*deinit dev */
	xradio_dev_deinit(hw_priv);

	/*reinit status refer to hif. */
	hw_priv->powersave_enabled = false;
	hw_priv->wsm_caps.firmwareReady = 0;
	atomic_set(&hw_priv->bh_rx, 0);
	atomic_set(&hw_priv->bh_tx, 0);
	atomic_set(&hw_priv->bh_term, 0);
	hw_priv->buf_id_tx = 0;
	hw_priv->buf_id_rx = 0;
	hw_priv->wsm_rx_seq = 0;
	hw_priv->wsm_tx_seq = 0;
	hw_priv->device_can_sleep = 0;
	hw_priv->hw_bufs_used = 0;
	memset(&hw_priv->hw_bufs_used_vif, 0, sizeof(hw_priv->hw_bufs_used_vif));
	for (i = 0; i < (XRWL_MAX_VIFS-1); ++i) {
		hw_priv->scan_delay_status[i] = XRADIO_SCAN_ALLOW;
		hw_priv->scan_delay_time[i] = 0;
	}
	atomic_set(&hw_priv->query_cnt, 0);
	hw_priv->query_packetID = 0;
	tx_policy_init(hw_priv);

	/*move parent to plat_device*/
	ret = device_move(&hw_priv->hw->wiphy->dev, &hw_priv->plat_device->dev, 0);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:device move parent to plat_device failed\n", __func__);
		goto exit;
	}
	ret = mac80211_ifdev_move(hw_priv->hw, &hw_priv->plat_device->dev, 0);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:net_device move parent to plat_device failed\n", __func__);
		goto exit;
	}

	/*reinit sdio sbus. */
#if !CONFIG_HIF_BY_PASS
	sbus_sdio_deinit();
	hw_priv->pdev = sbus_sdio_init((struct sbus_ops **)&hw_priv->sbus_ops,
				       &hw_priv->sbus_priv);
	if (!hw_priv->pdev) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:sbus_sdio_init failed\n", __func__);
		ret = -ETIMEDOUT;
		goto exit;
	}
#else
	hifbypass_driver_deinit();
	hw_priv->pdev = hifbypass_driver_init(&hw_priv->hifbypass_priv);
	if (!hw_priv->pdev) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:hifbypass init failed\n", __func__);
		ret = -ETIMEDOUT;
		goto exit;
	}
#endif
	/*move parent to sdio device*/
	ret = device_move(&hw_priv->hw->wiphy->dev, hw_priv->pdev, 1);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:device move parent to sdio failed\n", __func__);
		goto exit;
	}
	SET_IEEE80211_DEV(hw_priv->hw, hw_priv->pdev);
	ret = mac80211_ifdev_move(hw_priv->hw, hw_priv->pdev, 1);
	if (ret < 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:net_device move parent to sdio failed\n", __func__);
		goto exit;
	}

#if !CONFIG_HIF_BY_PASS
	rfkill = device_find_child(&hw_priv->hw->wiphy->dev, NULL, xradio_find_rfkill);
	device_move(rfkill, &hw_priv->hw->wiphy->dev, 1);
	put_device(rfkill);
#endif

	/*wake up bh thread. */
	if (hw_priv->bh_thread == NULL) {
		hw_priv->bh_error = 0;
		atomic_set(&hw_priv->tx_lock, 0);
		xradio_register_bh(hw_priv);
	} else {
#ifdef CONFIG_XRADIO_SUSPEND_POWER_OFF
		WARN_ON(xradio_bh_resume(hw_priv));
#endif
	}

	/* Load firmware and register Interrupt Handler */
	ret = xradio_load_firmware(hw_priv);
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s:xradio_load_firmware failed(%d).\n",
			   __func__, ret);
		goto exit;
	}

#if !CONFIG_HIF_BY_PASS
	/* Set sdio blocksize. */
	hw_priv->sbus_ops->lock(hw_priv->sbus_priv);
	SYS_WARN(hw_priv->sbus_ops->set_block_size(hw_priv->sbus_priv,
		 SDIO_BLOCK_SIZE));
	hw_priv->sbus_ops->unlock(hw_priv->sbus_priv);
#endif
	if (wait_event_interruptible_timeout(hw_priv->wsm_startup_done,
				hw_priv->wsm_caps.firmwareReady, 3*HZ) <= 0) {

		/* TODO: Needs to find how to reset device */
		/*       in QUEUE mode properly.           */
		xradio_dbg(XRADIO_DBG_ERROR, "%s:Firmware Startup Timeout!\n",
			   __func__);
		ret = -ETIMEDOUT;
		goto exit;
	}
	xradio_dbg(XRADIO_DBG_ALWY, "%s:Firmware Startup Done.\n", __func__);

	/* Keep device wake up. */
#if !CONFIG_HIF_BY_PASS
	ret = xradio_reg_write_16(hw_priv, HIF_CONTROL_REG_ID, HIF_CTRL_WUP_BIT);
	if (xradio_reg_read_16(hw_priv, HIF_CONTROL_REG_ID, &ctrl_reg))
		ret = xradio_reg_read_16(hw_priv, HIF_CONTROL_REG_ID, &ctrl_reg);
	SYS_WARN(!(ctrl_reg & HIF_CTRL_RDY_BIT));
#else
	REG_SET_BIT(HIF_OV_CTRL, OV_WLAN_WAKE_UP);
	ctrl_reg = REG_GET_BIT(HIF_WLAN_STATE, WLAN_STATE_ACTIVE);
	SYS_WARN(!ctrl_reg);
#endif

	/*
	 * We finish device restart here.
	 */
	hw_priv->hw_restart = false;
#ifdef CONFIG_PM
	atomic_set(&hw_priv->suspend_state, XRADIO_RESUME);
	atomic_set(&hw_priv->suspend_lock_state, XRADIO_SUSPEND_LOCK_IDEL);
#endif

	wake_up(&hw_priv->wsm_wakeup_done);

	/* Set device mode parameter. */
	for (i = 0; i < xrwl_get_nr_hw_ifaces(hw_priv); i++) {
		/* Set low-power mode. */
		ret = wsm_set_operational_mode(hw_priv, &mode, i);
		/* Enable multi-TX confirmation */
		ret = wsm_use_multi_tx_conf(hw_priv, true, i);
	}

#ifdef CONFIG_DRIVER_V821
	hw_info = kzalloc(sizeof(*hw_info), GFP_KERNEL);
	if (!hw_info) {
		ret = -ENOMEM;
		xradio_dbg(XRADIO_DBG_ERROR, "hw_info malloc failed\n");
		goto exit;
	}
	xradio_get_oshwinfo(hw_info);
	SYS_WARN(wsm_set_hardware_info(hw_priv, hw_info));
	kfree(hw_info);
	hw_info = NULL;

	if (!xradio_efuse_read_dcxo(&freq_offset) && freq_offset)
		hw_priv->sdd_freq_offset = freq_offset;
	xradio_set_freq_offset(hw_priv, hw_priv->sdd_freq_offset);

	hw_priv->new_tx_conf_status = wsm_use_new_tx_confirm_status(hw_priv, true, 0);

#if XRADIO_SUPPORT_AIF_CONFIG
	SYS_WARN(wsm_write_mib(hw_priv, WSM_MIB_ID_AIF_ENABLE, &_aif_en, sizeof(_aif_en), 0));
	SYS_WARN(wsm_read_mib(hw_priv, WSM_MIB_ID_AIF_ENABLE, &aif_info, sizeof(aif_info), 0));
	xradio_dbg(XRADIO_DBG_ALWY, "xradio AIF %s default, ver: %d !!!\n", _aif_en ? "ON" : "OFF", aif_info.version);
#endif
#endif

#if 0
	/*
	 * IF USE xradio_register_common TO REINIT, U SHOULD ENABLE THIS.
	 * fix super standby hang in scan interface.
	 * Since resume from super standby, mac system will register ieee80211_hw to
	 * subsystem, and the wiphy->max_scan_ie_len will decrease 47 every round whose
	 * initial value is IEEE80211_MAX_DATA_LEN, after about 40~50 round, this value will
	 * under 143. and the 143 is the default scan ie length from supplicant, this will return
	 * -22 in nl80211.c
	 * if (n_ssids > wiphy->max_scan_ssids) {
	 *	return -EINVAL;
	 * }
	 * we need to reinit this value in super standby recovery.
	 */
	hw_priv->hw->wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;
#endif
	/*
	 * Restart umac, use mac80211_restart_hw.
	 * Use xradio_register_common may cause umac scan sync problems.
	 */
	if (!ret) {
		mac80211_restart_hw(hw_priv->hw);
	}

	/*
	 * Unlock queue of network. regardless of xradio_tx_queues_lock,
	 * this must be here to make sure all queue is unlock.
	 */
	for (i = 0; i < 4; ++i) {
		struct xradio_queue *queue = &hw_priv->tx_queue[i];
		spin_lock_bh(&queue->lock);
		if (queue->tx_locked_cnt > 0) {
			queue->tx_locked_cnt = 0;
			mac80211_wake_queue(hw_priv->hw, queue->queue_id);
		}
		spin_unlock_bh(&queue->lock);
	}

exit:
#ifdef CONFIG_PM
	xradio_pm_unlock_awake(&hw_priv->pm_state);
#endif
	sema_init(&hw_priv->wsm_oper_lock, 1);
	xradio_dbg(XRADIO_DBG_ALWY, "Set wsm_oper_lock %d!\n", hw_priv->wsm_oper_lock.count);

	xradio_dbg(XRADIO_DBG_ALWY, "%s end!\n", __func__);

	return ret;
}
void xradio_restart_work(struct work_struct *work)
{
	struct xradio_common *hw_priv =
		container_of(work, struct xradio_common, hw_restart_work);
	xradio_dbg(XRADIO_DBG_ALWY, "%s\n", __func__);

	hw_priv->hw_restart_work_running = 1;
	if (hw_priv->bh_error) {
		xradio_unregister_bh(hw_priv);
	}
	if (unlikely(xradio_core_reinit(hw_priv))) {
		xradio_dbg(XRADIO_DBG_ALWY, "%s again!\n", __func__);
		down(&hw_priv->wsm_cmd_sema);
		hw_priv->hw_restart = true;
		up(&hw_priv->wsm_cmd_sema);
		if (hw_priv->bh_thread != NULL)
			xradio_unregister_bh(hw_priv);
		xradio_core_reinit(hw_priv);
	}
	hw_priv->hw_restart_work_running = 0;
}
#endif

int xradio_core_init(void)
{
	int err = -ENOMEM;
	u16 ctrl_reg;
	int if_id;
	struct ieee80211_hw *dev;
	struct xradio_common *hw_priv;
	struct wsm_operational_mode mode = {
		.power_mode = wsm_power_mode_quiescent,
		.disableMoreFlagUsage = true,
	};
#ifdef CONFIG_DRIVER_V821
	struct wsm_sethardware_info *hw_info = NULL;
	u8 freq_offset = 0;
#if XRADIO_SUPPORT_AIF_CONFIG
	u32 _aif_en = XRADIO_AIF_DEFAULT_SEL;
	struct wsm_aif_info aif_info;
#endif
#if XRADIO_USE_FW_SOFT_AGC_CCA_POLICY
	u32 fw_soft_agc_en = 1;
#endif
	wlan_ext_lfclock_param_t lfclock_param;
#endif

	xradio_version_show();

#ifdef USE_RPMSG_IN_LINUX
	if (xradio_rpmsg_init()) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_rpmsg_init failed\n");
		return err;
	}
#endif

	/*init xradio_common */
	dev = xradio_init_common(sizeof(struct xradio_common));
	if (!dev) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_init_common failed\n");
		goto err0;
	}

	hw_priv = dev->priv;
	if (xradio_device_init(hw_priv)) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_device_init failed\n");
		xradio_free_common(dev);
		goto err0;
	}

#if (!CONFIG_HIF_BY_PASS)
	/*init sdio sbus */
	hw_priv->pdev = sbus_sdio_init((struct sbus_ops **)&hw_priv->sbus_ops,
				&hw_priv->sbus_priv);
	if (!hw_priv->pdev) {
		err = -ETIMEDOUT;
		xradio_dbg(XRADIO_DBG_ERROR, "sbus_sdio_init failed\n");
		goto err1;
	}
#else
#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	FW_TXRXBUF_SET_WF_STAT(FW_TXRXBUF_WF_STAT_WF_ON);
#endif

	hw_priv->pdev = hifbypass_driver_init((struct hifbypass_priv **)&hw_priv->hifbypass_priv);
	if (!hw_priv->pdev) {
		xradio_dbg(XRADIO_DBG_ERROR, "hifbypass_driver_init failed\n");
		goto err1;
	}
#endif

#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	FW_TXRXBUF_SET_WF_STAT(FW_TXRXBUF_WF_STAT_PWR_ON);
#endif

#if XRADIO_SDD_DIFF_HOSC_FREQ_FILE_MERGE
	xradio_hosc_frequency_get(hw_priv);
#endif

	/* WSM callbacks. */
	hw_priv->wsm_cbc.scan_complete = xradio_scan_complete_cb;
	hw_priv->wsm_cbc.tx_confirm = xradio_tx_confirm_cb;
	hw_priv->wsm_cbc.rx = xradio_rx_cb;
	hw_priv->wsm_cbc.suspend_resume = xradio_suspend_resume;
	/* hw_priv->wsm_cbc.set_pm_complete = xradio_set_pm_complete_cb; */
	hw_priv->wsm_cbc.channel_switch = xradio_channel_switch_cb;

	/*init pm and wakelock. */
#ifdef CONFIG_PM
	err = xradio_pm_init(&hw_priv->pm_state, hw_priv);
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_pm_init failed(%d).\n", err);
		goto err2;
	}
#endif
	/* Register bh thread*/
	err = xradio_register_bh(hw_priv);
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_register_bh failed(%d).\n",
			   err);
		goto err3;
	}

	/* Load firmware and register Interrupt Handler */
	err = xradio_load_firmware(hw_priv);
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_load_firmware failed(%d).\n",
			   err);
		goto err4;
	}

#if !CONFIG_HIF_BY_PASS
	/* Set sdio blocksize. */
	hw_priv->sbus_ops->lock(hw_priv->sbus_priv);
	SYS_WARN(hw_priv->sbus_ops->set_block_size(hw_priv->sbus_priv,
			SDIO_BLOCK_SIZE));
	hw_priv->sbus_ops->unlock(hw_priv->sbus_priv);
#endif

	if (wait_event_interruptible_timeout(hw_priv->wsm_startup_done,
				hw_priv->wsm_caps.firmwareReady, 3*HZ) <= 0) {

		/* TODO: Needs to find how to reset device */
		/*       in QUEUE mode properly.           */
		xradio_dbg(XRADIO_DBG_ERROR, "Firmware Startup Timeout!\n");
		err = -ETIMEDOUT;
		goto err5;
	}
	xradio_dbg(XRADIO_DBG_ALWY, "Firmware Startup Done.\n");

#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	FW_TXRXBUF_SET_WF_STAT(FW_TXRXBUF_WF_STAT_FW_RDY);
#endif

	/* Keep device wake up. */
#if !CONFIG_HIF_BY_PASS
	SYS_WARN(xradio_reg_write_16(hw_priv, HIF_CONTROL_REG_ID,
		 HIF_CTRL_WUP_BIT));
	if (xradio_reg_read_16(hw_priv, HIF_CONTROL_REG_ID, &ctrl_reg))
		SYS_WARN(xradio_reg_read_16(hw_priv, HIF_CONTROL_REG_ID, &ctrl_reg));
	SYS_WARN(!(ctrl_reg & HIF_CTRL_RDY_BIT));
#else
	REG_SET_BIT(HIF_OV_CTRL, OV_WLAN_WAKE_UP);
	ctrl_reg = REG_READ_U32(HIF_WLAN_STATE);
	SYS_WARN(!(ctrl_reg & WLAN_STATE_ACTIVE));
#endif

	/* ETF mode don't need following steps, just return here*/
#ifdef CONFIG_XRADIO_ETF
	if (etf_is_connect()) {
		etf_set_core(hw_priv);
		xradio_dbg(XRADIO_DBG_ALWY, "%s Enter ETF mode!\n", __func__);
		return 0;
	}
#endif

#ifdef CONFIG_DRIVER_V821
	err = xradio_rw_efuse_init();
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_rw_efuse_init failed\n");
		goto err5;
	}
	hw_info = kzalloc(sizeof(*hw_info), GFP_KERNEL);
	if (!hw_info) {
		err = -ENOMEM;
		xradio_dbg(XRADIO_DBG_ERROR, "hw_info malloc failed\n");
		xradio_rw_efuse_deinit();
		goto err5;
	}
	xradio_get_oshwinfo(hw_info);
	SYS_WARN(wsm_set_hardware_info(hw_priv, hw_info));
	kfree(hw_info);
	hw_info = NULL;

	if (hw_priv->hw_revision == V821B_HW_REV0) {
		lfclock_param.SysLfclockFreq = 32000;//XR_PRCM_GetWlanLFClock();
		lfclock_param.SysLfclockType = SYS_LFCLOCK_TYPE_INT32K_RCCAL;//XR_PRCM_GetWlanLFClockType();
		xradio_dbg(XRADIO_DBG_ALWY, "[WLAN-lpm] SysLfclockFreq:%d, SysLfclockType:%d\n",
			lfclock_param.SysLfclockFreq, lfclock_param.SysLfclockType);
		if (lfclock_param.SysLfclockType == SYS_LFCLOCK_TYPE_INT32K_RCCAL) {
			lfclock_param.PwrCalibCount = 80000;
		} else if (lfclock_param.SysLfclockType == SYS_LFCLOCK_TYPE_EXT32K) {
			lfclock_param.PwrCalibCount = 78125;
		} else {
			lfclock_param.PwrCalibCount = 0;
		}
		SYS_WARN(wsm_write_mib(hw_priv, WSM_MIB_ID_SET_LFCLOCK_PARAM,
			(void *)(&lfclock_param), sizeof(wlan_ext_lfclock_param_t), 0));
	}

	if (!xradio_efuse_read_dcxo(&freq_offset) && freq_offset)
		hw_priv->sdd_freq_offset = freq_offset;

	hw_priv->new_tx_conf_status = wsm_use_new_tx_confirm_status(hw_priv, true, 0);

#if XRADIO_SUPPORT_AIF_CONFIG
	SYS_WARN(wsm_write_mib(hw_priv, WSM_MIB_ID_AIF_ENABLE, &_aif_en, sizeof(_aif_en), 0));
	SYS_WARN(wsm_read_mib(hw_priv, WSM_MIB_ID_AIF_ENABLE, &aif_info, sizeof(aif_info), 0));
	xradio_dbg(XRADIO_DBG_ALWY, "xradio AIF %s default, ver: %d !!!\n", _aif_en ? "ON" : "OFF", aif_info.version);
#endif

#if XRADIO_USE_FW_SOFT_AGC_CCA_POLICY
	SYS_WARN(wsm_write_mib(hw_priv, WSM_MIB_ID_FW_SOFT_AGC_CCA_POLICY, &fw_soft_agc_en, sizeof(fw_soft_agc_en), 0));
#endif

#endif
	wsm_get_tx_policy_cnt(hw_priv, &hw_priv->tx_policy_limit);
	if (hw_priv->tx_policy_limit != TX_POLICY_CACHE_SIZE)
		xradio_dbg(XRADIO_DBG_WARN, "tx policy cahce:%d\n", hw_priv->tx_policy_limit);
	tx_policy_init(hw_priv);

	/* Set device mode parameter. */
	for (if_id = 0; if_id < xrwl_get_nr_hw_ifaces(hw_priv); if_id++) {
		/* Set low-power mode. */
		SYS_WARN(wsm_set_operational_mode(hw_priv, &mode, if_id));
		/* Enable multi-TX confirmation */
		SYS_WARN(wsm_use_multi_tx_conf(hw_priv, true, if_id));
	}

#ifdef CONFIG_DRIVER_V821
	xradio_set_freq_offset(hw_priv, hw_priv->sdd_freq_offset);
#endif

	/* Register wireless net device. */
	err = xradio_register_common(dev);
	if (err) {
		xradio_dbg(XRADIO_DBG_ERROR,
			   "xradio_register_common failed(%d)!\n", err);
		goto err6;
	}

	return err;

err6:
#ifdef CONFIG_DRIVER_V821
	if (hw_info)
		kfree(hw_info);
	xradio_rw_efuse_deinit();
#endif
err5:
	xradio_dev_deinit(hw_priv);
err4:
	xradio_unregister_bh(hw_priv);
err3:
#ifdef CONFIG_PM
	xradio_pm_deinit(&hw_priv->pm_state);
err2:
#endif
#if !CONFIG_HIF_BY_PASS
	sbus_sdio_deinit();
#else
	hifbypass_driver_deinit();
#endif
err1:
#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	FW_TXRXBUF_CLR_WF_STAT(FW_TXRXBUF_WF_STAT_MASK);
#endif
	xradio_device_deinit(hw_priv);
	xradio_free_common(dev);
err0:
#ifdef USE_RPMSG_IN_LINUX
	xradio_rpmsg_deinit();
#endif
	return err;
}

void xradio_core_deinit(void)
{
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);

	if (g_hw_priv) {
#ifdef HW_RESTART
		down(&g_hw_priv->wsm_cmd_sema);
		g_hw_priv->exit_sync = true;
		up(&g_hw_priv->wsm_cmd_sema);
		cancel_work_sync(&g_hw_priv->hw_restart_work);
#endif

#ifdef CONFIG_XRADIO_ETF
		if (etf_is_connect()) {
			etf_set_core(NULL);
			xradio_dbg(XRADIO_DBG_ALWY, "%s Exit ETF mode!\n", __func__);
		} else
			xradio_unregister_common(g_hw_priv->hw);
#else
		xradio_unregister_common(g_hw_priv->hw);
#endif
		xradio_dev_deinit(g_hw_priv);
		xradio_unregister_bh(g_hw_priv);
#ifdef CONFIG_PM
		xradio_pm_deinit(&g_hw_priv->pm_state);
#endif
		xradio_device_deinit(g_hw_priv);
#ifdef CONFIG_DRIVER_V821
		xradio_rw_efuse_deinit();
#endif
		xradio_free_common(g_hw_priv->hw);
#if !CONFIG_HIF_BY_PASS
		sbus_sdio_deinit();
#else
#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
		FW_TXRXBUF_CLR_WF_STAT(FW_TXRXBUF_WF_STAT_WF_ON | FW_TXRXBUF_WF_STAT_FW_RDY);
#endif
		hifbypass_driver_deinit();
#endif
#ifdef USE_RPMSG_IN_LINUX
		xradio_rpmsg_deinit();
#endif
	}
#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
	FW_TXRXBUF_CLR_WF_STAT(FW_TXRXBUF_WF_STAT_MASK);
#endif
	return;
}

/* Init Module function -> Called by insmod */
int __init xradio_core_entry(void)
{
	int ret = 0;
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);
	ret = xradio_plat_init();
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "xradio_plat_init failed(%d)!\n", ret);
		return ret;
	}
	ret = xradio_host_dbg_init();
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s dbg init failed(%d)!\n", __func__, ret);
		xradio_plat_deinit();
		return ret;
	}
#ifdef CONFIG_XRADIO_ETF
	ret = xradio_etf_init();
	if (ret) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s ETF init failed(%d)!\n", __func__, ret);
		xradio_host_dbg_deinit();
		xradio_plat_deinit();
		return ret;
	}
#endif
	return 0;
}

/* Called at Driver Unloading */
void xradio_core_exit(void)
{
#ifdef CONFIG_XRADIO_ETF
	xradio_etf_deinit();
#endif
	xradio_host_dbg_deinit();
	xradio_plat_deinit();
	xradio_dbg(XRADIO_DBG_TRC, "%s\n", __func__);
}

enum xr_chip_type xr_get_chip_type(void)
{
	int ver = sunxi_chip_alter_version();

	if (ver == SUNXI_CHIP_ALTER_VERSION_V821)
		return XR_CHIP_TYPE_V821;
	else if (ver == SUNXI_CHIP_ALTER_VERSION_V821B)
		return XR_CHIP_TYPE_V821B;
	else
		return XR_CHIP_TYPE_NONE;
}
