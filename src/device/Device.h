/*
 * Device.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#include <atomic>
#include <cstdbool>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>

#include "../Beetle.h"
#include "../data/Semaphore.h"
#include "../Handle.h"

typedef void (*TransactionCallback)(uint8_t *, int);

typedef struct {
	uint8_t *buf;
	int len;
	TransactionCallback cb;
} transaction_t;

class Handle;

class Device {
public:
	Device(Beetle &beetle, std::string name);
	virtual ~Device();
	std::string getName() { return name; }
	device_t getId() { return id; };

	void start();
	void stop();
	bool isStopped() { return !running; };

	std::set<int> getSubscribers();
	std::map<uint16_t, Handle *> getHandles();
	int getHighestHandle();

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, TransactionCallback);

protected:
	virtual void startInternal() = 0;
	void handleRead(uint8_t *buf, int len);
	virtual bool write(uint8_t *buf, int len) = 0;
private:
	Beetle &beetle;

	bool running;

	device_t id;
	std::string name;

	int highestHandle;
	std::map<uint16_t, Handle *> handles;

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
