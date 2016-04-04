/*
 * LEPeripheral.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "LEPeripheral.h"

#include <bluetooth/l2cap.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <queue>

#include "../ble/att.h"
#include "../Debug.h"

LEPeripheral::LEPeripheral(Beetle &beetle, bdaddr_t addr, AddrType addrType
		) : VirtualDevice(beetle), readThread(), writeThread() {
	type = "LEPeripheral";

	bdaddr = addr;
	bdaddrType = addrType;

    struct sockaddr_l2 loc_addr = {0};
    loc_addr.l2_family = AF_BLUETOOTH;
    bdaddr_t tmp = {0, 0, 0, 0, 0, 0}; // BDADDR_ANY
    bacpy(&loc_addr.l2_bdaddr, &tmp);
    loc_addr.l2_psm = 0;
    loc_addr.l2_cid = htobs(ATT_CID);
    loc_addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

    struct sockaddr_l2 rem_addr = {0};
    rem_addr.l2_family = AF_BLUETOOTH;
    bacpy(&rem_addr.l2_bdaddr, &bdaddr);
    rem_addr.l2_psm = 0;
    rem_addr.l2_cid = htobs(ATT_CID);
    rem_addr.l2_bdaddr_type = (addrType == PUBLIC) ? BDADDR_LE_PUBLIC : BDADDR_LE_RANDOM;

    sockfd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sockfd < 0) {
        throw DeviceException("could not create socket");
    }
    if (bind(sockfd, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        close(sockfd);
        throw DeviceException("could not bind");
    }
    if (connect(sockfd, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0) {
        close(sockfd);
        throw DeviceException("could not connect");
    }
}

LEPeripheral::~LEPeripheral() {
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

void LEPeripheral::startInternal() {
    writeThread = std::thread(&LEPeripheral::writeDaemon, this);
    readThread = std::thread(&LEPeripheral::readDaemon, this);
}

bool LEPeripheral::write(uint8_t *buf, int len) {
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

void LEPeripheral::readDaemon() {
	if (debug) pdebug(getName() + " readDaemon started");
	uint8_t buf[64];
	while (!isStopped()) {
		int n = read(sockfd, buf, sizeof(buf));
		if (debug) {
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
			stop(); // TODO not sure if this is the right thing to do
			break;
		} else if (n == 0) {
			continue;
		} else {
			if (debug) pdebug(buf, n);
			readHandler(buf, n);
		}
	}
	if (debug) pdebug(getName() + " readDaemon exited");
}

void LEPeripheral::writeDaemon() {
	if (debug) pdebug(getName() + " writeDaemon started");
	while (!isStopped()) {
		try {
			queued_write_t qw = writeQueue.pop();
			::write(sockfd, qw.buf, qw.len); // TODO check return values
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

int LEPeripheral::getMTU() {
	return ATT_DEFAULT_LE_MTU;
}


