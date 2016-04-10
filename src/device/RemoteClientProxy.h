/*
 * RemoteClientProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef DEVICE_REMOTECLIENTPROXY_H_
#define DEVICE_REMOTECLIENTPROXY_H_

#include "TCPConnection.h"

/*
 * Dummy device representing a client at a different Beetle gateway.
 */
class RemoteClientProxy: public TCPConnection {
public:
	RemoteClientProxy(Beetle &beetle, int sockfd, std::string client, device_t proxyFor_);
	virtual ~RemoteClientProxy();
private:
	device_t proxyFor;
};

#endif /* DEVICE_REMOTECLIENTPROXY_H_ */
