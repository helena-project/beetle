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
#include <memory>

#include "Beetle.h"
#include "ble/att.h"
#include "Debug.h"
#include "sync/OrderedThreadPool.h"
#include "util/write.h"

TCPConnection::TCPConnection(Beetle &beetle, SSL *ssl_, int sockfd_, struct sockaddr_in sockaddr_, bool isEndpoint,
		HandleAllocationTable *hat) :
		VirtualDevice(beetle, isEndpoint, hat) {
	ssl = ssl_;
	sockfd = sockfd_;
	sockaddr = sockaddr_;
	stopped = false;
}

TCPConnection::~TCPConnection() {
	stopped = true;

	pendingWrites.wait();

	beetle.readers.remove(sockfd);

	if (debug_socket) {
		pdebug("shutting down socket");
	}

	SSL_shutdown(ssl);
	shutdown(sockfd, SHUT_RDWR);
	SSL_free(ssl);
	close(sockfd);
}

void TCPConnection::startInternal() {
	beetle.readers.add(sockfd, [this] {
		if (stopped) {
			return;
		}

		uint8_t buf[256];
		uint8_t len;

		// read length of ATT message
		int bytesRead = SSL_read(ssl, &len, sizeof(len));
		if (bytesRead <= 0) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket errno: " << strerror(errno);
			}
			stopInternal();
		} else {
			assert(bytesRead == 1);
			if (debug_socket) {
				pdebug("tcp expecting " + std::to_string(len) + " bytes");
			}

			time_t startTime = time(NULL);

			struct timeval defaultTimeout;
			defaultTimeout.tv_sec = 0;
			defaultTimeout.tv_usec = 100000;

			fd_set fdSet;
			FD_ZERO(&fdSet);
			FD_SET(sockfd, &fdSet);

			// read payload ATT message
			bytesRead = 0;
			while (!stopped && bytesRead < len) {
				if (difftime(time(NULL), startTime) > TIMEOUT_PAYLOAD) {
					if (debug) {
						pdebug("timed out reading payload");
					}
					stopInternal();
					break;
				}

				int result = SSL_pending(ssl);

				if (result <= 0) {
					struct timeval timeout = defaultTimeout;
					fd_set readFds = fdSet;
					fd_set exceptFds = fdSet;
					result = select(sockfd+1, &readFds, NULL, &exceptFds, &timeout);
					if (result < 0) {
						std::stringstream ss;
						ss << "select failed : " << strerror(errno);
						pdebug(ss.str());
						stopInternal();
						break;
					}
				}

				if (result > 0) {
					int n = SSL_read(ssl, buf + bytesRead, len - bytesRead);
					if (n < 0) {
						std::cerr << "socket errno: " << strerror(errno) << std::endl;
						stopInternal();
						break;
					} else {
						bytesRead += n;
					}
				}
			}

			if (bytesRead < len) {
				return;
			}

			if (debug_socket) {
				phex(buf, bytesRead);
			}

			readHandler(buf, bytesRead);
		}
	});
}

bool TCPConnection::write(uint8_t *buf, int len) {
	if (stopped) {
		return false;
	}

	assert(buf);
	assert(len > 0);

	boost::shared_array<uint8_t> bufCpy(new uint8_t[len]);

	memcpy(bufCpy.get(), buf, len);
	pendingWrites.increment();
	beetle.writers.schedule(getId(), [this, bufCpy, len] {
		uint8_t bufLen = len;
		if (SSL_write_all(ssl, &bufLen, 1) != 1 || SSL_write_all(ssl, bufCpy.get(), len) != len) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket write failed : " << strerror(errno);
				pdebug(ss.str());
			}
			stopInternal();
		} else {
			if (debug_socket) {
				pdebug(getName() + " wrote " + std::to_string(len) + " bytes");
				phex(bufCpy.get(), len);
			}
		}
		pendingWrites.decrement();
	});
	return true;
}

struct sockaddr_in TCPConnection::getSockaddr() {
	return sockaddr;
}

void TCPConnection::stopInternal() {
	if(!stopped.exchange(true)) {
		if (debug) {
			pdebug("terminating " + getName());
		}
		beetle.removeDevice(getId());
	}
}

