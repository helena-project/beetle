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

#include "AutoConnect.h"
#include "Beetle.h"
#include "controller/AccessControl.h"
#include "controller/NetworkStateClient.h"
#include "controller/NetworkDiscoveryClient.h"
#include "controller/ControllerClient.h"
#include "CLI.h"
#include "Debug.h"
#include "ipc/UnixDomainSocketServer.h"
#include "Scanner.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "tcp/TCPDeviceServer.h"
#include "tcp/SSLConfig.h"
#include "HCI.h"


/* Global debug variables */
bool debug;				// Debug.h
bool debug_scan;		// Scan.h
bool debug_discovery;	// VirtualDevice.h
bool debug_router;		// Router.h
bool debug_socket;		// Debug.h
bool debug_controller;	// Debug.h

void setDebugAll() {
	debug = true;
	debug_scan = true;
	debug_router = true;
	debug_socket = true;
	debug_discovery = true;
	debug_controller = true;
}

std::string getDefaultName() {
	char name[100];
	assert(gethostname(name, sizeof(name)) == 0);
	return "btl@" + std::string(name);
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
	bool noResetHci;
	bool noController;
	bool noTcp;
	bool noIpc;
	std::string name;
	std::string path;
	std::string beetleControllerHostPort;
	std::string certificate;
	std::string key;
	bool noVerifyPeers;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
			("help,h", "")
			("name,n", po::value<std::string>(&name)->default_value(getDefaultName()),
					"Name this Beetle gateway")
			("scan,s", po::value<bool>(&scanningEnabled)
					->implicit_value(true)
					->default_value(false),
					"Enable scanning for BLE devices")
			("tcp-port,p", po::value<int>(&tcpPort)
					->default_value(3002),
					"Specify remote TCP server port")
			("ipc,u", po::value<std::string>(&path)
					->default_value("/tmp/beetle"),
					"Unix domain socket path")
			("controller,c", po::value<std::string>(&beetleControllerHostPort)
					->default_value("localhost:443"),
					"Host and port of the Beetle control")
			("debug,d", po::value<bool>(&debug)
					->implicit_value(true),
					"Enable general debugging")
			("auto-connect-all", po::value<bool>(&autoConnectAll)
					->implicit_value(true)
					->default_value(false),
					"Connect to all nearby BLE devices")
			("cert", po::value<std::string>(&certificate)
					->default_value("../certs/cert.pem"),
					"Gateway certificate file")
			("key", po::value<std::string>(&key)
					->default_value("../certs/key.pem"),
					"Gateway private key file")
			("no-verify", po::value<bool>(&noVerifyPeers)
					->default_value(true)	// TODO fix this
					->implicit_value(true),
					"Disable certificate verification")
			("no-reset-hci", po::value<bool>(&noResetHci)
					->default_value(false)
					->implicit_value(true),
					"Set hci down/up at start-up")
			("no-controller", po::value<bool>(&noController)
					->default_value(false)
					->implicit_value(true),
					"Disable network state, discovery, and access control")
			("no-tcp", po::value<bool>(&noTcp)
					->default_value(false)
					->implicit_value(true),
					"Disable remote access over tcp")
			("no-ipc", po::value<bool>(&noIpc)
					->default_value(false)
					->implicit_value(true),
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
			("debug-controller", po::value<bool>(&debug_controller)
					->implicit_value(true)
					->default_value(false),
					"Enable debugging for the control plane")
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
	} catch (po::error& e) {
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << desc << std::endl;
		return 1;
	}

	if (debugAll) {
		setDebugAll();
	}

	if (!noResetHci) {
		HCI::resetHCI();
	}

	SSLConfig clientSSLConfig(!noVerifyPeers);
	TCPServerProxy::initSSL(&clientSSLConfig);

	try {
		signal(SIGPIPE, sigpipe_handler_ignore);

		Beetle btl(name);

		std::unique_ptr<SSLConfig> serverSSLConfig;
		std::unique_ptr<TCPDeviceServer> tcpServer;
		if (!noTcp) {
			std::cout << "using certificate: " << certificate << std::endl;
			std::cout << "using key: " << key << std::endl;
			serverSSLConfig.reset(new SSLConfig(!noVerifyPeers, true, certificate, key));
			tcpServer.reset(new TCPDeviceServer(btl, *serverSSLConfig, tcpPort));
		}

		std::unique_ptr<UnixDomainSocketServer> ipcServer;
		if (!noIpc) {
			ipcServer.reset(new UnixDomainSocketServer(btl, path));
		}

		AutoConnect autoConnect(btl, autoConnectAll);

		std::unique_ptr<ControllerClient> controllerClient;
		std::unique_ptr<NetworkStateClient> networkState;
		std::unique_ptr<AccessControl> accessControl;
		std::unique_ptr<NetworkDiscoveryClient> networkDiscovery;
		if (!noController) {
			controllerClient.reset(new ControllerClient(btl, beetleControllerHostPort, !noVerifyPeers));

			networkState.reset(new NetworkStateClient(btl, *controllerClient, tcpPort));
			btl.registerAddDeviceHandler(networkState->getAddDeviceHandler());
			btl.registerRemoveDeviceHandler(networkState->getRemoveDeviceHandler());
			btl.registerUpdateDeviceHandler(networkState->getUpdateDeviceHandler());

			networkDiscovery.reset(new NetworkDiscoveryClient(btl, *controllerClient));
			btl.setDiscoveryClient(networkDiscovery.get());

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
