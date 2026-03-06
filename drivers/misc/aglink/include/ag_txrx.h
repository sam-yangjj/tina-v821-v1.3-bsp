#ifndef __AGLINK_AGLINK_PROTO_H
#define __AGLINK_AGLINK_PROTO_H

#define AG_LINK_TX_MAX_LEN 2048

typedef int (*ag_rx_cb)(int dev_id, u8 type, u8 *buff, u16 len);

void aglink_register_rx_cb(ag_rx_cb cb);

int aglink_tx_data(int dev_id, u8 type, u8 *buff, u16 len);

int aglink_get_device_id(int dev_id);

const char *aglink_ad_type_to_string(uint8_t type);

const char *aglink_vd_type_to_string(uint8_t type);

const char *aglink_mode_to_string(uint8_t mode);

void aglink_flow_ctl_timeout_set(int dev_id, u8 time);
#endif
