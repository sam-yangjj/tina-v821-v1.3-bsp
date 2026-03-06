/*
 * v821_wlan/aglink_txrxif.h
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * hardware interface APIs for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef _AGLINK_TXRXIF_H_
#define _AGLINK_TXRXIF_H_

int aglink_txrx_init(struct aglink_hw *aglink_hw, int dev_id);

void aglink_txrx_deinit(struct aglink_hw *aglink_hw);

void aglink_wake_up_data_tx_work(struct aglink_hw *aglink_hw);

void aglink_wake_up_data_rx_work(struct aglink_hw *aglink_hw);

#endif
