/*
 * Autoconnect.cpp
 *
 *  Created on: Apr 6, 2016
 *      Author: james
 */

#include "AutoConnect.h"

#include <boost/thread/pthread/shared_mutex.hpp>
#include <iostream>
#include <thread>

#include "Beetle.h"
#include "device/BeetleDevice.h"
#include "device/LEPeripheral.h"
#include "Debug.h"
#include "Device.h"
#include "hat/HAT.h"


AutoConnect::AutoConnect(Beetle &beetle, bool connectAll_) : beetle{beetle} {
	connectAll = connectAll_;
}

AutoConnect::~AutoConnect() {
	// nothing to do
}

DiscoveryHandler AutoConnect::getDiscoveryHandler() {
	return [this](std::string addr, peripheral_info_t info) -> void {
		if (maxConcurrentAttempts.try_wait()) {
			std::lock_guard<std::mutex> lg(m);
			time_t now = time(NULL);

			if (!connectAll && configEntries.find(addr) == configEntries.end()) {
				maxConcurrentAttempts.notify();
				return;
			}

			if (lastAttempt.find(addr) != lastAttempt.end()
					&& difftime(now, lastAttempt[addr]) < minAttemptInterval) {
				maxConcurrentAttempts.notify();
				return;
			}

			if (debug) {
				pdebug("trying to auto-connect to " + addr);
			}

			autoconnect_config_t config;
			if (configEntries.find(addr) != configEntries.end()) {
				config = configEntries[addr];
			} else {
				assert(connectAll);
				config.discover = true;
				config.name = info.name;
				config.addr = addr;
			}

			std::thread t(&AutoConnect::connect, this, info, config);
			t.detach();

			lastAttempt[addr] = now;
		}
	};
}

void AutoConnect::connect(peripheral_info_t info, autoconnect_config_t config) {
	VirtualDevice* device = NULL;
	try {
		device = new LEPeripheral(beetle, info.bdaddr, info.bdaddrType);
		beetle.addDevice(device, config.discover);

		if (config.discover) {
			device->start();

			beetle.hatMutex.lock_shared();
			handle_range_t handles = beetle.hat->getDeviceRange(device->getId());
			beetle.hatMutex.unlock_shared();

			if (!HAT::isNullRange(handles)) {
				beetle.beetleDevice->servicesChanged(handles, device->getId());
			}
		} else {
			device->startNd();
		}

		if (debug) {
			pdebug("auto-connected to " + device->getName());
			pdebug(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		pdebug("auto-connection attempt failed");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
	maxConcurrentAttempts.notify();
}
