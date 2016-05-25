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
#include <openssl/ossl_typ.h>

#include "sync/Countdown.h"
#include "device/VirtualDevice.h"

/*
 * Remote "device" connected using TCP.
 */
class TCPConnection: public VirtualDevice {
public:
	virtual ~TCPConnection();

	int getMTU();
	struct sockaddr_in getSockaddr();

protected:
	/*
	 * Cannot directly instantiate a TCPConnection
	 */
	TCPConnection(Beetle &beetle, SSL *ssl, int sockfd, struct sockaddr_in sockaddr,
			bool isEndpoint, HandleAllocationTable *hat = NULL);

	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	SSL *ssl;
	int sockfd;

	struct sockaddr_in sockaddr;

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* TCPCONNECTION_H_ */
