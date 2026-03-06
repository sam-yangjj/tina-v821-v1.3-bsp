#ifndef _XRMAC_IOCTL_H
#define _XRMAC_IOCTL_H

#include "net/mac80211.h"
#include "ieee80211_i.h"

#ifdef XRADIO_RAW_FRM_SUP

#define IOCTL_REQ_BASE                    0x0000
#define IOCTL_CNF_BASE                    0x4000
#define IOCTL_IND_BASE                    0x8000


#define IOCTL_GENERIC_BASE                (IOCTL_REQ_BASE | 0x0200)

typedef struct CMD_REQ_HDR_S {
	u16 len;                /* length in bytes from MsgLen to end of payload */
	u16 msgId;
	u8  data[];
	/* Payload is followed */
} CMD_REQ_HDR;

typedef struct CMD_CNF_HDR_S {
	u16 len;                /* length in bytes from MsgLen to end of payload */
	u16 msgId;
	int result;
	u8  data[];
	/* Payload is followed */
} CMD_CNF_HDR;

/**
 * @brief Wlan extended command definition
 */
typedef enum wlan_ext_cmd {
	WLAN_EXT_CMD_SET_PM_DTIM = 0,
	WLAN_EXT_CMD_GET_PM_DTIM,
	WLAN_EXT_CMD_SET_PS_CFG,
	WLAN_EXT_CMD_SET_AMPDU_TXNUM,
	WLAN_EXT_CMD_SET_TX_RETRY_CNT,
	WLAN_EXT_CMD_SET_PM_TX_NULL_PERIOD,
	WLAN_EXT_CMD_SET_BCN_WIN_US,
	WLAN_EXT_CMD_GET_BCN_STATUS,
	WLAN_EXT_CMD_SET_PHY_PARAM,
	WLAN_EXT_CMD_SET_SCAN_PARAM,
	WLAN_EXT_CMD_SET_LISTEN_INTERVAL,//10
	WLAN_EXT_CMD_SET_AUTO_SCAN,
	WLAN_EXT_CMD_SET_P2P_SVR,
	WLAN_EXT_CMD_SET_P2P_WKP_CFG,
	WLAN_EXT_CMD_SET_P2P_KPALIVE_CFG,
	WLAN_EXT_CMD_SET_P2P_HOST_SLEEP,
	WLAN_EXT_CMD_SET_BCN_WIN_CFG,
	WLAN_EXT_CMD_SET_FORCE_B_RATE,
	WLAN_EXT_CMD_SET_RCV_SPECIAL_FRM,
	WLAN_EXT_CMD_SET_SEND_RAW_FRM_CFG,
	WLAN_EXT_CMD_SET_SNIFF_SYNC_CFG,//20
	WLAN_EXT_CMD_SET_RCV_FRM_FILTER_CFG,
	WLAN_EXT_CMD_SET_POWER_LEVEL_TAB,
	WLAN_EXT_CMD_GET_POWER_LEVEL_TAB,
	WLAN_EXT_CMD_SET_SWITCH_CHN_CFG,
	WLAN_EXT_CMD_GET_CURRENT_CHN,
	WLAN_EXT_CMD_SET_SNIFF_KP_ACTIVE,
	WLAN_EXT_CMD_SET_FRM_FILTER,
	WLAN_EXT_CMD_SET_TEMP_FRM,
	WLAN_EXT_CMD_SET_UPDATE_TEMP_IE,
	WLAN_EXT_CMD_SET_SYNC_FRM_SEND,//30
	WLAN_EXT_CMD_SET_SNIFF_EXTERN_CFG,
	WLAN_EXT_CMD_GET_SNIFF_STAT,
	WLAN_EXT_CMD_GET_TEMP_VOLT,
	WLAN_EXT_CMD_SET_CHANNEL_FEC,
	WLAN_EXT_CMD_SET_TEMP_VOLT_AUTO_UPLOAD,
	WLAN_EXT_CMD_SET_TEMP_VOLT_THRESH,
	WLAN_EXT_CMD_SET_BCN_FREQ_OFFS_TIME,
	WLAN_EXT_CMD_SET_LFCLOCK_PARAM,
	WLAN_EXT_CMD_SET_EDCA_PARAM,
	WLAN_EXT_CMD_GET_EDCA_PARAM,
	WLAN_EXT_CMD_GET_STATS_CODE,

	WLAN_EXT_CMD_SET_SDD_FREQ_OFFSET,
	WLAN_EXT_CMD_GET_SDD_FREQ_OFFSET,
	WLAN_EXT_CMD_SET_SDD_POWER,
	WLAN_EXT_CMD_GET_SDD_POWER,
	WLAN_EXT_CMD_GET_SDD_FILE,

	WLAN_EXT_CMD_GET_TX_RATE = 50,
	WLAN_EXT_CMD_GET_SIGNAL,
	WLAN_EXT_CMD_SET_MIXRATE,

	WLAN_EXT_CMD_SET_MBUF_LIMIT,
	WLAN_EXT_CMD_SET_AMPDU_REORDER_AGE,
	WLAN_EXT_CMD_SET_SCAN_FREQ,
	WLAN_EXT_CMD_SET_RX_STACK_SIZE, /* Should be called before wlan_attach() */
	WLAN_EXT_CMD_SET_RX_QUEUE_SIZE, /* Should be called before wlan_attach() */
	WLAN_EXT_CMD_SET_AMRR_PARAM,
	WLAN_EXT_CMD_SET_BCN_RX_11B_ONLY,
	WLAN_EXT_CMD_AUTO_BCN_OFFSET_SET,
	WLAN_EXT_CMD_AUTO_BCN_OFFSET_READ,
	WLAN_EXT_CMD_SET_SNIFFER, /* set sniffer param */
	WLAN_EXT_CMD_SET_MIMO_PARAM,
	WLAN_EXT_CMD_SET_PS_POLICY,
	WLAN_EXT_CMD_SET_PRE_RX_BCN,
	WLAN_EXT_CMD_SET_STAY_AWAKE_TMO,
	WLAN_EXT_CMD_SET_AUTO_POWER,
	WLAN_EXT_CMD_SET_BCN_LOST_COMP,
	WLAN_EXT_CMD_SET_BCN_INTVL_CALIB,
	WLAN_EXT_CMD_GET_CUR_BCN_INTVL,
	WLAN_EXT_CMD_SET_AUTH_TMO_AND_TRIES,
	WLAN_EXT_CMD_SET_ASSOC_TMO_AND_TRIES,
	WLAN_EXT_CMD_SET_BSS_LOSS_THOLD,
	WLAN_EXT_CMD_SET_ARP_KPALIVE,
	WLAN_EXT_CMD_SET_BCN_WITHOUT_DATA,
	WLAN_EXT_CMD_SET_BCN_TIM_NO_DATA_TMO,
	WLAN_EXT_CMD_SET_FILTER_TYPE,
	WLAN_EXT_CMD_STOP_RCV_SPECIAL_FRM,
	WLAN_EXT_CMD_SEND_RAW_FRM,
	WLAN_EXT_CMD_SET_PM,
	WLAN_EXT_CMD_SET_TX_MODE
} wlan_ext_cmd_t;

struct ndo_do_ioctl_ops {
	int (*set_rcv_special_frm)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_send_raw_frm_cfg)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_sniff_auto_wakeup)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_switch_ch_cfg)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*get_cur_channel)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_sniff_kp_active)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_frm_filter)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_temp_frm)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*send_sync_frm)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_sniff_ext_param)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*get_rcv_stat)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*send_raw_frm)(struct ieee80211_hw *hw, u8 *param, u16 len);
	int (*set_pm)(struct ieee80211_hw *hw, u8 *param, u16 len);
};

struct raw_tx_mode_s {
	int  tx_mode;
	u32 tx_timeout;
};

#define RAW_TX_TIMEOUT_MIN        20
#define RAW_TX_TIMEOUT_MAX        5000

#define XR_STRINGIFY(x)           #x

#endif  /* XRADIO_RAW_FRM_SUP */
#endif  /* _XR_IOCTL_H */
