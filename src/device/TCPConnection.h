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

#include "../Beetle.h"
#include "../sync/BlockingQueue.h"
#include "shared.h"
#include "VirtualDevice.h"

/*
 * Remote "device" connected using TCP.
 */
class TCPConnection: public VirtualDevice {
public:
	TCPConnection(Beetle &beetle, int sockfd, std::string name, struct sockaddr_in sockaddr);
	virtual ~TCPConnection();

	int getMTU();
	struct sockaddr_in getSockaddr();

protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	int sockfd;

	std::thread readThread;
	std::thread writeThread;

	struct sockaddr_in sockaddr;

	BlockingQueue<queued_write_t> writeQueue;

	void readDaemon();
	void writeDaemon();
};

#endif /* TCPCONNECTION_H_ */
