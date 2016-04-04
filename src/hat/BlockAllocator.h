/*
 * BlockAllocator.h
 *
 *  Created on: Mar 29, 2016
 *      Author: james
 */

#ifndef HAT_BLOCKALLOCATOR_H_
#define HAT_BLOCKALLOCATOR_H_

#include <cstdbool>
#include <cstdint>

#include "../Beetle.h"
#include "HAT.h"

/*
 * Implementation of the HAT interface that allocates
 * fixed size blocks to devices. The first block is
 * reserved for Beetle.
 */
class BlockAllocator: public HAT {
public:
	BlockAllocator(int blockSize);
	virtual ~BlockAllocator();
	handle_range_t getDeviceRange(device_t d);
	device_t getDeviceForHandle(uint16_t h);
	handle_range_t getHandleRange(uint16_t h);
	bool reserve(device_t d, int n);
	bool reserve(device_t d);
	void free(device_t d);
private:
	int blockSize;
	int numBlocks;
	device_t *blocks;
};

#endif /* HAT_BLOCKALLOCATOR_H_ */
