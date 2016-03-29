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

#include "Beetle.h"
#include "Handle.h"
#include "data/Semaphore.h"

typedef int device_t;

typedef void (*TransactionCallback)(char *, int);

typedef struct {
	char *buf;
	int len;
	TransactionCallback cb;
} transaction_t;

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
	std::map<uint16_t, Handle> getHandles();
	int getHandleOffset();
	int getHighestHandle();

	bool writeResponse(char *buf, int len);
	bool writeCommand(char *buf, int len);
	bool writeTransaction(char *buf, int len, TransactionCallback);

protected:
	void handleRead(char *buf, int len);
	virtual bool write(char *buf, int len);
private:
	Beetle beetle;

	bool running;

	device_t id;
	std::string name;

	int handleOffset;
	int highestHandle;
	std::map<uint16_t, Handle> handles;

	/*
	 * Server transactions
	 */
	void handleTransactionResponse(char *buf, int len);
	transaction_t *currentTransaction;
	std::queue<transaction_t *> pendingTransactions;
	std::mutex transactionMutex;

	/*
	 * Client transactions
	 */
	Semaphore transactionSemaphore;

	static std::atomic_int idCounter = 0;
};

#endif /* DEVICE_H_ */
