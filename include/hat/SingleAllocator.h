/*
 * SingleAllocator.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef HAT_SINGLEALLOCATOR_H_
#define HAT_SINGLEALLOCATOR_H_

#include "../../include/hat/HandleAllocationTable.h"

/*
 * Maps only one device.
 */
class SingleAllocator: public HandleAllocationTable {
public:
	SingleAllocator(device_t device);
	virtual ~SingleAllocator();
	std::set<device_t> getDevices();
	handle_range_t getDeviceRange(device_t d);
	device_t getDeviceForHandle(uint16_t h);
	handle_range_t getHandleRange(uint16_t h);
	handle_range_t reserve(device_t d, int n = 0);
	handle_range_t free(device_t d);
private:
	device_t id;
};

#endif /* HAT_SINGLEALLOCATOR_H_ */
