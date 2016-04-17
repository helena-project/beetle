/*
 * UUID.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef INCLUDE_UUID_H_
#define INCLUDE_UUID_H_

#include <stddef.h>
#include <cstdint>
#include <string>

typedef struct {
	unsigned char value[16];
} uuid_t;

class UUID {
public:
	/*
	 * An all 0s uuid
	 */
	UUID();

	/*
	 * A uuid with the bluetooth base
	 */
	UUID(uint16_t);

	/*
	 * A uuid from the buffer. len must be either 2 or 16
	 */
	UUID(uint8_t *buf, size_t len);

	/*
	 * An uuid from an existing uuid
	 */
	UUID(uuid_t uuid_);

	virtual ~UUID();

	/*
	 * Returns the full 128-bit uuid.
	 */
	uuid_t get();

	/*
	 * Returns the 16-bit short uuid.
	 */
	uint16_t getShort();

	/*
	 * Returns whether the uuid has the Bluetooth base
	 */
	bool isShort();

	/*
	 * Are they the same uuid?
	 */
	bool compareTo(UUID &other);

	std::string str();
private:
	uuid_t uuid;
};

#endif /* INCLUDE_UUID_H_ */
