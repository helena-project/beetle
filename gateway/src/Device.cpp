/*
 * Device.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "Device.h"

#include <bluetooth/bluetooth.h>
#include <cstring>
#include <set>
#include <utility>

#include "Beetle.h"
#include "ble/att.h"
#include "Handle.h"
#include "hat/BlockAllocator.h"
#include "UUID.h"

std::atomic_int Device::idCounter(1);

const std::string Device::deviceType2Str[] = {
		"BeetleInternal", 	// 0
		"LePeripheral", 	// 1
		"TcpClient",		// 2
		"IpcApplication",	// 3
		"TcpClientProxy",	// 4
		"TcpServerProxy",	// 5
};

Device::Device(Beetle &beetle_, HandleAllocationTable *hat_) :
		Device(beetle_, idCounter++, hat_) {
}

Device::Device(Beetle &beetle_, device_t id_, HandleAllocationTable *hat_) :
		hat(hat_), beetle(beetle_) {
	id = id_;
	if (!hat) {
		hat = std::make_unique<BlockAllocator>(256);
	}
	type = UNKNOWN;
}

Device::~Device() {

}

device_t Device::getId() {
	return id;
}

std::string Device::getName() {
	return name;
}

Device::DeviceType Device::getType(){
	return type;
}

std::string Device::getTypeStr() {
	return deviceType2Str[type];
}

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
	std::map<uint16_t,uint8_t> charCccdsToWrite;
	for (auto &kv : handles) {
		auto cvH = std::dynamic_pointer_cast<CharacteristicValue>(kv.second);
		if (cvH) {
			uint8_t initialState = 0;
			initialState += (cvH->subscribersNotify.empty()) ? 0 : 1;
			initialState += (cvH->subscribersIndicate.empty()) ? 0 : 1 << 1;

			cvH->subscribersNotify.erase(d);
			cvH->subscribersIndicate.erase(d);

			uint8_t newState = 0;
			newState += (cvH->subscribersNotify.empty()) ? 0 : 1;
			newState += (cvH->subscribersIndicate.empty()) ? 0 : 1 << 1;

			if (type != BEETLE_INTERNAL && newState != initialState) {
				charCccdsToWrite[cvH->getCharHandle()] = newState;
			}
		}
		/*
		 * Makes assumption that cccd follows attribute value
		 */
		auto cccdH = std::dynamic_pointer_cast<ClientCharCfg>(kv.second);
		if (cccdH && type != BEETLE_INTERNAL && charCccdsToWrite.find(cccdH->getCharHandle()) != charCccdsToWrite.end()) {
			int reqLen = 5;
			uint8_t req[reqLen];
			memset(req, 0, reqLen);
			req[0] = ATT_OP_WRITE_REQ;
			*(uint16_t *) (req + 1) = htobs(cccdH->getHandle());
			uint16_t charHandle = cccdH->getCharHandle();
			req[3] = (charCccdsToWrite[charHandle] & 1) ? 1 : 0;
			req[4] = (charCccdsToWrite[charHandle] & (1 << 1)) ? 1 : 0;
			writeTransaction(req, reqLen, [](uint8_t *resp, int respLen) {});
		}
	}
}
