/*
 * ControllerConnection.h
 *
 *  Created on: Jun 6, 2016
 *      Author: james
 */

#ifndef CONTROLLER_CONTROLLERCONNECTION_H_
#define CONTROLLER_CONTROLLERCONNECTION_H_

#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "BeetleTypes.h"

class ControllerClient;

class ControllerConnection {
public:
	ControllerConnection(Beetle &beetle, std::shared_ptr<ControllerClient> client, int maxReconnectionAttempts);
	virtual ~ControllerConnection();

	/*
	 * Blocks, waiting for CLI to exit.
	 */
	void join();

private:
	Beetle &beetle;

	std::shared_ptr<ControllerClient> client;

	int maxReconnectionAttempts;

	std::unique_ptr<boost::asio::ip::tcp::iostream> stream;

	bool getCommand(std::vector<std::string> &ret);
	bool reconnect();

	void doMapLocal(const std::vector<std::string>& cmd);
	void doUnmapLocal(const std::vector<std::string>& cmd);
	void doMapRemote(const std::vector<std::string>& cmd);
	void doUnmapRemote(const std::vector<std::string>& cmd);

	void socketDaemon();
	std::thread daemonThread;
	bool daemonRunning;
};

#endif /* CONTROLLER_CONTROLLERCONNECTION_H_ */
