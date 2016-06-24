/*
 * IPCApplication.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "device/socket/IPCApplication.h"

#include <boost/shared_array.hpp>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>

#include "Beetle.h"
#include "Debug.h"
#include "device/socket/shared.h"
#include "sync/OrderedThreadPool.h"
#include "util/write.h"

IPCApplication::IPCApplication(Beetle &beetle, int sockfd, std::string name_, struct sockaddr_un sockaddr_,
		struct ucred ucred_) :
		SeqPacketConnection(beetle, sockfd, true, std::list<delayed_packet_t>()) {
	type = IPC_APPLICATION;
	name = name_;
	sockaddr = sockaddr_;
	ucred = ucred_;
}

IPCApplication::~IPCApplication() {
	// nothing to do
}

struct ucred IPCApplication::getUcred() {
	return ucred;
}

struct sockaddr_un IPCApplication::getSockaddr() {
	return sockaddr;
}
