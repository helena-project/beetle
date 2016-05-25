/*
 * TCPServerProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: James Hong
 */

#ifndef BLE_REMOTESERVERPROXY_H_
#define BLE_REMOTESERVERPROXY_H_

#include <string>
#include <openssl/ossl_typ.h>

#include "device/socket/TCPConnection.h"
#include "tcp/SSLConfig.h"

/*
 * Dummy device representing a server at a different Beetle gateway.
 */
class TCPServerProxy: public TCPConnection {
public:
	TCPServerProxy(Beetle &beetle, SSL *ssl, int sockfd, std::string serverGateway,
			struct sockaddr_in serverGatewaySockAddr, device_t remoteProxyTo);
	virtual ~TCPServerProxy();

	device_t getRemoteDeviceId();
	std::string getServerGateway();

	/*
	 * Static methods for connection establishment.
	 */
	static void initSSL(SSLConfig *sslConfig);
	static TCPServerProxy *connectRemote(Beetle &beetle, std::string server,
			int port, device_t remoteProxyTo);
private:
	device_t remoteProxyTo;
	std::string serverGateway;

	static SSLConfig *sslConfig;
};


#endif /* BLE_REMOTESERVERPROXY_H_ */
