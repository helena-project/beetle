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

class CLI {
public:
	CLI(Beetle &beetle);
	virtual ~CLI();
	void join();
private:
	bool getCommand(std::vector<std::string> &ret);

	Beetle beetle;
	std::thread t;
	void cmdLineDaemon();
};

#endif /* CLI_H_ */
