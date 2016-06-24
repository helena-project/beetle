/*
 * Autoconnect.h
 *
 *  Created on: Apr 6, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_AUTOCONNECT_H_
#define INCLUDE_AUTOCONNECT_H_

#include <bluetooth/bluetooth.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <set>
#include <functional>

#include "BeetleTypes.h"
#include "scan/Scanner.h"

class AutoConnect {
public:
	AutoConnect(Beetle &beetle, bool connectAll = false, double minBackoff = 60.0,
			std::string whitelistFile = "");
	virtual ~AutoConnect();

	/*
	 * Return callback for scanner.
	 */
	DiscoveryHandler getDiscoveryHandler();

	/*
	 * Return the timeout daemon.
	 */
	std::function<void()> getDaemon();
private:
	Beetle &beetle;

	/*
	 * Attempt to connect to a peripheral in a worker thread.
	 */
	void connect(peripheral_info_t info, bool discover);

	bool connectAll;

	/*
	 * Minimum number of seconds between connection attempts
	 */
	double minBackoff;

	/*
	 * Bluetooth addresses to ignore
	 */
	std::set<std::string> whitelist;

	/*
	 * Bluetooth addresses to ignore
	 */
	std::set<std::string> blacklist;

	/*
	 * Only one connection attempt at a time
	 */
	std::mutex connectMutex;

	std::map<std::string, time_t> lastAttempt;
	std::mutex lastAttemptMutex;
};

#endif /* INCLUDE_AUTOCONNECT_H_ */
