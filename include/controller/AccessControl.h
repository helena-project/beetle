/*
 * AccessControl.h
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#ifndef CONTROLLER_ACCESSCONTROL_H_
#define CONTROLLER_ACCESSCONTROL_H_

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
#include <controller/ControllerClient.h>
#include <Debug.h>
#include <UUID.h>
#include <controller/access/Rule.h>

class Handle;

typedef int rule_t;

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
	AccessControl(Beetle &beetle, ControllerClient &client);
	virtual ~AccessControl();

	/*
	 * Caller should hold at least a read lock on devices.
	 */
	bool canMap(Device *from, Device *to);

	/*
	 * Return: whether access is permitted, and the error code if not
	 *
	 * Caller should hold device and handle locks.
	 */
	bool canAccessHandle(Device *client, Device *server, Handle *h, uint8_t op, uint8_t &attErr);

	/*
	 * Return: whether access is permitted, and the properties.
	 *
	 * Caller should hold device and handle locks.
	 */
	bool getCharAccessProperties(Device *client, Device *server, Handle *h, uint8_t &properties);

	/*
	 * Return: whether the uuid type may be read.
	 *
	 * Caller should hold device and handle locks.
	 */
	bool canReadType(Device *client, Device *server, UUID &attType);

	/*
	 * Get handler to clear irrelevant cached rules.
	 */
	RemoveDeviceHandler getRemoveDeviceHandler();
private:
	Beetle &beetle;

	ControllerClient &client;

	std::map<std::pair<device_t, device_t>, cached_mapping_info_t> cache;
	boost::shared_mutex cacheMutex;

	bool handleCanMapResponse(Device *from, Device *to, std::stringstream &response);
};

#endif /* CONTROLLER_ACCESSCONTROL_H_ */
