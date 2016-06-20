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

AutoConnect::AutoConnect(Beetle &beetle, bool connectAll_, double minBackoff_, std::string autoConnectWhitelist) :
		beetle { beetle } {
	connectAll = connectAll_;
	minBackoff = minBackoff_;

	if (autoConnectWhitelist != "") {
		if (!file_exists(autoConnectWhitelist)) {
			throw std::invalid_argument("whitelist file does not exist - " + autoConnectWhitelist);
		}

		std::cout << "whitelist file: " << autoConnectWhitelist << std::endl;

		std::ifstream ifs(autoConnectWhitelist);
		std::string line;
		while (std::getline(ifs, line)) {
			std::string addr = trimmed(line);
			if (addr.find('#') != addr.npos) {
				addr = addr.substr(0, addr.find('#'));
			}

			trim(addr);
			if (addr == "") {
				continue;
			}

			if (is_bd_addr(addr)) {
				bdaddr_t bdaddr;
				str2ba(addr.c_str(), &bdaddr);
				whitelist.insert(ba2str_cpp(bdaddr));
			} else {
				throw ParseExecption("invalid address in whitelist file - " + line);
			}
		}

		for (auto &addr : whitelist) {
			std::cout << "  " << addr << std::endl;
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
	return [this](peripheral_info_t info) {
		std::string addr = ba2str_cpp(info.bdaddr);

		std::unique_lock<std::mutex> connectLk(connectMutex, std::try_to_lock);
		if (connectLk.owns_lock()) {
			/*
			 * Consult whitelist
			 */
			if (!connectAll && whitelist.find(addr) == whitelist.end()) {
				if (debug_scan) {
					std::stringstream ss;
					ss << "whitelist does not contain " << addr;
					pdebug(ss.str());
				}
				return;
			}

			/*
			 * Have we tried to connect recently?
			 */
			std::unique_lock<std::mutex> lastAttemptLk(lastAttemptMutex);
			if (lastAttempt.find(addr) != lastAttempt.end()) {
				if (debug_scan) {
					std::stringstream ss;
					ss << "recently tried connecting to " << addr;
					pdebug(ss.str());
				}
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
							memcmp(le->getBdaddr().b, info.bdaddr.b, sizeof(bdaddr_t)) == 0) {
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
