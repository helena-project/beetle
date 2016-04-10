/*
 * RemoteClientProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#include "RemoteClientProxy.h"

#include <string>

#include "../Beetle.h"
#include "../hat/SingleAllocator.h"

RemoteClientProxy::RemoteClientProxy(Beetle &beetle, int sockfd, std::string client,
		device_t proxyFor_) : TCPConnection(beetle, sockfd, "") {
	name = client + "-" + std::to_string(proxyFor_);
	type = "ClientTCPProxy";
	delete hat;
	hat = new SingleAllocator(proxyFor_);
	proxyFor = proxyFor_;
}

RemoteClientProxy::~RemoteClientProxy() {
	// TODO Auto-generated destructor stub
}

