/*
 * TCPConnection.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "device/socket/TCPConnection.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <openssl/ossl_typ.h>
#include <sstream>

#include "Beetle.h"
#include "ble/att.h"
#include "Debug.h"
#include "sync/OrderedThreadPool.h"
#include "util/write.h"

TCPConnection::TCPConnection(Beetle &beetle, SSL *ssl_, int sockfd_, struct sockaddr_in sockaddr_, bool isEndpoint,
		HandleAllocationTable *hat) :
		VirtualDevice(beetle, isEndpoint, hat), readThread() {
	ssl = ssl_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
}

TCPConnection::~TCPConnection() {
	terminated = true;

	pendingWrites.wait();
	if (debug_socket) {
		pdebug("shutting down socket");
	}
	SSL_shutdown(ssl);
	SSL_free(ssl);
	shutdown(sockfd, SHUT_RDWR);
	if (readThread.joinable())
		readThread.join();
	close(sockfd);
}

void TCPConnection::startInternal() {
	readThread = std::thread(&TCPConnection::readDaemon, this);
}

bool TCPConnection::write(uint8_t *buf, int len) {
	if (terminated) {
		return false;
	}

	uint8_t *bufCpy = new uint8_t[len];
	memcpy(bufCpy, buf, len);
	pendingWrites.increment();
	beetle.writers.schedule(getId(), [this, bufCpy, len] {
		uint8_t bufLen = len;
		if (SSL_write_all(ssl, &bufLen, 1) != 1 || SSL_write_all(ssl, bufCpy, len) != len) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket write failed : " << strerror(errno);
				pdebug(ss.str());
			}
			terminate();
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
	if (debug) {
		pdebug(getName() + " readDaemon started");
	}
	uint8_t buf[256];
	uint8_t len;
	while (true) {
		// read length of ATT message
		int bytesRead = SSL_read(ssl, &len, sizeof(len));
		if (bytesRead <= 0) {
			std::cerr << "socket errno: " << strerror(errno) << std::endl;
			terminate();
			break;
		} else {
			assert(bytesRead == 1);
			if (debug_socket) {
				pdebug("tcp expecting " + std::to_string(len) + " bytes");
			}

			// TODO perhaps 0 can be used as a heartbeat?
			if (len == 0) {
				continue;
			}

			// read payload ATT message
			bytesRead = 0;
			while (bytesRead < len) {
				int n = SSL_read(ssl, buf + bytesRead, len - bytesRead);
				if (n < 0) {
					std::cerr << "socket errno: " << strerror(errno) << std::endl;
					terminate();
					break;
				} else {
					bytesRead += n;
				}
			}

			if (bytesRead < len) {
				break; // terminate called
			}

			if (debug_socket) {
				pdebug(buf, bytesRead);
			}

			readHandler(buf, bytesRead);
		}
	}
	if (debug) {
		pdebug(getName() + " readDaemon exited");
	}
}

int TCPConnection::getMTU() {
	return ATT_DEFAULT_LE_MTU;
}

struct sockaddr_in TCPConnection::getSockaddr() {
	return sockaddr;
}

void TCPConnection::terminate() {
	if(!terminated.exchange(true)) {
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

