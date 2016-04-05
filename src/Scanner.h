/*
 * Discover.h
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

typedef struct {
	std::string name;
	bdaddr_t bdaddr;
	AddrType bdaddrType;
} discovered_t;

class Scanner {
public:
	Scanner();
	virtual ~Scanner();

	void start();
	void stop();
	std::list<discovered_t> getDiscovered();
private:
	bool started;
	bool stopped;

	std::list<discovered_t> discovered;
	std::mutex discoveredMutex;

	std::thread t;
	void discoverDaemon();
};

#endif /* SCANNER_H_ */
