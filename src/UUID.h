/*
 * UUID.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef UUID_H_
#define UUID_H_

#include <stddef.h>
#include <cstdint>

typedef struct {
	unsigned char buf[16];
} uuid_t;

class UUID {
public:
	UUID(const unsigned char *buf, size_t len);
	UUID(uuid_t uuid_) : uuid{uuid_} {};
	virtual ~UUID();
	uuid_t get();
	uint16_t getShort();
	bool isShort();
private:
	uuid_t uuid;
};

#endif /* UUID_H_ */
