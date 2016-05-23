/*
 * CLI.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef INCLUDE_CLI_H_
#define INCLUDE_CLI_H_

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Beetle.h"
#include "BeetleConfig.h"
#include "controller/NetworkDiscoveryClient.h"
#include "scan/Scanner.h"

class CLI {
public:
	CLI(Beetle &beetle, BeetleConfig beetleConfig, NetworkDiscoveryClient *discovery = NULL);
	virtual ~CLI();
	/*
	 * Block program exit with CLI.
	 */
	void join();

	/*
	 * Get handler to execute on device discovered.
	 */
	DiscoveryHandler getDiscoveryHander();
private:
	/*
	 * Reads a line from stdin and parses tokens into ret.
	 */
	bool getCommand(std::vector<std::string> &ret);

	Beetle &beetle;
	BeetleConfig beetleConfig;

	NetworkDiscoveryClient *networkDiscovery;

	std::thread t;
	void cmdLineDaemon();

	void doHelp(const std::vector<std::string>& cmd);
	void doScan(const std::vector<std::string>& cmd);
	void doConnect(const std::vector<std::string>& cmd, bool discoverHandles);
	void doRemote(const std::vector<std::string>& cmd);
	void doDiscover(const std::vector<std::string>& cmd);
	void doDisconnect(const std::vector<std::string>& cmd);
	void doMap(const std::vector<std::string>& cmd);
	void doUnmap(const std::vector<std::string>& cmd);
	void doListDevices(const std::vector<std::string>& cmd);
	void doListHandles(const std::vector<std::string>& cmd);
	void doListOffsets(const std::vector<std::string>& cmd);
	void doSetMaxConnectionInterval(const std::vector<std::string>& cmd);
	void doDumpData(const std::vector<std::string>& cmd);
	void doSetDebug(const std::vector<std::string>& cmd);

	/*
	 * Matches a device string, which may be an address, device id, or name (case-insensitive).
	 */
	Device *matchDevice(const std::string &device);

	std::mutex discoveredMutex;
	std::map<std::string, peripheral_info_t> discovered;
	std::map<std::string, std::string> aliases;
	int aliasCounter;
};

#endif /* INCLUDE_CLI_H_ */
