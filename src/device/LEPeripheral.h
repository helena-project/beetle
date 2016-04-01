/*
 * Peripheral.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef LEPERIPHERAL_H_
#define LEPERIPHERAL_H_

#include <bluetooth/bluetooth.h>
#include <cstdint>
#include <string>
#include <thread>

#include "../sync/BlockingQueue.h"
#include "Device.h"
#include "shared.h"

enum AddrType {
	PUBLIC, RANDOM,
};

class LEPeripheral: public Device {
public:
	LEPeripheral(Beetle &beetle, bdaddr_t addr, AddrType addrType);
	virtual ~LEPeripheral();
	std::string getName();
	int getMTU();
protected:
	bool write(uint8_t *buf, int len);
private:
	bdaddr_t bdaddr;
	AddrType bdaddrType;

	int sockfd;

	std::thread readThread;
	std::thread writeThread;

	BlockingQueue<queued_write_t> writeQueue;

	void readDaemon();
	void writeDaemon();
};

#endif /* LEPERIPHERAL_H_ */
