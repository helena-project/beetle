/*
 * Debug.h
 *
 *  Created on: Apr 2, 2016
 *      Author: root
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

extern bool debug;

inline void pdebug(std::string mesg) {
	std::cerr << mesg << std::endl;
}

inline void pdebug(uint8_t *buf, int len) {
	fprintf(stderr, "[");
	for (int i = 0; i < len; i++) {
		fprintf(stderr, " %02x", buf[i]);
	}
	fprintf(stderr, " ]\n");
}

#endif /* DEBUG_H_ */
