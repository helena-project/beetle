/*
 * Debug.h
 *
 *  Created on: Apr 2, 2016
 *      Author: james
 */

#ifndef INCLUDE_DEBUG_H_
#define INCLUDE_DEBUG_H_

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

// General debugging.
extern bool debug;

// Print every time a socket is written or read.
extern bool debug_socket;

// Print debug information about the control-plane
extern bool debug_network;

inline void pdebug(std::string mesg) {
	std::cerr << mesg << std::endl;
}

inline void pwarn(std::string mesg) {
	std::cerr << "warning: " << mesg << std::endl;
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

#endif /* INCLUDE_DEBUG_H_ */
