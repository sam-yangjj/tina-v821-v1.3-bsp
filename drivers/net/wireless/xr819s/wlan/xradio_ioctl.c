/**
 ******************************************************************************
 *
 * @file xrmac_ioctl.c
 *
 * @brief Entry point of the xradio driver
 *
 * Copyright (C) RivieraWaves 2012-2021
 *
 ******************************************************************************
 */


#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/inetdevice.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <net/ip.h>
#include <linux/etherdevice.h>
#include "../umac/ieee80211_i.h"

#include "xradio_ioctl.h"
#include "xradio.h"
#include "wsm.h"


#ifdef XRADIO_RAW_FRM_SUP

u32 rx_spex_enable;

int xradio_ioctl_set_rcv_special_frm(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_rcv_spec_frm_param_set_t *rcv_spec_frm = (wlan_ext_rcv_spec_frm_param_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ receive spec frame config ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable : %d\n", rcv_spec_frm->Enable);
	xradio_dbg(XRADIO_DBG_NIY, "cfg : 0x%04X\n", rcv_spec_frm->u32RecvSpecFrameCfg);
	rx_spex_enable = rcv_spec_frm->Enable;
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_RECV_SPEC_FRAME,
			    (void *)param, sizeof(wlan_ext_rcv_spec_frm_param_set_t), if_id);

	return ret;
}

int xradio_ioctl_set_send_raw_frm_cfg(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_send_raw_frm_param_set_t *send_raw_frm = (wlan_ext_send_raw_frm_param_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ send raw frame config ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable : %d\n", send_raw_frm->Enable);
	xradio_dbg(XRADIO_DBG_NIY, "cfg : 0x%04X\n", send_raw_frm->u16SendRawFrameCfg);
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SEND_RAW_FRAME,
			    (void *)param, sizeof(wlan_ext_send_raw_frm_param_set_t), if_id);
	return ret;
}

int xradio_ioctl_set_sniff_kp_active(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_sniff_kp_active_set_t *kp_active = (wlan_ext_sniff_kp_active_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ keep active set ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable: 0x%04X\n", kp_active->Enable);
	xradio_dbg(XRADIO_DBG_NIY, "Config: 0x%04X\n", kp_active->u32Config);
	rx_spex_enable = kp_active->Enable;
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SNIFF_KEEP_ACTIVE,
			    (void *)param, sizeof(wlan_ext_sniff_kp_active_set_t), if_id);

	return ret;
}

int xradio_ioctl_set_sniff_auto_wakeup(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_sniff_sync_param_set_t *sniff_auto_wkp = (wlan_ext_sniff_sync_param_set_t *)param;
#ifdef SUPPORT_HT40
	struct phy_mode_cfg PhyModeCfg;
#endif

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ sniffer auto wakeup ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable: %d\n", sniff_auto_wkp->Enable);
	xradio_dbg(XRADIO_DBG_NIY, "Channel: %d\n", sniff_auto_wkp->ChannelNum);
	xradio_dbg(XRADIO_DBG_NIY, "SyncFrameType: 0x%X\n", sniff_auto_wkp->SyncFrameType);
	xradio_dbg(XRADIO_DBG_NIY, "SniffSyncCfg: 0x%04X\n", sniff_auto_wkp->u32SniffSyncCfg);
	if ((sniff_auto_wkp->u32SniffSyncCfg & SNIFF_SYNC_METHOD) == SNIFF_SYNC_AT_HOST) {
		xradio_dbg(XRADIO_DBG_NIY, "Wakeup Period: %dms\n", sniff_auto_wkp->time_sync_at_host.WakeupPeriod_ms);
		xradio_dbg(XRADIO_DBG_NIY, "Keep Active Period: %dms\n", sniff_auto_wkp->time_sync_at_host.KeepActivePeriod_ms);
		xradio_dbg(XRADIO_DBG_NIY, "Flag: 0x%02X\n", sniff_auto_wkp->time_sync_at_host.Flag);
		xradio_dbg(XRADIO_DBG_NIY, "Start Time: %dms\n", sniff_auto_wkp->time_sync_at_host.StartTime);
	} else {
		xradio_dbg(XRADIO_DBG_NIY, "Flag: 0x%02X\n", sniff_auto_wkp->time_sync_at_wifi.Flag);
		xradio_dbg(XRADIO_DBG_NIY, "SyncDTIM: %d\n", sniff_auto_wkp->time_sync_at_wifi.SyncDTIM);
		xradio_dbg(XRADIO_DBG_NIY, "MaxLostSyncPacket: %d\n", sniff_auto_wkp->time_sync_at_wifi.MaxLostSyncPacket);
		xradio_dbg(XRADIO_DBG_NIY, "TSFOffset: %d\n", sniff_auto_wkp->time_sync_at_wifi.TSFOffset);
		xradio_dbg(XRADIO_DBG_NIY, "AdaptiveExpansion: %d\n", sniff_auto_wkp->time_sync_at_wifi.AdaptiveExpansion);
		xradio_dbg(XRADIO_DBG_NIY, "KeepActiveNumAfterLostSync: %d\n", sniff_auto_wkp->time_sync_at_wifi.KeepActiveNumAfterLostSync);
		xradio_dbg(XRADIO_DBG_NIY, "ActiveTime: %d\n", sniff_auto_wkp->time_sync_at_wifi.ActiveTime);
		xradio_dbg(XRADIO_DBG_NIY, "MaxAdaptiveExpansionLimit: %d\n", sniff_auto_wkp->time_sync_at_wifi.MaxAdaptiveExpansionLimit);
		xradio_dbg(XRADIO_DBG_NIY, "MaxKeepAliveTime: %d\n", sniff_auto_wkp->time_sync_at_wifi.MaxKeepAliveTime);
	}

#ifdef SUPPORT_HT40
	if (sniff_auto_wkp->Enable) {
		memset(&PhyModeCfg, 0, sizeof(struct phy_mode_cfg));
		PhyModeCfg.ModemFlags = 0x7;
		ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SET_PHY_MODE_CFG,
					(void *)&PhyModeCfg, sizeof(struct phy_mode_cfg), if_id);
	}
#endif
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SNIFF_AUTO_WAKEUP,
			    (void *)param, sizeof(wlan_ext_sniff_sync_param_set_t), if_id);
	return ret;
}

int xradio_ioctl_set_switch_ch_cfg(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_switch_channel_param_set_t switch_chn;
	wlan_ext_get_cur_channel_t cur_channel;

	memcpy(&switch_chn, (wlan_ext_switch_channel_param_set_t *)param,
		  sizeof(wlan_ext_switch_channel_param_set_t));
	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ switch channel config ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable : %d\n", switch_chn.Enable);
	xradio_dbg(XRADIO_DBG_NIY, "Band : %d\n", switch_chn.Band);
	xradio_dbg(XRADIO_DBG_NIY, "Flag : 0x%04X\n", switch_chn.Flag);
	xradio_dbg(XRADIO_DBG_NIY, "ChannelNum : %d\n", switch_chn.ChannelNum);
	xradio_dbg(XRADIO_DBG_NIY, "SwitchChannelTime : %d\n", switch_chn.SwitchChannelTime);
	if (!switch_chn.Enable) {
		xradio_dbg(XRADIO_DBG_NIY, "switch back\n");
		ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SWITCH_CHANNEL,
				    (void *)&switch_chn, sizeof(wlan_ext_switch_channel_param_set_t), if_id);
		msleep(15);
		goto out;
	}

	if (switch_chn.SwitchChannelTime > SEND_RAW_FRAME_MAX_SWITCH_CHANNEL_TIME) {
		xradio_dbg(XRADIO_DBG_ERROR, "Invalid SwitchChannelTime %d, max: %d\n",
			 switch_chn.SwitchChannelTime,
			 SEND_RAW_FRAME_MAX_SWITCH_CHANNEL_TIME);
		ret = -1;
		goto out;
	}

	if (switch_chn.ChannelNum > 14 || switch_chn.ChannelNum == 0) {
		xradio_dbg(XRADIO_DBG_ERROR, "Invalid ChannelNum %d\n",    switch_chn.ChannelNum);
		ret = -1;
		goto out;
	}

	ret = wsm_read_mib(hw_priv, WSM_MIB_ID_GUBEI_GET_CHANNEL,
			   &cur_channel, sizeof(cur_channel), 0);

	if ((u8)(cur_channel.Channel) != hw_priv->channel->hw_value) {
		//Switch back first
		xradio_dbg(XRADIO_DBG_NIY, "switch back\n");
		switch_chn.Enable = 0;
		ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SWITCH_CHANNEL,
				    (void *)&switch_chn, sizeof(wlan_ext_switch_channel_param_set_t), if_id);
		msleep(15);
	}
	if ((u8)(switch_chn.ChannelNum) != hw_priv->channel->hw_value) {
		xradio_dbg(XRADIO_DBG_NIY, "switch to %d\n", switch_chn.ChannelNum);
		switch_chn.Enable = 1;
		ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SWITCH_CHANNEL,
				    (void *)&switch_chn, sizeof(wlan_ext_switch_channel_param_set_t), if_id);
	}

out:
	return ret;
}

int xradio_ioctl_get_cur_channel(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_get_cur_channel_t cur_channel;

	ret = wsm_read_mib(hw_priv, WSM_MIB_ID_GUBEI_GET_CHANNEL,
			   &cur_channel, sizeof(cur_channel), 0);
	*(u32 *)param = cur_channel.Channel;
	xradio_dbg(XRADIO_DBG_NIY, "cur_channel = %d\n", cur_channel.Channel);

	return ret;
}

int xradio_ioctl_set_frm_filter(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_frm_filter_set_t *frm_filter = (wlan_ext_frm_filter_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ frame filter set ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Filter1Cfg: 0x%04X\n", frm_filter->Filter1Cfg);
	xradio_dbg(XRADIO_DBG_NIY, "Filter2Cfg: 0x%04X\n", frm_filter->Filter2Cfg);
	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ frame filter 1 ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Filter enable: 0x%04X\n", frm_filter->Filter1.FilterEnable);
	xradio_dbg(XRADIO_DBG_NIY, "And mask: 0x%04X\n", frm_filter->Filter1.AndOperationMask);
	xradio_dbg(XRADIO_DBG_NIY, "Or mask: 0x%04X\n", frm_filter->Filter1.OrOperationMask);
	xradio_dbg(XRADIO_DBG_NIY, "Frame Type: 0x%02X\n", frm_filter->Filter1.FrameType);
	xradio_dbg(XRADIO_DBG_NIY, "Mac mask: 0x%02X\n", frm_filter->Filter1.MacAddrMask);
	xradio_dbg(XRADIO_DBG_NIY, "Mac1 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 frm_filter->Filter1.MacAddrA1[0], frm_filter->Filter1.MacAddrA1[1],
		 frm_filter->Filter1.MacAddrA1[2], frm_filter->Filter1.MacAddrA1[3],
		 frm_filter->Filter1.MacAddrA1[4], frm_filter->Filter1.MacAddrA1[5]);
	xradio_dbg(XRADIO_DBG_NIY, "Mac2 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		frm_filter->Filter1.MacAddrA2[0], frm_filter->Filter1.MacAddrA2[1],
		frm_filter->Filter1.MacAddrA2[2], frm_filter->Filter1.MacAddrA2[3],
		frm_filter->Filter1.MacAddrA2[4], frm_filter->Filter1.MacAddrA2[5]);
	xradio_dbg(XRADIO_DBG_NIY, "Mac3 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		frm_filter->Filter1.MacAddrA3[0], frm_filter->Filter1.MacAddrA3[1],
		frm_filter->Filter1.MacAddrA3[2], frm_filter->Filter1.MacAddrA3[3],
		frm_filter->Filter1.MacAddrA3[4], frm_filter->Filter1.MacAddrA3[5]);


	if (frm_filter->Filter1.FilterEnable & RCV_FRM_FILTER_PAYLOAD) {
		xradio_dbg(XRADIO_DBG_NIY, "Payload Offset: %d\n", frm_filter->Filter1.PayloadCfg.PayloadOffset);
		xradio_dbg(XRADIO_DBG_NIY, "Payload Len: %d\n", frm_filter->Filter1.PayloadCfg.PayloadLength);
		xradio_dbg(XRADIO_DBG_NIY, "Payload: %02X%02X%02X%02X%02X%02X...\n",
			 frm_filter->Filter1.PayloadCfg.Payload[0], frm_filter->Filter1.PayloadCfg.Payload[1],
			 frm_filter->Filter1.PayloadCfg.Payload[2], frm_filter->Filter1.PayloadCfg.Payload[3],
			 frm_filter->Filter1.PayloadCfg.Payload[4], frm_filter->Filter1.PayloadCfg.Payload[5]);
	} else if (frm_filter->Filter1.FilterEnable & RCV_FRM_FILTER_IE) {
		xradio_dbg(XRADIO_DBG_NIY, "ElementId: %d\n", frm_filter->Filter1.IeCfg.ElementId);
		xradio_dbg(XRADIO_DBG_NIY, "Length: %d\n", frm_filter->Filter1.IeCfg.Length);
		xradio_dbg(XRADIO_DBG_NIY, "OUI: %02X%02X%02X...\n",
			 frm_filter->Filter1.IeCfg.OUI[0],
			 frm_filter->Filter1.IeCfg.OUI[1],
			 frm_filter->Filter1.IeCfg.OUI[2]);
	}

	xradio_dbg(XRADIO_DBG_NIY, "------------ frame filter 2 ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Filter enable: 0x%04X\n", frm_filter->Filter2.FilterEnable);
	xradio_dbg(XRADIO_DBG_NIY, "And mask: 0x%04X\n", frm_filter->Filter2.AndOperationMask);
	xradio_dbg(XRADIO_DBG_NIY, "Or mask: 0x%04X\n", frm_filter->Filter2.OrOperationMask);
	xradio_dbg(XRADIO_DBG_NIY, "Frame Type: 0x%02X\n", frm_filter->Filter2.FrameType);
	xradio_dbg(XRADIO_DBG_NIY, "Mac mask: 0x%02X\n", frm_filter->Filter2.MacAddrMask);
	xradio_dbg(XRADIO_DBG_NIY, "Mac1 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 frm_filter->Filter2.MacAddrA1[0], frm_filter->Filter2.MacAddrA1[1],
		 frm_filter->Filter2.MacAddrA1[2], frm_filter->Filter2.MacAddrA1[3],
		 frm_filter->Filter2.MacAddrA1[4], frm_filter->Filter2.MacAddrA1[5]);
	xradio_dbg(XRADIO_DBG_NIY, "Mac2 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 frm_filter->Filter2.MacAddrA2[0], frm_filter->Filter2.MacAddrA2[1],
		 frm_filter->Filter2.MacAddrA2[2], frm_filter->Filter2.MacAddrA2[3],
		 frm_filter->Filter2.MacAddrA2[4], frm_filter->Filter2.MacAddrA2[5]);
	xradio_dbg(XRADIO_DBG_NIY, "Mac3 Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 frm_filter->Filter2.MacAddrA3[0], frm_filter->Filter2.MacAddrA3[1],
		 frm_filter->Filter2.MacAddrA3[2], frm_filter->Filter2.MacAddrA3[3],
		 frm_filter->Filter2.MacAddrA3[4], frm_filter->Filter2.MacAddrA3[5]);
	if (frm_filter->Filter2.FilterEnable & RCV_FRM_FILTER_PAYLOAD) {
		xradio_dbg(XRADIO_DBG_NIY, "Payload Offset: %d\n", frm_filter->Filter2.PayloadCfg.PayloadOffset);
		xradio_dbg(XRADIO_DBG_NIY, "Payload Len: %d\n", frm_filter->Filter2.PayloadCfg.PayloadLength);
		xradio_dbg(XRADIO_DBG_NIY, "Payload: %02X%02X%02X%02X%02X%02X...\n",
			 frm_filter->Filter2.PayloadCfg.Payload[0], frm_filter->Filter2.PayloadCfg.Payload[1],
			 frm_filter->Filter2.PayloadCfg.Payload[2], frm_filter->Filter2.PayloadCfg.Payload[3],
			 frm_filter->Filter2.PayloadCfg.Payload[4], frm_filter->Filter2.PayloadCfg.Payload[5]);
	} else if (frm_filter->Filter2.FilterEnable & RCV_FRM_FILTER_IE) {
		xradio_dbg(XRADIO_DBG_NIY, "ElementId: %d\n", frm_filter->Filter2.IeCfg.ElementId);
		xradio_dbg(XRADIO_DBG_NIY, "Length: %d\n", frm_filter->Filter2.IeCfg.Length);
		xradio_dbg(XRADIO_DBG_NIY, "OUI: %02X%02X%02X...\n",
			 frm_filter->Filter2.IeCfg.OUI[0],
			 frm_filter->Filter2.IeCfg.OUI[1],
			 frm_filter->Filter2.IeCfg.OUI[2]);
	}
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_FRAME_FILTER_SET,
			    (void *)param, sizeof(wlan_ext_frm_filter_set_t), if_id);

	return ret;
}

int xradio_ioctl_set_temp_frm(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	struct sk_buff *skb;
	u8 *data;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	struct wsm_template_frame frame = {
		.frame_type = WSM_FRAME_TYPE_BEACON,
	};
	wlan_ext_temp_frm_set_t *tmp = (wlan_ext_temp_frm_set_t *)param;

	frame.frame_type |= 0x80;
	skb = xr_alloc_skb(tmp->FrmLength);
	if (!skb) {
		xradio_dbg(XRADIO_DBG_ERROR, "%s xr_alloc_skb failed!\n", __func__);
		ret = -ENOMEM;
	}
	memcpy(skb->data, tmp->Frame, tmp->FrmLength);
	data = (u8 *)skb->data;
	skb_put(skb, tmp->FrmLength);
	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ template frm set ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "frm len: %d\n", tmp->FrmLength);
	xradio_dbg(XRADIO_DBG_NIY, "frame: %02x:%02x:%02x:%02x:%02x:%02x...\n",
		*(data + 0), *(data + 1),
		*(data + 2), *(data + 3),
		*(data + 4), *(data + 5));
	frame.skb = skb;
	ret = wsm_set_template_frame(hw_priv, &frame, if_id);

	return ret;
}

int xradio_ioctl_send_sync_frm(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_send_sync_frm_set_t *sync_frm = (wlan_ext_send_sync_frm_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ sync frm set ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "Enable: 0x%X\n", sync_frm->Enable);
	xradio_dbg(XRADIO_DBG_NIY, "BcnInterval: %d\n", sync_frm->BcnInterval);
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_SEND_SYNC_FRM,
			    (void *)param, sizeof(wlan_ext_send_sync_frm_set_t), if_id);

	return ret;
}

int xradio_ioctl_set_sniff_ext_param(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	u8 if_id = 0; /* TODO: get if_id from vap */
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	wlan_ext_sniff_extern_param_set_t *ext_param = (wlan_ext_sniff_extern_param_set_t *)param;

	xradio_dbg(XRADIO_DBG_NIY, "\n");
	xradio_dbg(XRADIO_DBG_NIY, "------------ sync ext param set ------------\n");
	xradio_dbg(XRADIO_DBG_NIY, "SniffExtFuncEnable: 0x%X\n", ext_param->SniffExtFuncEnable);
	xradio_dbg(XRADIO_DBG_NIY, "SniffRetryAfterLostSync: %d\n", ext_param->SniffRetryAfterLostSync);
	xradio_dbg(XRADIO_DBG_NIY, "SniffAdaptiveExpenRight: %d\n", ext_param->SniffAdaptiveExpenRight);
	xradio_dbg(XRADIO_DBG_NIY, "SniffRetryDtim: %d\n", ext_param->SniffRetryDtim);
	ret = wsm_write_mib(hw_priv, WSM_MIB_ID_GUBEI_FUNC_PARAM_EXTERN,
			    (void *)param, sizeof(wlan_ext_sniff_extern_param_set_t), if_id);

	return ret;
}

static int xradio_get_gubei_receive_stat(struct xradio_common *hw_priv, wlan_ext_bcn_status_t *arg)
{
	return wsm_read_mib(hw_priv, WSM_MIB_ID_GUBEI_GET_RECEIVE_STAT, arg, sizeof(*arg), 0);
}

int xradio_ioctl_get_rcv_stat(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret;
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;

	ret = xradio_get_gubei_receive_stat(hw_priv, (wlan_ext_bcn_status_t *)param);

	return ret;
}

#ifdef CONFIG_XRADIO_DEBUGFS
extern u8 ps_disable;
extern u8 ps_idleperiod;
extern u8 ps_changeperiod;
extern u8 low_pwr_disable;
#endif

u8 xr_ioctl_ps_disable;
u8 xr_ioctl_low_pwr_disable;

int xradio_ioctl_set_pm(struct ieee80211_hw *hw, u8 *param, u16 len)
{
	int ret = 0;
	struct xradio_common *hw_priv = (struct xradio_common *)hw->priv;
	int if_id = 0;
	int pm_enable = *(int *)param;
	struct wsm_set_pm ps = {
		.pmMode = WSM_PSM_FAST_PS,
		.fastPsmIdlePeriod = 0xC8  /* defaut 100ms */
	};
	u32 val = wsm_power_mode_quiescent;

	xr_ioctl_ps_disable = !pm_enable;
	xr_ioctl_low_pwr_disable = !pm_enable;

#ifdef CONFIG_XRADIO_DEBUGFS
	ps_disable = xr_ioctl_ps_disable;
	low_pwr_disable = xr_ioctl_low_pwr_disable;
#endif

	/* set ps_disable */
	if (xr_ioctl_ps_disable) {
		ps.pmMode = WSM_PSM_ACTIVE;
	}

#ifdef CONFIG_XRADIO_DEBUGFS
	if (ps_idleperiod) {
		ps.fastPsmIdlePeriod = ps_idleperiod << 1;
	}
	if (ps_changeperiod) {
		ps.apPsmChangePeriod = ps_changeperiod << 1;
	}
#endif
	wsm_set_pm(hw_priv, &ps, 0);
	if (hw_priv->vif_list[1]) {
		ret = wsm_set_pm(hw_priv, &ps, 1);
	}

	/* set low_ps_able*/
	if (xr_ioctl_low_pwr_disable) {
		val = wsm_power_mode_active;
	}

	val |= BIT(4);   /* disableMoreFlagUsage */
	for (if_id = 0; if_id < xrwl_get_nr_hw_ifaces(hw_priv); if_id++) {
		ret = wsm_write_mib(hw_priv, WSM_MIB_ID_OPERATIONAL_POWER_MODE, &val,
				sizeof(val), if_id);
	}

	return ret;
}

#endif /* XRADIO_RAW_FRM_SUP */
