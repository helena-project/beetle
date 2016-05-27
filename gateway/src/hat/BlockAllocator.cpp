/*
 * BlockAllocator.cpp
 *
 *  Created on: Mar 29, 2016
 *      Author: James Hong
 */

#include "hat/BlockAllocator.h"

#include <assert.h>
#include <iostream>
#include <utility>

#define MAX_HANDLE 0xFFFF

BlockAllocator::BlockAllocator(int size) {
	blockSize = size;
	assert((MAX_HANDLE + 1) % blockSize == 0);
	numBlocks = (MAX_HANDLE + 1) / blockSize;
	blocks.reset(new device_t[numBlocks]);
	blocks.get()[0] = BEETLE_RESERVED_DEVICE;
	for (int i = 1; i < numBlocks; i++) {
		blocks.get()[i] = NULL_RESERVED_DEVICE;
	}
}

BlockAllocator::~BlockAllocator() {

}

std::set<device_t> BlockAllocator::getDevices() {
	std::set<device_t> ret;
	for (int i = 0; i < numBlocks; i++) {
		if (blocks.get()[i] != NULL_RESERVED_DEVICE) {
			ret.insert(blocks.get()[i]);
		}
	}
	return ret;
}

handle_range_t BlockAllocator::getDeviceRange(device_t d) {
	for (int i = 0; i < numBlocks; i++) {
		if (blocks.get()[i] == d) {
			uint16_t baseHandle = i * blockSize;
			handle_range_t ret;
			ret.start = baseHandle;
			ret.end = baseHandle + blockSize - 1;
			return ret;
		}
	}
	return handle_range_t { 0, 0 };
}

device_t BlockAllocator::getDeviceForHandle(uint16_t handle) {
	return blocks.get()[handle / blockSize];
}

handle_range_t BlockAllocator::getHandleRange(uint16_t handle) {
	uint16_t base = (handle / blockSize) * blockSize;
	handle_range_t ret;
	ret.start = base;
	ret.end = base + blockSize - 1;
	return ret;
}

handle_range_t BlockAllocator::reserve(device_t d, int n) {
	assert(n < blockSize);
	for (int i = 1; i < numBlocks; i++) {
		if (blocks.get()[i] == NULL_RESERVED_DEVICE) {
			blocks.get()[i] = d;
			uint16_t base = i * (uint16_t) blockSize;
			uint16_t ceil = base + (uint16_t) blockSize - 1;
			return handle_range_t { base, ceil };
		}
	}
	return handle_range_t { 0, 0 };
}

handle_range_t BlockAllocator::free(device_t d) {
	for (int i = 1; i < numBlocks; i++) {
		if (blocks.get()[i] == d) {
			blocks.get()[i] = NULL_RESERVED_DEVICE;
			uint16_t base = i * (uint16_t) blockSize;
			uint16_t ceil = base + (uint16_t) blockSize - 1;
			return handle_range_t { base, ceil };
		}
	}
	return handle_range_t { 0, 0 };
}
