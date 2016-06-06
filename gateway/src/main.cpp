/*
 * main.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: James Hong
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
#include "BeetleTypes.h"
#include "CLI.h"
#include "controller/AccessControl.h"
#include "controller/ControllerClient.h"
#include "controller/NetworkDiscoveryClient.h"
#include "controller/NetworkStateClient.h"
#include "controller/ControllerCLI.h"
#include "Debug.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "HCI.h"
#include "ipc/UnixDomainSocketServer.h"
#include "StaticTopo.h"
#include "scan/AutoConnect.h"
#include "scan/Scanner.h"
#include "sync/TimedDaemon.h"
#include "tcp/SSLConfig.h"
#include "tcp/TCPDeviceServer.h"


/* Global debug variables */
bool debug;
bool debug_scan;
bool debug_discovery;
bool debug_router;
bool debug_socket;
bool debug_controller;
bool debug_performance;

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
	std::string configFile = "";
	bool autoConnectAll = false;
	bool resetHci = false;
	bool debugAll = false;
	bool enableController = false;
	bool enableTcp = false;
	bool enableIpc = false;
	bool enableStaticTopo = false;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("print-defaults,p", "Print default configuration")
			("config,c", po::value<std::string>(&configFile)
					->default_value(""),
					"Beetle configuration file")
			("tcp,t", "Enable tcp sockets, overriding config if necessary.")
			("ipc,i", "Enable ipc sockets, overriding config if necessary.")
			("controller,C", "Enable controller, overriding config if necessary.")
			("static-topo,s", "Enable static mappings, overriding config if necessary.")
			("connect-all,a", "Connect to all nearby BLE peripherals.")
			("reset-hci", po::value<bool>(&resetHci)
					->default_value(true),
					"Set hci down/up at start-up.")
			("debug,d", "")
			("debug-all,D", "");

	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		if (vm.count("help")) {
			std::cout << "Beetle Gateway Linux" << std::endl << desc << std::endl;
			return 0;
		}
		if (vm.count("print-defaults")) {
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
		if (vm.count("tcp")) {
			enableTcp = true;
		}
		if (vm.count("ipc")) {
			enableIpc = true;
		}
		if (vm.count("controller")) {
			enableController = true;
		}
		if (vm.count("static-topo")) {
			enableStaticTopo = true;
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

		/* Listen for remote connections */
		std::unique_ptr<TCPDeviceServer> tcpServer;
		if (btlConfig.tcpEnabled || enableTcp) {
			std::cout << "using certificate: " << btlConfig.sslServerCert << std::endl;
			std::cout << "using key: " << btlConfig.sslServerKey << std::endl;
			tcpServer = std::make_unique<TCPDeviceServer>(btl,
					new SSLConfig(btlConfig.sslVerifyPeers, true, btlConfig.sslServerCert, btlConfig.sslServerKey),
					btlConfig.tcpPort);
		}

		/* Listen for local applications */
		std::unique_ptr<UnixDomainSocketServer> ipcServer;
		if (btlConfig.ipcEnabled || enableIpc) {
			ipcServer.reset(new UnixDomainSocketServer(btl, btlConfig.ipcPath));
		}

		/* Setup controller modules */
		std::shared_ptr<ControllerClient> controllerClient;
		std::shared_ptr<NetworkStateClient> networkState;
		std::shared_ptr<AccessControl> accessControl;
		std::shared_ptr<NetworkDiscoveryClient> networkDiscovery;
		std::shared_ptr<ControllerCLI> controllerCli;
		if (btlConfig.controllerEnabled || enableController) {
			controllerClient = std::make_shared<ControllerClient>(btl, btlConfig.controllerHost,
					btlConfig.controllerPort, btlConfig.sslVerifyPeers);

			/*
			 * Informs controller of gateway events. Also, adds session token to controller client.
			 */
			networkState = std::make_shared<NetworkStateClient>(btl, controllerClient, btlConfig.tcpPort);
			btl.registerAddDeviceHandler(networkState->getAddDeviceHandler());
			btl.registerRemoveDeviceHandler(networkState->getRemoveDeviceHandler());
			btl.registerUpdateDeviceHandler(networkState->getUpdateDeviceHandler());
			btl.registerMapDevicesHandler(networkState->getMapDevicesHandler());
			btl.registerUnmapDevicesHandler(networkState->getUnmapDevicesHandler());

			networkDiscovery = std::make_shared<NetworkDiscoveryClient>(btl, controllerClient);
			btl.setDiscoveryClient(networkDiscovery);

			accessControl = std::make_shared<AccessControl>(btl, controllerClient);
			btl.setAccessControl(accessControl);

			if (btlConfig.controllerCommandEnabled) {
				controllerCli = std::make_shared<ControllerCLI>(btl, btlConfig.controllerHost,
						btlConfig.controllerCommandPort, controllerClient->getSessionToken(),
						btlConfig, networkDiscovery, true);
			}
		}

		/* Setup static topology listener */
		std::unique_ptr<StaticTopo> staticTopo;
		if (btlConfig.staticTopoEnabled || enableStaticTopo) {
			staticTopo = std::make_unique<StaticTopo>(btl, btlConfig.staticTopoMappings);
			btl.registerUpdateDeviceHandler(staticTopo->getUpdateDeviceHandler());
		}

		/* Make a CLI if remote control is disabled */
		std::unique_ptr<CLI> cli;
		if (!controllerCli) {
			cli = std::make_unique<CLI>(btl, btlConfig, networkDiscovery);
		}

		/* Setup scanning and autoconnect modules */
		std::unique_ptr<AutoConnect> autoConnect;
		std::unique_ptr<Scanner> scanner;
		if (btlConfig.scanEnabled) {
			scanner = std::make_unique<Scanner>();
			autoConnect = std::make_unique<AutoConnect>(btl, autoConnectAll || btlConfig.autoConnectAll,
					btlConfig.autoConnectMinBackoff, btlConfig.autoConnectWhitelist);
			scanner->registerHandler(autoConnect->getDiscoveryHandler());
			if (cli) {
				scanner->registerHandler(cli->getDiscoveryHander());
			}
			if (controllerCli) {
				scanner->registerHandler(controllerCli->get()->getDiscoveryHander());
			}
			scanner->start();
		}

		/* Initialize all timers */
		TimedDaemon timers;
		timers.repeat(btl.getDaemon(), 5);
		if (cli) {
			timers.repeat(cli->getDaemon(), 5);
		}
		if (controllerCli) {
			timers.repeat(controllerCli->get()->getDaemon(), 5);
		}
		if (autoConnect) {
			timers.repeat(autoConnect->getDaemon(), 5);
		}

		/* Block on exit */
		if (cli) {
			cli->join();
		}
		if (controllerCli) {
			controllerCli->join();
		}

	} catch (std::exception& e) {
		std::cerr << "Unhandled exception reached the top of main: " << std::endl
				<< "  " << e.what() << std::endl;
		return 2;
	}
	return 0;
}
