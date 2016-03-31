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

#include "Beetle.h"
#include "UUID.h"

class CachedHandle {
public:
	CachedHandle();
	virtual ~CachedHandle();
	uint8_t *value = NULL;
	int len = 0;
	time_t time = 0;
	std::set<device_t> cachedSet;
};

class Handle {
public:
	Handle(uint16_t handle, uuid_t uuid, uint16_t serviceHandle, uint16_t charHandle, uint16_t endGroupHandle);
	virtual ~Handle();
	uint16_t getCharHandle();
	uint16_t getHandle();
	uint16_t getServiceHandle();
	uint16_t getEndGroupHandle();
	std::set<device_t>& getSubscribers();
	UUID getUuid();
	CachedHandle getCached();
	void setCached(uint8_t *val, int len);
	bool isCacheInfinite() {return cacheInfinite;};
private:
	uint16_t handle;
	UUID uuid;
	uint16_t serviceHandle;
	uint16_t charHandle;
	uint16_t endGroupHandle;
	std::set<device_t> subscribers;

	std::mutex cacheMutex;
	CachedHandle cache;
	bool cacheInfinite;
};

#endif /* HANDLE_H_ */
