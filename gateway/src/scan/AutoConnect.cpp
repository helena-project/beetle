/*
 * Autoconnect.cpp
 *
 *  Created on: Apr 6, 2016
 *      Author: James Hong
 */

#include <scan/AutoConnect.h>

#include <cassert>
#include <iostream>
#include <thread>

#include "Beetle.h"
#include "device/socket/LEPeripheral.h"
#include "Debug.h"
#include "Device.h"

AutoConnect::AutoConnect(Beetle &beetle, bool connectAll_) : beetle{beetle}, daemonThread() {
	connectAll = connectAll_;
	daemonRunning = true;
	daemonThread = std::thread(&AutoConnect::daemon, this, 60);
}

AutoConnect::~AutoConnect() {
	daemonRunning = false;
	if (daemonThread.joinable()) {
		daemonThread.join();
	}
}

DiscoveryHandler AutoConnect::getDiscoveryHandler() {
	return [this](std::string addr, peripheral_info_t info) {
		if (maxConcurrentAttempts.try_wait()) {
			/*
			 * Consult whitelist
			 */
			if (!connectAll && configEntries.find(addr) == configEntries.end()) {
				maxConcurrentAttempts.notify();
				return;
			}

			/*
			 * Have we tried to connect recently?
			 */
			std::unique_lock<std::mutex> lastAttemptLk(lastAttemptMutex);
			if (lastAttempt.find(addr) != lastAttempt.end()) {
				maxConcurrentAttempts.notify();
				return;
			} else {
				time_t now = time(NULL);
				lastAttempt[addr] = now;
				lastAttemptLk.unlock();
			}

			/*
			 * Are we already connected? This is needed to support some peripherals
			 * which keep advertising even after connecting.
			 */
			boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
			for (auto &kv : beetle.devices) {
				LEPeripheral *le = NULL;
				if (kv.second->getType() == Device::LE_PERIPHERAL &&
						(le = dynamic_cast<LEPeripheral *>(kv.second))) {
					if (le->getAddrType() == info.bdaddrType &&
							memcmp(le->getBdaddr().b, info.bdaddr.b, sizeof(bdaddr_t))) {
						maxConcurrentAttempts.notify();
						return;
					}
				}
			}
			devicesLk.unlock();

			if (debug_scan) {
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
		}
	};
}

void AutoConnect::connect(peripheral_info_t info, autoconnect_config_t config) {
	VirtualDevice* device = NULL;
	try {
		device = new LEPeripheral(beetle, info.bdaddr, info.bdaddrType);
		beetle.addDevice(device);

		device->start(config.discover);

		if (debug_scan) {
			pdebug("auto-connected to " + device->getName());
			pdebug(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		std::cerr << "caught exception: " << e.what() << std::endl;
		pdebug("auto-connection attempt failed");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
	maxConcurrentAttempts.notify();
}

void AutoConnect::daemon(int seconds) {
	if (debug_scan) {
		pdebug("autoConnectDaemon started");
	}

	// semi-busy wait to reduce shutdown latency
	int ticks = 1;
	while (daemonRunning) {
		if (ticks % seconds == 0) {
			time_t now = time(NULL);
			std::lock_guard<std::mutex> lg(lastAttemptMutex);
			for (auto it = lastAttempt.cbegin(); it != lastAttempt.cend();) {
				if (difftime(now, it->second) > minAttemptInterval) {
					if (debug_scan) {
						pdebug("can try connecting to '" + it->first + "' again");
					}
					lastAttempt.erase(it++);
				} else {
					++it;
				}
			}
		}

		sleep(1);
		ticks++;
	}

	if (debug_scan) {
		pdebug("autoConnectDaemon exited");
	}
}
