/*
 * HandleAllocationTable.h
 *
 *  Created on: Mar 29, 2016
 *      Author: james
 */

#ifndef INCLUDE_HAT_HANDLEALLOCATIONTABLE_H_
#define INCLUDE_HAT_HANDLEALLOCATIONTABLE_H_

#include <cstdint>
#include <set>
#include <sstream>
#include <string>

#include "../Beetle.h"

typedef struct {
	uint16_t start;
	uint16_t end;

	std::string str() {
		std::stringstream ss;
		ss << "[" << start << "," << end << "]";
		return ss.str();
	}
	bool isNull() {
		return start == 0 && end == 0;
	}
} handle_range_t;

/*
 * A Handle allocation table interface, to map devices to their
 * allotted handle ranges.
 */
class HandleAllocationTable {
public:
	HandleAllocationTable() {};
	virtual ~HandleAllocationTable() {};

	/*
	 * Returns a set of devices mapped into this hat.
	 */
	virtual std::set<device_t> getDevices() = 0;

	/*
	 * Returns the range allotted to the device or [0,0]
	 * otherwise.
	 */
	virtual handle_range_t getDeviceRange(device_t) = 0;

	/*
	 * Returns the device that owns the handle.
	 */
	virtual device_t getDeviceForHandle(uint16_t) = 0;

	/*
	 * Returns the handle range that the handle falls in.
	 */
	virtual handle_range_t getHandleRange(uint16_t) = 0;

	/*
	 * Reserve at least a space of size. Returns success.
	 */
	virtual handle_range_t reserve(device_t device, int size = 0) = 0;

	/*
	 * Release any handle ranges owned by the device.
	 */
	virtual handle_range_t free(device_t) = 0;
};

#endif /* INCLUDE_HAT_HANDLEALLOCATIONTABLE_H_ */
