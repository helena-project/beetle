/*
 * TCPClient.cpp
 *
 *  Created on: May 1, 2016
 *      Author: james
 */

#include "device/socket/tcp/TCPClient.h"

#include <netinet/in.h>
#include <string>

#include "Beetle.h"

TCPClient::TCPClient(Beetle &beetle, int sockfd, std::string name_, struct sockaddr_in sockaddr)
: TCPConnection(beetle, sockfd, sockaddr, true) {
	type = TCP_CLIENT;
	name = name_;
}

TCPClient::~TCPClient() {

}

