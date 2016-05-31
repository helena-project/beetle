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

IPCApplication::IPCApplication(Beetle &beetle, int sockfd_, std::string name_, struct sockaddr_un sockaddr_,
		struct ucred ucred_) :
		VirtualDevice(beetle, true) {
	type = IPC_APPLICATION;
	name = name_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
	ucred = ucred_;
	stopped = false;

	if (!request_device_name(sockfd, name, delayedPackets)) {
		throw DeviceException("could not discover name");
	}
}

IPCApplication::~IPCApplication() {
	stopped = true;

	pendingWrites.wait();

	beetle.readers.remove(sockfd);

	if (debug_socket) {
		pdebug("shutting down socket for " + getName());
	}
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
}

void IPCApplication::startInternal() {
	for (delayed_packet_t &packet : delayedPackets) {
		readHandler(packet.buf.get(), packet.len);
	}
	delayedPackets.clear();

	beetle.readers.add(sockfd, [this] {
		if (stopped) {
			return;
		}

		uint8_t buf[256];
		int n = read(sockfd, buf, sizeof(buf));
		if (debug_socket) {
			pdebug("read " + std::to_string(n) + " bytes from " + getName());
		}
		if (n <= 0) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket errno: " << strerror(errno);
			}
			stopInternal();
		} else {
			if (debug_socket) {
				phex(buf, n);
			}
			readHandler(buf, n);
		}
	});
}

bool IPCApplication::write(uint8_t *buf, int len) {
	if (stopped) {
		return false;
	}

	assert(buf);
	assert(len > 0);

	boost::shared_array<uint8_t> bufCpy(new uint8_t[len]);
	memcpy(bufCpy.get(), buf, len);
	pendingWrites.increment();
	beetle.writers.schedule(getId(), [this, bufCpy, len] {
		if (write_all(sockfd, bufCpy.get(), len) != len) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket write failed : " << strerror(errno);
				pdebug(ss.str());
			}
			stopInternal();
		} else {
			if (debug_socket) {
				pdebug("wrote " + std::to_string(len) + " bytes to " + getName());
				phex(bufCpy.get(), len);
			}
		}
		pendingWrites.decrement();
	});
	return true;
}

void IPCApplication::stopInternal() {
	if(!stopped.exchange(true)) {
		if (debug) {
			pdebug("terminating " + getName());
		}
		beetle.removeDevice(getId());
	}
}
