/*
 * CLI.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
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

#include "Beetle.h"
#include "ble/helper.h"
#include "controller/NetworkDiscoveryClient.h"
#include "Debug.h"
#include "Device.h"
#include "device/socket/LEPeripheral.h"
#include "device/socket/tcp/TCPClientProxy.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "Handle.h"
#include "hat/HandleAllocationTable.h"
#include "Router.h"

CLI::CLI(Beetle &beetle, BeetleConfig beetleConfig, std::shared_ptr<NetworkDiscoveryClient> discovery) :
		beetle(beetle), beetleConfig(beetleConfig), networkDiscovery(discovery), inputDaemon() {
	aliasCounter = 0;
	inputDaemon = std::thread(&CLI::cmdLineDaemon, this);
}

CLI::~CLI() {
	if (inputDaemon.joinable()) inputDaemon.join();
}

DiscoveryHandler CLI::getDiscoveryHander() {
	/*
	 * All this handler does is add the device to the list of discovered devices.
	 */
	return [this](std::string addr, peripheral_info_t info) {
		std::lock_guard<std::mutex> lg(discoveredMutex);
		if (discovered.find(addr) == discovered.end()) {
			discovered_t entry;
			entry.alias = "d" + std::to_string(aliasCounter++);
			entry.info = info;
			discovered[addr] = entry;
		} else {
			if (info.name.length() != 0) {
				discovered[addr].info = info; // overwrite previous name
			}
		}
		discovered[addr].lastSeen = time(NULL);
	};
}

std::function<void()> CLI::getDaemon() {
	return [this] {
		time_t now = time(NULL);

		std::lock_guard<std::mutex> lg(discoveredMutex);
		for (auto it = discovered.cbegin(); it != discovered.cend();) {
			if (difftime(now, it->second.lastSeen) > 60 * 5) { // 5 minutes
				discovered.erase(it++);
			} else {
				++it;
			}
		}
	};
}

void CLI::join() {
	assert(inputDaemon.joinable());
	inputDaemon.join();
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
		} else if (c1 == "s" || c1 == "scan") {
			doScan(cmd);
		} else if (c1 == "connect") {
			doConnect(cmd, true);
		} else if (c1 == "connect-nd") {
			doConnect(cmd, false);
		} else if (c1 == "disconnect") {
			doDisconnect(cmd);
		} else if (c1 == "r" || c1 == "remote") {
			doRemote(cmd);
		} else if (c1 == "network") {
			doNetworkDiscover(cmd);
		} else if (c1 == "map") {
			doMap(cmd);
		} else if (c1 == "unmap") {
			doUnmap(cmd);
		} else if (c1 == "d" || c1 == "devices") {
			doListDevices(cmd);
		} else if (c1 == "ha" || c1 == "handles") {
			doListHandles(cmd);
		} else if (c1 == "o" || c1 == "offsets") {
			doListOffsets(cmd);
		} else if (c1 == "interval") {
			doSetMaxConnectionInterval(cmd);
		} else if (c1 == "debug") {
			doSetDebug(cmd);
		} else if (c1 == "dump") {
			doDumpData(cmd);
		} else if (c1 == "name") {
			printMessage(beetle.name);
		} else if (c1 == "port") {
			printMessage(std::to_string(beetleConfig.tcpPort));
		} else if (c1 == "path") {
			printMessage(beetleConfig.ipcPath);
		} else if (c1 == "q" || c1 == "quit") {
			printMessage("CLI exiting...");
			break;
		} else {
			printUsageError("unknown command");
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
		boost::char_separator<char> sep { " " };
		boost::tokenizer<boost::char_separator<char>> tokenizer { line, sep };
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
	printMessage("");
	printMessage("  scan,s\t\tPrint results from background scan.");
	printMessage("  connect\t\tConnect to a peripheral.");
	printMessage("  connect-nd\t\tConnect without handle discovery.");
	printMessage("  network\t\tPerform network-wide discovery.");
	printMessage("  remote,r\t\tConnect to a device at a remote gateway.");
	printMessage("  disconnect");
	printMessage("");
	printMessage("  map\t\t\tMap handles from one virtual device to another.");
	printMessage("  unmap");
	printMessage("");
	printMessage("  devices,d\t\tPrint connected devices.");
	printMessage("  handles,ha\t\tPrint a device's handles.");
	printMessage("  offsets,o\t\tPrint mappings for a device.");
	printMessage("");
	printMessage("  interval\t\tSet max-connection interval for all devices.");
	printMessage("  dump\t\tDump data into console.");
	printMessage("");
	printMessage("  debug\t\tSet debugging level.");
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
		ss << d.second.alias << "\t" << d.first << "\t"
				<< ((d.second.info.bdaddrType == LEPeripheral::AddrType::PUBLIC) ? "public" : "random") << "\t"
				<< d.second.info.name;
		printMessage(ss.str());
	}
}

void CLI::doConnect(const std::vector<std::string>& cmd, bool discoverHandles) {
	if (cmd.size() != 2 && cmd.size() != 3) {
		printUsage("connect deviceAddr public|random");
		printUsage("connect alias");
		return;
	}
	LEPeripheral::AddrType addrType;
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
			addrType = LEPeripheral::AddrType::PUBLIC;
		} else if (cmd[2] == "random") {
			addrType = LEPeripheral::AddrType::RANDOM;
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
		for (auto &kv : discovered) {
			if (kv.second.alias == cmd[1]) {
				addr = kv.second.info.bdaddr;
				addrType = kv.second.info.bdaddrType;
				found = true;
				break;
			}
		}
		if (!found) {
			printUsageError("no device with alias " + cmd[1]);
			return;
		}
	}

	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		device = std::make_shared<LEPeripheral>(beetle, addr, addrType);

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);

		device->start(discoverHandles);

		printMessage("connected to " + std::to_string(device->getId()) + " : " + device->getName());

		if (debug) {
			printMessage(
					device->getName() + " has handle range [0," + std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		pexcept(e);
		printMessage("connection attempt failed: try again perhaps?");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}

void CLI::doRemote(const std::vector<std::string>& cmd) {
	if (cmd.size() != 3) {
		printUsage("remote host:port remoteId");
		printUsage("remote remoteGateway remoteId");
		return;
	}
	std::string host;
	int port;
	device_t remoteId;
	size_t colonIndex = cmd[1].find(':');
	if (colonIndex != std::string::npos) {
		/*
		 * Parse from host:port
		 */
		host = cmd[1].substr(0, colonIndex);
		try {
			port = std::stoi(cmd[1].substr(colonIndex + 1, cmd[1].length()));
		} catch (std::invalid_argument &e) {
			printUsageError("invalid port");
			return;
		}
	} else {
		/*
		 * Lookup by gateway name
		 */
		if (!networkDiscovery) {
			printUsageError("Beetle controller not enabled. Gateway lookup unavailable.");
			return;
		}
		if (!networkDiscovery->findGatewayByName(cmd[1], host, port)) {
			printMessage("failed to locate " + cmd[1]);
			return;
		}
	}

	try {
		remoteId = std::stol(cmd[2]);
	} catch (std::invalid_argument &e) {
		printUsageError("invalid remoteId");
		return;
	}

	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		device.reset(TCPServerProxy::connectRemote(beetle, host, port, remoteId));

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);

		device->start();

		printMessage("connected to remote " + std::to_string(device->getId()) + " : " + device->getName());
		if (debug) {
			printMessage(
					device->getName() + " has handle range [0," + std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		pexcept(e);
		printMessage("remote connection attempt failed: try again perhaps?");
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}

void CLI::doNetworkDiscover(const std::vector<std::string>& cmd) {
	if (!networkDiscovery) {
		printUsageError("Beetle controller not enabled. Network discovery unavailable.");
		return;
	}

	if (cmd.size() != 2 && cmd.size() != 3) {
		printUsage("network d");
		printUsage("network s uuid");
		printUsage("network c uuid");
		return;
	}

	bool success = false;
	std::list<discovery_result_t> discovered;
	if (cmd[1] == "d") {
		success = networkDiscovery->discoverDevices(discovered);
	} else if (cmd[1] == "s") {
		if (cmd.size() != 3) {
			printUsageError("no uuid specified");
			return;
		}
		try {
			success = networkDiscovery->discoverByUuid(UUID(cmd[2]), discovered, true);
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
			success = networkDiscovery->discoverByUuid(UUID(cmd[2]), discovered, false);
		} catch (std::invalid_argument &e) {
			printUsageError("could not parse uuid");
			return;
		}
	} else {
		printUsageError("invalid argument: " + cmd[1]);
	}

	if (success) {
		for (auto &d : discovered) {
			printMessage("device : " + d.name);
			printMessage("  gateway : " + d.gateway);
			printMessage("  remote id : " + std::to_string(d.id));
			printMessage("  ip : " + d.ip);
			printMessage("  port : " + std::to_string(d.port));
		}
	} else {
		printMessage("network discovery failed");
	}
}

void CLI::doDisconnect(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("disconnect name|id|addr");
		return;
	}

	boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	std::shared_ptr<Device> device = matchDevice(cmd[1]);
	if (device != NULL) {
		auto virtualDevice = std::dynamic_pointer_cast<VirtualDevice>(device);
		if (virtualDevice) {
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
		return;
	}

	boost::shared_lock<boost::shared_mutex> lg(beetle.devicesMutex);
	if (beetle.devices.size() == 0) {
		printMessage("no devices connected");
		return;
	}

	for (auto &kv : beetle.devices) {
		std::shared_ptr<Device> d = kv.second;
		printMessage(d->getName());
		printMessage("  id : " + std::to_string(d->getId()));
		printMessage("  type : " + d->getTypeStr());
		printMessage("  mtu : " + std::to_string(d->getMTU()));
		printMessage("  highestHandle : " + std::to_string(d->getHighestHandle()));

		auto le = std::dynamic_pointer_cast<LEPeripheral>(d);
		if (le) {
			printMessage("  deviceAddr : " + ba2str_cpp(le->getBdaddr()));
			std::stringstream ss;
			ss << "  addrType : " << ((le->getAddrType() == LEPeripheral::AddrType::PUBLIC) ? "public" : "random");
			printMessage(ss.str());
		}

		auto tcp = std::dynamic_pointer_cast<TCPConnection>(d);
		if (tcp) {
			struct sockaddr_in sockaddr = tcp->getSockaddr();
			printMessage("  addr : " + std::string(inet_ntoa(sockaddr.sin_addr)));
			printMessage("  port : " + std::to_string(sockaddr.sin_port));
		}

		auto rcp = std::dynamic_pointer_cast<TCPClientProxy>(d);
		if (rcp) {
			printMessage("  client-gateway : " + rcp->getClientGateway());
		}

		auto rsp = std::dynamic_pointer_cast<TCPServerProxy>(d);
		if (rsp) {
			printMessage("  server-gateway : " + rsp->getServerGateway());
		}
	}
}

void CLI::doListHandles(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("handles name|id|addr");
		return;
	}

	boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	std::shared_ptr<Device> device = matchDevice(cmd[1]);

	if (device != NULL) {
		boost::unique_lock<std::recursive_mutex> handleLk(device->handlesMutex);
		for (auto &kv : device->handles) {
			printMessage(kv.second->str());
		}
	} else {
		printMessage(cmd[1] + " does not exist");
	}
}

void CLI::doListOffsets(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("offsets device");
		return;
	}

	boost::shared_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	std::shared_ptr<Device> device = matchDevice(cmd[1]);

	if (device) {
		std::lock_guard<std::mutex> hatLg(device->hatMutex);

		std::map<uint16_t, std::pair<uint16_t, std::shared_ptr<Device>>>tmp; // use a map to sort by start handle
		for (device_t from : device->hat->getDevices()) {
			handle_range_t handleRange = device->hat->getDeviceRange(from);
			if (handleRange.isNull()) {
				continue;
			}
			tmp[handleRange.start] = std::pair<uint16_t, std::shared_ptr<Device>>(handleRange.end,
					beetle.devices[from]);
		}

		printMessage("start\tend\tid\tname");
		for (auto &kv : tmp) {
			std::stringstream ss;
			ss << kv.first << '\t' << kv.second.first << '\t' << kv.second.second->getId() << '\t'
					<< kv.second.second->getName();
			printMessage(ss.str());
		}
	}
}

void CLI::doSetMaxConnectionInterval(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("interval milliseconds");
		return;
	}

	uint16_t newInterval;
	try {
		newInterval = std::stoul(cmd[1]);
	} catch (std::invalid_argument &e) {
		printUsageError("invalid interval");
		return;
	}

	boost::unique_lock<boost::shared_mutex> deviceslk(beetle.devicesMutex);
	for (auto &kv : beetle.devices) {
		auto peripheral = std::dynamic_pointer_cast<LEPeripheral>(kv.second);
		if (peripheral) {
			struct l2cap_conninfo connInfo = peripheral->getL2capConnInfo();
			beetle.hci.setConnectionInterval(connInfo.hci_handle, newInterval, newInterval, 0, 0x0C80, 0);
		}
	}
}

void CLI::doDumpData(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2) {
		printUsage("dump latency|config");
		return;
	}

	if (cmd[1] == "latency") {
		std::stringstream ss;
		std::string delim = "";

		boost::shared_lock<boost::shared_mutex> lk(beetle.devicesMutex);
		for (auto &kv : beetle.devices) {
			auto vd = std::dynamic_pointer_cast<VirtualDevice>(kv.second);
			if (vd) {
				auto latencies = vd->getTransactionLatencies();
				if (latencies.size() > 0) {
					std::cout << vd->getId() << "\t";
					for (uint64_t latency : latencies) {
						std::cout << delim << std::fixed << latency;
						delim = ",";
					}
					std::cout << std::endl;
				}
			}
		}
	} else if (cmd[1] == "config") {
		std::cout << beetleConfig.str() << std::endl;
	} else {
		printUsageError(cmd[1] + " not recognized");
	}
}

void CLI::doSetDebug(const std::vector<std::string>& cmd) {
	if (cmd.size() != 2 && cmd.size() != 3) {
		printUsage("debug on|off");
		printUsage("debug category on|off");
		return;
	}

	bool value;
	std::string setting = cmd[cmd.size() - 1];
	if (setting == "on") {
		value = true;
	} else if (setting == "off") {
		value = false;
	} else {
		printUsageError("unrecognized setting: " + setting);
		return;
	}

	if (cmd.size() == 2) {
		debug = value;
	} else {
		bool isAll = cmd[1] == "all";
		bool isValid = isAll;
		if (isAll) {
			debug = value;
		}
		if (isAll || cmd[1] == "scan") {
			debug_scan = value;
			isValid |= true;
		}
		if (isAll || cmd[1] == "router") {
			debug_router = value;
			isValid |= true;
		}
		if (isAll || cmd[1] == "socket") {
			debug_socket = value;
			isValid |= true;
		}
		if (isAll || cmd[1] == "discovery") {
			debug_discovery = value;
			isValid |= true;
		}
		if (isAll || cmd[1] == "controller") {
			debug_controller = value;
			isValid |= true;
		}
		if (isAll || cmd[1] == "performance") {
			debug_performance = value;
			isValid |= true;
		}
		if (!isValid) {
			printUsageError("unrecognized category: " + cmd[1]);
		}
	}
}

/*
 * Should be holding devices lock
 */
std::shared_ptr<Device> CLI::matchDevice(const std::string &input) {
	std::shared_ptr<Device> device = NULL;

	if (isBdAddr(input)) {
		// match by address
		bdaddr_t addr;
		if (str2ba(input.c_str(), &addr) == 0) {
			for (auto &kv : beetle.devices) {
				auto peripheral = std::dynamic_pointer_cast<LEPeripheral>(kv.second);
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

