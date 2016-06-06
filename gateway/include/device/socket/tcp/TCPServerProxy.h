/*
 * TCPServerProxy.h
 *
 *  Created on: Apr 10, 2016
 *      Author: James Hong
 */

#ifndef BLE_REMOTESERVERPROXY_H_
#define BLE_REMOTESERVERPROXY_H_

#include <ctime>
#include <string>
#include <openssl/ossl_typ.h>

#include "BeetleTypes.h"
#include "device/socket/TCPConnection.h"

class SSLConfig;

/*
 * Dummy device representing a server at a different Beetle gateway.
 */
class TCPServerProxy: public TCPConnection {
public:
	TCPServerProxy(Beetle &beetle, SSL *ssl, int sockfd, std::string serverGateway,
			struct sockaddr_in serverGatewaySockAddr, device_t remoteProxyTo);
	virtual ~TCPServerProxy();

	/*
	 * Returns the id at the remote gateway.
	 */
	device_t getRemoteDeviceId();

	/*
	 * Returns the name of the server gateway.
	 */
	std::string getServerGateway();

	/*
	 * Returns whether the internal virtual device has timed out.
	 */
	bool isLive();

	/* Time to timeout proxy after unuse */
	static constexpr int PROXY_UNUSED_TIMEOUT = 60;

	/*
	 * Static methods for connection establishment.
	 */
	static void initSSL(SSLConfig *sslConfig);
	static TCPServerProxy *connectRemote(Beetle &beetle, std::string server,
			int port, device_t remoteProxyTo);
private:
	device_t remoteProxyTo;
	std::string serverGateway;

	time_t createdAt;

	static SSLConfig *sslConfig;
};


#endif /* BLE_REMOTESERVERPROXY_H_ */
