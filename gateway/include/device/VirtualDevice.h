/*
 * VirtualDevice.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef VIRTUALDEVICE_H_
#define VIRTUALDEVICE_H_

#include <boost/shared_array.hpp>
#include <stddef.h>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>
#include <memory>

#include "BeetleTypes.h"
#include "Device.h"
#include "UUID.h"

/*
 * Base class that implements GATT and asynchronous transactions.
 */
class VirtualDevice: public Device {
public:
	virtual ~VirtualDevice();

	/*
	 * Starts up the virtual device to begin processing requests.
	 */
	void start(bool discoverHandles = true);

	void writeResponse(uint8_t *buf, int len);
	void writeCommand(uint8_t *buf, int len);
	void writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);
	int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp);

	int getMTU();

	virtual bool isLive();

	int getHighestForwardedHandle();

	/*
	 * Get the latencies in ms for the latest transactions.
	 */
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
	bool isEndpoint;

	int mtu;

	time_t connectedTime;

	int highestForwardedHandle;

	/*
	 * Number of unfinished client transactions. Only ever accessed
	 * by the thread that calls readHandler().
	 *
	 * TODO: use this to detect misbehaving clients
	 */
	int unfinishedClientTransactions;
	long lastTransactionMillis;

	/*
	 * Server transactions
	 */
	typedef struct {
		boost::shared_array<uint8_t> buf;
		int len;
		time_t time;
		std::function<void(uint8_t*, int)> cb;
	} transaction_t;

	void handleTransactionResponse(uint8_t *buf, int len);
	std::shared_ptr<transaction_t> currentTransaction;
	std::queue<std::shared_ptr<transaction_t>> pendingTransactions;
	std::vector<uint64_t> transactionLatencies;
	std::mutex transactionMutex;

	/*
	 * Helper methods
	 */
	void discoverNetworkServices(UUID serviceUuid);
	void setupBeetleService(int handleAlloc);

	/* Timeout transaction and shutdown the device. */
	static constexpr int ASYNC_TRANSACTION_TIMEOUT = 60;

	/* Timeout blocking transaction, but not shutdown. */
	static constexpr int BLOCKING_TRANSACTION_TIMEOUT = 5;

	/* Number of datapoints to buffer. */
	static constexpr int MAX_TRANSACTION_LATENCIES = 20;
};

#endif /* VIRTUALDEVICE_H_ */
