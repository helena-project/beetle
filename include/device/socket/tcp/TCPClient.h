/*
 * TCPClient.h
 *
 *  Created on: May 1, 2016
 *      Author: james
 */

#ifndef DEVICE_SOCKET_TCP_TCPCLIENT_H_
#define DEVICE_SOCKET_TCP_TCPCLIENT_H_

#include <openssl/ossl_typ.h>
#include <string>

#include "Beetle.h"
#include "device/socket/TCPConnection.h"

class TCPClient: public TCPConnection {
public:
	TCPClient(Beetle &beetle, SSL *ssl, int sockfd, std::string name, struct sockaddr_in sockaddr);
	virtual ~TCPClient();
};

#endif /* DEVICE_SOCKET_TCP_TCPCLIENT_H_ */
