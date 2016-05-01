/*
 * TCPClientProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "device/socket/tcp/TCPClientProxy.h"

#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <netinet/in.h>
#include <map>

#include "hat/SingleAllocator.h"

TCPClientProxy::TCPClientProxy(Beetle &beetle, SSL *ssl, int sockfd, std::string clientGateway_,
		struct sockaddr_in clientGatewaySockAddr_, device_t localProxyFor_)
: TCPConnection(beetle, ssl, sockfd, clientGatewaySockAddr_, false, new SingleAllocator(localProxyFor_)) {
	/*
	 * Make sure the device exists locally.
	 */
	boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
	if (beetle.devices.find(localProxyFor_) == beetle.devices.end()) {
		throw new DeviceException("no device for " + std::to_string(localProxyFor_));
	}

	name = "Proxy for " +  std::to_string(localProxyFor_) + " to " + clientGateway_;
	type = TCP_CLIENT_PROXY;
	clientGateway = clientGateway_;
	localProxyFor = localProxyFor_;
}

TCPClientProxy::~TCPClientProxy() {
	// Nothing to do, handled by superclass
}

device_t TCPClientProxy::getLocalDeviceId() {
	return localProxyFor;
};

std::string TCPClientProxy::getClientGateway() {
	return clientGateway;
};
