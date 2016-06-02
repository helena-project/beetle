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

	struct bdaddr_t_less : std::binary_function<bdaddr_t, bdaddr_t, bool> {
		bool operator() (const bdaddr_t &a, const bdaddr_t &b) const {
			return memcmp(a.b, b.b, sizeof(bdaddr_t));
		}
	};

	/*
	 * Bluetooth addresses to ignore
	 */
	std::set<bdaddr_t, bdaddr_t_less> whitelist;

	/*
	 * Only one connection attempt at a time
	 */
	std::mutex connectMutex;

	std::map<bdaddr_t, time_t, bdaddr_t_less> lastAttempt;
	std::mutex lastAttemptMutex;
};

#endif /* INCLUDE_AUTOCONNECT_H_ */
