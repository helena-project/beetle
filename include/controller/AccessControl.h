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
#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <ble/gatt.h>
#include <Beetle.h>
#include <Debug.h>
#include <UUID.h>

class Handle;

typedef int rule_t;

class Rule {
public:
	uint8_t properties;
	bool integrity;
	bool encryption;
	bool exclusive;
	time_t lease;
	void setProperties(std::string s) {
		for (char c : s) {
			switch (c) {
			case 'e':
				exclusive = true;
				break;
			case 'b':
				properties |= GATT_CHARAC_PROP_BCAST;
				break;
			case 'r':
				properties |= GATT_CHARAC_PROP_READ;
				break;
			case 'w':
				properties |= GATT_CHARAC_PROP_WRITE_NR | GATT_CHARAC_PROP_WRITE | GATT_CHARAC_PROP_AUTH_SIGN_WRITE;
				break;
			case 'n':
				properties |= GATT_CHARAC_PROP_NOTIFY;
				break;
			case 'i':
				properties |= GATT_CHARAC_PROP_IND;
				break;
			default:
				pwarn("received unsupported property");
			}
		}
	}
};

typedef struct {
	std::map<rule_t, Rule> rules;

	/*
	 * Mapping of service uuids to maps of char uuids to sets of relevant rules.
	 * Uuids are stored in uppercase.
	 */
	std::map<UUID, std::map<UUID, std::set<rule_t>>> service_char_rules;
} cached_mapping_info_t;

class AccessControl {
public:
	AccessControl(Beetle &beetle, std::string hostAndPort);
	virtual ~AccessControl();

	/*
	 * Caller should hold at least a read lock on devices.
	 */
	bool canMap(Device *from, Device *to);

	/*
	 * Caller should have device and handle locks.
	 */
	bool canAccessHandle(Device *client, Device *server, Handle *h, uint8_t op, uint8_t &attErr);
private:
	Beetle &beetle;

	std::string hostAndPort;

	boost::network::http::client *client;

	std::map<std::pair<device_t, device_t>, cached_mapping_info_t> cache;
	boost::shared_mutex cacheMutex;

	bool handleCanMapResponse(Device *from, Device *to, std::stringstream &response);
};

#endif /* CONTROLLER_ACCESSCONTROL_H_ */
