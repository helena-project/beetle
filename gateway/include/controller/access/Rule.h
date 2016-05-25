/*
 * Ruled.h
 *
 *  Created on: Apr 24, 2016
 *      Author: James Hong
 */

#ifndef CONTROLLER_ACCESS_RULE_H_
#define CONTROLLER_ACCESS_RULE_H_

#include <cstdint>
#include <ctime>
#include <list>
#include <string>
#include <memory>

#include "ble/gatt.h"
#include "Debug.h"

/* Forward declarations */
class DynamicAuth;

typedef uint32_t rule_t;

class Rule {
public:
	uint8_t properties;
	bool integrity;
	bool encryption;
	int exclusiveId;
	time_t lease;
	int priority = 0;
	std::list<std::shared_ptr<DynamicAuth>> additionalAuth;
	void setProperties(std::string s) {
		properties |= GATT_CHARAC_PROP_EXT;
		for (char c : s) {
			switch (c) {
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

#endif /* CONTROLLER_ACCESS_RULE_H_ */
