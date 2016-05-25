/*
 * VirtualDevice.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef VIRTUALDEVICE_H_
#define VIRTUALDEVICE_H_

#include <stddef.h>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

#include "BeetleTypes.h"
#include "Device.h"
#include "UUID.h"

// Debug the GATT handle discovery.
extern bool debug_discovery;

typedef struct {
	uint8_t *buf;
	int len;
	std::function<void(uint8_t*, int)> cb;
} transaction_t;

/*
 * Base class that implements GATT and asynchronous transactions.
 */
class VirtualDevice: public Device {
public:
	virtual ~VirtualDevice();

	void start();
	void startNd();
	void stop();
	bool isStopped();

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);
	int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp);

	int getMTU();

	std::vector<uint64_t> getTransactionLatencies();
protected:
	/*
	 * Cannot instantiate a VirtualDevice
	 */
	VirtualDevice(Beetle &beetle, bool isEndpoint, HandleAllocationTable *hat = NULL);

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

	bool isEndpoint;

	int mtu;

	/*
	 * Number of unfinished client transactions. Only ever accessed
	 * by the thread that calls readHandler().
	 * TODO: use this to detect misbehaving clients
	 */
	int unfinishedClientTransactions;
	long lastTransactionMillis;

	/*
	 * Server transactions
	 */
	void handleTransactionResponse(uint8_t *buf, int len);
	transaction_t *currentTransaction;
	std::queue<transaction_t *> pendingTransactions;
	std::vector<uint64_t> transactionLatencies;
	std::mutex transactionMutex;

	/*
	 * Helper methods
	 */
	void discoverNetworkServices(UUID serviceUuid);
};

#endif /* VIRTUALDEVICE_H_ */