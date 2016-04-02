/*
 * Handle.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef HANDLE_H_
#define HANDLE_H_

#include <cstdint>
#include <ctime>
#include <mutex>
#include <set>
#include <time.h>

#include "Beetle.h"
#include "UUID.h"

class CachedHandle {
public:
	virtual ~CachedHandle() {};
	void set(uint8_t *value, int len) {
		if (this->value != NULL) {
			delete[] this->value;
		}
		this->value = value;
		this->len = len;
		::time(&(this->time));
	};
	uint8_t *value = NULL;
	int len = 0;
	time_t time = 0;
	std::set<device_t> cachedSet;
};

class Handle {
public:
	Handle() : uuid{0} {};

	virtual ~Handle() {};

	CachedHandle& getCache() {
		return cache;
	}

	bool isCacheInfinite() {
		return cacheInfinite;
	}

	void setCacheInfinite(bool cacheInfinite) {
		this->cacheInfinite = cacheInfinite;
	}

	uint16_t getCharHandle() {
		return charHandle;
	}

	void setCharHandle(uint16_t charHandle) {
		this->charHandle = charHandle;
	}

	uint16_t getEndGroupHandle() {
		return endGroupHandle;
	}

	void setEndGroupHandle(uint16_t endGroupHandle) {
		this->endGroupHandle = endGroupHandle;
	}

	uint16_t getHandle() {
		return handle;
	}

	void setHandle(uint16_t handle) {
		this->handle = handle;
	}

	uint16_t getServiceHandle() {
		return serviceHandle;
	}

	void setServiceHandle(uint16_t serviceHandle) {
		this->serviceHandle = serviceHandle;
	}

	std::set<device_t>& getSubscribers() {
		return subscribers;
	}

	void setSubscribers(const std::set<device_t>& subscribers) {
		this->subscribers = subscribers;
	}

	UUID& getUuid() {
		return uuid;
	}

	void setUuid(UUID uuid) {
		this->uuid = uuid;
	}

private:
	uint16_t handle;
	UUID uuid;
	uint16_t serviceHandle;
	uint16_t charHandle;
	uint16_t endGroupHandle;
	std::set<device_t> subscribers;

	CachedHandle cache;
	bool cacheInfinite;
};

#endif /* HANDLE_H_ */
