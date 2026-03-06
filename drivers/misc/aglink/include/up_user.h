/*
 * aglink/up_user.h
 *
 * Copyright (c) 2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * laumy <liumingyuan@allwinnertech.com>
 *
 * usrer protocol flow APIs for drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef __XR_UP_CMD_H__
#define __XR_UP_CMD_H__

int aglink_up_user_init(int dev_id);
int aglink_up_user_deinit(int dev_id);
int aglink_up_user_push(int dev_id, u8 type, u8 *buf, u32 len);
void aglink_up_user_save_ad_time(struct ag_time *time);

#endif
