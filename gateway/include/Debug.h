/*
 * Debug.h
 *
 *  Created on: Apr 2, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_DEBUG_H_
#define INCLUDE_DEBUG_H_

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

// General debugging.
extern bool debug;

// Print debug information when scanning an automatically connecting
extern bool debug_scan;

// Print debug information when discovering handles
extern bool debug_discovery;

// Print debug information in router
extern bool debug_router;

// Print every time a socket is written or read.
extern bool debug_socket;

// Print debug information about the control-plane
extern bool debug_controller;

// Print debug information performance
extern bool debug_performance;

inline void pdebug(std::string mesg) {
	std::cerr << mesg << std::endl;
}

inline void phex(uint8_t *buf, int len) {
	fprintf(stderr, "[");
	for (int i = 0; i < len; i++) {
		fprintf(stderr, " %02x", buf[i]);
	}
	fprintf(stderr, " ]\n");
}

inline void pwarn(std::string mesg) {
	std::cerr << "warning: " << mesg << std::endl;
}

inline void pexcept(std::exception &e) {
	std::cerr << "caught exception: " << e.what() << std::endl;
}

#endif /* INCLUDE_DEBUG_H_ */
