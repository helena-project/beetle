/*
 * LEPeripheral.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_DEVICE_SOCKET_LEPERIPHERAL_H_
#define INCLUDE_DEVICE_SOCKET_LEPERIPHERAL_H_

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <cstdint>
#include <thread>
#include <functional>

#include "HCI.h"
#include "sync/Countdown.h"
#include "device/socket/SeqPacketConnection.h"
#include "shared.h"

class LEDevice: public SeqPacketConnection {
public:
	enum AddrType {
		PUBLIC, RANDOM,
	};

	virtual ~LEDevice();

	bdaddr_t getBdaddr();
	AddrType getAddrType();
	struct l2cap_conninfo getL2capConnInfo();

	bool setConnectionInterval(uint16_t interval);

	static LEDevice *newPeripheral(Beetle &beetle, HCI &hci, bdaddr_t addr,
			AddrType addrType);
	static LEDevice *newCentral(Beetle &beetle, HCI &hci, int sockfd,
			struct sockaddr_l2 sockaddr, std::function<void()> onDisconnect);

protected:
	LEDevice(Beetle &beetle, HCI &hci, int sockfd, struct sockaddr_l2 sockaddr,
			struct l2cap_conninfo connInfo, DeviceType type, std::string name,
			std::list<delayed_packet_t> delayedPackets,
			std::function<void()> onDisconnect = NULL);

private:
	HCI &hci;
	struct sockaddr_l2 sockaddr;
	struct l2cap_conninfo connInfo;

	std::function<void()> onDisconnect;
};

#endif /* INCLUDE_DEVICE_SOCKET_LEPERIPHERAL_H_ */
