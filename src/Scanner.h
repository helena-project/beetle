/*
 * Scanner.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef SCANNER_H_
#define SCANNER_H_

#include <bluetooth/bluetooth.h>
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include "device/LEPeripheral.h"

// Print debug info in scanning module
extern bool debug_scan;

typedef struct {
	std::string name;
	bdaddr_t bdaddr;
	AddrType bdaddrType;
} peripheral_info_t;

class Scanner {
public:
	Scanner();
	virtual ~Scanner();

	/*
	 * Start BLE scanning.
	 */
	void start();

	/*
	 * Stop BLE scanning.
	 */
	void stop();

	/*
	 * Return a list of discovered peripherals.
	 */
	std::map<std::string, peripheral_info_t> getDiscovered();
private:
	bool started;
	bool stopped;

	std::map<std::string, peripheral_info_t> discovered;
	std::mutex discoveredMutex;

	std::thread t;
	void scanDaemon();
};

#endif /* SCANNER_H_ */
