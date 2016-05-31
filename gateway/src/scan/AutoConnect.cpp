/*
 * Autoconnect.cpp
 *
 *  Created on: Apr 6, 2016
 *      Author: James Hong
 */

#include <scan/AutoConnect.h>

#include <bluetooth/bluetooth.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <utility>

#include "Beetle.h"
#include "ble/helper.h"
#include "device/socket/LEPeripheral.h"
#include "Debug.h"
#include "Device.h"
#include "sync/ThreadPool.h"
#include "util/file.h"
#include "util/trim.h"

AutoConnect::AutoConnect(Beetle &beetle, bool connectAll_, double minBackoff_, std::string autoConnectBlacklist) :
		beetle { beetle } {
	connectAll = connectAll_;
	minBackoff = minBackoff_;

	if (autoConnectBlacklist != "") {
		if (!file_exists(autoConnectBlacklist)) {
			throw std::invalid_argument("blacklist file does not exist - " + autoConnectBlacklist);
		} else {
			std::ifstream ifs(autoConnectBlacklist);
			std::string line;
			while (std::getline(ifs, line)) {
				std::string addr = trimmed(line);
				if (addr == "") {
					continue;
				}

				if (is_bd_addr(addr)) {
					blacklist.insert(addr);
				} else {
					throw std::domain_error("invalid address in blacklist file - " + addr);
				}
			}
		}
	}
}

AutoConnect::~AutoConnect() {

}

std::function<void()> AutoConnect::getDaemon() {
	return [this] {
		time_t now = time(NULL);

		std::lock_guard<std::mutex> lg(lastAttemptMutex);
		for (auto it = lastAttempt.cbegin(); it != lastAttempt.cend();) {
			if (difftime(now, it->second) > minBackoff) {
				if (debug_scan) {
					pdebug("can try connecting to '" + it->first + "' again");
				}
				lastAttempt.erase(it++);
			} else {
				++it;
			}
		}
	};
}

DiscoveryHandler AutoConnect::getDiscoveryHandler() {
	return [this](std::string addr, peripheral_info_t info) {
		std::unique_lock<std::mutex> connectLk(connectMutex, std::try_to_lock);
		if (connectLk.owns_lock()) {
			/*
			 * Consult blacklist
			 */
			if (!connectAll || blacklist.find(addr) != blacklist.end()) {
				return;
			}

			/*
			 * Have we tried to connect recently?
			 */
			std::unique_lock<std::mutex> lastAttemptLk(lastAttemptMutex);
			if (lastAttempt.find(addr) != lastAttempt.end()) {
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
				std::shared_ptr<LEPeripheral> le = NULL;
				if (kv.second->getType() == Device::LE_PERIPHERAL &&
						(le = std::dynamic_pointer_cast<LEPeripheral>(kv.second))) {
					if (le->getAddrType() == info.bdaddrType &&
							memcmp(le->getBdaddr().b, info.bdaddr.b, sizeof(bdaddr_t))) {
						return;
					}
				}
			}
			devicesLk.unlock();

			if (debug_scan) {
				pdebug("trying to auto-connect to " + addr);
			}

			beetle.workers.schedule([this, info]{
				std::lock_guard<std::mutex> connectLg(connectMutex, std::adopt_lock);
				connect(info, true);
			});

			// do not unlock
			connectLk.release();
		} else {
			if (debug_scan) {
				pdebug("auto-connect in progress, skipping");
			}
		}
	};
}

void AutoConnect::connect(peripheral_info_t info, bool discover) {
	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		device = std::make_shared<LEPeripheral>(beetle, info.bdaddr, info.bdaddrType);

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);

		device->start(discover);

		if (debug_scan) {
			pdebug("auto-connected to " + device->getName());
			pdebug(device->getName() + " has handle range [0," + std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		if (debug) {
			pexcept(e);
			pdebug("failed to connect to " + ba2str_cpp(info.bdaddr));
		}
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}
