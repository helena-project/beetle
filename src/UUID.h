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
#include <string>

typedef struct {
	unsigned char value[16];
} uuid_t;

class UUID {
public:
	UUID();
	UUID(uint16_t);
	UUID(uint8_t *buf, size_t len);
	UUID(uuid_t uuid_);
	virtual ~UUID();
	uuid_t get();
	uint16_t getShort();
	bool isShort();
	std::string str();
private:
	uuid_t uuid;
};

#endif /* UUID_H_ */
