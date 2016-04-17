/*
 * TCPServerProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "../../../../include/device/socket/tcp/TCPServerProxy.h"

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <sstream>

#include "../../../../include/Device.h"
#include "../../../../include/hat/SingleAllocator.h"
#include "../../../../include/tcp/ConnParams.h"
#include "../shared.h"

TCPServerProxy::TCPServerProxy(Beetle &beetle, int sockfd, std::string serverGateway_,
		struct sockaddr_in serverGatewaySockAddr_, device_t remoteProxyTo_)
: TCPConnection(beetle, sockfd, "", serverGatewaySockAddr_, new SingleAllocator(NULL_RESERVED_DEVICE)) {
	type = TCP_SERVER_PROXY;

	serverGateway = serverGateway_;
	remoteProxyTo = remoteProxyTo_;
}

TCPServerProxy::~TCPServerProxy() {
	// TODO Auto-generated destructor stub
}

TCPServerProxy *TCPServerProxy::connectRemote(Beetle &beetle, std::string host,
		int port, device_t remoteProxyTo) {
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

    /*
     * Format the plaintext parameters.
     */
    std::stringstream ss;
    ss << TCP_PARAM_GATEWAY << " " << beetle.name << "\n"
    		<< TCP_PARAM_DEVICE << " " << std::to_string(remoteProxyTo);

    std::string params = ss.str();
    uint32_t paramsLength = htonl(params.length());
    if (write_all(sockfd, (uint8_t *)&paramsLength, sizeof(paramsLength)) == false) {
    	close(sockfd);
    	throw DeviceException("could not write params length");
    }

    if (write_all(sockfd, (uint8_t *)params.c_str(), params.length()) == false) {
    	close(sockfd);
    	throw DeviceException("could not write all the params");
    }

    return new TCPServerProxy(beetle, sockfd, host, serv_addr, remoteProxyTo);
}

