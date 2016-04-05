/*
 * Device.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Device.h"

#include <bluetooth/bluetooth.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <utility>

#include "ble/att.h"
#include "ble/gatt.h"
#include "Beetle.h"
#include "device/BeetleDevice.h"
#include "hat/HAT.h"
#include "Handle.h"

std::atomic_int Device::idCounter(1);

Device::Device(Beetle &beetle_) : beetle(beetle_) {
	id = idCounter++;
}

Device::Device(Beetle &beetle_, device_t id_) : beetle(beetle_) {
	id = id_;
}

Device::~Device() {
	for (auto &kv : handles) {
		delete kv.second;
	}
	handles.clear();

	boost::unique_lock<boost::shared_mutex> lk(beetle.hatMutex);
	handle_range_t handles = beetle.hat->free(id);
	lk.release();

	if (!HAT::isNullRange(handles)) {
		beetle.beetleDevice->servicesChanged(handles);
	}
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
