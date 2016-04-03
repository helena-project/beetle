/*
 * BeetleDevice.cpp
 *
 *  Created on: Apr 3, 2016
 *      Author: root
 */

#include "BeetleDevice.h"

BeetleDevice::BeetleDevice(Beetle &beetle) : Device(beetle) {
	id = BEETLE_RESERVED_DEVICE;
	name = "Beetle";
	type = "Beetle";
}

BeetleDevice::~BeetleDevice() {

}

bool BeetleDevice::writeResponse(uint8_t *buf, int len) {
	throw DeviceException(getName() + " does not make requests");
}

bool BeetleDevice::writeCommand(uint8_t *buf, int len) {
	// TODO implement beetle local write
	return true;
}

bool BeetleDevice::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	uint8_t *resp;
	int respLen = writeTransactionBlocking(buf, len, resp);
	cb(resp, respLen);
	return true;
}

int BeetleDevice::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	// TODO implement beetle local transaction
	return 0;
}

