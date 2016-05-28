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
#include "device/VirtualDevice.h"
#include "device/socket/shared.h"

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

	std::atomic_bool stopped;
	void stopInternal();

	std::list<delayed_packet_t> delayedPackets;

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* INCLUDE_DEVICE_SOCKET_IPCAPPLICATION_H_ */
