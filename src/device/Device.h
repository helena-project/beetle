/*
 * Device.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>

#include "../Beetle.h"
#include "../Handle.h"
#include "../sync/Semaphore.h"

typedef struct {
	uint8_t *buf;
	int len;
	std::function<void(uint8_t*, int)> cb;
} transaction_t;

class Handle;

class Device {
public:
	Device(Beetle &beetle);
	virtual ~Device();

	device_t getId() { return id; };
	std::string getName() { return name; };

	void start();
	void stop();
	bool isStopped() { return !running; };

	int getHighestHandle();
	std::map<uint16_t, Handle *> handles;
	std::recursive_mutex handlesMutex;

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);
	int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp);

	virtual int getMTU() = 0;
protected:
	/*
	 * Called by derived class when a packet is received.
	 */
	void readHandler(uint8_t *buf, int len);
	/*
	 * Called by base class to write packet.
	 */
	virtual bool write(uint8_t *buf, int len) = 0;
private:
	Beetle &beetle;

	bool running;

	device_t id;
	std::string name;

	/*
	 * Server transactions
	 */
	void handleTransactionResponse(uint8_t *buf, int len);
	transaction_t *currentTransaction;
	std::queue<transaction_t *> pendingTransactions;
	std::mutex transactionMutex;

	/*
	 * Client transactions
	 */
	Semaphore transactionSemaphore;

	static std::atomic_int idCounter;

};

#endif /* DEVICE_H_ */
