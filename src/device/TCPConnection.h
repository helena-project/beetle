/*
 * TCPConnection.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef TCPCONNECTION_H_
#define TCPCONNECTION_H_

#include <bluetooth/bluetooth.h>
#include <cstdint>
#include <string>
#include <thread>

#include "../sync/BlockingQueue.h"
#include "VirtualDevice.h"
#include "shared.h"

/*
 * Remote "device" connected using TCP.
 */
class TCPConnection: public VirtualDevice {
public:
	TCPConnection(Beetle &beetle, int sockfd, std::string name);
	virtual ~TCPConnection();

	int getMTU();

protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	int sockfd;

	std::thread readThread;
	std::thread writeThread;

	BlockingQueue<queued_write_t> writeQueue;

	void readDaemon();
	void writeDaemon();
};

#endif /* TCPCONNECTION_H_ */
