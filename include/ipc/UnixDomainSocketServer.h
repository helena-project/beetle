/*
 * UnixDomainSocketServer.h
 *
 *  Created on: Apr 15, 2016
 *      Author: james
 */

#ifndef INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_
#define INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_

#include <string>
#include <thread>

#include "Beetle.h"

class UnixDomainSocketServer {
public:
	UnixDomainSocketServer(Beetle &beetle, std::string path);
	virtual ~UnixDomainSocketServer();

private:
	Beetle &beetle;

	bool running;

	std::string path;

	std::thread t;
	void serverDaemon();

	void startIPCDeviceHelper(int clifd, struct sockaddr_un cliaddr, struct ucred clicred);
};

#endif /* INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_ */
