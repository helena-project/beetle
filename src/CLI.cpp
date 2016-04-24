/*
 * CLI.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "CLI.h"

#include <arpa/inet.h>
#include <bluetooth/bluetooth.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>
#include <netinet/in.h>
#include <stddef.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "ble/helper.h"
#include "device/socket/LEPeripheral.h"
#include "device/socket/tcp/TCPClientProxy.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "Debug.h"
#include "Device.h"
#include "hat/HandleAllocationTable.h"
#include "Handle.h"

CLI::CLI(Beetle &beetle, int port_, std::string path_, NetworkDiscovery *discovery)
: beetle(beetle), networkDiscovery(discovery), t() {
	port = port_;
	path = path_;
	aliasCounter = 0;
	t = std::thread(&CLI::cmdLineDaemon, this);
}

CLI::~CLI() {
	if (t.joinable()) t.join();
}

DiscoveryHandler CLI::getDiscoveryHander() {
	/*
	 * All this handler does is add the device to the list of discovered devices.
	 */
	return [this](std::string addr, peripheral_info_t info) {
		std::lock_guard<std::mutex> lg(discoveredMutex);
		if (discovered.find(addr) == discovered.end()) {
			aliases[addr] = "d" + std::to_string(aliasCounter++);
		}
		if (discovered.find(addr) != discovered.end() && discovered[addr].name.length() != 0) {
			if (info.name.length() != 0) {
				discovered[addr] = info; // overwrite previous name
			}
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
	printMessage("Welcome to Beetle CLI. Use 'help' for a list of commands.");
	while (true) {
		std::vector<std::string> cmd;
		if (!getCommand(cmd)) return;
		if (cmd.size() == 0) continue;

		std::string c1 = cmd[0];
		if (c1 == "h" || c1 == "help") {
			doHelp(cmd);
		} else if (c1 == "scan") {
			doScan(cmd);
		} else if (c1 == "connect") {
			doConnect(cmd, true);
		} else if (c1 == "connect-nd") {
			doConnect(cmd, false);
		} else if (c1 == "disconnect") {
			doDisconnect(cmd);
		} else if (c1 == "remote") {
			doRemote(cmd);
		} else if (c1 == "discover") {
			doDiscover(cmd);
		} else if (c1 == "map") {
			doMap(cmd);
		} else if (c1 == "unmap") {
			doUnmap(cmd);
		} else if (c1 == "devices") {
			doListDevices(cmd);
		} else if (c1 == "handles") {
			doListHandles(cmd);
		} else if (c1 == "offsets") {
			doListOffsets(cmd);
		} else if (c1 == "q" || c1 == "quit") {
			break;
		} else if (c1 == "name") {
			printMessage(beetle.name);
		} else if (c1 == "port") {
			printMessage(std::to_string(port));
		} else if (c1 == "path") {
			printMessage(path);
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

void CLI::doHelp(const std::vector<std::string>& cmd) {
	printMessage("Supported commands:");
	printMessage("  help,h");
	printMessage("  name\t\tPrint the name of this instance.");
	printMessage("  port\t\tPrint the tcp port for remote connections.");
	printMessage("  path\t\tPrint the unix domain socket path.");
	printMessage("  scan\t\tPrint results from background scan.");
	printMessage("  discover\t\tPerform network-wide discovery.");
	printMessage("  connect\t\tConnect to a peripheral.");
	printMessage("  connect-nd\t\tConnect without handle discovery.");
	printMessage("  remote\t\tConnect to a device at a remote gateway.");
	printMessage("  disconnect");
	printMessage("  map\t\t\tMap handles from one virtual device to another.");
	printMessage("  unmap");
	printMessage("  devices\t\tPrint connected devices.");
	printMessage("  handles\t\tPrint a device's handles.");
	printMessage("  offsets\t\tPrint mappings for a device.");
	printMessage("  quit,q");
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
		if (!isBdAddr(cmd[1]) || str2ba(cmd[1].c_str(), &addr) != 0) {
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

		if (discoverHandles) {
			device->start();
		} else {
			device->startNd();
		}

		printMessage("connected to " + std::to_string(device->getId())
			+ " : " + device->getName());

		beetle.addDevice(device);
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

void CLI::doRemote(const std::vector<std::string>& cmd) {
	if (cmd.size() != 3) {
		printUsage("remote host:port remoteId");
		return;
	}

	size_t colonIndex = cmd[1].find(':');
	if (colonIndex == std::string::npos) {
		printUsageError("invalid host:port pair");
		return;
	}
	std::string host = cmd[1].substr(0, colonIndex);

	int port;
	device_t remoteId;
	try {
		port = std::stoi(cmd[1].substr(colonIndex + 1, cmd[1].length()));
		remoteId = std::stol(cmd[2]);
	} catch (std::invalid_argument &e) {
		printUsageError("invalid port or remoteId");
		return;
	}

	VirtualDevice* device = NULL;
	try {
		device = TCPServerProxy::connectRemote(beetle, host, port, remoteId);
		beetle.addDevice(device);

		device->start();

		printMessage("connected to remote " + std::to_string(device->getId())
			+ " : " + device->getName());
		if (debug) {
			printMessage(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		printMessage("remote connection attempt failed: try again perhaps?");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}

void CLI::doDiscover(const std::vector<std::string>& cmd) {
	if (!networkDiscovery) {
		printUsageError("Beetle controller not enabled. Network discovery unavailable.");
		return;
	}

	if (cmd.size() != 2 && cmd.size() != 3) {
		printUsage("discover d");
		printUsage("discover s uuid");
		printUsage("discover c uuid");
		return;
	}

	std::list<discovery_result_t> discovered;
	if (cmd[1] == "d") {
		discovered = networkDiscovery->discoverDevices();
	} else if (cmd[1] == "s"){
		if (cmd.size() != 3) {
			printUsageError("no uuid specified");
			return;
		}
		try {
			discovered = networkDiscovery->discoverByUuid(UUID(cmd[2]), true);
		} catch (std::invalid_argument &e) {
			printUsageError("could not parse uuid");
			return;
		}
	} else if (cmd[1] == "c") {
		if (cmd.size() != 3) {
			printUsageError("no uuid specified");
			return;
		}
		try {
			discovered = networkDiscovery->discoverByUuid(UUID(cmd[2]), false);
		} catch (std::invalid_argument &e) {
			printUsageError("could not parse uuid");
			return;
		}
	} else {
		printUsageError("invalid argument: " + cmd[1]);
	}

	for (auto &d : discovered) {
		printMessage("device : " + d.name);
		printMessage("  gateway : " + d.gateway);
		printMessage("  remote id : " + std::to_string(d.id));
		printMessage("  ip : " + d.ip);
		printMessage("  port : " + std::to_string(d.port));
	}
}

void CLI::doDisconnect(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("disconnect name|id|addr");
		return;
	}

	boost::unique_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	Device *device = matchDevice(cmd[1]);
	if (device != NULL) {
		VirtualDevice *virtualDevice = dynamic_cast<VirtualDevice *>(device);
		if (virtualDevice) {
			virtualDevice->stop();
			device_t deviceId = virtualDevice->getId();
			deviceslk.unlock();
			beetle.removeDevice(deviceId);
		} else {
			printMessage("cannot stop " + device->getName());
		}
	} else {
		printMessage(cmd[1] + " does not exist");
	}
}

void CLI::doMap(const std::vector<std::string>& cmd) {
	if (cmd.size() != 3) {
		printUsage("map fromId toId");
	} else {
		device_t from;
		device_t to;
		try {
			from = std::strtol(cmd[1].c_str(), NULL, 10);
			to = std::strtol(cmd[2].c_str(), NULL, 10);
		} catch (std::invalid_argument &e) {
			std::cout << "caught invalid argument exception: " << e.what() << std::endl;
			return;
		}
		beetle.mapDevices(from, to);
	}
}

void CLI::doUnmap(const std::vector<std::string>& cmd) {
	if (cmd.size() != 3) {
		printUsage("unmap fromId toId");
	} else {
		device_t from;
		device_t to;
		try {
			from = std::stol(cmd[1].c_str());
			to = std::stol(cmd[2].c_str());
		} catch (std::invalid_argument &e) {
			std::cout << "caught invalid argument exception: " << e.what() << std::endl;
			return;
		}
		beetle.unmapDevices(from, to);
	}
}

void CLI::doListDevices(const std::vector<std::string>& cmd) {
	if (cmd.size() != 1) {
		printUsage("devices");
	} else {
		boost::shared_lock<boost::shared_mutex> lg(beetle.devicesMutex);
		if (beetle.devices.size() == 0) {
			printMessage("no devices connected");
			return;
		}

		for (auto &kv : beetle.devices) {
			Device *d = kv.second;
			printMessage(d->getName());
			printMessage("  id : " + std::to_string(d->getId()));
			printMessage("  type : " + d->getTypeStr());
			printMessage("  mtu : " + std::to_string(d->getMTU()));
			printMessage("  highestHandle : " + std::to_string(d->getHighestHandle()));

			LEPeripheral *le =  dynamic_cast<LEPeripheral *>(d);
			if (le) {
				printMessage("  deviceAddr : " + ba2str_cpp(le->getBdaddr()));
				std::stringstream ss;
				ss << "  addrType : " << ((le->getAddrType() == PUBLIC) ? "public" : "random");
				printMessage(ss.str());
			}

			TCPConnection *tcp = dynamic_cast<TCPConnection *>(d);
			if (tcp) {
				struct sockaddr_in sockaddr = tcp->getSockaddr();
				printMessage("  addr : " + std::string(inet_ntoa(sockaddr.sin_addr)));
				printMessage("  port : " + std::to_string(sockaddr.sin_port));
			}

			TCPClientProxy *rcp = dynamic_cast<TCPClientProxy *>(d);
			if (rcp) {
				printMessage("  client-gateway : " + rcp->getClientGateway());
			}

			TCPServerProxy *rsp = dynamic_cast<TCPServerProxy *>(d);
			if (rsp) {
				printMessage("  server-gateway : " + rsp->getServerGateway());
			}
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
		boost::unique_lock<std::recursive_mutex> handleLk(device->handlesMutex);
		for (auto &kv : device->handles) {
			printMessage(kv.second->str());
		}
	} else {
		printMessage(cmd[1] + " does not exist");
	}
}

void CLI::doListOffsets(std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("offsets device");
	} else {
		boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
		Device *device = matchDevice(cmd[1]);

		if (device) {
			std::lock_guard<std::mutex> hatLg(device->hatMutex);

			std::map<uint16_t, std::pair<uint16_t, Device *>> tmp; // use a map to sort by start handle
			for (device_t from : device->hat->getDevices()) {
				handle_range_t handleRange = device->hat->getDeviceRange(from);
				if (handleRange.isNull()) {
					continue;
				}
				tmp[handleRange.start] = std::pair<uint16_t, Device *>(handleRange.end, beetle.devices[from]);
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
}

/*
 * Should be holding devices lock
 */
Device *CLI::matchDevice(const std::string &input) {
	Device *device = NULL;

	if (isBdAddr(input)) {
		// match by address
		bdaddr_t addr;
		if (str2ba(input.c_str(), &addr) == 0) {
			for (auto &kv : beetle.devices) {
				LEPeripheral *peripheral = dynamic_cast<LEPeripheral *>(kv.second);
				if (peripheral && memcmp(peripheral->getBdaddr().b, addr.b, sizeof(bdaddr_t)) == 0) {
					device = peripheral;
					break;
				}
			}
		}
	} else {
		device_t id = NULL_RESERVED_DEVICE;
		try {
			id = std::stoll(input);
		} catch (std::invalid_argument &e) {
			if (debug) {
				pdebug(input + " did not match a device id");
			}
		}

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

