/*
 * UUID.h
 *
 *  Created on: Mar 24, 2016
 *      Author: James Hong
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
	UUID(uint8_t *buf, size_t len, bool reversed = true);

	/*
	 * Load uuid from string.
	 */
	UUID(std::string s);

	virtual ~UUID();

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

	std::string str(bool forceLong = false) const;
private:
	uuid_t uuid;
};

#endif /* INCLUDE_UUID_H_ */
