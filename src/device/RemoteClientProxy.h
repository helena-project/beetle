/*
 * RemoteClientProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef DEVICE_REMOTECLIENTPROXY_H_
#define DEVICE_REMOTECLIENTPROXY_H_

#include <netinet/in.h>
#include <string>

#include "../Beetle.h"
#include "TCPConnection.h"

/*
 * Dummy device representing a client at a different Beetle gateway.
 */
class RemoteClientProxy: public TCPConnection {
public:
	RemoteClientProxy(Beetle &beetle, int sockfd, std::string clientGateway,
			struct sockaddr_in clientGatewaySockAddr, device_t localProxyFor);
	virtual ~RemoteClientProxy();

	device_t getLocalDeviceId() { return localProxyFor; };
	std::string getClientGateway() { return	clientGateway; };
	struct sockaddr_in getServerGatewaySockAddr() {	return clientGatewaySockAddr; };

private:
	device_t localProxyFor;
	std::string clientGateway;
	struct sockaddr_in clientGatewaySockAddr;
};

#endif /* DEVICE_REMOTECLIENTPROXY_H_ */
