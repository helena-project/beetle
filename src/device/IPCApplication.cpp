/*
 * IPCApplication.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "IPCApplication.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>
#include <string>
#include <thread>

#include "../Beetle.h"
#include "../Debug.h"
#include "../Device.h"
#include "../sync/BlockingQueue.h"
#include "shared.h"
#include "VirtualDevice.h"

IPCApplication::IPCApplication(Beetle &beetle, int sockfd_, std::string name_,
		struct sockaddr_un sockaddr_, struct ucred ucred_) : VirtualDevice(beetle), readThread(), writeThread() {
	type = IPC_APPLICATION;
	name = name_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
	ucred = ucred_;
}

IPCApplication::~IPCApplication() {
	std::queue<queued_write_t> *q = writeQueue.destroy();
	if (q != NULL) {
		while (q->size() > 0) {
			queued_write_t qw = q->front();
			delete[] qw.buf;
			q->pop();
		}
		delete q;
	}
	
	shutdown(sockfd, SHUT_RD);
	if (writeThread.joinable()) writeThread.join();
	if (readThread.joinable()) readThread.join();
	close(sockfd);
}

void IPCApplication::startInternal() {
    writeThread = std::thread(&IPCApplication::writeDaemon, this);
    readThread = std::thread(&IPCApplication::readDaemon, this);
}

bool IPCApplication::write(uint8_t *buf, int len) {
	queued_write_t qw;
	qw.buf = new uint8_t[len];
	memcpy(qw.buf, buf, len);
	qw.len = len;
	try {
		writeQueue.push(qw);
	} catch (QueueDestroyedException &e) {
		return false;
	}
	return true;
}

void IPCApplication::readDaemon() {
	if (debug) pdebug(getName() + " readDaemon started");
	uint8_t buf[64];
	while (!isStopped()) {
		int n = read(sockfd, buf, sizeof(buf));
		if (debug_socket) {
			pdebug(getName() + " read " + std::to_string(n) + " bytes");
		}
		if (n < 0) {
			int error;
			socklen_t len = sizeof(error);
			if (int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
				std::cerr << "error getting socket error code: " << strerror(retval) << std::endl;
			} else {
				std::cerr << "socket error: " << strerror(error) << std::endl;
			}
			if (!isStopped()) {
				stop();
				beetle.removeDevice(getId());
			}
			break;
		} else if (n == 0) {
			continue;
		} else {
			if (debug_socket) {
				pdebug(buf, n);
			}
			readHandler(buf, n);
		}
	}
	if (debug) pdebug(getName() + " readDaemon exited");
}

void IPCApplication::writeDaemon() {
	if (debug) pdebug(getName() + " writeDaemon started");
	while (!isStopped()) {
		try {
			queued_write_t qw = writeQueue.pop();
			if (write_all(sockfd, qw.buf, qw.len) != qw.len) {
				if (!isStopped()) {
					stop();
					beetle.removeDevice(getId());
				}
			}
			if (debug_socket) {
				pdebug(getName() + " wrote " + std::to_string(qw.len) + " bytes");
				pdebug(qw.buf, qw.len);
			}
			delete[] qw.buf;
		} catch (QueueDestroyedException &e) {
			break;
		}
	}
	if (debug) pdebug(getName() + " writeDaemon exited");
}


