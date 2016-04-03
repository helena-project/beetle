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
	void set(uint8_t *value, int len);
	uint8_t *value = NULL;
	int len = 0;
	time_t time = 0;
	std::set<device_t> cachedSet;
};

class Handle {
public:
	bool isCacheInfinite();
	void setCacheInfinite(bool cacheInfinite);

	uint16_t getCharHandle();
	void setCharHandle(uint16_t charHandle);

	uint16_t getEndGroupHandle();
	void setEndGroupHandle(uint16_t endGroupHandle);

	uint16_t getHandle();
	void setHandle(uint16_t handle);

	uint16_t getServiceHandle();
	void setServiceHandle(uint16_t serviceHandle);

	UUID& getUuid();
	void setUuid(UUID uuid);

	std::string str();

	CachedHandle cache;
	std::set<device_t> subscribers;
private:
	uint16_t handle = 0;
	UUID uuid;
	uint16_t serviceHandle = 0;
	uint16_t charHandle = 0;
	uint16_t endGroupHandle = 0;
	bool cacheInfinite;
};

#endif /* HANDLE_H_ */
