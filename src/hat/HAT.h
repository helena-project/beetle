/*
 * HAT.h
 *
 *  Created on: Mar 29, 2016
 *      Author: james
 */

#ifndef HAT_H_
#define HAT_H_

#include <cstdbool>
#include <cstdint>
#include <utility>

#include "../Device.h"

typedef struct {
	uint16_t start;
	uint16_t end;
} handle_range_t;

/*
 * A Handle allocation table interface, to map devices to their
 * allotted handle ranges.
 */
class HAT {
public:
	HAT() {};
	virtual ~HAT() {};

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
	virtual bool reserve(device_t, int) = 0;

	/*
	 * Reserve space for device.
	 */
	virtual bool reserve(device_t) = 0;

	/*
	 * Release any handle ranges owned by the device.
	 */
	virtual void free(device_t) = 0;
};

#endif /* HAT_H_ */
