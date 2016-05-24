/*
 * Autoconnect.h
 *
 *  Created on: Apr 6, 2016
 *      Author: james
 */

#ifndef INCLUDE_AUTOCONNECT_H_
#define INCLUDE_AUTOCONNECT_H_

#include <ctime>
#include <map>
#include <mutex>
#include <string>

#include "Beetle.h"
#include "sync/Semaphore.h"
#include "scan/Scanner.h"

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

	// TODO this should probably be from a database or something
	std::map<std::string, autoconnect_config_t> configEntries;

	// TODO these should be configurable
	bool connectAll = false;	// this is a bad idea
	Semaphore maxConcurrentAttempts{1};
	double minAttemptInterval = 60;

	/*
	 * Time out entries in lastAttempt.
	 */
	bool daemonRunning;
	std::thread daemonThread;
	void daemon(int seconds);

	std::map<std::string, time_t> lastAttempt;
	std::mutex lastAttemptMutex;
};

#endif /* INCLUDE_AUTOCONNECT_H_ */
