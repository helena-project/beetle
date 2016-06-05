/*
 * CLI.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_CLI_H_
#define INCLUDE_CLI_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "BeetleTypes.h"
#include "BeetleConfig.h"
#include "scan/Scanner.h"

/* Forward declarations */
class NetworkDiscoveryClient;

class CLI {
public:
	CLI(Beetle &beetle, BeetleConfig beetleConfig, std::shared_ptr<NetworkDiscoveryClient> discovery = NULL,
			std::iostream *iostream = NULL, bool useDaemon = false);

	virtual ~CLI();
	/*
	 * Block program exit with CLI.
	 */
	void join();

	/*
	 * Get handler to execute on device discovered.
	 */
	DiscoveryHandler getDiscoveryHander();

	/*
	 * Get the timeout daemon.
	 */
	std::function<void()> getDaemon();
private:
	/*
	 * Reads a line from stdin and parses tokens into ret.
	 */
	bool getCommand(std::vector<std::string> &ret);

	Beetle &beetle;
	BeetleConfig beetleConfig;

	/*
	 * Use a stream other than stdin and stdout.
	 */
	std::unique_ptr<std::iostream> iostream;

	std::shared_ptr<NetworkDiscoveryClient> networkDiscovery;

	bool useDaemon;
	std::thread inputDaemon;
	void cmdLineDaemon();

	void printUsage(std::string usage);
	void printUsageError(std::string error);
	void printMessage(std::string message);

	void doHelp(const std::vector<std::string>& cmd);
	void doScan(const std::vector<std::string>& cmd);
	void doConnect(const std::vector<std::string>& cmd, bool discoverHandles);
	void doRemote(const std::vector<std::string>& cmd);
	void doNetworkDiscover(const std::vector<std::string>& cmd);
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
	std::shared_ptr<Device> matchDevice(const std::string &device);

	typedef struct {
		std::string alias;
		peripheral_info_t info;
		time_t lastSeen;
	} discovered_t;

	std::map<std::string, discovered_t> discovered;
	std::mutex discoveredMutex;
	int aliasCounter;
};

#endif /* INCLUDE_CLI_H_ */
