/*
 * UnixDomainSocketServer.h
 *
 *  Created on: Apr 15, 2016
 *      Author: james
 */

#ifndef IPC_UNIXDOMAINSOCKETSERVER_H_
#define IPC_UNIXDOMAINSOCKETSERVER_H_

#include <map>
#include <mutex>
#include <string>

#include "../Beetle.h"

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

#endif /* IPC_UNIXDOMAINSOCKETSERVER_H_ */
