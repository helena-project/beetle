/*
 * BeetleConfig.h
 *
 *  Created on: May 23, 2016
 *      Author: James Hong
 */

#ifndef BEETLECONFIG_H_
#define BEETLECONFIG_H_

#include <string>

class ConfigException : public std::exception {
public:
	ConfigException(std::string msg) : msg(msg) {};
	ConfigException(const char *msg) : msg(msg) {};
	~ConfigException() throw() {};
	const char *what() const throw() { return this->msg.c_str(); };
private:
	std::string msg;
};


class BeetleConfig {
public:
	/*
	 * Load configuration from json file.
	 */
	BeetleConfig(std::string filename = "");
	virtual ~BeetleConfig();

	/*
	 * Dump configuration as a json object.
	 */
	std::string str(unsigned int indent = 4) const;

	/*
	 * Gateway name
	 */
	std::string name;

	/*
	 * Run command line interface.
	 */
	bool cliEnabled = true;

	/*
	 * Scan settings
	 */
	bool scanEnabled = true;
	std::string scanDev = "";

	/*
	 * Autoconnect settings
	 */
	bool autoConnectAll = false;
	double autoConnectMinBackoff = 60.0;
	std::string autoConnectWhitelist = "../examples/whitelist.txt";	// whitelist file

	/*
	 * TCP settings
	 */
	bool tcpEnabled = false;
	int tcpPort = 3002;

	/*
	 * Unix domain socket settings
	 */
	bool ipcEnabled = false;
	std::string ipcPath = "/tmp/beetle";

	/*
	 * Peripheral settings
	 */
	bool advertiseEnabled = false;
	std::string advertiseDev = "";

	/*
	 * Controller settings
	 */
	bool controllerEnabled = false;
	std::string controllerHost = "localhost";
	int controllerApiPort = 3003;
	bool controllerControlEnabled = true;
	int controllerControlPort = 3004;
	int controllerControlMaxReconnect = 5;

	/*
	 * Static topology
	 */
	bool staticTopoEnabled = false;
	std::string staticTopoMappings = "../examples/mappings.txt";	// static mappings file

	/*
	 * SSL settings
	 */
	std::string sslKey = "../certs/key.pem";
	std::string sslCert = "../certs/cert.pem";
	std::string sslCaCert = "../certs/cert.pem";
	bool sslVerifyPeers = true;

	/*
	 * Debug settings
	 */
	bool debugController = false;
	bool debugDiscovery = false;
	bool debugPerformance = false;
	bool debugRouter = false;
	bool debugScan = false;
	bool debugTopology = false;
	bool debugSocket = false;
};

#endif /* BEETLECONFIG_H_ */
