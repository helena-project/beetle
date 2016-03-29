/*
 * Peripheral.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "LEPeripheral.h"

#include <bits/socket_type.h>
#include <bluetooth/l2cap.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <queue>

#include "../ble/att.h"

LEPeripheral::LEPeripheral(Beetle &beetle, std::string name, bdaddr_t addr, AddrType addrType) : Device(beetle, name) {
	bdaddr = addr;
	bdaddrType = addrType;

    struct sockaddr_l2 loc_addr = {0};
    loc_addr.l2_family = AF_BLUETOOTH;
    bacpy(&loc_addr.l2_bdaddr, BDADDR_ANY);
    loc_addr.l2_psm = 0;
    loc_addr.l2_cid = htobs(ATT_CID);
    loc_addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

    struct sockaddr_l2 rem_addr = {0};
    rem_addr.l2_family = AF_BLUETOOTH;
    bacpy(&rem_addr.l2_bdaddr, &bdaddr);
    rem_addr.l2_psm = 0;
    rem_addr.l2_cid = htobs(ATT_CID);
    rem_addr.l2_bdaddr_type = BDADDR_LE_RANDOM;

    sockfd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sockfd < 0) {
        throw "could not create socket";
    }
    if (bind(sockfd, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        close(sockfd);
        throw "could not bind";
    }
    if (connect(sockfd, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0) {
        close(sockfd);
        throw "could not connect";
    }

    writeThread(writeDaemon);
    readThread(readDaemon);
}

LEPeripheral::~LEPeripheral() {
	std::queue<queued_write_t> *q = writeQueue.destroy();
	if (q != NULL) {
		while (q->size() > 0) {
			queued_write_t qw = q->front();
			free(qw.buf);
			q->pop();
		}
		delete q;
	}
	writeThread.join();
	readThread.join();
	close(sockfd);
}

bool LEPeripheral::write(char *buf, int len) {
	queued_write_t qw;
	qw.buf = malloc(len);
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
	char buf[256];
	while (!isStopped()) {
		int n = read(sockfd, buf, sizeof(buf));
		if (n < 0) {
			stop();
			break;
		} else if (n == 0) {
			continue;
		} else {
			handleRead(buf, n);
		}
	}
}

void LEPeripheral::writeDaemon() {
	while (!isStopped()) {
		try {
			queued_write_t qw = writeQueue.pop();
			::write(sockfd, qw.buf, qw.len);
			free(qw.buf);
		} catch (QueueDestroyedException &e) {
			break;
		}
	}
}


