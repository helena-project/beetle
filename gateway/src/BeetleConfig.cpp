/*
 * BeetleConfig.cpp
 *
 *  Created on: May 23, 2016
 *      Author: James Hong
 */

#include "BeetleConfig.h"

#include <unistd.h>
#include <json/json.hpp>
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>

#include "util/file.h"

static std::string getDefaultName() {
	char name[100];
	assert(gethostname(name, sizeof(name)) == 0);
	return "btl@" + std::string(name);
}

BeetleConfig::BeetleConfig(std::string filename) {
	using json = nlohmann::json;

	json config;
	if (filename != "") {
		if (!file_exists(filename)) {
			throw ConfigException("configuration file not found");
		}
		std::ifstream ifs(filename);
		ifs >> config;
	}

	if (config.count("name")) {
		name = config["name"];
	} else {
		name = getDefaultName();
	}

	if (config.count("cli")) {
		cliEnabled = config["cli"];
	}

	if (config.count("scan")) {
		json scanConfig = config["scan"];
		for (json::iterator it = scanConfig.begin(); it != scanConfig.end(); ++it) {
			if (it.key() == "enable") {
				scanEnabled = it.value();
			} else if (it.key() == "bdaddr") {
				scanBdaddr = it.value();
			} else {
				throw ConfigException("unknown scan param: " + it.key());
			}
		}
	}

	if (config.count("staticTopo")) {
		json scanConfig = config["staticTopo"];
		for (json::iterator it = scanConfig.begin(); it != scanConfig.end(); ++it) {
			if (it.key() == "enable") {
				staticTopoEnabled = it.value();
			} else if (it.key() == "mappings") {
				staticTopoMappings = it.value();
				if (staticTopoMappings != "" && !file_exists(staticTopoMappings)) {
					throw ConfigException("file does not exist: " + staticTopoMappings);
				}
			} else {
				throw ConfigException("unknown static topo param: " + it.key());
			}
		}
	}

	if (config.count("autoConnect")) {
		json autoConnectConfig = config["autoConnect"];
		for (json::iterator it = autoConnectConfig.begin(); it != autoConnectConfig.end(); ++it) {
			if (it.key() == "all") {
				autoConnectAll = it.value();
			} else if (it.key() == "minBackoff") {
				autoConnectMinBackoff = it.value();
			} else if (it.key() == "whitelist") {
				autoConnectWhitelist = it.value();
				if (autoConnectWhitelist != "" && !file_exists(autoConnectWhitelist)) {
					throw ConfigException("file does not exist: " + autoConnectWhitelist);
				}
			} else {
				throw ConfigException("unknown autoConnect param: " + it.key());
			}
		}
	}

	if (config.count("tcp")) {
		json tcpConfig = config["tcp"];
		for (json::iterator it = tcpConfig.begin(); it != tcpConfig.end(); ++it) {
			if (it.key() == "enable") {
				tcpEnabled = it.value();
			} else if (it.key() == "port") {
				tcpPort = it.value();
			} else {
				throw ConfigException("unknown tcp param: " + it.key());
			}
		}
	}

	if (config.count("ipc")) {
		json ipcConfig = config["ipc"];
		for (json::iterator it = ipcConfig.begin(); it != ipcConfig.end(); ++it) {
			if (it.key() == "enable") {
				ipcEnabled = it.value();
			} else if (it.key() == "path") {
				ipcPath = it.value();
			} else {
				throw ConfigException("unknown ipc param: " + it.key());
			}
		}
	}

	if (config.count("advertise")) {
		json advertiseConfig = config["advertise"];
		for (json::iterator it = advertiseConfig.begin(); it != advertiseConfig.end(); ++it) {
			if (it.key() == "enable") {
				advertiseEnabled = it.value();
			} else if (it.key() == "bdaddr") {
				advertiseBdaddr = it.value();
			} else {
				throw ConfigException("unknown advertisement param: " + it.key());
			}
		}
	}

	if (config.count("controller")) {
		json controllerConfig = config["controller"];
		for (json::iterator it = controllerConfig.begin(); it != controllerConfig.end(); ++it) {
			if (it.key() == "enable") {
				controllerEnabled = it.value();
			} else if (it.key() == "host") {
				controllerHost = it.value();
			} else if (it.key() == "apiPort") {
				controllerApiPort = it.value();
			} else if (it.key() == "controlEnable") {
				controllerControlEnabled = it.value();
			} else if (it.key() == "controlPort") {
				controllerControlPort = it.value();
			} else if (it.key() == "controlMaxReconnect") {
				controllerControlMaxReconnect = it.value();
			}else {
				throw ConfigException("unknown controller param: " + it.key());
			}
		}
	}

	if (config.count("ssl")) {
		json sslConfig = config["ssl"];
		for (json::iterator it = sslConfig.begin(); it != sslConfig.end(); ++it) {
			if (it.key() == "verifyPeers") {
				sslVerifyPeers = it.value();
			} else if (it.key() == "key") {
				sslKey = it.value();
				if (sslKey != "" && !file_exists(sslKey)) {
					throw ConfigException("file does not exist: " + sslKey);
				}
			} else if (it.key() == "cert") {
				sslCert = it.value();
				if (sslCert != "" && !file_exists(sslCert)) {
					throw ConfigException("file does not exist: " + sslCert);
				}
			} else if (it.key() == "caCert") {
				sslCaCert = it.value();
				if (sslCaCert != "" && !file_exists(sslCaCert)) {
					throw ConfigException("file does not exist: " + sslCaCert);
				}
			} else {
				throw ConfigException("unknown ssl param: " + it.key());
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
			} else if (it.key() == "topology") {
				debugTopology = it.value();
			} else if (it.key() == "socket") {
				debugSocket = it.value();
			} else if (it.key() == "router") {
				debugRouter = it.value();
			} else if (it.key() == "performance") {
				debugPerformance = it.value();
			} else if (it.key() == "controller") {
				debugController = it.value();
			} else {
				throw ConfigException("unknown debug param: " + it.key());
			}
		}
	}
}

BeetleConfig::~BeetleConfig() {
	// nothing to do
}

std::string BeetleConfig::str(unsigned int indent) const {
	using json = nlohmann::json;

	json config;
	config["name"] = name;
	config["cli"] = cliEnabled;

	{
		json scan;
		scan["enable"] = scanEnabled;
		scan["bdaddr"] = scanBdaddr;
		config["scan"] = scan;
	}

	{
		json autoConnect;
		autoConnect["all"] = autoConnectAll;
		autoConnect["minBackoff"] = autoConnectMinBackoff;
		autoConnect["whitelist"] = autoConnectWhitelist;
		config["autoConnect"] = autoConnect;
	}

	{
		json staticTopo;
		staticTopo["enable"] = staticTopoEnabled;
		staticTopo["mappings"] = staticTopoMappings;
		config["staticTopo"] = staticTopo;
	}

	{
		json tcp;
		tcp["enable"] = tcpEnabled;
		tcp["port"] = tcpPort;
		config["tcp"] = tcp;
	}

	{
		json ipc;
		ipc["enable"] = ipcEnabled;
		ipc["path"] = ipcPath;
		config["ipc"] = ipc;
	}

	{
		json advertise;
		advertise["enable"] = advertiseEnabled;
		advertise["bdaddr"] = advertiseBdaddr;
		config["advertise"] = advertise;
	}

	{
		json controller;
		controller["enable"] = controllerEnabled;
		controller["host"] = controllerHost;
		controller["apiPort"] = controllerApiPort;
		controller["controlEnable"] = controllerControlEnabled;
		controller["controlPort"] = controllerControlPort;
		controller["controlMaxReconnect"] = controllerControlMaxReconnect;
		config["controller"] = controller;
	}

	{
		json ssl;
		ssl["verifyPeers"] = sslVerifyPeers;
		ssl["cert"] = sslCert;
		ssl["key"] = sslKey;
		ssl["caCert"] = sslCaCert;
		config["ssl"] = ssl;
	}

	{
		json debug;
		debug["controller"] = debugController;
		debug["discovery"] = debugDiscovery;
		debug["performance"] = debugPerformance;
		debug["router"] = debugRouter;
		debug["scan"] = debugScan;
		debug["topology"] = debugTopology;
		debug["socket"] = debugSocket;
		config["debug"] = debug;
	}

	return config.dump(indent);
}
