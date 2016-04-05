/*
 * TCPConnection.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "TCPConnection.h"

#include <asm-generic/socket.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <queue>

#include "../ble/att.h"
#include "../Debug.h"
#include "../Device.h"

TCPConnection::TCPConnection(Beetle &beetle, int sockfd_) :
VirtualDevice(beetle), readThread(), writeThread() {
	type = "TCPConnection";
	sockfd = sockfd_;
}

TCPConnection::~TCPConnection() {
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

void TCPConnection::startInternal() {
    writeThread = std::thread(&TCPConnection::writeDaemon, this);
    readThread = std::thread(&TCPConnection::readDaemon, this);
}

bool TCPConnection::write(uint8_t *buf, int len) {
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
			if (debug) {
				pdebug("expecting " + std::to_string(len) + " bytes");
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
			if (debug) pdebug(buf, bytesRead);
			readHandler(buf, bytesRead);
		}
	}
	if (debug) pdebug(getName() + " readDaemon exited");
}

void TCPConnection::writeDaemon() {
	if (debug) pdebug(getName() + " writeDaemon started");
	while (!isStopped()) {
		try {
			queued_write_t qw = writeQueue.pop();
			uint8_t len = (uint8_t) qw.len;
			::write(sockfd, &len, 1); // TODO check return values
			::write(sockfd, qw.buf, qw.len);
			if (debug) {
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

int TCPConnection::getMTU() {
	return ATT_DEFAULT_LE_MTU;
}


