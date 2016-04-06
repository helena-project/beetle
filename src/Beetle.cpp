
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
#include "device/BeetleDevice.h"
#include "Debug.h"
#include "Device.h"
#include "hat/BlockAllocator.h"
#include "hat/HAT.h"
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
	pdebug("running: " + command);
	assert(system(command.c_str()) == 0);
	command = "hciconfig " + hciName + " up";
	pdebug("running: " + command);
	assert(system(command.c_str()) == 0);
}

int main(int argc, char *argv[]) {
	int tcpPort = 5000; // default port
	bool scanningEnabled = true;
	bool debugAll = false;
	bool autoConnectAll = false;
	bool resetHci = true;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("scan,s", po::value<bool>(&scanningEnabled), "Enable scanning for BLE devices (default: true")
			("tcp-port", po::value<int>(&tcpPort), "Specify TCP server port (default: 5000)")
			("auto-connect-all", po::value<bool>(&autoConnectAll), "Connect to all nearby BLE devices (default: false")
			("reset-hci", po::value<bool>(&resetHci), "Set hci down/up at start-up (default: true")
			("debug", po::value<bool>(&debug), "Enable general debugging (default: true)")
			("debug-discovery", po::value<bool>(&debug_discovery), "Enable debugging for GATT discovery (default: false)")
			("debug-scan", po::value<bool>(&debug_scan), "Enable debugging for BLE scanning (default: false)")
			("debug-socket", po::value<bool>(&debug_socket), "Enable debugging for sockets (default: false)")
			("debug-router", po::value<bool>(&debug_router), "Enable debugging for router (default: false)")
			("debug-all", po::value<bool>(&debugAll), "Enable ALL debugging (default: false)");
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
		Beetle btl;
		TCPDeviceServer tcpServer(btl, tcpPort);
		AutoConnect autoConnect(btl, autoConnectAll);
		CLI cli(btl);

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

Beetle::Beetle() {
	hat = new BlockAllocator(256);
	router = new Router(*this);
	beetleDevice = new BeetleDevice(*this, "Beetle");
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
}

Beetle::~Beetle() {

}

void Beetle::addDevice(Device *d, bool allocateHandles) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	boost::unique_lock<boost::shared_mutex> hatLk(hatMutex);
	devices[d->getId()] = d;
	if (allocateHandles) {
		handle_range_t range = hat->reserve(d->getId());
		assert(!HAT::isNullRange(range));
	}
}

void Beetle::removeDevice(device_t id) {
	if (id == BEETLE_RESERVED_DEVICE) {
		pdebug("not allowed to remove Beetle!");
		return;
	}
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
	Device *d = devices[id];
	devices.erase(id);
	for (auto &kv : devices) {
		kv.second->unsubscribeAll(id);
	}
	std::thread t([d]() { delete d; });
	t.detach();
}

