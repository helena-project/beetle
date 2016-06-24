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

	static LEDevice *newPeripheral(Beetle &beetle, bdaddr_t addr, AddrType addrType);
	static LEDevice *newCentral(Beetle &beetle, int sockfd, struct sockaddr_l2 sockaddr);

protected:
	LEDevice(Beetle &beetle, int sockfd, struct sockaddr_l2 sockaddr,
			struct l2cap_conninfo connInfo, DeviceType type, std::string name,
			std::list<delayed_packet_t> delayedPackets);
private:
	bdaddr_t bdaddr;
	AddrType bdaddrType;

	struct l2cap_conninfo connInfo;
};

#endif /* INCLUDE_DEVICE_SOCKET_LEPERIPHERAL_H_ */
