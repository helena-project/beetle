/*
 * LEPeripheral.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_LEPERIPHERAL_H_
#define DEVICE_LEPERIPHERAL_H_

#include <bluetooth/bluetooth.h>
#include <cstdint>
#include <thread>

#include "../Beetle.h"
#include "../sync/Countdown.h"
#include "VirtualDevice.h"

enum AddrType {
	PUBLIC, RANDOM,
};

class LEPeripheral: public VirtualDevice {
public:
	LEPeripheral(Beetle &beetle, bdaddr_t addr, AddrType addrType);
	virtual ~LEPeripheral();

	bdaddr_t getBdaddr() { return bdaddr; };
	AddrType getAddrType() { return bdaddrType; };
protected:
	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	bdaddr_t bdaddr;
	AddrType bdaddrType;

	int sockfd;

	std::thread readThread;
	void readDaemon();

	Countdown pendingWrites;
};

#endif /* DEVICE_LEPERIPHERAL_H_ */
