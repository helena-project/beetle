/*
 * TCPConnection.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "device/socket/TCPConnection.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>

#include "ble/att.h"
#include "Debug.h"
#include "sync/OrderedThreadPool.h"
#include "shared.h"

TCPConnection::TCPConnection(Beetle &beetle, int sockfd_, std::string name_,
		struct sockaddr_in sockaddr_, HandleAllocationTable *hat) : VirtualDevice(beetle, hat), readThread() {
	type = TCP_CONNECTION;
	name = name_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
}

TCPConnection::~TCPConnection() {
	pendingWrites.wait();

	shutdown(sockfd, SHUT_RDWR);
	if (readThread.joinable()) readThread.join();
	close(sockfd);
}

void TCPConnection::startInternal() {
    readThread = std::thread(&TCPConnection::readDaemon, this);
}

bool TCPConnection::write(uint8_t *buf, int len) {
	uint8_t *bufCpy = new uint8_t[len];
	memcpy(bufCpy, buf, len);
	pendingWrites.increment();
	beetle.writers.schedule(getId(), [this, bufCpy, len] {
		uint8_t bufLen = len;
		if (write_all(sockfd, &bufLen, 1) != 1 || write_all(sockfd, bufCpy, len) != len) {
			if (!isStopped()) {
				stop();
				beetle.removeDevice(getId());
			}
		}
		if (debug_socket) {
			pdebug(getName() + " wrote " + std::to_string(len) + " bytes");
			pdebug(bufCpy, len);
		}
		delete[] bufCpy;
		pendingWrites.decrement();
	});
	return true;
}

void TCPConnection::readDaemon() {
	if (debug) pdebug(getName() + " readDaemon started");
	uint8_t buf[255];
	uint8_t len;
	while (!isStopped()) {
		// read length of ATT message
		int bytesRead = read(sockfd, &len, sizeof(len));
		if (bytesRead < 0) {
			int error;
			socklen_t errorLen = sizeof(error);
			if (int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errorLen) != 0) {
				std::cerr << "error getting socket error code: " << strerror(retval) << std::endl;
			} else {
				std::cerr << "socket error: " << strerror(error) << std::endl;
			}
			stop();
			beetle.removeDevice(getId());
			break;
		} else if (bytesRead == 0) {
			continue;
		} else {
			assert(bytesRead == 1);
			if (debug_socket) {
				pdebug("tcp expecting " + std::to_string(len) + " bytes");
			}

			// read payload ATT message
			bytesRead = 0;
			while (!isStopped() && bytesRead < len) {
				int n = read(sockfd, buf + bytesRead, len - bytesRead);
				if (n < 0) {
					int error;
					socklen_t errorLen = sizeof(error);
					if (int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errorLen) != 0) {
						std::cerr << "error getting socket error code: " << strerror(retval) << std::endl;
					} else {
						std::cerr << "socket error: " << strerror(error) << std::endl;
					}
					if (!isStopped()) {
						stop();
						beetle.removeDevice(getId());
					}
					break;
				} else {
					bytesRead += n;
				}
			}

			if (isStopped()) break;

			assert(bytesRead == len);
			if (debug_socket) {
				pdebug(buf, bytesRead);
			}
			readHandler(buf, bytesRead);
		}
	}
	if (debug) pdebug(getName() + " readDaemon exited");
}

int TCPConnection::getMTU() {
	return ATT_DEFAULT_LE_MTU;
}

struct sockaddr_in TCPConnection::getSockaddr() {
	return sockaddr;
}

