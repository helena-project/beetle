/*
 * Handle.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Handle.h"

Handle::Handle(uint16_t handle, UUID uuid, uint16_t serviceHandle, uint16_t charHandle) {
	Handle::handle = handle;
	Handle::uuid = uuid;
	Handle::serviceHandle = serviceHandle;
	Handle::charHandle = charHandle;
}

uint16_t Handle::getCharHandle() {
	return charHandle;
}

uint16_t Handle::getHandle() {
	return handle;
}

uint16_t Handle::getServiceHandle() {
	return serviceHandle;
}

std::set<device_t>& Handle::getSubscribers() {
	return subscribers;
}

const UUID& Handle::getUuid() {
	return uuid;
}



