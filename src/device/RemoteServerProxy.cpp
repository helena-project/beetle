/*
 * RemoteServerProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "RemoteServerProxy.h"

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <sstream>
#include <string>

#include "../Beetle.h"
#include "../Device.h"
#include "../hat/SingleAllocator.h"
#include "shared.h"

RemoteServerProxy::RemoteServerProxy(Beetle &beetle, int sockfd, std::string server,
		device_t proxyTo_) : TCPConnection(beetle, sockfd, "") {
	name = server + "-" + std::to_string(proxyTo_);
	type = "ServerTCPProxy";
	delete hat;
	hat = new SingleAllocator(proxyTo_);
	proxyTo = proxyTo_;
}

RemoteServerProxy::~RemoteServerProxy() {
	// TODO Auto-generated destructor stub
}

RemoteServerProxy *RemoteServerProxy::connectRemote(Beetle &beetle, std::string host,
		int port, device_t proxyTo) {
	struct hostent *server;
	server = gethostbyname(host.c_str());
	if (server == NULL) {
		throw DeviceException("could not get host");
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		throw DeviceException("error opening socket");
	}

	struct sockaddr_in serv_addr = {0};
	serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    	close(sockfd);
    	throw DeviceException("error connecting");
    }

    std::stringstream ss;
    ss << "client " << "beetle" << "\n" << "device " << proxyTo;

    std::string params = ss.str();
    uint32_t paramsLength = htonl(params.length());
    if (write_all(sockfd, (uint8_t *)&paramsLength, sizeof(paramsLength)) == false) {
    	close(sockfd);
    	throw DeviceException("could not write params length");
    }

    if (write_all(sockfd, (uint8_t *)params.c_str(), params.length()) == false) {
    	close(sockfd);
    	throw DeviceException("could not write params length");
    }

    return new RemoteServerProxy(beetle, sockfd, host, proxyTo);
}

