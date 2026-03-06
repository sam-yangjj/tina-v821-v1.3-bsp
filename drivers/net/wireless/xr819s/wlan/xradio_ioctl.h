#ifndef _XRDIO_IOCTL_H
#define _XRDIO_IOCTL_H


#ifdef XRADIO_RAW_FRM_SUP


#define FRM_TYPE_ASRQ       (0)
#define FRM_TYPE_ASRSP      (1)
#define FRM_TYPE_RSRQ       (2)
#define FRM_TYPE_RSRSP      (3)
#define FRM_TYPE_PBRQ       (4)
#define FRM_TYPE_PBRSP      (5)
#define FRM_TYPE_BCN        (6)
#define FRM_TYPE_ATIM       (7)
#define FRM_TYPE_DAS        (8)
#define FRM_TYPE_AUTH       (9)
#define FRM_TYPE_DAUTH      (10)
#define FRM_TYPE_ACTION     (11)
#define FRM_TYPE_NULLDATA   (12)
#define FRM_TYPE_DATA       (13)
#define FRM_TYPE_QOSDATA    (14)
#define FRM_TYPE_ARPREQ     (15)
#define FRM_TYPE_SA_QUERY   (16)
#define FRM_TYPE_MANAGER    (17)
#define FRM_TYPE_ALL_DATA   (18)
#define FRM_TYPE_MAX        (19)


#define MAX_FRM_LEN       (694)
typedef struct wlan_ext_temp_frm_set {
	uint16_t  FrmLength;
	uint8_t   Frame[MAX_FRM_LEN];
} wlan_ext_temp_frm_set_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SYNC_FRM_SEND
 */
#define SEND_SYNC_FRM_ADVANCE_ENABLE        (1 << 0)
typedef struct wlan_ext_send_sync_frm_set {
	uint8_t   Enable;
	uint8_t   SendSyncFrmAdvanceTime;//unit:ms
	uint16_t  Flag;
	uint32_t  BcnInterval;
} wlan_ext_send_sync_frm_set_t;//units:1024us

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SNIFFER
 */
typedef struct wlan_ext_sniffer_param {
	uint32_t channel;    /* channel 0 means disabling sniffer and switching back to original mode */
	uint32_t dwell_time; /* in us, 0 means disabling sniffer and switching back to original mode */
} wlan_ext_sniffer_param_t;

/**
 * @brief Parameter for EXT_FRAME_INFO
 */
typedef struct __ext_frame_info {
	uint8_t    chanNumber;
	uint8_t    rxedRate;
	int8_t     Rssi;
	uint8_t    flags;
	uint16_t   frame_size;
	uint16_t   reserved;
} EXT_FRAME_INFO;

typedef struct rx_ext_frames_ind {
	uint16_t  frame_num;
	uint16_t  frame_drop;
	EXT_FRAME_INFO frames[1];
} RX_EXT_FRAMES_IND;

/* monitor */
typedef enum {
	AUTH_ALG_OPEN,
	AUTH_ALG_SHARED,
	AUTH_ALG_LEAP,
} auth_alg;

struct frame_info {
	uint16_t recv_channel;  /* frame receved channel */
	uint16_t ap_channel;    /* ap channel if the frame is beacon or probe response frame */
	uint8_t type;
	uint8_t rssi;
	auth_alg alg;
};


/**
 * @brief Parameter for WLAN_EXT_CMD_SET_RCV_SPECIAL_FRM
 */
#define SEND_DUPLICATE_FRAME_TO_HOST_ENABLE    (1 << 0)
#define RECV_UNICAST_FRAME_ENABLE              (1 << 1)
#define RECV_BROADCAST_FRAME_ENABLE            (1 << 2)

typedef struct wlan_ext_rcv_spec_frm_param_set {
	uint32_t  Enable; //0 or 1
	uint32_t  u32RecvSpecFrameCfg;
} wlan_ext_rcv_spec_frm_param_set_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SEND_RAW_FRM_CFG
 */
#define SEND_RAW_FRAME_ALLOCATE_SEQNUM        (1 << 0)    //wifi allocate tx frame sequence number, not from Host
#define SEND_RAW_FRAME_NO_ACK                 (1 << 1)    //wifi will not wait for ack after send raw frame if bit is be set
typedef struct wlan_ext_send_raw_frm_param_set {
	uint8_t     Enable;
	uint8_t     Reserved;
	uint16_t    u16SendRawFrameCfg;//reserved for now
} wlan_ext_send_raw_frm_param_set_t;

#define SEND_RAW_FRAME_MAX_SWITCH_CHANNEL_TIME   5000
typedef struct wlan_ext_switch_channel_param_set {
	uint8_t     Enable;
	uint8_t     Band;
	uint16_t    Flag;//reserved now
	uint32_t    ChannelNum;
	uint32_t    SwitchChannelTime;
} wlan_ext_switch_channel_param_set_t;

typedef struct wlan_ext_get_cur_channel {
	uint16_t    Channel;
	uint16_t    Reserved;
} wlan_ext_get_cur_channel_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SNIFF_SYNC_CFG
 */
//u32SniffSyncCfg
#define SNIFF_SYNC_AT_HOST              (0x1)
#define SNIFF_SYNC_AT_WIFI              (0x0)
#define SNIFF_SYNC_METHOD               (1 << 0)
#define SNIFF_SYNC_DUPLICATE_TO_HOST    (1 << 1)
#define SNIFF_SYNC_DISABLE_TIMER        (1 << 2)
#define SNIFF_SYNC_UPLOAD_DATA_FRM      (1 << 3)
#define SNIFF_SYNC_UNICAST_FRM          (1 << 4)
#define SNIFF_SYNC_MULTICAST_FRM        (1 << 5)

//SYNC_AT_HOST:Flag
#define SNIFF_SYNC_AT_HOST_FRAME_SEND_TO_HOST         (1 << 0) //send frame to host if enable

//SYNC_AT_WIFI:Flag
#define SNIFF_SYNC_AT_WIFI_FRAME_SEND_TO_HOST         (1 << 0)
#define SNIFF_SYNC_AT_WIFI_SEND_HOST_LOST_SYNC        (1 << 1)
#define SNIFF_SYNC_AT_WIFI_ADAPTIVE_EXPANSION_ENABLE  (1 << 2)
#define SNIFF_SYNC_AT_WIFI_SEND_SYNC_INDC_TO_HOST     (1 << 3)
#define SNIFF_SYNC_AT_WIFI_UPLOAD_WHEN_PL_MISMATCH    (1 << 4)
#define SNIFF_SYNC_AT_WIFI_SYNC_USE_FILTER1           (1 << 5)
#define SNIFF_SYNC_AT_WIFI_SYNC_USE_FILTER2           (1 << 6)
#define SNIFF_SYNC_AT_WIFI_WAKEUP_ADVANCE_ENABLE      (1 << 7)

typedef struct wlan_ext_sniff_sync_param_set {
	uint8_t   Enable;
	uint8_t   ChannelNum;
	uint16_t  Reserved0;
	uint32_t  SyncFrameType;
	uint32_t  u32SniffSyncCfg;
	union {
		struct {
			uint32_t  WakeupPeriod_ms;       //unit:millisecond
			uint32_t  KeepActivePeriod_ms;   //unit:millisecond
			uint8_t   Flag;
			uint8_t   Reserved1;
			uint16_t  Reserved2;
			uint32_t  StartTime;            //unit:millisecond
		} __attribute__((packed)) time_sync_at_host;
		struct {
			uint8_t   Flag;
			uint8_t   SyncDTIM;
			uint8_t   MaxLostSyncPacket;
			uint8_t   TSFOffset;                  //unit:byte
			uint8_t   AdaptiveExpansion;          //unit:millisecond
			uint8_t   KeepActiveNumAfterLostSync; //unit:millisecond
			uint16_t  ActiveTime;                 //unit:millisecond
			uint8_t   MaxAdaptiveExpansionLimit;  //unit:millisecond
			uint8_t   WakeupAdvanceTime;          //unit:millisecond
			uint16_t  MaxKeepAliveTime;           //unit:millisecond
		} __attribute__((packed)) time_sync_at_wifi;
	} __attribute__((packed));
} wlan_ext_sniff_sync_param_set_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_RCV_FRM_FILTER_CFG
 */
#define RCV_FRM_FILTER_FRAME_TYPE      (1 << 0)
#define RCV_FRM_FILTER_MAC_ADDRESS     (1 << 1)
#define RCV_FRM_FILTER_PAYLOAD         (1 << 2)
#define RCV_FRM_FILTER_IE              (1 << 3)

#define RCV_FRM_FILTER_MAC_ADDR_A1     (1 << 0)
#define RCV_FRM_FILTER_MAC_ADDR_A2     (1 << 1)
#define RCV_FRM_FILTER_MAC_ADDR_A3     (1 << 2)


#define RCV_FRM_FILTER_PAYLOAD_LEN_MAX 256
#define RCV_FRM_FILTER_IE_LEN_MAX      256

typedef struct rcv_frm_filter {
	uint16_t  FilterEnable;
	uint16_t  AndOperationMask;
	uint16_t  OrOperationMask;
	uint16_t  Reserved;
	uint32_t  FrameType;
	uint8_t   MacAddrMask;
	uint8_t   Reserved1;
	uint8_t   MacAddrA1[6];
	uint8_t   MacAddrA2[6];
	uint8_t   MacAddrA3[6];
	union{
		struct {
			uint16_t  PayloadOffset;
			uint16_t  PayloadLength;
			uint8_t   Payload[RCV_FRM_FILTER_PAYLOAD_LEN_MAX];
		} __attribute__((packed)) PayloadCfg;
		struct {
			uint8_t  ElementId;
			uint8_t  Length;
			uint16_t Reserved;
			uint8_t  OUI[RCV_FRM_FILTER_IE_LEN_MAX];
		} __attribute__((packed)) IeCfg;
	} __attribute__((packed));
} rcv_frm_filter_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_FRM_FILTER
 */
#define FRAME_FILTER_ENABLE           (1 << 0)
typedef struct wlan_ext_frm_filter_set {
	uint32_t  Filter1Cfg;
	uint32_t  Filter2Cfg;
	rcv_frm_filter_t  Filter1;
	rcv_frm_filter_t  Filter2;
} wlan_ext_frm_filter_set_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SNIFF_EXTERN_CFG
 */
//SniffExtFuncEnable
#define SNIFF_ADAPT_EXPEN_DIFF_ENABLE       (1 << 0)
typedef struct wlan_ext_sniff_extern_param_set {
	uint32_t    SniffExtFuncEnable;
	uint32_t    Reserved0;
	uint8_t     SniffRetryAfterLostSync;
	uint8_t     SniffAdaptiveExpenRight;
	uint8_t     SniffRetryDtim;
	uint8_t     Reserved1;
} wlan_ext_sniff_extern_param_set_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_GET_BCN_STATUS
 */
typedef struct wlan_ext_bcn_status {
	uint32_t bcn_duration;
	int32_t  bcn_delay_max;
	int32_t  bcn_delay_sum;
	uint16_t bcn_delay_cnt[8];
	uint16_t bcn_rx_cnt;
	uint16_t bcn_miss_cnt;
} wlan_ext_bcn_status_t;

/**
 * @brief Parameter for WLAN_EXT_CMD_SET_SNIFF_KP_ACTIVE
 */
//u32Config
#define SNIFF_FRM_ALLOCATE_SEQ_NUM       (1 << 0)

typedef struct wlan_ext_sniff_kp_active_set {
	uint32_t  Enable;
	uint32_t  u32Config;
} wlan_ext_sniff_kp_active_set_t;


int xradio_ioctl_set_rcv_special_frm(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_send_raw_frm_cfg(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_sniff_kp_active(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_sniff_auto_wakeup(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_switch_ch_cfg(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_get_cur_channel(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_frm_filter(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_temp_frm(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_send_sync_frm(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_sniff_ext_param(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_get_rcv_stat(struct ieee80211_hw *hw, u8 *param, u16 len);
int xradio_ioctl_set_pm(struct ieee80211_hw *hw, u8 *param, u16 len);

#endif  /* XRADIO_RAW_FRM_SUP */

#endif  /* _XRDIO_IOCTL_H */
