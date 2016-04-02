/*
 * UUID.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "UUID.h"

#include <assert.h>
#include <stddef.h>
#include <cstdint>
#include <cstring>

const int UUID_LEN = 16;
const int SHORT_UUID_LEN = 2;
const uint8_t BLUETOOTH_BASE_UUID[16] = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

UUID::UUID() {
	uuid = {0};
}

UUID::UUID(uint8_t *buf, size_t len) {
	assert(len == SHORT_UUID_LEN || len == UUID_LEN);
	if (len == SHORT_UUID_LEN) {
		memset(uuid.buf, 0, UUID_LEN);
		memcpy(uuid.buf + 2, buf, len);
		memcpy(uuid.buf + 4, BLUETOOTH_BASE_UUID, 12);
	} else {
		memcpy(uuid.buf, buf, UUID_LEN);
	}
}

UUID::UUID(uuid_t uuid_) {
	uuid = uuid_;
}

UUID::UUID(uint16_t val) {
	memset(uuid.buf, 0, UUID_LEN);
	memcpy(uuid.buf + 2, &val, 2);
	memcpy(uuid.buf + 4, BLUETOOTH_BASE_UUID, 12);
}

UUID::~UUID() {

}

uuid_t UUID::get() {
	return uuid;
}

uint16_t UUID::getShort() {
	return *(uint16_t *)(uuid.buf + 2);
}

bool UUID::isShort() {
	return memcmp(uuid.buf + 4, BLUETOOTH_BASE_UUID, 12);
}

