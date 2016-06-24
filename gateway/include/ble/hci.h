/*
 * hci_utils.h
 *
 *  Created on: Jun 24, 2016
 *      Author: james
 */

#ifndef BLE_HCI_H_
#define BLE_HCI_H_

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <set>

inline void hci_reset_dev(std::string dev) {
	if (system(NULL) == 0) {
		throw std::runtime_error("call to system failed");
	}

	std::string command = "hciconfig " + dev + " down";
	sleep(1); // sleeping seems to ensure that the settings do get applied
	std::cout << "system: " << command << std::endl;
	if (system(command.c_str()) != 0) {
		throw std::runtime_error(command + " failed");
	}
	command = "hciconfig " + dev + " up";
	std::cout << "system: " << command << std::endl;
	if (system(command.c_str()) != 0) {
		throw std::runtime_error(command + " failed");
	}
	sleep(1);
}

inline std::string hci_get_dev_name(int devId) {
	hci_dev_info devInfo;
	hci_devinfo(devId, &devInfo);
	return std::string(devInfo.name);
}

inline std::string hci_default_dev() {
	int devId = hci_get_route(NULL);
	if (devId < 0) {
		throw std::runtime_error("could not get default hci device");
	}
	return hci_get_dev_name(devId);
}

inline std::set<int> hci_get_device_ids() {
	std::set<int> deviceIds;
	hci_for_each_dev(HCI_UP, [](int s, int deviceId, long ptr) {
		std::set<int> *deviceIds = (std::set<int> *)ptr;
		deviceIds->insert(deviceId);
		return 0;
	}, (long)&deviceIds);
	return deviceIds;
}

#endif /* BLE_HCI_H_ */
