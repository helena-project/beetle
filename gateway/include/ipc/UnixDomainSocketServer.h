/*
 * UnixDomainSocketServer.h
 *
 *  Created on: Apr 15, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_
#define INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_

#include <string>
#include <thread>

#include "BeetleTypes.h"

class UnixDomainSocketServer {
public:
	UnixDomainSocketServer(Beetle &beetle, std::string path);
	virtual ~UnixDomainSocketServer();

private:
	Beetle &beetle;
	int fd;
};

#endif /* INCLUDE_IPC_UNIXDOMAINSOCKETSERVER_H_ */
