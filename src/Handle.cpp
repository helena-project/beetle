/*
 * Handle.cpp
 *
 *  Created on: Apr 2, 2016
 *      Author: root
 */

#include "Handle.h"

#include <sstream>

#include "ble/gatt.h"
#include "Debug.h"

void CachedHandle::set(uint8_t *value, int len) {
	if (this->value != NULL) {
		delete[] this->value;
	}
	this->value = value;
	this->len = len;
	::time(&(this->time));
}

Handle::Handle() {

}

Handle::~Handle() {

}

bool Handle::isCacheInfinite() {
	return cacheInfinite;
}

void Handle::setCacheInfinite(bool cacheInfinite) {
	this->cacheInfinite = cacheInfinite;
}

uint16_t Handle::getCharHandle() {
	return charHandle;
}

void Handle::setCharHandle(uint16_t charHandle) {
	this->charHandle = charHandle;
}

uint16_t Handle::getEndGroupHandle() {
	return endGroupHandle;
}

void Handle::setEndGroupHandle(uint16_t endGroupHandle) {
	this->endGroupHandle = endGroupHandle;
}

uint16_t Handle::getHandle() {
	return handle;
}

void Handle::setHandle(uint16_t handle) {
	this->handle = handle;
}

uint16_t Handle::getServiceHandle() {
	return serviceHandle;
}

void Handle::setServiceHandle(uint16_t serviceHandle) {
	this->serviceHandle = serviceHandle;
}

UUID& Handle::getUuid() {
	return uuid;
}

void Handle::setUuid(UUID uuid_) {
	uuid = uuid_;
}

std::string Handle::str() {
	std::stringstream ss;
	ss << handle << "\t" << uuid.str() << '\t' << serviceHandle
			<< '\t' << charHandle << "\tcache: [";
	for (int i = 0; i < cache.len; i++) {
		ss << (unsigned int) cache.value[i];
		if (i < cache.len - 1) {
			ss << ',';
		}
	}
	ss << ']';

	if (uuid.isShort() && uuid.getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
		ss << "\tsubscribers:" << subscribers.size();
	}
	return ss.str();
}

PrimaryService::PrimaryService() {
	uuid = UUID(GATT_PRIM_SVC_UUID);
}

std::string PrimaryService::str() {
	std::stringstream ss;
	ss << handle << "\t" << "PrimaryService" << "\t";
	if (false) {

	} else {
		ss << UUID(cache.value, cache.len).str();
	}
	return ss.str();
}

Characteristic::Characteristic() {
	uuid = UUID(GATT_CHARAC_UUID);
}

std::string Characteristic::str() {
	std::stringstream ss;
	ss << handle << "\t" << "Characteristic" << "\t" << serviceHandle << "\t";
	if (false) {

	} else {
		pdebug(cache.value, cache.len);
		// ss << UUID(cache.value, cache.len).str();
	}
	return ss.str();
}


