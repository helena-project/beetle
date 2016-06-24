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

#include "ble/hci.h"
#include "ble/utils.h"
#include "Debug.h"

HCI::HCI(std::string dev) {
	assert(dev != "");
	deviceId = hci_devid(dev.c_str());
	if (deviceId < 0) {
		throw HCIException("could not get hci device");
	}

	deviceHandle = hci_open_dev(deviceId);
	if (deviceHandle < 0) {
		throw HCIException("could not get handle to hci device");
	}
}

HCI::~HCI() {
	hci_close_dev(deviceHandle);
}

int HCI::getDeviceId() {
	return deviceId;
}

bool HCI::setConnectionInterval(uint16_t hciHandle, uint16_t minInterval,
		uint16_t maxInterval, uint16_t latency, uint16_t supervisionTimeout, int to) {
	std::lock_guard<std::mutex> lg(m);
	if (hci_le_conn_update(deviceHandle, hciHandle, minInterval, maxInterval, latency,
			supervisionTimeout, to) < 0) {
		if (debug) {
			std::stringstream ss;
			ss << "error setting HCI connection interval : " << strerror(errno);
			pwarn(ss.str());
		}
		return false;
	}
	return true;
}

int HCI::newCommand(std::function<int(int deviceHandle)> f) {
	std::lock_guard<std::mutex> lg(m);
	return f(deviceHandle);
}

