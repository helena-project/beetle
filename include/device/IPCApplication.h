/*
 * IPCApplication.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_IPCAPPLICATION_H_
#define DEVICE_IPCAPPLICATION_H_

#include <sys/socket.h>
#include <sys/un.h>
#include <cstdint>
#include <thread>

#include "../../include/device/VirtualDevice.h"
#include "../Beetle.h"
#include "../sync/Countdown.h"

enum AddrType {
	PUBLIC, RANDOM,
};

class IPCApplication: public VirtualDevice {
public:
	IPCApplication(Beetle &beetle, int sockfd, std::string name, struct sockaddr_un sockaddr, struct ucred ucred_);
	virtual ~IPCApplication();

protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:

	int sockfd;
	struct sockaddr_un sockaddr;
	struct ucred ucred;

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* DEVICE_IPCAPPLICATION_H_ */
