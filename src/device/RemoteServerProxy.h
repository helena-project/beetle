/*
 * RemoteServerProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef BLE_REMOTESERVERPROXY_H_
#define BLE_REMOTESERVERPROXY_H_

#include <string>

#include "../Beetle.h"
#include "TCPConnection.h"

/*
 * Dummy device representing a server at a different Beetle gateway.
 */
class RemoteServerProxy: public TCPConnection {
public:
	RemoteServerProxy(Beetle &beetle, int sockfd, std::string serverGateway,
			struct sockaddr_in serverGatewaySockAddr, device_t remoteProxyTo);
	virtual ~RemoteServerProxy();

	device_t getRemoteDeviceId() { return remoteProxyTo; };
	std::string getServerGateway() { return	serverGateway; };

	static RemoteServerProxy *connectRemote(Beetle &beetle, std::string server,
			int port, device_t remoteProxyTo_);
private:
	device_t remoteProxyTo;
	std::string serverGateway;
};


#endif /* BLE_REMOTESERVERPROXY_H_ */
