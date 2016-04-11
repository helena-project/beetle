/*
 * RemoteClientProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef DEVICE_REMOTECLIENTPROXY_H_
#define DEVICE_REMOTECLIENTPROXY_H_

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

private:
	device_t localProxyFor;
	std::string clientGateway;
};

#endif /* DEVICE_REMOTECLIENTPROXY_H_ */
