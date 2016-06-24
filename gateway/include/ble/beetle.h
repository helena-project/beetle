/*
 * beetle.h
 *
 *  Created on: Jun 20, 2016
 *      Author: James Hong
 */

#ifndef BLE_BEETLE_H_
#define BLE_BEETLE_H_

/* Custom service for Beetle metadata */
#define BEETLE_SERVICE_UUID 0xBE00

/* 6 byte bdaddr characteristic */
#define BEETLE_CHARAC_BDADDR_UUID 0xBE01

/* 1 byte descriptor for the address type */
#define BEETLE_DESC_BDADDR_TYPE_UUID 0xBE02

/* The range of handles allocated to the virtual device */
#define BEETLE_CHARAC_HANDLE_RANGE_UUID 0xBE03

/* The epoch time the connection began at */
#define BEETLE_CHARAC_CONNECTED_TIME_UUID 0xBE04

/* The name of the first hop gateway */
#define BEETLE_CHARAC_CONNECTED_GATEWAY_UUID 0xBE05

#endif /* BLE_BEETLE_H_ */
