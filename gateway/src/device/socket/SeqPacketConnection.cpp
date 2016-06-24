/*
 * SEQPacketConnection.cpp
 *
 *  Created on: Jun 24, 2016
 *      Author: James Hong
 */

#include "device/socket/SeqPacketConnection.h"

#include <sys/socket.h>
#include <unistd.h>

#include "Beetle.h"
#include "device/socket/SeqPacketConnection.h"
#include "Debug.h"
#include "sync/SocketSelect.h"

SeqPacketConnection::SeqPacketConnection(Beetle &beetle, int sockfd_, bool isEndpoint,
		std::list<delayed_packet_t> delayedPackets_) :
	VirtualDevice(beetle, isEndpoint) {
	sockfd = sockfd_;
	delayedPackets = delayedPackets_;
}

SeqPacketConnection::~SeqPacketConnection() {
	stopped = true;

	pendingWrites.wait();

	beetle.readers.remove(sockfd);

	if (debug_socket) {
		pdebug("shutting down socket for " + getName());
	}
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
}

void SeqPacketConnection::startInternal() {
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
				pdebug(ss.str());
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

bool SeqPacketConnection::write(uint8_t *buf, int len) {
	if (stopped) {
		return false;
	}

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

void SeqPacketConnection::stopInternal() {
	if(!stopped.exchange(true)) {
		if (debug) {
			pdebug("terminating " + getName());
		}
		beetle.removeDevice(getId());
	}
}
