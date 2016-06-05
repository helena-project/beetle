/*
 * ControllerCLI.h
 *
 *  Created on: Jun 5, 2016
 *      Author: james
 */

#ifndef CONTROLLER_CONTROLLERCLI_H_
#define CONTROLLER_CONTROLLERCLI_H_

#include <memory>
#include <string>

#include "BeetleTypes.h"

class BeetleConfig;
class CLI;
class NetworkDiscoveryClient;

/*
 * Export CLI over a TCP socket
 */
class ControllerCLI {
public:
	ControllerCLI(Beetle &beetle, std::string host, int port, BeetleConfig beetleConfig,
			std::shared_ptr<NetworkDiscoveryClient> discovery = NULL, bool useDaemon = true);
	virtual ~ControllerCLI();

	/*
	 * Blocks, waiting for CLI to exit.
	 */
	void join();
private:
	std::unique_ptr<CLI> cli;
};

#endif /* CONTROLLER_CONTROLLERCLI_H_ */
