/*
 * TCPServerProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "device/socket/tcp/TCPServerProxy.h"

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <sstream>

#include <Debug.h>
#include "Device.h"
#include "hat/SingleAllocator.h"
#include "tcp/TCPConnParams.h"
#include "../shared.h"

TCPServerProxy::TCPServerProxy(Beetle &beetle, int sockfd, std::string serverGateway_,
		struct sockaddr_in serverGatewaySockAddr_, device_t remoteProxyTo_)
: TCPConnection(beetle, sockfd, "", serverGatewaySockAddr_, new SingleAllocator(NULL_RESERVED_DEVICE)) {
	type = TCP_SERVER_PROXY;

	serverGateway = serverGateway_;
	remoteProxyTo = remoteProxyTo_;
}

TCPServerProxy::~TCPServerProxy() {
	// Nothing to do, handled by superclass
}

device_t TCPServerProxy::getRemoteDeviceId() {
	return remoteProxyTo;
}

std::string TCPServerProxy::getServerGateway() {
	return serverGateway;
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

    std::string clientParams = ss.str();
    uint32_t clientParamsLen = htonl(clientParams.length());
    if (write_all(sockfd, (uint8_t *)&clientParamsLen, sizeof(clientParamsLen)) == false) {
    	close(sockfd);
    	throw DeviceException("could not write params length");
    }

    if (write_all(sockfd, (uint8_t *)clientParams.c_str(), clientParams.length()) == false) {
    	close(sockfd);
    	throw DeviceException("could not write all the params");
    }

	/*
	 * Read params from the server.
	 */
	uint32_t serverParamsLen;
	if (read(sockfd, &serverParamsLen, sizeof(uint32_t)) != sizeof(uint32_t)) {
		close(sockfd);
		throw DeviceException("could not read tcp connection server parameters length");
	}

	serverParamsLen = ntohl(serverParamsLen);
	if (debug) {
		pdebug("expecting " + std::to_string(serverParamsLen) + " bytes of parameters");
	}

	std::map<std::string, std::string> serverParams;
	if (!readParamsHelper(sockfd, serverParamsLen, serverParams)) {
		close(sockfd);
		throw DeviceException("unable to read parameters");
	}

	if (debug) {
		for (auto &kv : serverParams) {
			pdebug("parsed param (" + kv.first + "," + kv.second + ")");
		}
		pdebug("done reading tcp connection parameters");
	}

	if (serverParams.find(TCP_PARAM_GATEWAY) == serverParams.end()) {
		close(sockfd);
		throw DeviceException("server gateway did not respond with name");
	}

    /*
     * Instantiate the virtual device around the client socket.
     */
    return new TCPServerProxy(beetle, sockfd, serverParams[TCP_PARAM_GATEWAY], serv_addr, remoteProxyTo);
}

