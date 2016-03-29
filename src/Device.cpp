/*
 * Device.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "Device.h"

#include <assert.h>
#include <stdlib.h>
#include <cstdbool>
#include <queue>
#include <string>

#include "ble/att.h"
#include "Router.h"

Device::Device(Beetle &beetle, std::string name) {
	Device::beetle = beetle;
	Device::name = name;
	id = idCounter++;

	handleOffset = -1;
	highestHandle = 0;

	running = true;

	currentTransaction = NULL;

	transactionSemaphore(1);
}

Device::~Device() {
	transactionSemaphore.wait(); // wait for client transaction to finish
}

void Device::start() {

}

void Device::stop() {
	assert(running);
	running = false;

	transactionMutex.lock();
	if (currentTransaction) {
		// TODO abort
		free(currentTransaction->buf);
		delete currentTransaction;
	}
	while (pendingTransactions.size() > 0) {
		transaction_t *t = pendingTransactions.front();
		// TODO abort
		free(t->buf);
		delete t;
		pendingTransactions.pop();
	}
	transactionMutex.unlock();

	//	TODO doCleanup;
}

bool Device::writeCommand(char *buf, int len) {
	return isStopped() || write(buf, len);
}

bool Device::writeResponse(char *buf, int len) {
	return isStopped() || write(buf, len);
}

bool Device::writeTransaction(char *buf, int len, TransactionCallback cb) {
	if (isStopped) {
		return false;
	}

	transaction_t *t = new transaction_t;
	t->buf = malloc(len);
	memcpy(t->buf, buf, len);
	t->len = len;
	t->cb = TransactionCallback;

	std::lock_guard<std::mutex> lg(transactionMutex);
	if (currentTransaction == NULL) {
		currentTransaction = t;
		assert(write(t->buf, t->len));
	} else {
		pendingTransactions.push(t);
	}
	return true;
}

void Device::handleTransactionResponse(char *buf, int len) {
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

void Device::handleRead(char *buf, int len) {
	if (((buf[0] & 1 == 1) && buf[0] != ATT_OP_HANDLE_NOTIFY && buf[0] != ATT_OP_HANDLE_IND)
			|| buf[0] == ATT_OP_HANDLE_CNF)
	{
		handleTransactionResponse(buf, len);
	} else {
		beetle.router.route(buf, len, id);
	}
}

