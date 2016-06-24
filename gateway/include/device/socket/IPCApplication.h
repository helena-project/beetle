/*
 * IPCApplication.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_DEVICE_SOCKET_IPCAPPLICATION_H_
#define INCLUDE_DEVICE_SOCKET_IPCAPPLICATION_H_

#include <sys/socket.h>
#include <sys/un.h>
#include <cstdint>
#include <thread>
#include <list>

#include "sync/Countdown.h"
#include "device/socket/SeqPacketConnection.h"
#include "device/socket/shared.h"

class IPCApplication: public SeqPacketConnection {
public:
	IPCApplication(Beetle &beetle, int sockfd, std::string name,
			struct sockaddr_un sockaddr, struct ucred ucred_);
	virtual ~IPCApplication();

	struct sockaddr_un getSockaddr();
	struct ucred getUcred();
private:
	struct sockaddr_un sockaddr;
	struct ucred ucred;
};

#endif /* INCLUDE_DEVICE_SOCKET_IPCAPPLICATION_H_ */
