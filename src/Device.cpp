/*
 * Device.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Device.h"

#include <bluetooth/bluetooth.h>
#include <cstring>
#include <set>
#include <utility>

#include "ble/att.h"
#include "ble/gatt.h"
#include "hat/BlockAllocator.h"

std::atomic_int Device::idCounter(1);

const std::string Device::deviceType2Str[] = {
		"BeetleInternal", 	// 0
		"LePeripheral", 	// 1
		"TcpConnection",	// 2
		"IpcApplication",	// 3
		"TcpClientProxy",	// 4
		"TcpServerProxy",	// 5
};

Device::Device(Beetle &beetle_, HandleAllocationTable *hat_) : Device(beetle_, idCounter++, hat_) {};

Device::Device(Beetle &beetle_, device_t id_, HandleAllocationTable *hat_) : beetle(beetle_) {
	id = id_;
	hat = hat_;
	if (!hat) {
		hat = new BlockAllocator(256);
	}
	type = UNKNOWN;
}

Device::~Device() {
	for (auto &kv : handles) {
		delete kv.second;
	}
	handles.clear();
	delete hat;
}

device_t Device::getId() {
	return id;
};

std::string Device::getName() {
	return name;
};

Device::DeviceType Device::getType() {
	return type;
};

std::string Device::getTypeStr() {
	return deviceType2Str[type];
};

int Device::getHighestHandle() {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	if (handles.size() > 0) {
		return handles.rbegin()->first;
	} else {
		return -1;
	}
}

void Device::unsubscribeAll(device_t d) {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	for (auto &kv : handles) {
		Handle *handle = kv.second;
		if (handle->getUuid().isShort() &&
				handle->getUuid().getShort() == GATT_CLIENT_CHARAC_CFG_UUID &&
				handle->subscribers.find(d) != handle->subscribers.end()) {
			handle->subscribers.erase(d);
			if (handle->subscribers.size() == 0) {
				int reqLen = 5;
				uint8_t req[reqLen];
				memset(req, 0, reqLen);
				req[0] = ATT_OP_WRITE_REQ;
				*(uint16_t *)(req + 1) = htobs(handle->getHandle());
				writeTransaction(req, reqLen, [](uint8_t *resp, int respLen) -> void {});
			}
		}
	}
}
