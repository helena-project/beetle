/*
 * BeetleConfig.cpp
 *
 *  Created on: May 23, 2016
 *      Author: james
 */

#include "BeetleConfig.h"

#include <unistd.h>
#include <json/json.hpp>
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>

inline bool fileExists(const std::string& name) {
    return (access(name.c_str(), F_OK) != -1);
}

static std::string getDefaultName() {
	char name[100];
	assert(gethostname(name, sizeof(name)) == 0);
	return "btl@" + std::string(name);
}

BeetleConfig::BeetleConfig(std::string filename) {
	using json = nlohmann::json;

	json config;
	if (filename != "") {
		if (!fileExists(filename)) {
			throw std::invalid_argument("configuration file not found");
		}
		std::ifstream ifs(filename);
		ifs >> config;
	}

	if (config.count("name")) {
		name = config["name"];
	} else {
		name = getDefaultName();
	}

	if (config.count("scan")) {
		json scanConfig = config["scan"];
		for (json::iterator it = scanConfig.begin(); it != scanConfig.end(); ++it) {
			if (it.key() == "enable") {
				scanEnabled = it.value();
			} else {
				throw std::invalid_argument("unknown scan param: " + it.key());
			}
		}
	}

	if (config.count("tcp")) {
		json tcpConfig = config["tcp"];
		for (json::iterator it = tcpConfig.begin(); it != tcpConfig.end(); ++it) {
			if (it.key() ==  "enable") {
				tcpEnabled = it.value();
			} else if (it.key() == "port") {
				tcpPort = it.value();
			} else {
				throw std::invalid_argument("unknown tcp param: " + it.key());
			}
		}
	}

	if (config.count("ipc")) {
		json ipcConfig = config["ipc"];
		for (json::iterator it = ipcConfig.begin(); it != ipcConfig.end(); ++it) {
			if (it.key() ==  "enable") {
				ipcEnabled = it.value();
			} else if (it.key() == "path") {
				ipcPath = it.value();
			} else {
				throw std::invalid_argument("unknown ipc param: " + it.key());
			}
		}
	}

	if (config.count("controller")) {
		json controllerConfig = config["controller"];
		for (json::iterator it = controllerConfig.begin(); it != controllerConfig.end(); ++it) {
			if (it.key() ==  "enable") {
				controllerEnabled = it.value();
			} else if (it.key() == "host") {
				controllerHost = it.value();
			} else if (it.key() == "port") {
				controllerPort = it.value();
			} else {
				throw std::invalid_argument("unknown controller param: " + it.key());
			}
		}
	}

	if (config.count("ssl")) {
		json sslConfig = config["ssl"];
		for (json::iterator it = sslConfig.begin(); it != sslConfig.end(); ++it) {
			if (it.key() == "verifyPeers") {
				sslVerifyPeers = it.value();
			} else if (it.key() == "serverKey") {
				sslServerKey = it.value();
			} else if (it.key() == "serverCert") {
				sslServerCert = it.value();
			} else if (it.key() == "clientKey") {
				sslClientKey = it.value();
			} else if (it.key() == "clientCert") {
				sslClientCert = it.value();
			} else {
				throw std::invalid_argument("unknown ssl param: " + it.key());
			}
		}
	}

	if (config.count("debug")) {
		json debugConfig = config["debug"];
		for (json::iterator it = debugConfig.begin(); it != debugConfig.end(); ++it) {
			if (it.key() == "discovery") {
				debugDiscovery = it.value();
			} else if (it.key() == "scan") {
				debugScan = it.value();
			} else if (it.key() == "socket") {
				debugSocket = it.value();
			} else if (it.key() == "router") {
				debugRouter = it.value();
			} else if (it.key() == "performance") {
				debugPerformance = it.value();
			} else if (it.key() == "controller") {
				debugController = it.value();
			} else {
				throw std::invalid_argument("unknown debug param: " + it.key());
			}
		}
	}
}

BeetleConfig::~BeetleConfig() {
	// nothing to do
}

std::string BeetleConfig::getControllerHostAndPort() const {
	return controllerHost + ":" + std::to_string(controllerPort);
}

std::string BeetleConfig::str() const {
	using json = nlohmann::json;

	json config;
	config["name"] = name;

	json scan;
	scan["enable"] = scanEnabled;
	config["scan"] = scan;

	json tcp;
	tcp["enable"] = tcpEnabled;
	tcp["port"] = tcpPort;
	config["tcp"] = tcp;

	json ipc;
	ipc["enable"] = ipcEnabled;
	ipc["path"] = ipcPath;
	config["ipc"] = ipc;

	json controller;
	controller["enable"] = controllerEnabled;
	controller["host"] = controllerHost;
	controller["port"] = controllerPort;
	config["controller"] = controller;

	json ssl;
	ssl["verifyPeers"] = sslVerifyPeers;
	ssl["serverCert"] = sslServerCert;
	ssl["serverKey"] = sslServerKey;
	ssl["clientCert"] = sslClientCert;
	ssl["clientKey"] = sslClientKey;
	config["ssl"] = ssl;

	json debug;
	debug["controller"] = debugController;
	debug["discovery"] = debugDiscovery;
	debug["performance"] = debugPerformance;
	debug["router"] = debugRouter;
	debug["scan"] = debugScan;
	debug["socket"] = debugSocket;
	config["debug"] = debug;

	return config.dump(4);
}
