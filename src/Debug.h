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

/*
 * General debugging.
 */
extern bool debug;

/*
 * Extra debugging when routing.
 */
extern bool debug_router;

/*
 * Print every time a socket is written or read.
 */
extern bool debug_socket;

inline void pdebug(std::string mesg) {
	std::cerr << mesg << std::endl;
}

/*
 * Print bytes as hex.
 */
inline void pdebug(uint8_t *buf, int len) {
	fprintf(stderr, "[");
	for (int i = 0; i < len; i++) {
		fprintf(stderr, " %02x", buf[i]);
	}
	fprintf(stderr, " ]\n");
}

#endif /* DEBUG_H_ */
