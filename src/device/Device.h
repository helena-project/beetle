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
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>

#include "../Beetle.h"
#include "../data/Semaphore.h"
#include "../Handle.h"

typedef struct {
	uint8_t *buf;
	int len;
	std::function<void(uint8_t*, int)> cb;
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

	int getHighestHandle();
	std::map<uint16_t, Handle *> &getHandles();
	std::recursive_mutex handlesMutex;

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);

	virtual int getMTU() = 0;
protected:
	virtual void startInternal() = 0;
	void handleRead(uint8_t *buf, int len);
	virtual bool write(uint8_t *buf, int len) = 0;
private:
	Beetle &beetle;

	bool running;

	device_t id;
	std::string name;

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
