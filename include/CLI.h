/*
 * CLI.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef CLI_H_
#define CLI_H_

#include <cstdbool>
#include <string>
#include <thread>
#include <vector>

#include "../include/Beetle.h"
#include "../include/Scanner.h"

class CLI {
public:
	CLI(Beetle &beetle, int port, std::string path);
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

	int port;
	std::string path;

	std::thread t;
	void cmdLineDaemon();

	void doHelp(const std::vector<std::string>& cmd);
	void doScan(const std::vector<std::string>& cmd);
	void doConnect(const std::vector<std::string>& cmd, bool discoverHandles);
	void doRemote(const std::vector<std::string>& cmd);
	void doDisconnect(const std::vector<std::string>& cmd);
	void doMap(const std::vector<std::string>& cmd);
	void doUnmap(const std::vector<std::string>& cmd);
	void doListDevices(const std::vector<std::string>& cmd);
	void doListHandles(const std::vector<std::string>& cmd);
	void doListOffsets(std::vector<std::string>& cmd);

	/*
	 * Matches a device string, which may be an address, device id, or name (case-insensitive).
	 */
	Device *matchDevice(const std::string &device);

	std::mutex discoveredMutex;
	std::map<std::string, peripheral_info_t> discovered;
	std::map<std::string, std::string> aliases;
	int aliasCounter;
};

#endif /* CLI_H_ */
