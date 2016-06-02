/*
 * HciManager.cpp
 *
 *  Created on: Apr 24, 2016
 *      Author: James Hong
 */

#include <HCI.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <errno.h>
#include <sstream>
#include <unistd.h>

#include "Debug.h"

HCI::HCI() {
	int deviceId = hci_get_route(NULL);
	if (deviceId < 0) {
		throw HCIException("could not get hci device");
	}

	dd = hci_open_dev(deviceId);
	if (dd < 0) {
		throw HCIException("could not get handle to hci device");
	}
}

HCI::~HCI() {
	hci_close_dev(dd);
}

void HCI::resetHCI() {
	assert(system(NULL) != 0);
	int hciDevice = hci_get_route(NULL);
	assert(hciDevice >= 0);
	std::string hciName = "hci" + std::to_string(hciDevice);
	std::string command = "hciconfig " + hciName + " down";
	sleep(1); // sleeping seems to ensure that the settings do get applied
	pdebug("System: " + command);
	assert(system(command.c_str()) == 0);
	command = "hciconfig " + hciName + " up";
	pdebug("System: " + command);
	assert(system(command.c_str()) == 0);
	sleep(1);
}

bool HCI::setConnectionInterval(uint16_t hciHandle, uint16_t minInterval, uint16_t maxInterval, uint16_t latency,
		uint16_t supervisionTimeout, int to) {
	std::lock_guard<std::mutex> lg(m);
	if (hci_le_conn_update(dd, hciHandle, minInterval, maxInterval, latency, supervisionTimeout, to) < 0) {
		if (debug) {
			std::stringstream ss;
			ss << "error setting HCI connection interval : " << strerror(errno);
			pwarn(ss.str());
		}
		return false;
	}
	return true;
}

