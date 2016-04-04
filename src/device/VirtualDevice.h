/*
 * VirtualDevice.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef VIRTUALDEVICE_H_
#define VIRTUALDEVICE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>

#include "Device.h"

typedef struct {
	uint8_t *buf;
	int len;
	std::function<void(uint8_t*, int)> cb;
} transaction_t;

class VirtualDevice: public Device {
public:
	VirtualDevice(Beetle &beetle);
	virtual ~VirtualDevice();

	void start();
	void startNd();
	void stop();
	bool isStopped() { return stopped; };

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);
	int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp);
protected:
	/*
	 * Called by derived class when a packet is received.
	 */
	void readHandler(uint8_t *buf, int len);

	/*
	 * Called by base class to write packet.
	 */

	virtual bool write(uint8_t *buf, int len) = 0;
	/*
	 * Called at the beginning of start() to start the internals (any daemons).
	 */

	virtual void startInternal() = 0;
private:
	bool stopped;
	bool started;

	/*
	 * Server transactions
	 */
	void handleTransactionResponse(uint8_t *buf, int len);
	transaction_t *currentTransaction;
	std::queue<transaction_t *> pendingTransactions;
	std::mutex transactionMutex;

	static std::atomic_int idCounter;
};

#endif /* VIRTUALDEVICE_H_ */
