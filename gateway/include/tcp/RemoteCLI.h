/*
 * ControllerCLI.h
 *
 *  Created on: Jun 5, 2016
 *      Author: james
 */

#ifndef CONTROLLER_REMOTECLI_H_
#define CONTROLLER_REMOTECLI_H_

#include <memory>
#include <string>

#include "BeetleTypes.h"

class BeetleConfig;
class CLI;
class ControllerClient;
class NetworkDiscoveryClient;

/*
 * Export CLI over a TCP socket
 */
class RemoteCLI {
public:
	RemoteCLI(Beetle &beetle, std::string host, int port, BeetleConfig beetleConfig,
			std::shared_ptr<NetworkDiscoveryClient> discovery = NULL, bool useDaemon = true);
	virtual ~RemoteCLI();

	/*
	 * Blocks, waiting for CLI to exit.
	 */
	void join();

	/*
	 * Return the internal CLI owned by this.
	 */
	CLI *get();
private:
	std::unique_ptr<CLI> cli;
};

#endif /* CONTROLLER_REMOTECLI_H_ */
