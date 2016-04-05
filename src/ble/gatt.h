/*
 * gatt.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef BLE_GATT_H_
#define BLE_GATT_H_

/* GATT Profile Attribute types */
#define GATT_PRIM_SVC_UUID		0x2800
#define GATT_SND_SVC_UUID		0x2801
#define GATT_INCLUDE_UUID		0x2802
#define GATT_CHARAC_UUID		0x2803

/* GATT Characteristic Types */
#define GATT_CHARAC_DEVICE_NAME			0x2A00
#define GATT_CHARAC_APPEARANCE			0x2A01
#define GATT_CHARAC_PERIPHERAL_PRIV_FLAG	0x2A02
#define GATT_CHARAC_RECONNECTION_ADDRESS	0x2A03
#define GATT_CHARAC_PERIPHERAL_PREF_CONN	0x2A04
#define GATT_CHARAC_SERVICE_CHANGED		0x2A05

/* GATT Characteristic Descriptors */
#define GATT_CHARAC_EXT_PROPER_UUID	0x2900
#define GATT_CHARAC_USER_DESC_UUID	0x2901
#define GATT_CLIENT_CHARAC_CFG_UUID	0x2902
#define GATT_SERVER_CHARAC_CFG_UUID	0x2903
#define GATT_CHARAC_FMT_UUID		0x2904
#define GATT_CHARAC_AGREG_FMT_UUID	0x2905
#define GATT_CHARAC_VALID_RANGE_UUID	0x2906
#define GATT_EXTERNAL_REPORT_REFERENCE	0x2907
#define GATT_REPORT_REFERENCE		0x2908

/* Client Characteristic Configuration bit field */
#define GATT_CLIENT_CHARAC_CFG_NOTIF_BIT	0x0001
#define GATT_CLIENT_CHARAC_CFG_IND_BIT		0x0002

/* Characteristic Property Bits */
#define GATT_CHARAC_PROP_BCAST 	0x01
#define GATT_CHARAC_PROP_READ	0x02
#define GATT_CHARAC_PROP_WRITE_NR	0x04
#define GATT_CHARAC_PROP_WRITE	0x08
#define GATT_CHARAC_PROP_NOTIFY	0x10
#define GATT_CHARAC_PROP_IND 	0x20
#define GATT_CHARAC_PROP_AUTH_SIGN_WRITE	0x40
#define GATT_CHARAC_PROP_EXT	0x80

/* GAP Service UUIDs */
#define GATT_GAP_SERVICE_UUID 	0x1800
#define GATT_GAP_CHARAC_DEVICE_NAME_UUID	 0x2A00
#define GATT_GAP_CHARAC_PERIPH_PRIV_FLAG_UUID	0x2A02
#define GATT_GAP_CHARAC_PREF_CONN_PARAMS_UUID	0x2A04

/* GATT Service UUIDs */
#define GATT_GATT_SERVICE_UUID 	0x1801
#define GATT_GATT_CHARAC_SERVICE_CHANGED_UUID	0x2A05

/* Base UUID */
const uint8_t BLUETOOTH_BASE_UUID[16] = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

#endif /* BLE_GATT_H_ */
