/*
 * AccessControl.h
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#ifndef CONTROLLER_ACCESSCONTROL_H_
#define CONTROLLER_ACCESSCONTROL_H_

#include <boost/network/protocol/http/client.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <Beetle.h>

typedef int rule_t;

class Rule {
public:
	std::string properties;
	bool integrity;
	bool encryption;
	time_t lease;
};

typedef struct {
	std::map<rule_t, Rule> rules;
	std::map<std::string, std::map<std::string, std::set<rule_t>>> service_char_rules;
} cached_mapping_info_t;

class AccessControl {
public:
	AccessControl(Beetle &beetle, std::string hostAndPort);
	virtual ~AccessControl();

	/*
	 * Caller should hold at least a read lock on devices.
	 */
	bool canMap(Device *from, Device *to);
private:
	Beetle &beetle;

	std::string hostAndPort;

	boost::network::http::client *client;

	std::map<std::pair<device_t, device_t>, cached_mapping_info_t> cache;
	boost::shared_mutex cacheMutex;

	bool handleCanMapResponse(Device *from, Device *to, std::string body);
};

#endif /* CONTROLLER_ACCESSCONTROL_H_ */
