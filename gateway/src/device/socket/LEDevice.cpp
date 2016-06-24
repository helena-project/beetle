/*
 * LEPeripheral.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include <device/socket/LEDevice.h>

#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sstream>
#include <unistd.h>
#include <memory>

#include "Beetle.h"
#include "ble/att.h"
#include "Debug.h"
#include "Device.h"
#include "device/socket/shared.h"
#include "sync/OrderedThreadPool.h"
#include "util/write.h"

LEDevice *LEDevice::newPeripheral(Beetle &beetle, bdaddr_t bdaddr, AddrType addrType) {
	struct sockaddr_l2 loc_addr;
	memset(&loc_addr, 0, sizeof(struct sockaddr_l2));
	loc_addr.l2_family = AF_BLUETOOTH;
	bdaddr_t tmp;
	memset(tmp.b, 0, sizeof(bdaddr_t));
	bacpy(&loc_addr.l2_bdaddr, &tmp);
	loc_addr.l2_psm = 0;
	loc_addr.l2_cid = htobs(ATT_CID);
	loc_addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

	struct sockaddr_l2 rem_addr;
	memset(&rem_addr, 0, sizeof(struct sockaddr_l2));
	rem_addr.l2_family = AF_BLUETOOTH;
	bacpy(&rem_addr.l2_bdaddr, &bdaddr);
	rem_addr.l2_psm = 0;
	rem_addr.l2_cid = htobs(ATT_CID);
	rem_addr.l2_bdaddr_type = (addrType == PUBLIC) ? BDADDR_LE_PUBLIC : BDADDR_LE_RANDOM;

	int sockfd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (sockfd < 0) {
		throw DeviceException("could not create socket");
	}
	if (bind(sockfd, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) < 0) {
		close(sockfd);
		throw DeviceException("could not bind");
	}
	if (connect(sockfd, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) < 0) {
		close(sockfd);
		throw DeviceException("could not connect");
	}

	struct l2cap_conninfo connInfo;
	unsigned int connInfoLen = sizeof(connInfo);
	if (getsockopt(sockfd, SOL_L2CAP, L2CAP_CONNINFO, &connInfo, &connInfoLen) < 0) {
		close(sockfd);
		throw DeviceException("could not get l2cap conn info");
	}

	if (debug_socket) {
		std::stringstream ss;
		ss << "peripheral hci handle: " << connInfo.hci_handle;
		pdebug(ss.str());
	}

	std::string name;
	std::list<delayed_packet_t> delayedPackets;
	if (!request_device_name(sockfd, name, delayedPackets)) {
		close(sockfd);
		throw DeviceException("could not discover device name");
	}

	return new LEDevice(beetle, sockfd, rem_addr, connInfo, LE_PERIPHERAL, name, delayedPackets);
}

LEDevice *LEDevice::newCentral(Beetle &beetle, int sockfd, struct sockaddr_l2 addr) {
	struct l2cap_conninfo connInfo;
	unsigned int connInfoLen = sizeof(connInfo);
	if (getsockopt(sockfd, SOL_L2CAP, L2CAP_CONNINFO, &connInfo, &connInfoLen) < 0) {
		close(sockfd);
		throw DeviceException("could not get l2cap conn info");
	}

	std::string name;
	std::list<delayed_packet_t> delayedPackets;
	if (!request_device_name(sockfd, name, delayedPackets)) {
		close(sockfd);
		throw DeviceException("could not discover device name");
	}

	return new LEDevice(beetle, sockfd, addr, connInfo, LE_CENTRAL, name, delayedPackets);
}

LEDevice::LEDevice(Beetle &beetle, int sockfd, struct sockaddr_l2 sockaddr,
		struct l2cap_conninfo connInfo, DeviceType type_, std::string name_,
		std::list<delayed_packet_t> delayedPackets) :
		SeqPacketConnection(beetle, sockfd, true, delayedPackets),
		connInfo(connInfo) {
	type = type_;

	name = name_;
	bdaddr = sockaddr.l2_bdaddr;
	bdaddrType = (sockaddr.l2_bdaddr_type == BDADDR_LE_PUBLIC) ? PUBLIC : RANDOM;

	if (type == LE_PERIPHERAL) {
		/*
		 * Set default connection interval.
		 */
		beetle.hci.setConnectionInterval(connInfo.hci_handle, 10, 40, 0, 0x0C80, 0);
	}
}

LEDevice::~LEDevice() {
	// nothing to do
}

bdaddr_t LEDevice::getBdaddr() {
	return bdaddr;
}

LEDevice::AddrType LEDevice::getAddrType() {
	return bdaddrType;
}

struct l2cap_conninfo LEDevice::getL2capConnInfo() {
	return connInfo;
}


