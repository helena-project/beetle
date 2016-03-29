/*
 * Handle.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Handle.h"

Handle::Handle(uint16_t handle_, uuid_t uuid_, uint16_t serviceHandle_, uint16_t charHandle_) :	uuid(uuid_) {
	handle = handle_;
	serviceHandle = serviceHandle_;
	charHandle = charHandle_;
}

Handle::~Handle() {

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

UUID Handle::getUuid() {
	return uuid;
}



