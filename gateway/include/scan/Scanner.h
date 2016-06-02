/*
 * Scanner.h
 *
 *  Created on: Mar 24, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_SCANNER_H_
#define INCLUDE_SCANNER_H_

#include <bluetooth/bluetooth.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "device/socket/LEPeripheral.h"

class ScannerException : public std::exception {
public:
	ScannerException(std::string msg) : msg(msg) {};
	ScannerException(const char *msg) : msg(msg) {};
	~ScannerException() throw() {};
	const char *what() const throw() { return this->msg.c_str(); };
private:
	std::string msg;
};

/*
 * Result from scan and advertisement.
 */
typedef struct {
	std::string name;
	bdaddr_t bdaddr;
	LEPeripheral::AddrType bdaddrType;
} peripheral_info_t;

/*
 * Callback upon discovery.
 */
typedef std::function<void(peripheral_info_t info)> DiscoveryHandler;

class Scanner {
public:
	Scanner();
	virtual ~Scanner();

	/*
	 * Start BLE scanning.
	 */
	void start();

	/*
	 * Register a callback to be called upon discovery.
	 */
	void registerHandler(DiscoveryHandler);
private:
	uint16_t scanInterval;
	uint16_t scanWindow;

	std::vector<DiscoveryHandler> handlers;

	std::shared_ptr<std::atomic_flag> running;
};

#endif /* INCLUDE_SCANNER_H_ */
