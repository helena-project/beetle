/*
 * Autoconnect.h
 *
 *  Created on: Apr 6, 2016
 *      Author: james
 */

#ifndef AUTOCONNECT_H_
#define AUTOCONNECT_H_

#include <ctime>
#include <map>
#include <mutex>
#include <string>

#include "../sync/Semaphore.h"
#include "../Scanner.h"

typedef struct {
	std::string addr;
	std::string name;
	bool discover;		// start with discover
} autoconnect_config_t;

class AutoConnect {
public:
	AutoConnect(Beetle &beetle, bool connectAll = false);
	virtual ~AutoConnect();

	/*
	 * Return callback for scanner.
	 */
	DiscoveryHandler getDiscoveryHandler();
private:
	Beetle &beetle;

	/*
	 * Attempt to connect to a peripheral in separate thread.
	 */
	void connect(peripheral_info_t info, autoconnect_config_t config);

	std::mutex m;
	std::map<std::string, autoconnect_config_t> configEntries;
	std::map<std::string, time_t> lastAttempt;

	// TODO these should be configurable
	bool connectAll = false;	// this is a bad idea
	Semaphore maxConcurrentAttempts{2};
	double minAttemptInterval = 60;
};

#endif /* AUTOCONNECT_H_ */
