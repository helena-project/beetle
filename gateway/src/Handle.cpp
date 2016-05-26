/*
 * Handle.cpp
 *
 *  Created on: Apr 2, 2016
 *      Author: James Hong
 */

#include "Handle.h"

#include <bluetooth/bluetooth.h>
#include <sstream>

#include "ble/gatt.h"

CachedHandle::~CachedHandle() {
	if (value != NULL) {
		delete[] value;
	}
}

void CachedHandle::set(uint8_t *value_, int len_) {
	if (value != NULL) {
		delete[] value;
	}
	value = value_;
	len = len_;
	::time(&(time));
}

Handle::Handle() {

}

Handle::~Handle() {

}

bool Handle::isCacheInfinite() const {
	return cacheInfinite;
}

void Handle::setCacheInfinite(bool cacheInfinite) {
	this->cacheInfinite = cacheInfinite;
}

uint16_t Handle::getCharHandle() const {
	return charHandle;
}

void Handle::setCharHandle(uint16_t charHandle) {
	this->charHandle = charHandle;
}

uint16_t Handle::getEndGroupHandle() const {
	return endGroupHandle;
}

void Handle::setEndGroupHandle(uint16_t endGroupHandle) {
	this->endGroupHandle = endGroupHandle;
}

uint16_t Handle::getHandle() const {
	return handle;
}

void Handle::setHandle(uint16_t handle) {
	this->handle = handle;
}

uint16_t Handle::getServiceHandle() const {
	return serviceHandle;
}

void Handle::setServiceHandle(uint16_t serviceHandle) {
	this->serviceHandle = serviceHandle;
}

UUID Handle::getUuid() const {
	return uuid;
}

void Handle::setUuid(UUID uuid_) {
	uuid = uuid_;
}

std::string Handle::str() const {
	std::stringstream ss;
	ss << handle << "\t" << uuid.str() << "\tsH=" << serviceHandle << "\tcH=" << charHandle << "\tnSub="
			<< subscribers.size();
	if (subscribers.size() > 0) {
		ss << "\tsub=[";
		std::string sep = "";
		for (device_t d : subscribers) {
			ss << sep << d;
			sep = ",";
		}
		ss << "]";
	}
	if (cache.value != NULL) {
		ss << "\tcache: [";
		std::string sep = "";
		for (int i = 0; i < cache.len; i++) {
			ss << sep << (unsigned int) cache.value[i];
			sep = ",";
		}
		ss << ']';
	}
	return ss.str();
}

PrimaryService::PrimaryService() {
	uuid = UUID(GATT_PRIM_SVC_UUID);
}

UUID PrimaryService::getServiceUuid() const {
	return UUID(cache.value, cache.len);
}

std::string PrimaryService::str() const {
	std::stringstream ss;
	ss << handle << "\t" << "[PrimaryService]" << "\tuuid=" << UUID(cache.value, cache.len).str() << "\tend="
			<< endGroupHandle;
	return ss.str();
}

Characteristic::Characteristic() {
	uuid = UUID(GATT_CHARAC_UUID);
}

uint16_t Characteristic::getAttrHandle() const {
	return *(uint16_t *) (cache.value + 1);
}

UUID Characteristic::getCharUuid() const {
	return UUID(cache.value + 3, cache.len - 3);
}

uint8_t Characteristic::getProperties() const {
	return cache.value[0];
}

static std::string getPropertiesString(uint8_t properties) {
	std::stringstream ss;
	ss << ((properties & GATT_CHARAC_PROP_BCAST) ? 'b' : '-');
	ss << ((properties & GATT_CHARAC_PROP_READ) ? 'r' : '-');
	ss << ((properties & GATT_CHARAC_PROP_WRITE_NR) ? 'W' : '-');
	ss << ((properties & GATT_CHARAC_PROP_WRITE) ? 'w' : '-');
	ss << ((properties & GATT_CHARAC_PROP_NOTIFY) ? 'n' : '-');
	ss << ((properties & GATT_CHARAC_PROP_IND) ? 'i' : '-');
	ss << ((properties & GATT_CHARAC_PROP_AUTH_SIGN_WRITE) ? 's' : '-');
	ss << ((properties & GATT_CHARAC_PROP_EXT) ? 'e' : '-');
	return ss.str();
}

std::string Characteristic::str() const {
	std::stringstream ss;
	ss << handle << "\t" << "[Characteristic]" << "\tsH=" << serviceHandle << "\t";
	if (cache.value != NULL && cache.len >= 5) {
		uint8_t properties = cache.value[0];
		ss << "uuid=" << UUID(cache.value + 3, cache.len - 3).str() << "\tvH=" << btohs(*(uint16_t * )(cache.value + 1))
				<< "\t" << getPropertiesString(properties) << "\tend=" << endGroupHandle;
	} else {
		ss << "unknown or malformed";
	}
	return ss.str();
}

ClientCharCfg::ClientCharCfg() {
	uuid = UUID(GATT_CLIENT_CHARAC_CFG_UUID);
}

std::string ClientCharCfg::str() const {
	std::stringstream ss;
	ss << handle << "\t" << "[ClientCharCfg]" << "\tsH=" << serviceHandle << "\tcH=" << charHandle;
	return ss.str();
}

