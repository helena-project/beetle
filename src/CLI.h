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

#include "Beetle.h"
#include "Scanner.h"

class CLI {
public:
	CLI(Beetle &beetle, Scanner &scanner);
	virtual ~CLI();
	void join();
private:
	/*
	 * Reads a line from stdin and parses tokens into ret.
	 */
	bool getCommand(std::vector<std::string> &ret);

	Beetle &beetle;
	Scanner &scanner;

	std::thread t;
	void cmdLineDaemon();

	void doScan(const std::vector<std::string>& cmd);
	void doConnect(const std::vector<std::string>& cmd);
	void doDisconnect(const std::vector<std::string>& cmd);
	void doListDevices(const std::vector<std::string>& cmd);
	void doListHandles(const std::vector<std::string>& cmd);
	void doListOffsets(std::vector<std::string>& cmd);
	void doToggleDebug(const std::vector<std::string>& cmd);

	Device *matchDevice(const std::string &device);
};

#endif /* CLI_H_ */
