/*
 * TCPConnection.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef TCPCONNECTION_H_
#define TCPCONNECTION_H_


#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <thread>

#include "Beetle.h"
#include "sync/Countdown.h"
#include "device/VirtualDevice.h"

/*
 * Remote "device" connected using TCP.
 */
class TCPConnection: public VirtualDevice {
public:
	TCPConnection(Beetle &beetle, int sockfd, std::string name,
			struct sockaddr_in sockaddr, HandleAllocationTable *hat = NULL);
	virtual ~TCPConnection();

	int getMTU();
	struct sockaddr_in getSockaddr();

protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	int sockfd;

	struct sockaddr_in sockaddr;

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* TCPCONNECTION_H_ */
