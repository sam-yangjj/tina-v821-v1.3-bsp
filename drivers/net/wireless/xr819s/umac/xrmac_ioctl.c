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

#include "ieee80211_i.h"
#include "xrmac_ioctl.h"


#ifdef XRADIO_RAW_FRM_SUP

static int xrmac_set_rcv_special_frm(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_rcv_special_frm) {
		ret = local->ioctl_ops->set_rcv_special_frm(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_rcv_special_frm is null!\n");
	}

	return ret;
}

static int xrmac_set_send_raw_frm_cfg(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_send_raw_frm_cfg) {
		ret = local->ioctl_ops->set_send_raw_frm_cfg(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_send_raw_frm_cfg is null!\n");
	}

	return ret;
}

static int xrmac_set_sniff_kp_active(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_sniff_kp_active) {
		ret = local->ioctl_ops->set_sniff_kp_active(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_sniff_kp_active is null!\n");
	}

	return ret;
}

static int xrmac_set_sniff_auto_wakeup(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_sniff_auto_wakeup) {
		ret = local->ioctl_ops->set_sniff_auto_wakeup(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_sniff_auto_wakeup is null!\n");
	}

	return ret;
}

static int xrmac_set_switch_ch_cfg(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_switch_ch_cfg) {
		ret = local->ioctl_ops->set_switch_ch_cfg(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_switch_ch_cfg is null!\n");
	}

	return ret;
}

static int xrmac_get_cur_channel(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->get_cur_channel) {
		ret = local->ioctl_ops->get_cur_channel(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: get_cur_channel is null!\n");
	}

	return ret;
}

static int xrmac_set_frm_filter(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_frm_filter) {
		ret = local->ioctl_ops->set_frm_filter(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_frm_filter is null!\n");
	}

	return ret;
}

static int xrmac_set_temp_frm(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_temp_frm) {
		ret = local->ioctl_ops->set_temp_frm(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_temp_frm is null!\n");
	}

	return ret;
}

static int xrmac_send_sync_frm(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->send_sync_frm) {
		ret = local->ioctl_ops->send_sync_frm(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: send_sync_frm is null!\n");
	}

	return ret;
}

static int xrmac_set_sniff_ext_param(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_sniff_ext_param) {
		ret = local->ioctl_ops->set_sniff_ext_param(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: set_sniff_ext_param is null!\n");
	}

	return ret;
}

static int xrmac_get_rcv_stat(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->get_rcv_stat) {
		ret = local->ioctl_ops->get_rcv_stat(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "[XRADIO-RAW]: get_rcv_stat is null!\n");
	}

	return ret;
}

int xrmac_set_raw_frm_tx_status(struct ieee80211_vif *vif, u8 status)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);

	if (!sdata) {
		printk(KERN_ERR "[XRADIO-RAW]: sdata is null.\n");
		return -1;
	}

	if (!status) {
		sdata->raw_state = SDATA_RAW_TX_STATE_SUCCESS;
	} else {
		sdata->raw_state = SDATA_RAW_TX_STATE_FAILED;
	}
	wake_up(&sdata->tx_raw_wq);

	return 0;
}
EXPORT_SYMBOL_GPL(xrmac_set_raw_frm_tx_status);

static int xrmac_get_raw_frm_tx_status(struct ieee80211_sub_if_data *sdata)
{
	int ret = -1;

	if (!sdata || !sdata->raw_tx_timeout) {
		printk(KERN_ERR "[XRADIO-RAW]: get tx status failed\n");
		return -1;
	}

	ret = wait_event_timeout(sdata->tx_raw_wq, !(sdata->raw_state == SDATA_RAW_TX_STATE_TXING),
				sdata->raw_tx_timeout);
	if (likely(ret > 0)) {
		if (sdata->raw_state == SDATA_RAW_TX_STATE_SUCCESS) {
			ret = 0;
		} else if(sdata->raw_state == SDATA_RAW_TX_STATE_FAILED) {
			ret = -2;
		} else  {
			printk(KERN_WARNING "[XRADIO-RAW]: tx expectations.\n");
			ret = -3;
		}
	} else {
		sdata->fourway_state = SDATA_RAW_TX_STATE_TXED;
		printk(KERN_WARNING "[XRADIO-RAW]: tx raw frame timeout.\n");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int xrmac_send_raw_frm(struct net_device *dev, u8 *param, u16 len)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	struct ieee80211_local *local = sdata->local;
	int ret = 0;
	struct sk_buff *skb = NULL;

	if (sdata->raw_tx_mode == SDATA_RAW_TX_MODE_BLOCKING)
		down(&sdata->tx_raw_lock);

	if (len < sizeof(struct ieee80211_hdr_3addr)) {
		printk(KERN_ERR "[XRADIO-RAW]: %s skb len too short! len = %d\n", __func__, len);
		ret = -1;
	}

	skb = dev_alloc_skb(local->hw.extra_tx_headroom + IEEE80211_WEP_IV_LEN +
					24 + 6 + len + IEEE80211_WEP_ICV_LEN);
	if (!skb) {
		printk(KERN_ERR "[XRADIO-RAW]: %s xr_alloc_skb failed!\n", __func__);
		ret = -ENOMEM;
	}

	skb_reserve(skb, local->hw.extra_tx_headroom);
	memcpy(skb->data, param, len);
	skb_put(skb, len);
	skb->cb[sizeof(skb->cb) -1] = 1;   // raw_frame_flag
	IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_INTFL_DONT_ENCRYPT;
	sdata->raw_state = SDATA_RAW_TX_STATE_TXING;
	ieee80211_tx_skb(sdata, skb);

	if (sdata->raw_tx_mode == SDATA_RAW_TX_MODE_BLOCKING) {
		ret = xrmac_get_raw_frm_tx_status(sdata);
		up(&sdata->tx_raw_lock);
	}

	sdata->raw_state = SDATA_RAW_TX_STATE_TXED;

	return ret;
}

static int xrmac_set_pm(struct ieee80211_local *local, u8 *param, u16 len)
{
	struct ieee80211_hw *hw = local_to_hw(local);
	int ret;

	if (hw && local->ioctl_ops->set_pm) {
		ret = local->ioctl_ops->set_pm(hw, param, len);
	} else {
		ret = -1;
		printk(KERN_ERR "set_pm is null!\n");
	}

	return ret;
}

static int xrmac_raw_set_tx_mode(struct net_device *dev, u8 *param, u16 len)
{
	struct ieee80211_sub_if_data *sdata = IEEE80211_DEV_TO_SUB_IF(dev);
	int ret = 0;
	struct raw_tx_mode_s *raw_tx_mode;

	if (!sdata) {
		printk(KERN_ERR "[XRADIO-RAW]: sdata is null.\n");
		return -1;
	}

	if (len >= sizeof(struct raw_tx_mode_s)) {
		raw_tx_mode = (struct raw_tx_mode_s *)param;
		sdata->raw_tx_mode = raw_tx_mode->tx_mode;

		if (raw_tx_mode && (raw_tx_mode->tx_timeout >= RAW_TX_TIMEOUT_MIN && raw_tx_mode->tx_timeout <= RAW_TX_TIMEOUT_MAX))
			sdata->raw_tx_timeout = ((unsigned long)raw_tx_mode->tx_timeout * HZ) / 1000;
	}
	printk(KERN_DEBUG "[XRADIO-RAW]: tx_mode = %s,  tx_timeout = %u (HZ = %u).\n",
		raw_tx_mode? XR_STRINGIFY(SDATA_RAW_TX_MODE_BLOCKING) : XR_STRINGIFY(SDATA_RAW_TX_MODE_NORMAL),
		sdata->raw_tx_timeout, HZ);

	return ret;
}

int xrmac_ndo_do_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	u16 len;
	int ret = 0;
	u8 buf[1024];
	u8 cnf_buf[64];
	CMD_REQ_HDR *cmdreq;
	CMD_CNF_HDR *cmdcnf;
	u8 *msg_data;
	u8 cnf_param;
	struct ieee80211_sub_if_data *sdata = netdev_priv(dev);  //IEEE80211_DEV_TO_SUB_IF
	struct ieee80211_local *local = sdata->local;

	if (!sdata) {
		printk(KERN_ERR "[XRADIO-RAW]: %s: sdata is null!\n", __func__);
		ret = -1;
		goto out;
	}

	memset(buf, 0, 1024);
	memset(cnf_buf, 0, 64);
	cnf_param = 0;

	///TODO: add ioctl command handler later
	switch (cmd) {
	case (SIOCDEVPRIVATE+1):

		ret = copy_from_user(&len, req->ifr_data, 2);
		if (ret || len > 1020) {
			printk(KERN_ERR "[XRADIO-RAW]: %s: invalid msg len: %d, ret = %d\n", __func__, len, ret);
			break;
		}

		ret = copy_from_user(buf, req->ifr_data, len+4);
		if (ret) {
			printk(KERN_ERR "[XRADIO-RAW]: copy_from_user error, ret = %d", ret);
			return -1;
		}

		cmdreq = (CMD_REQ_HDR *)buf;
		msg_data = (u8 *)cmdreq->data;
		len = cmdreq->len;
		printk(KERN_DEBUG "[XRADIO-RAW]: %s recv msg: Len = %d, MsgId = %d\n", __func__, len, cmdreq->msgId);

		switch (cmdreq->msgId) {
		case WLAN_EXT_CMD_SET_RCV_SPECIAL_FRM: {
			ret = xrmac_set_rcv_special_frm(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_SEND_RAW_FRM_CFG: {
			ret = xrmac_set_send_raw_frm_cfg(local, msg_data, len);
			break;
		}

		case WLAN_EXT_CMD_SET_SNIFF_SYNC_CFG: {
			ret = xrmac_set_sniff_auto_wakeup(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_SWITCH_CHN_CFG: {
			ret = xrmac_set_switch_ch_cfg(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_GET_CURRENT_CHN: {
			ret = xrmac_get_cur_channel(local, msg_data, len);
			cnf_param = 4;
			break;
		}
		case WLAN_EXT_CMD_SET_SNIFF_KP_ACTIVE: {
			ret = xrmac_set_sniff_kp_active(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_FRM_FILTER: {
			ret = xrmac_set_frm_filter(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_TEMP_FRM: {
			ret =  xrmac_set_temp_frm(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_SYNC_FRM_SEND: {
			ret =  xrmac_send_sync_frm(local, msg_data, len);
			break;
		}
		case WLAN_EXT_CMD_SET_SNIFF_EXTERN_CFG: {
			ret =  xrmac_set_sniff_ext_param(local, msg_data, len);
			break;
		}

		case WLAN_EXT_CMD_GET_SNIFF_STAT: {
			ret = xrmac_get_rcv_stat(local, msg_data, len);

			break;
		}
		case WLAN_EXT_CMD_STOP_RCV_SPECIAL_FRM: {
			ret = -EOPNOTSUPP;
			break;
		}

		case WLAN_EXT_CMD_SEND_RAW_FRM: {
			ret = xrmac_send_raw_frm(dev, msg_data, len);
			break;
		}

		case WLAN_EXT_CMD_SET_PM: {
			ret = xrmac_set_pm(local, msg_data, len);
			break;
		}

		case WLAN_EXT_CMD_SET_TX_MODE: {
			ret = xrmac_raw_set_tx_mode(dev, msg_data, len);
			break;
		}

		default:
			printk(KERN_ERR "[XRADIO-RAW]: %s The request is not supported: MsgId = %d\n",
				__func__, cmdreq->msgId);
			ret = -EOPNOTSUPP;
		}

		//printk(KERN_ERR "\n*******%s ret = %d*******\n", __func__, ret);
out:
		cmdcnf = (CMD_CNF_HDR *)cnf_buf;
		cmdcnf->len = cnf_param + sizeof(ret);
		cmdcnf->msgId = cmdreq->msgId | IOCTL_CNF_BASE;
		cmdcnf->result = ret;

		if (cnf_param > 0 && cnf_param < 56) {
			memcpy(cmdcnf->data, msg_data, cnf_param);
		}

		if (copy_to_user(req->ifr_data, cnf_buf, sizeof(cnf_buf))) {
			printk(KERN_ERR "[XRADIO-RAW]: %s copy_to_user err, CnfId = %d, len = %d, ret = %d\n",
				__func__, cmdcnf->msgId, cmdcnf->len, ret);
		}

		break;
	default:
		ret = -EOPNOTSUPP;
	}
	return ret;
}

#endif /* XRADIO_RAW_FRM_SUP */
