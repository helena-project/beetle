/*
 * Handle.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef HANDLE_H_
#define HANDLE_H_

#include <cstdint>
#include <set>

#include "Device.h"
#include "UUID.h"

class Handle {
public:
	Handle(uint16_t handle, UUID uuid, uint16_t serviceHandle, uint16_t charHandle);
	virtual ~Handle();
	uint16_t getCharHandle();
	uint16_t getHandle();
	uint16_t getServiceHandle();
	std::set<device_t>& getSubscribers();
	const UUID& getUuid();
private:
	uint16_t handle;
	UUID uuid;
	uint16_t serviceHandle;
	uint16_t charHandle;
	std::set<device_t> subscribers;
};

#endif /* HANDLE_H_ */
