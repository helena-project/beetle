
#include "Beetle.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <boost/program_options.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <cassert>
#include <iostream>
#include <thread>
#include <utility>
#include <cstdint>

#include "AutoConnect.h"
#include "CLI.h"
#include "device/BeetleMetaDevice.h"
#include "Debug.h"
#include "Device.h"
#include "Router.h"
#include "Scanner.h"
#include "tcp/TCPDeviceServer.h"
#include "ipc/UnixDomainSocketServer.h"

/* Global debug variables */
bool debug = true;				// Debug.h
bool debug_scan = false;		// Scan.h
bool debug_discovery = false;	// VirtualDevice.h
bool debug_router = false;		// Router.h
bool debug_socket = false;		// Debug.h

void resetHciHelper() {
	assert(system(NULL) != 0);
	int hciDevice = hci_get_route(NULL);
	assert(hciDevice >= 0);
	std::string hciName = "hci" + std::to_string(hciDevice);
	std::string command = "hciconfig " + hciName + " down";
	pdebug("System: " + command);
	assert(system(command.c_str()) == 0);
	command = "hciconfig " + hciName + " up";
	pdebug("System: " + command);
	assert(system(command.c_str()) == 0);
}

std::string getDefaultName() {
	char name[100];
	assert(gethostname(name, sizeof(name)) == 0);
	return "beetle@" + std::string(name);
}

int main(int argc, char *argv[]) {
	int tcpPort;
	bool scanningEnabled;
	bool debugAll;
	bool autoConnectAll;
	bool resetHci;
	std::string name;
	std::string path;

	std::string beetleConrollerHostPort = "localhost:80";

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("name,n", po::value<std::string>(&name)->default_value(getDefaultName()),
					"Name this Beetle gateway")
			("scan,s", po::value<bool>(&scanningEnabled)->implicit_value(true),
					"Enable scanning for BLE devices")
			("tcp-port,p", po::value<int>(&tcpPort)->default_value(5000),
					"Specify remote TCP server port")
			("ipc,u", po::value<std::string>(&path)->default_value("/tmp/beetle"),
					"Unix domain socket path")
			("master,c", po::value<std::string>(&beetleConrollerHostPort)->default_value("localhost:80"),
					"Host and port of the Beetle control")
			("debug,d", po::value<bool>(&debug)->implicit_value(true),
					"Enable general debugging (default: true)")
			("auto-connect-all", po::value<bool>(&autoConnectAll)->default_value(false),
					"Connect to all nearby BLE devices")
			("reset-hci", po::value<bool>(&resetHci)->default_value(true),
					"Set hci down/up at start-up")
			("debug-discovery", po::value<bool>(&debug_discovery)->implicit_value(true),
					"Enable debugging for GATT discovery")
			("debug-scan", po::value<bool>(&debug_scan)->implicit_value(true),
					"Enable debugging for BLE scanning")
			("debug-socket", po::value<bool>(&debug_socket)->implicit_value(true),
					"Enable debugging for sockets")
			("debug-router", po::value<bool>(&debug_router)->implicit_value(true),
					"Enable debugging for router")
			("debug-all", po::value<bool>(&debugAll)->implicit_value(true),
					"Enable ALL debugging");
	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		if (vm.count("help")) {
			std::cout << "Beetle Linux" << std::endl << desc << std::endl;
			return 0;
		}
		po::notify(vm);
	} catch(po::error& e) {
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << desc << std::endl;
		return 1;
	}

	if (debugAll) {
		debug = true;
		debug_scan = true;
		debug_router = true;
		debug_socket = true;
		debug_discovery = true;
	}

	if (resetHci) resetHciHelper();

	try {
		Beetle btl(name);
		TCPDeviceServer tcpServer(btl, tcpPort);
		UnixDomainSocketServer ipcServer(btl, path);
		AutoConnect autoConnect(btl, autoConnectAll);

		CLI cli(btl, tcpPort, path);

		Scanner scanner;
		scanner.registerHandler(cli.getDiscoveryHander());
		scanner.registerHandler(autoConnect.getDiscoveryHandler());

		if (scanningEnabled) {
			scanner.start();
		}

		cli.join();
	} catch(std::exception& e) {
		std::cerr << "Unhandled Exception reached the top of main: "
				<< e.what() << ", application will now exit" << std::endl;
		return 2;
	}
	return 0;
}

Beetle::Beetle(std::string name_) : workers(1), writers(4) {
	router = new Router(*this);
	beetleDevice = new BeetleMetaDevice(*this, name_);
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
	name = name_;
}

Beetle::~Beetle() {

}

void Beetle::addDevice(Device *d) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	device_t id = d->getId();
	devices[id] = d;
	deviceslk.unlock();

	for (auto &h : addHandlers) {
		workers.schedule([&h,id]() -> void { h(id); });
	}
}

void Beetle::removeDevice(device_t id) {
	if (id == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to remove Beetle!");
		return;
	}
	boost::unique_lock<boost::shared_mutex> devicesLk(devicesMutex);
	if (devices.find(id) == devices.end()) {
		pwarn("removing non existant device!");
		return;
	}

	Device *d = devices[id];
	devices.erase(id);

	for (auto &kv : devices) {
		/*
		 * Cancel subscriptions and inform that services have changed
		 */
		assert(kv.first != id);
		kv.second->unsubscribeAll(id);
		if (!kv.second->hat->getDeviceRange(id).isNull()) {
			beetleDevice->informServicesChanged(kv.second->hat->free(id), kv.first);
		}
	}

	workers.schedule([d]() -> void { delete d; });

	for (auto &h : removeHandlers) {
		workers.schedule([&h,id]() -> void { h(id); });
	}
}

void Beetle::mapDevices(device_t from, device_t to) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to map Beetle");
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		pwarn("not allowed to map null device");
	} else if (from == to) {
		pwarn("cannot map device to itself");
	} else {
		boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex);
		if (devices.find(from) == devices.end()) {
			pwarn(std::to_string(from) + " does not id a device");
		} else if (devices.find(to) == devices.end()) {
			pwarn(std::to_string(to) +" does not id a device");
		} else {
			Device *toD = devices[to];

			std::lock_guard<std::mutex> hatLg(toD->hatMutex);
			if (!toD->hat->getDeviceRange(from).isNull()) {
				std::stringstream ss;
				ss << from << " is already mapped into " << to << "'s space";
				pwarn(ss.str());
			} else {
				handle_range_t range = toD->hat->reserve(from);
				if (debug) {
					pdebug("reserved " + range.str() + " at device " + std::to_string(to));
				}
			}
		}
	}
}

void Beetle::unmapDevices(device_t from, device_t to) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to unmap Beetle");
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		pwarn("unmapping null is a nop");
	} else if (from == to) {
		pwarn("cannot unmap self from self");
	} else {
		if (devices.find(from) == devices.end()) {
			pwarn(std::to_string(from) + " does not id a device");
		} else if (devices.find(to) == devices.end()) {
			pwarn(std::to_string(to) +" does not id a device");
		} else {
			Device *toD = devices[to];

			std::lock_guard<std::mutex> hatLg(toD->hatMutex);
			handle_range_t range = toD->hat->free(from);
			if (debug) {
				pdebug("freed " + range.str() + " at device " + std::to_string(to));
			}
		}
	}
}

void Beetle::registerAddDeviceHandler(AddDeviceHandler h) {
	addHandlers.push_back(h);
}

void Beetle::registerRemoveDeviceHandler(RemoveDeviceHandler h) {
	removeHandlers.push_back(h);
}


