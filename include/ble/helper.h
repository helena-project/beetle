/*
 * helper.h
 *
 *  Created on: Mar 31, 2016
 *      Author: james
 */

#ifndef INCLUDE_BLE_HELPER_H_
#define INCLUDE_BLE_HELPER_H_

#include <bluetooth/bluetooth.h>
#include <cstdint>
#include <string>

#include "../UUID.h"
#include "att.h"

inline int pack_error_pdu(uint8_t opCode, uint16_t handle, uint8_t errCode, uint8_t *&buf) {
	buf = new uint8_t[5];
	buf[0] = ATT_OP_ERROR;
	buf[1] = opCode;
	*(uint16_t *)(buf + 2) = htobs(handle);
	buf[4] = errCode;
	return 5;
}

inline int pack_read_by_type_req_pdu(uint16_t attType, uint16_t startHandle, uint16_t endHandle, uint8_t *&buf) {
	buf = new uint8_t[7];
	buf[0] = ATT_OP_READ_BY_TYPE_REQ;
	*(uint16_t*)(buf + 1) = htobs(startHandle);
	*(uint16_t*)(buf + 3) = htobs(endHandle);
	*(uint16_t*)(buf + 5) = htobs(attType);
	return 7;
}

inline bool parse_find_info_request(uint8_t *buf, int len, uint16_t &startHandle, uint16_t &endHandle) {
	if (len < 5) {
		return false; // invalid length
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	return true;
}

inline bool parse_find_by_type_value_request(uint8_t *buf, int len, uint16_t &startHandle,
		uint16_t &endHandle, uint16_t &attType, uint8_t *&attValue, int &attValLen) {
	if (len < 7) {
		return false;
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	attType = *(uint16_t *)(buf + 5);
	attValue = buf + 7;
	attValLen = len - 7;
	return true;
}

inline bool parse_read_by_type_value_request(uint8_t *buf, int len, uint16_t &startHandle,
		uint16_t &endHandle, UUID *&uuid) {
	if (len != 7 && len != 21) {
		return false;
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	uuid = new UUID(buf + 5, len - 5);
	return true;
}

inline bool parse_read_by_group_type_request(uint8_t *buf, int len, uint16_t &startHandle,
		uint16_t &endHandle, UUID *&uuid) {
	if (len != 7 && len != 21) {
		return false;
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	uuid = new UUID(buf + 5, len - 5);
	return true;
}

inline std::string ba2str_cpp(bdaddr_t bdaddr) {
	char addr_c_str[20];
	ba2str(&bdaddr, addr_c_str);
	return std::string(addr_c_str);
}

inline bool isBdAddr(const std::string &s) {
	if (s.length() != 17) return false;
	for (int i = 0; i < (int)s.length(); i++) {
		char c = s[i];
		if (i % 3 == 2) {
			if (c != ':') return false;
		} else {
			if (!((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')
					|| (c >= '0' && c <= '9'))) return false;
		}
	}
	return true;
}


#endif /* INCLUDE_BLE_HELPER_H_ */
