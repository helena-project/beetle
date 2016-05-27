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
#include "sync/OrderedThreadPool.h"
#include "util/write.h"

IPCApplication::IPCApplication(Beetle &beetle, int sockfd_, std::string name_, struct sockaddr_un sockaddr_,
		struct ucred ucred_) :
		VirtualDevice(beetle, true), readThread() {
	type = IPC_APPLICATION;
	name = name_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
	ucred = ucred_;
	stopped = false;
}

IPCApplication::~IPCApplication() {
	stopped = true;

	pendingWrites.wait();
	if (debug_socket) {
		pdebug("shutting down socket");
	}
	shutdown(sockfd, SHUT_RDWR);
	if (readThread.joinable()) {
		readThread.join();
	}
	close(sockfd);
}

void IPCApplication::startInternal() {
	readThread = std::thread(&IPCApplication::readDaemon, this);
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
				pdebug(getName() + " wrote " + std::to_string(len) + " bytes");
				pdebug(bufCpy.get(), len);
			}
		}
		pendingWrites.decrement();
	});
	return true;
}

void IPCApplication::readDaemon() {
	if (debug) pdebug(getName() + " readDaemon started");
	uint8_t buf[256];
	while (true) {
		int n = read(sockfd, buf, sizeof(buf));
		if (debug_socket) {
			pdebug(getName() + " read " + std::to_string(n) + " bytes");
		}
		if (n <= 0) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket errno: " << strerror(errno);
			}
			stopInternal();
			break;
		} else {
			if (debug_socket) {
				pdebug(buf, n);
			}
			readHandler(buf, n);
		}
	}
	if (debug) pdebug(getName() + " readDaemon exited");
}

void IPCApplication::stopInternal() {
	if(!stopped.exchange(true)) {
		if (debug) {
			pdebug("terminating " + getName());
		}

		device_t id = getId();
		Beetle *beetlePtr = &beetle;

		/*
		 * Destructor may join or wait for caller. This prevents deadlock.
		 */
		beetle.workers.schedule([beetlePtr, id] { beetlePtr->removeDevice(id); });
	}
}
