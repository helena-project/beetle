
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
#include "hat/BlockAllocator.h"
#include "hat/HandleAllocationTable.h"
#include "Router.h"
#include "Scanner.h"
#include "tcp/TCPDeviceServer.h"

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
	int tcpPort = 5000;
	bool scanningEnabled = true;
	bool debugAll = false;
	bool autoConnectAll = false;
	bool resetHci = true;
	std::string name = "";

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("name,n", po::value<std::string>(&name),
					"Name this Beetle gateway (default: beetle@hostname)")
			("scan,s", po::value<bool>(&scanningEnabled),
					"Enable scanning for BLE devices (default: true")
			("tcp-port,p", po::value<int>(&tcpPort),
					"Specify remote TCP server port (default: 5000)")
			("debug,d", po::value<bool>(&debug),
					"Enable general debugging (default: true)")
			("auto-connect-all", po::value<bool>(&autoConnectAll),
					"Connect to all nearby BLE devices (default: false")
			("reset-hci", po::value<bool>(&resetHci),
					"Set hci down/up at start-up (default: true")
			("debug-discovery", po::value<bool>(&debug_discovery),
					"Enable debugging for GATT discovery (default: false)")
			("debug-scan", po::value<bool>(&debug_scan),
					"Enable debugging for BLE scanning (default: false)")
			("debug-socket", po::value<bool>(&debug_socket),
					"Enable debugging for sockets (default: false)")
			("debug-router", po::value<bool>(&debug_router),
					"Enable debugging for router (default: false)")
			("debug-all", po::value<bool>(&debugAll),
					"Enable ALL debugging (default: false)");
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
	if (name == "") {
		name = getDefaultName();
	}

	try {
		Beetle btl(name);
		TCPDeviceServer tcpServer(btl, tcpPort);
		AutoConnect autoConnect(btl, autoConnectAll);
		CLI cli(btl, tcpPort);

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

Beetle::Beetle(std::string name_) {
	router = new Router(*this);
	beetleDevice = new BeetleMetaDevice(*this, name_);
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
	name = name_;
}

Beetle::~Beetle() {

}

void Beetle::addDevice(Device *d) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	devices[d->getId()] = d;
}

void Beetle::removeDevice(device_t id) {
	if (id == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to remove Beetle!");
		return;
	}
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
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

	/*
	 * Spin deallocator thread to avoid blocking
	 */
	std::thread t([d]() { delete d; });
	t.detach();
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

