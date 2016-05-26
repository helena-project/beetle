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
#include "device/VirtualDevice.h"

class LEPeripheral: public VirtualDevice {
public:
	enum AddrType {
		PUBLIC, RANDOM,
	};

	LEPeripheral(Beetle &beetle, bdaddr_t addr, AddrType addrType);
	virtual ~LEPeripheral();

	bdaddr_t getBdaddr();
	AddrType getAddrType();
	struct l2cap_conninfo getL2capConnInfo();
protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	bdaddr_t bdaddr;
	AddrType bdaddrType;

	int sockfd;

	struct l2cap_conninfo connInfo;

	std::atomic_bool stopped;
	void stopInternal();

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* INCLUDE_DEVICE_SOCKET_LEPERIPHERAL_H_ */
