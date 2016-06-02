/*
 * Autoconnect.h
 *
 *  Created on: Apr 6, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_AUTOCONNECT_H_
#define INCLUDE_AUTOCONNECT_H_

#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <set>
#include <thread>
#include <functional>

#include "BeetleTypes.h"
#include "scan/Scanner.h"

struct ci_less : std::binary_function<std::string, std::string, bool> {
	struct nocase_compare : public std::binary_function<unsigned char,unsigned char,bool> {
		bool operator() (const unsigned char& c1, const unsigned char& c2) const {
			return tolower(c1) < tolower(c2);
		}
	};
	bool operator() (const std::string & s1, const std::string & s2) const {
		return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), nocase_compare());
	}
};

class AutoConnect {
public:
	AutoConnect(Beetle &beetle, bool connectAll = false, double minBackoff = 60.0, std::string whitelist = "");
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
	std::set<std::string, ci_less> whitelist;

	/*
	 * Only one connection attempt at a time
	 */
	std::mutex connectMutex;

	std::map<std::string, time_t, ci_less> lastAttempt;
	std::mutex lastAttemptMutex;
};

#endif /* INCLUDE_AUTOCONNECT_H_ */
