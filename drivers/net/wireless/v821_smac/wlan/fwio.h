/*
 * Firmware APIs for XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FWIO_H_INCLUDED
#define FWIO_H_INCLUDED

#define XR819_HW_REV0       (8190)
#define XR819_HW_REV1       (8191)
#define V821_HW_REV0        (8210)
#define V821B_HW_REV0       (8211)

#ifdef USE_VFS_FIRMWARE
#define XR819_BOOTLOADER    ("/vendor/etc/firmware/boot_xr819.bin")
#define XR819_FIRMWARE      ("/vendor/etc/firmware/fw_xr819.bin")
#define XR819_SDD_FILE      ("/vendor/etc/firmware/sdd_xr819.bin")
#define XR819_ETF_FIRMWARE   ("/vendor/etc/firmware/etf_xr819.bin")

#define XR819S_BOOTLOADER    ("/vendor/etc/firmware/boot_xr819s.bin")
#define XR819S_FIRMWARE      ("/vendor/etc/firmware/fw_xr819s.bin")
#define XR819S_SDD_FILE      ("/vendor/etc/firmware/sdd_xr819s.bin")
#define XR819S_ETF_FIRMWARE   ("/vendor/etc/firmware/etf_xr819s.bin")
#else

#ifdef CONFIG_DRIVER_V821
#define V821_BOOTLOADER    ("boot_v821.bin")
#define V821_FIRMWARE      ("fw_mac_v821.bin")
#define V821_SDD_FILE      ("sdd_v821.bin")
#define V821_ETF_FIRMWARE   ("etf_v821.bin")

#define V821B_BOOTLOADER    ("boot_v821b.bin")
#define V821B_FIRMWARE      ("fw_mac_v821b.bin")
#define V821B_SDD_FILE      ("sdd_v821b.bin")
#define V821B_ETF_FIRMWARE   ("etf_v821b.bin")
#endif

#define XR819_BOOTLOADER    ("boot_xr819.bin")
#define XR819_FIRMWARE      ("fw_xr819.bin")
#define XR819_SDD_FILE      ("sdd_xr819.bin")
#define XR819_ETF_FIRMWARE   ("etf_xr819.bin")

#define XR819S_BOOTLOADER    ("boot_xr819s.bin")
#define XR819S_FIRMWARE      ("fw_xr819s.bin")
#define XR819S_SDD_FILE      ("sdd_xr819s.bin")
#define XR819S_ETF_FIRMWARE   ("etf_xr819s.bin")
#endif

#define SDD_PTA_CFG_ELT_ID                  0xEB
#define SDD_REFERENCE_FREQUENCY_ELT_ID      0xC5
#define SDD_MAX_OUTPUT_POWER_2G4_ELT_ID     0xE3
#define SDD_MAX_OUTPUT_POWER_5G_ELT_ID      0xE4
#define SDD_XTAL_TRIM_ELT_ID                0xC9

#define SDD_MAX_SIZE                        (1024)

/* Special Section */

//Element ID
#define SDD_VERSION_ELT_ID                  0xC0
#define SDD_SECTION_HEADER_ELT_ID           0xC1
#define SDD_END_OF_CONTENT_ELT_ID           0xFE
#define SDD_LAST_SECT_ID                    0xFF

/* WLAN BLE Static Section */

//Section ID
#define SDD_WLAN_STATIC_SECT_ID             0x00

//Element ID
#define SDD_COUNTRY_INFO_ELT_ID             0xDF
#define SDD_WAKEUP_MARGIN_ELT_ID            0xE9


/* WLAN Dynamic Section */

//Section ID
#define SDD_WLAN_DYNAMIC_SECT_ID            0x01

//Element ID
#define SDD_REF_CLK_OFFSET_ELT_ID           0xCA
#define SDD_XTAL_TRIM_TEMPERATURE_ELT_ID    0xF1
#define SDD_XTAL_TRIM_CALIB_PARAMS_ELT_ID   0xF2

/* WLAN PHY Section */

//Section ID
#define SDD_WLAN_PHY_SECT_ID                0x03

//Element ID
#define SDD_USER_POWER_2G4_ELT_ID           0xB0
#define SDD_USER_POWER_5G_ELT_ID            0xB1
#define SDD_STD_RSSI_COMP_ELT_ID            0xB2


/* BT BLE PHY Section */

//Section ID
#define SDD_BT_BLE_PHY_SECT_ID              0x13

//Element ID
#define SDD_POWER_BO_VOLT_2G4_ELT_ID        0x20
#define SDD_POWER_BO_TEMP_2G4_ELT_ID        0x22

/* WLAN BT BLE ANT Section */

//Section ID
#define SDD_WLAN_BT_BLE_ANT_SECT_ID         0x06

//Element ID
#define SDD_ANT_MOD_ELT_ID                  0x11

#define DCXO_TRIM_KEY  0x12345600
#define DCXO_TRIM_ADDR 0x09002700

#define FIND_NEXT_SEC(e) (struct sdd_sec_header *)((u8 *)&e->type + e->sec_length)
#define FIND_SEC_ELT(s) (struct xradio_sdd *)((u8 *)&s->type + 2 + s->ie_length)

#define FIELD_OFFSET(type, field) ((u8 *)&((type *)0)->field - (u8 *)0)
#define FIND_NEXT_ELT(e) (struct xradio_sdd *)((u8 *)&e->data + e->length)
struct xradio_sdd {
	u8 id;
	u8 length;
	u8 data[];
};

struct sdd_sec_header {
	u8 type;
	u8 ie_length;
	u8 id;
	u8 major_ver;
	u8 minor_ver;
	u8 reserve;
	u16 sec_length;
};

struct xradio_common;
int xradio_load_firmware(struct xradio_common *hw_priv);
int xradio_dev_deinit(struct xradio_common *hw_priv);
#ifdef CONFIG_FW_TXRXBUF_USE_COEXIST_RAM
int xradio_fw_txrxbuf_load_epta_section(struct xradio_common *hw_priv);
#endif

#endif
