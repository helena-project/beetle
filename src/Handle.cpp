/*
 * Handle.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Handle.h"

Handle::Handle(uint16_t handle_, uuid_t uuid_, uint16_t serviceHandle_, uint16_t charHandle_, uint16_t endGroupHandle_) : uuid(uuid_) {
	handle = handle_;
	serviceHandle = serviceHandle_;
	charHandle = charHandle_;
	endGroupHandle = endGroupHandle_;
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

uint16_t Handle::getEndGroupHandle() {
	return endGroupHandle;
}

CachedHandle Handle::getCached() {
	std::lock_guard<std::mutex> lg(cacheMutex);
	CachedHandle ret;
	ret.value = new uint8_t[cache.len];
	memcpy(ret.value, cache.value, cache.len);
	ret.len = cache.len;
	ret.time = cache.time;
	return ret;
}

void Handle::setCached(uint8_t *val, int len) {
	std::lock_guard<std::mutex> lg(cacheMutex);
	delete[] cache.value;
	cache.len = len;
	cache.value = new uint8_t[len];
	memcpy(cache.value, val, len);
	time(&cache.time);
}

CachedHandle::CachedHandle() {
	value = NULL;
	len = 0;
	time = 0;
}

CachedHandle::~CachedHandle() {
	delete[] value;
}


