/*
 * CLI.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "CLI.h"

#include <bluetooth/bluetooth.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "ble/helper.h"
#include "device/BeetleDevice.h"
#include "device/LEPeripheral.h"
#include "Debug.h"
#include "Device.h"
#include "hat/HAT.h"
#include "Handle.h"

CLI::CLI(Beetle &beetle) : beetle(beetle), t() {
	t = std::thread(&CLI::cmdLineDaemon, this);
	aliasCounter = 0; // start this high to avoid confusion
}

CLI::~CLI() {
	// TODO Auto-generated destructor stub
}

DiscoveryHandler CLI::getDiscoveryHander() {
	/*
	 * All this handler does is add the device to the list of discovered devices.
	 */
	return [this](std::string addr, peripheral_info_t info) -> void {
		std::lock_guard<std::mutex> lg(discoveredMutex);
		if (discovered.find(addr) != discovered.end()) {
			aliases[addr] = "d" + std::to_string(aliasCounter++);
		}
		if (discovered.find(addr) != discovered.end() && discovered[addr].name.length() != 0) {
			// do nothing
		} else {
			discovered[addr] = info;
		}
	};
}

void CLI::join() {
	assert(t.joinable());
	t.join();
}

static void printUsage(std::string usage) {
	std::cout << "Usage: " << usage << std::endl;
}

static void printUsageError(std::string error) {
	std::cout << "Error: " << error << std::endl;
}

static void printMessage(std::string message) {
	std::cout << "  " << message << std::endl;
}


void CLI::cmdLineDaemon() {
	while (true) {
		std::vector<std::string> cmd;
		if (!getCommand(cmd)) return;
		if (cmd.size() == 0) continue;

		std::string c1 = cmd[0];
		if (c1 == "scan") {
			doScan(cmd);
		} else if (c1 == "connect") {
			doConnect(cmd, true);
		} else if (c1 == "connect-nd") {
			doConnect(cmd, false);
		} else if (c1 == "disconnect") {
			doDisconnect(cmd);
		} else if (c1 == "devices") {
			doListDevices(cmd);
		} else if (c1 == "handles") {
			doListHandles(cmd);
		} else if (c1 == "offsets") {
			doListOffsets(cmd);
		} else if (c1 == "debug") {
			doToggleDebug(cmd);
		} else if (c1 == "q" || c1 == "quit") {
			exit(0);
		} else {
			printMessage("unknown command");
		}
	}
}

bool CLI::getCommand(std::vector<std::string> &ret) {
	assert(ret.empty());
	std::cout << "> ";
	std::string line;
	getline(std::cin, line);
	transform(line.begin(), line.end(), line.begin(), ::tolower);
	if (std::cin.eof()) {
		std::cout << "Exiting..." << std::endl;
		return false;
	} else if (std::cin.bad()) {
		throw "Error";
	} else {
		boost::char_separator<char> sep{" "};
		boost::tokenizer<boost::char_separator<char>> tokenizer{line, sep};
		for (const auto &t : tokenizer) {
			ret.push_back(t);
		}
		return true;
	}
}

void CLI::doScan(const std::vector<std::string>& cmd) {
	if (cmd.size() != 1) {
		printUsage("scan");
		return;
	}

	printMessage("alias\taddr\t\t\ttype\tname");
	std::lock_guard<std::mutex> lg(discoveredMutex);
	for (auto &d : discovered) {
		std::stringstream ss;
		ss << aliases[d.first] << "\t" << d.first
				<< "\t" << ((d.second.bdaddrType == PUBLIC) ? "public" : "random")
				<< "\t" << d.second.name;
		printMessage(ss.str());
	}
}

void CLI::doConnect(const std::vector<std::string>& cmd, bool discoverHandles) {
	if (cmd.size() != 2 && cmd.size() != 3) {
		printUsage("connect deviceAddr public|random");
		printUsage("connect alias");
		return;
	}
	AddrType addrType;
	bdaddr_t addr;

	if (cmd.size() == 3) {
		/*
		 * Use address and type supplied by the user
		 */
		if (str2ba(cmd[1].c_str(), &addr) != 0) {
			printUsageError("invalid device address");
			return;
		}

		if (cmd[2] == "public") {
			addrType = PUBLIC;
		} else if (cmd[2] == "random") {
			addrType = RANDOM;
		} else {
			printUsageError("invalid address type");
			return;
		}
	} else {
		/*
		 * Look for device in discovered results
		 */
		std::lock_guard<std::mutex> lg(discoveredMutex);
		bool found = false;
		for (auto &kv : aliases) {
			if (kv.second == cmd[1]) {
				addr = discovered[kv.first].bdaddr;
				addrType = discovered[kv.first].bdaddrType;
				found = true;
				break;
			}
		}
		if (!found) {
			printUsageError("no device with alias " + cmd[1]);
			return;
		}
	}

	VirtualDevice* device = NULL;
	try {
		device = new LEPeripheral(beetle, addr, addrType);
		beetle.addDevice(device, discoverHandles);

		if (discoverHandles) {
			device->start();
		} else {
			device->startNd();
		}

		beetle.hatMutex.lock_shared();
		handle_range_t handles = beetle.hat->getDeviceRange(device->getId());
		beetle.hatMutex.unlock_shared();

		if (!HAT::isNullRange(handles)) {
			beetle.beetleDevice->servicesChanged(handles, device->getId());
		}

		printMessage("connected to " + device->getName());
		if (debug) {
			printMessage(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		printMessage("connection attempt failed: try again perhaps?");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}

void CLI::doDisconnect(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("disconnect name|id|addr");
		return;
	}

	boost::unique_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	Device *device = matchDevice(cmd[1]);
	deviceslk.unlock();

	if (device != NULL) {
		VirtualDevice *virtualDevice = dynamic_cast<VirtualDevice *>(device);
		if (virtualDevice) {
			virtualDevice->stop();
			beetle.removeDevice(device->getId());
		} else {
			printMessage("cannot stop " + device->getName());
		}
	} else {
		printMessage(cmd[1] + " does not exist");
	}
}

void CLI::doListDevices(const std::vector<std::string>& cmd) {
	boost::shared_lock<boost::shared_mutex> lg(beetle.devicesMutex);
	if (beetle.devices.size() == 0) {
		printMessage("no devices connected");
	} else {
		for (auto &kv : beetle.devices) {
			Device *d = kv.second;
			printMessage(d->getName());
			printMessage("  id : " + std::to_string(d->getId()));
			printMessage("  type : " + d->getType());
			printMessage("  mtu : " + std::to_string(d->getMTU()));
			printMessage("  highestHandle : " + std::to_string(d->getHighestHandle()));
		}
	}
}

void CLI::doListHandles(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("handles name|id|addr");
		return;
	}

	boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	Device *device = matchDevice(cmd[1]);

	if (device != NULL) {
		boost::shared_lock<boost::shared_mutex> hatLk(beetle.hatMutex);
		handle_range_t handleRange = beetle.hat->getDeviceRange(device->getId());

		boost::unique_lock<std::recursive_mutex> handleLk(device->handlesMutex);
		deviceslk.unlock();
		hatLk.unlock();

		if (handleRange.start == 0 && handleRange.end == 0) {
			printMessage("warning: " + device->getName() + " is not mapped.");
		}

		for (auto &kv : device->handles) {
			printMessage(std::to_string(kv.first + handleRange.start) + " : " + kv.second->str());
		}
	} else {
		printMessage(cmd[1] + " does not exist");
	}
}

void CLI::doToggleDebug(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("debug on|off");
	} else {
		if (cmd[1] == "on") {
			debug = true;
		} else if (cmd[1] == "off") {
			debug = false;
		} else {
			printUsageError("invalid debug setting");
		}
	}
}

void CLI::doListOffsets(std::vector<std::string>& cmd) {
	if (cmd.size() != 1) {
		printUsage("offsets");
	} else {
		boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
		boost::shared_lock<boost::shared_mutex> hatLk(beetle.hatMutex);

		std::map<uint16_t, std::pair<uint16_t, Device *>> tmp; // use a map to sort by start handle
		for (auto &kv : beetle.devices) {
			handle_range_t handleRange = beetle.hat->getDeviceRange(kv.first);
			if (handleRange.start == 0 && handleRange.end == 0) continue;
			tmp[handleRange.start] = std::pair<uint16_t, Device *>(handleRange.end, kv.second);
		}

		printMessage("start\tend\tid\tname");
		for (auto &kv : tmp) {
			std::stringstream ss;
			ss << kv.first << '\t' << kv.second.first << '\t' << kv.second.second->getId()
					<< '\t' << kv.second.second->getName();
			printMessage(ss.str());
		}
	}
}

Device *CLI::matchDevice(const std::string &input) {
	Device *device = NULL;

	bdaddr_t addr;
	if (str2ba(input.c_str(), &addr) == 0) {
		// match by address
		for (auto &kv : beetle.devices) {
			LEPeripheral *peripheral = dynamic_cast<LEPeripheral *>(kv.second);
			if (peripheral && memcmp(peripheral->getBdaddr().b, addr.b, sizeof(bdaddr_t)) == 0) {
				device = peripheral;
				break;
			}
		}
	} else {
		device_t id = NULL_RESERVED_DEVICE;
		try {
			id = std::stoi(input);
		} catch (std::invalid_argument &e) { };

		if (id == BEETLE_RESERVED_DEVICE || id > 0) {
			// match by id
			if (beetle.devices.find(id) != beetle.devices.end()) {
				device = beetle.devices[id];
			}
		} else {
			// match by name
			for (auto &kv : beetle.devices) {
				if (boost::iequals(kv.second->getName(), input)) {
					device = kv.second;
				}
			}
		}
	}
	return device;
}

