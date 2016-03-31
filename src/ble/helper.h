/*
 * helper.h
 *
 *  Created on: Mar 31, 2016
 *      Author: james
 */

#ifndef BLE_HELPER_H_
#define BLE_HELPER_H_

#include <cstdint>
#include <bluetooth/bluetooth.h>

#include "att.h"

inline int pack_error_pdu(uint8_t opCode, uint16_t handle, uint8_t errCode, uint8_t *&buf) {
	buf = new uint8_t[5];
	buf[0] = ATT_OP_ERROR;
	buf[1] = opCode;
	*(uint16_t *)(buf + 2) = htobs(handle);
	buf[4] = errCode;
	return 5;
}



#endif /* BLE_HELPER_H_ */
