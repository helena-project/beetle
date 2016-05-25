/*
 * SingleAllocator.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "hat/SingleAllocator.h"

#include <cstdint>
#include <set>
#include <utility>

#include "Beetle.h"

SingleAllocator::SingleAllocator(device_t device_) {
	id = device_;
}

SingleAllocator::~SingleAllocator() {

}

std::set<device_t> SingleAllocator::getDevices() {
	std::set<device_t> ret;
	if (id != NULL_RESERVED_DEVICE) ret.insert(id);
	return ret;
}

handle_range_t SingleAllocator::getDeviceRange(device_t d) {
	if (id != NULL_RESERVED_DEVICE && d == id) {
		return handle_range_t{0,0xffff};
	} else {
		return handle_range_t{0,0};
	}
}

device_t SingleAllocator::getDeviceForHandle(uint16_t h) {
	return id;
}

handle_range_t SingleAllocator::getHandleRange(uint16_t h) {
	return handle_range_t{0,0xffff};
}

handle_range_t SingleAllocator::reserve(device_t d, int n) {
	return handle_range_t{0,0};
}

handle_range_t SingleAllocator::free(device_t d) {
	if (d == id) id = NULL_RESERVED_DEVICE;
	return handle_range_t{0,0};
}
