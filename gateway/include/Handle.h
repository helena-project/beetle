/*
 * Handle.h
 *
 *  Created on: Mar 24, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_HANDLE_H_
#define INCLUDE_HANDLE_H_

#include <cstdint>
#include <ctime>
#include <memory>
#include <set>
#include <string>

#include "BeetleTypes.h"
#include "UUID.h"

class CachedHandle {
public:
	virtual ~CachedHandle();

	/*
	 * Transfers ownership of value pointer to cache.
	 */
	void set(uint8_t *value, int len);
	std::unique_ptr<uint8_t> value;

	int len = 0;
	time_t time = 0;
	std::set<device_t> cachedSet;
};

/*
 * A generic handle.
 */
class Handle {
public:
	Handle();
	virtual ~Handle();

	bool isCacheInfinite() const;
	void setCacheInfinite(bool cacheInfinite);

	/*
	 * Stores the handle of the characteristic, or the handle of the attribute
	 * value if this handle is a characteristic.
	 */
	uint16_t getCharHandle() const;
	void setCharHandle(uint16_t charHandle);

	uint16_t getEndGroupHandle() const;
	void setEndGroupHandle(uint16_t endGroupHandle);

	uint16_t getHandle() const;
	void setHandle(uint16_t handle);

	uint16_t getServiceHandle() const;
	void setServiceHandle(uint16_t serviceHandle);

	UUID getUuid() const;
	void setUuid(UUID uuid);

	virtual std::string str() const;

	CachedHandle cache;
	std::set<device_t> subscribers;
protected:
	uint16_t handle = 0;
	UUID uuid;
	uint16_t serviceHandle = 0;
	uint16_t charHandle = 0;
	uint16_t endGroupHandle = 0;
	bool cacheInfinite = false;
};

class PrimaryService: public Handle {
public:
	PrimaryService();
	UUID getServiceUuid() const;
	std::string str() const;
};

class Characteristic: public Handle {
public:
	Characteristic();
	uint16_t getAttrHandle() const;
	UUID getCharUuid() const;
	uint8_t getProperties() const;
	std::string str() const;
};

class CharacteristicValue: public Handle {
	/* just for understandability */
};

class ClientCharCfg: public Handle {
public:
	ClientCharCfg();
	std::string str() const;
};

#endif /* INCLUDE_HANDLE_H_ */
