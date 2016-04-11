/*
 * RemoteServerProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "RemoteServerProxy.h"

#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <sstream>

#include "../Device.h"
#include "../hat/SingleAllocator.h"
#include "../tcp/ConnParams.h"
#include "shared.h"

RemoteServerProxy::RemoteServerProxy(Beetle &beetle, int sockfd, std::string serverGateway_,
		struct sockaddr_in serverGatewaySockAddr_, device_t remoteProxyTo_)
: TCPConnection(beetle, sockfd, "") {
	type = "ServerTCPProxy";

	serverGatewaySockAddr = serverGatewaySockAddr_;
	serverGateway = serverGateway_;

	/*
	 * TODO: Not happy about this. Base class makes a hat...
	 */
	delete hat;
	hat = new SingleAllocator(NULL_RESERVED_DEVICE);

	remoteProxyTo = remoteProxyTo_;
}

RemoteServerProxy::~RemoteServerProxy() {
	// TODO Auto-generated destructor stub
}

RemoteServerProxy *RemoteServerProxy::connectRemote(Beetle &beetle, std::string host,
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

    return new RemoteServerProxy(beetle, sockfd, host, serv_addr, remoteProxyTo);
}

