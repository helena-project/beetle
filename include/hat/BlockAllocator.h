/*
 * BlockAllocator.h
 *
 *  Created on: Mar 29, 2016
 *      Author: james
 */

#ifndef INCLUDE_HAT_BLOCKALLOCATOR_H_
#define INCLUDE_HAT_BLOCKALLOCATOR_H_

#include <cstdint>
#include <set>

#include "../Beetle.h"
#include "HandleAllocationTable.h"

/*
 * Implementation of the HAT interface that allocates
 * fixed size blocks to devices. The first block is
 * reserved for Beetle.
 */
class BlockAllocator: public HandleAllocationTable {
public:
	BlockAllocator(int blockSize);
	virtual ~BlockAllocator();
	std::set<device_t> getDevices();
	handle_range_t getDeviceRange(device_t d);
	device_t getDeviceForHandle(uint16_t h);
	handle_range_t getHandleRange(uint16_t h);
	handle_range_t reserve(device_t d, int n = 0);
	handle_range_t free(device_t d);
private:
	int blockSize;
	int numBlocks;
	device_t *blocks;
};

#endif /* INCLUDE_HAT_BLOCKALLOCATOR_H_ */
