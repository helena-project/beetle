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
	// should never get called on exit
	delete[] blocks;
}

std::pair<uint16_t, uint16_t> BlockAllocator::getDeviceRange(device_t d) {
	for (int i = 0; i < numBlocks; i++) {
		if (blocks[i] == d) {
			uint16_t baseHandle = i * blockSize;
			return std::pair<uint16_t, uint16_t>(baseHandle, baseHandle + blockSize - 1);
		}
	}
	return std::pair<uint16_t, uint16_t>(0, 0);
}

device_t BlockAllocator::getDeviceForHandle(uint16_t handle) {
	return blocks[handle / blockSize];
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
