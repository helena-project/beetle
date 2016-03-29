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

#include "../device/Device.h"

class HAT {
public:
	HAT() {};
	virtual ~HAT() {};
	virtual std::pair<uint16_t, uint16_t> getDeviceRange(device_t) = 0;
	virtual device_t getDeviceForHandle(uint16_t) = 0;
	virtual bool reserve(device_t, int) = 0;
	virtual bool reserve(device_t) = 0;
	virtual void free(device_t) = 0;
};

#endif /* HAT_H_ */
