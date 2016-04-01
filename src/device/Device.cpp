/*
 * Device.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Device.h"

#include <assert.h>
#include <stdlib.h>
#include <cstring>

#include "../ble/att.h"
#include "../Router.h"

std::atomic_int Device::idCounter(1);

Device::Device(Beetle &beetle_, std::string name_) : beetle(beetle_), transactionSemaphore{1} {
	name = name_;
	id = idCounter++;

	running = true;

	currentTransaction = NULL;
}

Device::~Device() {
	transactionSemaphore.wait(); // wait for client transaction to finish
}

void Device::start() {
	startInternal();
}

void Device::stop() {
	assert(running);
	running = false;

	transactionMutex.lock();
	if (currentTransaction) {
		// TODO abort
		delete[] currentTransaction->buf;
		delete currentTransaction;
	}
	while (pendingTransactions.size() > 0) {
		transaction_t *t = pendingTransactions.front();
		// TODO abort
		delete[] t->buf;
		delete t;
		pendingTransactions.pop();
	}
	transactionMutex.unlock();

	//	TODO doCleanup;
}

bool Device::writeCommand(uint8_t *buf, int len) {
	return isStopped() || write(buf, len);
}

bool Device::writeResponse(uint8_t *buf, int len) {
	return isStopped() || write(buf, len);
}

bool Device::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	if (isStopped()) {
		return false;
	}

	transaction_t *t = new transaction_t;
	t->buf = new uint8_t[len];
	memcpy(t->buf, buf, len);
	t->len = len;
	t->cb = cb;

	std::lock_guard<std::mutex> lg(transactionMutex);
	if (currentTransaction == NULL) {
		currentTransaction = t;
		assert(write(t->buf, t->len));
	} else {
		pendingTransactions.push(t);
	}
	return true;
}

void Device::handleTransactionResponse(uint8_t *buf, int len) {
	transactionMutex.lock();
	if (!currentTransaction) {
		// TODO
		transactionMutex.unlock();
		return;
	} else {
		transaction_t *t = currentTransaction;
		if (pendingTransactions.size() > 0) {
			currentTransaction = pendingTransactions.front();
			pendingTransactions.pop();
			assert(write(t->buf, t->len));
		}
		transactionMutex.unlock();

		t->cb(buf, len);
		free(t->buf);
		delete t;
	}
}

void Device::handleRead(uint8_t *buf, int len) {
	if (((buf[0] & 1) == 1 && buf[0] != ATT_OP_HANDLE_NOTIFY && buf[0] != ATT_OP_HANDLE_IND)
			|| buf[0] == ATT_OP_HANDLE_CNF) {
		handleTransactionResponse(buf, len);
	} else {
		beetle.router->route(buf, len, id);
	}
}

int Device::getHighestHandle() {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	return handles.rbegin()->first;
}

std::map<uint16_t, Handle *> &Device::getHandles() {
	return handles;
}
