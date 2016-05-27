/*
 * UUID.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "UUID.h"

#include <algorithm>
#include <assert.h>
#include <boost/format.hpp>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "ble/gatt.h"

static inline void backward_memcpy(uint8_t *dst, uint8_t *src, int len) {
	for (int i = 0; i < len; i++) {
		dst[i] = src[len - 1 - i];
	}
}

UUID::UUID() {
	uuid = {0};
}

UUID::UUID(uint8_t *value, size_t len, bool reversed) {
	if (len != SHORT_UUID_LEN && len != UUID_LEN) {
		throw std::invalid_argument("invalid uuid");
	}
	if (len == SHORT_UUID_LEN) {
		memset(uuid.value, 0, UUID_LEN);
		if (reversed) {
			backward_memcpy(uuid.value + 2, value, len);
		} else {
			memcpy(uuid.value + 2, value, len);
		}
		memcpy(uuid.value + 4, BLUETOOTH_BASE_UUID, 12);
	} else {
		if (reversed) {
			backward_memcpy(uuid.value, value, len);
		} else {
			memcpy(uuid.value, value, len);
		}
	}
}

UUID::UUID(std::string s) {
	s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
	if (s.length() == SHORT_UUID_LEN * 2) {
		memset(uuid.value, 0, UUID_LEN);
		memcpy(uuid.value + 4, BLUETOOTH_BASE_UUID, 12);
		for (char i = 0; i < (int) s.length(); i += 2) {
			uuid.value[i / 2] = (char) std::stoi(s.substr(i, 2), NULL, 16);
		}
	} else if (s.length() == UUID_LEN * 2) {
		for (int i = 0; i < (int) s.length(); i += 2) {
			uuid.value[i / 2] = (char) std::stoi(s.substr(i, 2), NULL, 16);
		}
	} else {
		throw std::invalid_argument("invalid uuid");
	}
}

UUID::UUID(uint16_t val) {
	memset(uuid.value, 0, UUID_LEN);
	uuid.value[2] = val >> 8;
	uuid.value[3] = val;
	memcpy(uuid.value + 4, BLUETOOTH_BASE_UUID, 12);
}

UUID::~UUID() {

}

uint16_t UUID::getShort() const {
	return (uuid.value[2] << 8) + uuid.value[3];
}

bool UUID::isShort() const {
	return uuid.value[0] == 0 && uuid.value[1] == 0 && memcmp(uuid.value + 4, BLUETOOTH_BASE_UUID, 12) == 0;
}

std::string UUID::str(bool forceLong) const {
	std::stringstream ss;
	if (!forceLong && isShort()) {
		ss << boost::format("%02X") % static_cast<int>(uuid.value[2]);
		ss << boost::format("%02X") % static_cast<int>(uuid.value[3]);
	} else {
		for (int i = 0; i < UUID_LEN; i++) {
			if (i == 4 || i == 6 || i == 8 || i == 10) {
				ss << '-';
			}
			ss << boost::format("%02X") % static_cast<int>(uuid.value[i]);
		}
	}
	return ss.str();
}
