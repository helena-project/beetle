/*
 * LEPeripheral.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include <device/socket/LEDevice.h>

#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
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

LEDevice *LEDevice::newPeripheral(Beetle &beetle, HCI &hci, bdaddr_t bdaddr,
		AddrType addrType) {
	struct sockaddr_l2 loc_addr;
	memset(&loc_addr, 0, sizeof(struct sockaddr_l2));
	loc_addr.l2_family = AF_BLUETOOTH;
	hci_dev_info devInfo;
	hci_devinfo(hci.getDeviceId(), &devInfo);
	bacpy(&loc_addr.l2_bdaddr, &devInfo.bdaddr);
	loc_addr.l2_psm = 0;
	loc_addr.l2_cid = htobs(ATT_CID);
	loc_addr.l2_bdaddr_type = devInfo.type;

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

	return new LEDevice(beetle, hci, sockfd, rem_addr, connInfo, LE_PERIPHERAL,
			name, delayedPackets);
}

LEDevice *LEDevice::newCentral(Beetle &beetle, HCI &hci, int sockfd,
		struct sockaddr_l2 addr, std::function<void()> onDisconnect) {
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

	return new LEDevice(beetle, hci, sockfd, addr, connInfo, LE_CENTRAL,
			name, delayedPackets, onDisconnect);
}

LEDevice::LEDevice(
		Beetle &beetle,
		HCI &hci,
		int sockfd,
		struct sockaddr_l2 sockaddr_,
		struct l2cap_conninfo connInfo_,
		DeviceType type_,
		std::string name_,
		std::list<delayed_packet_t> delayedPackets,
		std::function<void()> onDisconnect_) :
		SeqPacketConnection(beetle, sockfd, true, delayedPackets), hci(hci) {
	type = type_;

	name = name_;
	sockaddr = sockaddr_;

	connInfo = connInfo_;

	onDisconnect = onDisconnect_;

	hci.setConnectionInterval(connInfo.hci_handle, 10, 40, 0, 0x0C80, 0);
}

LEDevice::~LEDevice() {
	if (onDisconnect != NULL) {
		beetle.workers.schedule(onDisconnect);
	}
}

bdaddr_t LEDevice::getBdaddr() {
	return sockaddr.l2_bdaddr;
}

LEDevice::AddrType LEDevice::getAddrType() {
	return sockaddr.l2_bdaddr_type == BDADDR_LE_PUBLIC ? PUBLIC : RANDOM;
}

struct l2cap_conninfo LEDevice::getL2capConnInfo() {
	return connInfo;
}

bool LEDevice::setConnectionInterval(uint16_t interval) {
	return hci.setConnectionInterval(connInfo.hci_handle, interval, interval, 0, 0x0C80, 0);
}


