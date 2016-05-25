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
#include <cstring>

const int UUID_LEN = 16;
const int SHORT_UUID_LEN = 2;

typedef struct {
	unsigned char value[UUID_LEN];
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
	 * Load uuid from string.
	 */
	UUID(std::string s);

	/*
	 * An uuid from an existing uuid
	 */
	UUID(uuid_t uuid_);

	virtual ~UUID();

	/*
	 * Returns the full 128-bit uuid.
	 */
	uuid_t get() const;

	/*
	 * Returns the 16-bit short uuid.
	 */
	uint16_t getShort() const;

	/*
	 * Returns whether the uuid has the Bluetooth base
	 */
	bool isShort() const;

	bool operator <(const UUID &rhs) const {
	    return memcmp(uuid.value, rhs.uuid.value, UUID_LEN) < 0;
	}

	bool operator ==(const UUID &rhs) const {
	    return memcmp(uuid.value, rhs.uuid.value, UUID_LEN) == 0;
	}

	std::string str() const;
private:
	uuid_t uuid;
};

#endif /* INCLUDE_UUID_H_ */
