/*
 * BlockAllocator.cpp
 *
 *  Created on: Mar 29, 2016
 *      Author: james
 */

#include "BlockAllocator.h"

#include <assert.h>

#include "../Beetle.h"

#define MAX_HANDLE 0xFFFF

BlockAllocator::BlockAllocator(int size) {
	blockSize = size;
	assert(MAX_HANDLE % blockSize == 0);
	numBlocks = MAX_HANDLE / blockSize;
	blocks = new device_t[numBlocks];
	blocks[0] = BEETLE_RESERVED_DEVICE;
	for (int i = 1; i < numBlocks; i++) {
		blocks[i] = NULL_RESERVED_DEVICE;
	}
}

BlockAllocator::~BlockAllocator() {
	// should never get called
	delete[] blocks;
}

handle_range_t BlockAllocator::getDeviceRange(device_t d) {
	for (int i = 0; i < numBlocks; i++) {
		if (blocks[i] == d) {
			uint16_t baseHandle = i * blockSize;
			return handle_range_t{baseHandle, baseHandle + blockSize - 1};
		}
	}
	return handle_range_t{0, 0};
}

device_t BlockAllocator::getDeviceForHandle(uint16_t handle) {
	return blocks[handle / blockSize];
}

handle_range_t BlockAllocator::getHandleRange(uint16_t handle) {
	uint16_t base = (handle / blockSize) * blockSize;
	return handle_range_t{base, base + blockSize - 1};
}

bool BlockAllocator::reserve(device_t d, int n) {
	assert(n > blockSize);
	return reserve(d);
}

bool BlockAllocator::reserve(device_t d) {
	for (int i = 1; i < numBlocks; i++) {
		if (blocks[i] == NULL_RESERVED_DEVICE) {
			blocks[i] = d;
			return true;
		}
	}
	return false;
}

void BlockAllocator::free(device_t d) {
	for (int i = 1; i < numBlocks; i++) {
		if (blocks[i] == d) {
			blocks[i] = NULL_RESERVED_DEVICE;
		}
	}
}
