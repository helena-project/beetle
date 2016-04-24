/*
 * main.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <boost/program_options.hpp>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <signal.h>

#include <AutoConnect.h>
#include <Beetle.h>
#include <controller/AccessControl.h>
#include <controller/NetworkState.h>
#include <controller/NetworkDiscovery.h>
#include <controller/ControllerClient.h>
#include <CLI.h>
#include <Debug.h>
#include <ipc/UnixDomainSocketServer.h>
#include <Scanner.h>
#include <tcp/TCPDeviceServer.h>

/* Global debug variables */
bool debug;				// Debug.h
bool debug_scan;		// Scan.h
bool debug_discovery;	// VirtualDevice.h
bool debug_router;		// Router.h
bool debug_socket;		// Debug.h
bool debug_network;		// Controller.h

void setDebugAll() {
	debug = true;
	debug_scan = true;
	debug_router = true;
	debug_socket = true;
	debug_discovery = true;
	debug_network = true;
}

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

void sigpipe_handler_ignore(int unused) {
	if (debug_socket) {
		pdebug("caught signal SIGPIPE");
	}
}

int main(int argc, char *argv[]) {
	int tcpPort;
	bool scanningEnabled;
	bool debugAll;
	bool autoConnectAll;
	bool resetHci;
	bool runController;
	bool serveTcp;
	bool serveIpc;
	std::string name;
	std::string path;
	std::string beetleControllerHostPort;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("name,n", po::value<std::string>(&name)->default_value(getDefaultName()),
					"Name this Beetle gateway")
			("scan,s", po::value<bool>(&scanningEnabled)
					->implicit_value(true),
					"Enable scanning for BLE devices")
			("tcp-port,p", po::value<int>(&tcpPort)
					->default_value(5000),
					"Specify remote TCP server port")
			("ipc,u", po::value<std::string>(&path)
					->default_value("/tmp/beetle"),
					"Unix domain socket path")
			("controller,c", po::value<std::string>(&beetleControllerHostPort)
					->default_value("localhost:80"),
					"Host and port of the Beetle control")
			("debug,d", po::value<bool>(&debug)
					->implicit_value(true),
					"Enable general debugging")
			("auto-connect-all", po::value<bool>(&autoConnectAll)
					->implicit_value(true)
					->default_value(false),
					"Connect to all nearby BLE devices")
			("no-reset-hci", po::value<bool>(&resetHci)
					->default_value(true)
					->implicit_value(false),
					"Set hci down/up at start-up")
			("no-controller", po::value<bool>(&runController)
					->default_value(true)
					->implicit_value(false),
					"Disable network state, discovery, and access control")
			("no-tcp", po::value<bool>(&serveTcp)
					->default_value(true)
					->implicit_value(false),
					"Disable remote access over tcp")
			("no-ipc", po::value<bool>(&serveIpc)
					->default_value(true)
					->implicit_value(false),
					"Disable access over unix domain sockets")
			("debug-discovery", po::value<bool>(&debug_discovery)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for GATT discovery")
			("debug-scan", po::value<bool>(&debug_scan)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for BLE scanning")
			("debug-socket", po::value<bool>(&debug_socket)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for sockets")
			("debug-router", po::value<bool>(&debug_router)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for router")
			("debug-network", po::value<bool>(&debug_network)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for control plane")
			("debug-all", po::value<bool>(&debugAll)
					->implicit_value(true)
					->default_value(false),
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

	if (debugAll) setDebugAll();
	if (resetHci) resetHciHelper();

	try {
		signal(SIGPIPE, sigpipe_handler_ignore);

		Beetle btl(name);

		std::unique_ptr<TCPDeviceServer> tcpServer;
		if (serveTcp) {
			tcpServer.reset(new TCPDeviceServer(btl, tcpPort));
		}

		std::unique_ptr<UnixDomainSocketServer> ipcServer;
		if (serveIpc) {
			ipcServer.reset(new UnixDomainSocketServer(btl, path));
		}

		AutoConnect autoConnect(btl, autoConnectAll);

		std::unique_ptr<ControllerClient> controllerClient;
		std::unique_ptr<NetworkState> networkState;
		std::unique_ptr<AccessControl> accessControl;
		std::unique_ptr<NetworkDiscovery> networkDiscovery;
		if (runController) {
			controllerClient.reset(new ControllerClient(beetleControllerHostPort));

			networkState.reset(new NetworkState(btl, *controllerClient));
			btl.registerAddDeviceHandler(networkState->getAddDeviceHandler());
			btl.registerRemoveDeviceHandler(networkState->getRemoveDeviceHandler());

			networkDiscovery.reset(new NetworkDiscovery(*controllerClient));

			accessControl.reset(new AccessControl(btl, *controllerClient));
			btl.setAccessControl(accessControl.get());
		}

		CLI cli(btl, tcpPort, path, networkDiscovery.get());

		std::unique_ptr<Scanner> scanner;
		if (scanningEnabled) {
			scanner.reset(new Scanner());
			scanner->registerHandler(cli.getDiscoveryHander());
			scanner->registerHandler(autoConnect.getDiscoveryHandler());
			scanner->start();
		}

		cli.join();
	} catch (std::exception& e) {
		std::cerr << "Unhandled exception reached the top of main: " << std::endl
				<< "  " << e.what() << std::endl;
		return 2;
	}
	return 0;
}
