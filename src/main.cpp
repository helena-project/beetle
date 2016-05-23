/*
 * main.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <boost/program_options.hpp>
#include <unistd.h>
#include <iostream>
#include <map>
#include <string>
#include <signal.h>
#include <cassert>
#include <memory>

#include "Beetle.h"
#include "BeetleConfig.h"
#include "CLI.h"
#include "controller/AccessControl.h"
#include "controller/ControllerClient.h"
#include "controller/NetworkDiscoveryClient.h"
#include "controller/NetworkStateClient.h"
#include "Debug.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "HCI.h"
#include "ipc/UnixDomainSocketServer.h"
#include "scan/AutoConnect.h"
#include "scan/Scanner.h"
#include "tcp/SSLConfig.h"
#include "tcp/TCPDeviceServer.h"


/* Global debug variables */
bool debug;				// Debug.h
bool debug_scan;		// Scan.h
bool debug_discovery;	// VirtualDevice.h
bool debug_router;		// Router.h
bool debug_socket;		// Debug.h
bool debug_controller;	// Debug.h
bool debug_performance; // Debug.h

void setDebugAll() {
	debug = true;
	debug_scan = true;
	debug_router = true;
	debug_socket = true;
	debug_discovery = true;
	debug_controller = true;
	debug_performance = true;
}

void setDebug(BeetleConfig btlConfig) {
	debug_scan = btlConfig.debugScan;
	debug_router = btlConfig.debugRouter;
	debug_socket = btlConfig.debugSocket;
	debug_discovery = btlConfig.debugDiscovery;
	debug_controller = btlConfig.debugController;
	debug_performance = btlConfig.debugPerformance;
}

void sigpipe_handler_ignore(int unused) {
	if (debug_socket) {
		pdebug("caught signal SIGPIPE");
	}
}

int main(int argc, char *argv[]) {
	std::string configFile;
	bool autoConnectAll;
	bool resetHci;
	bool debugAll;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("defaults,d", "Print default configuration")
			("config-file,c", po::value<std::string>(&configFile)
					->default_value(""),
					"Beetle configuration file")
			("connect-all,a", "Connect to all nearby BLE peripherals")
			("reset-hci", po::value<bool>(&resetHci)
					->default_value(true),
					"Set hci down/up at start-up")
			("debug", "")
			("debug-all", "");

	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		if (vm.count("help")) {
			std::cout << "Beetle Linux" << std::endl << desc << std::endl;
			return 0;
		}
		if (vm.count("defaults")) {
			std::cout << BeetleConfig().str() << std::endl;
			return 0;
		}
		if (vm.count("debug")) {
			debug = true;
		}
		if (vm.count("debug-all")) {
			debugAll = true;
		}
		if (vm.count("connect-all")) {
			autoConnectAll = true;
		}
		po::notify(vm);
	} catch (po::error& e) {
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << desc << std::endl;
		return 1;
	}

	BeetleConfig btlConfig(configFile);

	if (debugAll) {
		setDebugAll();
	} else {
		setDebug(btlConfig);
	}

	if (resetHci) {
		HCI::resetHCI();
	}

	SSLConfig clientSSLConfig(btlConfig.sslVerifyPeers);
	TCPServerProxy::initSSL(&clientSSLConfig);
	signal(SIGPIPE, sigpipe_handler_ignore);

	try {
		Beetle btl(btlConfig.name);

		std::unique_ptr<TCPDeviceServer> tcpServer;
		if (btlConfig.tcpEnabled) {
			std::cout << "using certificate: " << btlConfig.sslServerCert << std::endl;
			std::cout << "using key: " << btlConfig.sslServerKey << std::endl;
			tcpServer.reset(new TCPDeviceServer(btl,
					SSLConfig(btlConfig.sslVerifyPeers, true, btlConfig.sslServerCert, btlConfig.sslServerKey),
					btlConfig.tcpPort));
		}

		std::unique_ptr<UnixDomainSocketServer> ipcServer;
		if (btlConfig.ipcEnabled) {
			ipcServer.reset(new UnixDomainSocketServer(btl, btlConfig.ipcPath));
		}

		AutoConnect autoConnect(btl, autoConnectAll);

		std::unique_ptr<ControllerClient> controllerClient;
		std::unique_ptr<NetworkStateClient> networkState;
		std::unique_ptr<AccessControl> accessControl;
		std::unique_ptr<NetworkDiscoveryClient> networkDiscovery;
		if (btlConfig.controllerEnabled) {
			controllerClient.reset(new ControllerClient(btl, btlConfig.getControllerHostAndPort(),
					btlConfig.sslVerifyPeers));

			networkState.reset(new NetworkStateClient(btl, *controllerClient, btlConfig.tcpPort));
			btl.registerAddDeviceHandler(networkState->getAddDeviceHandler());
			btl.registerRemoveDeviceHandler(networkState->getRemoveDeviceHandler());
			btl.registerUpdateDeviceHandler(networkState->getUpdateDeviceHandler());

			networkDiscovery.reset(new NetworkDiscoveryClient(btl, *controllerClient));
			btl.setDiscoveryClient(networkDiscovery.get());

			accessControl.reset(new AccessControl(btl, *controllerClient));
			btl.setAccessControl(accessControl.get());
		}

		CLI cli(btl, btlConfig, networkDiscovery.get());

		std::unique_ptr<Scanner> scanner;
		if (btlConfig.scanEnabled) {
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
