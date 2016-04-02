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
#include <vector>

#include "../ble/att.h"
#include "../ble/gatt.h"
#include "../ble/helper.h"
#include "../Router.h"

std::atomic_int Device::idCounter(1);

Device::Device(Beetle &beetle_) : beetle(beetle_), transactionSemaphore{1} {
	id = idCounter++;

	running = true;

	currentTransaction = NULL;
}

Device::~Device() {
	transactionSemaphore.wait(); // wait for client transaction to finish
}

static std::string discoverDeviceName(Device *d);
static std::map<uint16_t, Handle *> discoverAllHandles(Device *d);
void Device::start() {
	name = discoverDeviceName(this);
	std::map<uint16_t, Handle *> handlesTmp = discoverAllHandles(this);
	handlesMutex.lock();
	handles = handlesTmp;
	handlesMutex.unlock();
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

int Device::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	Semaphore sema(0);
	int respLen;
	bool success = writeTransaction(buf, len, [&sema, &resp, &respLen](uint8_t *resp_, int respLen_) -> void {
		resp = new uint8_t[respLen_];
		memcpy(resp, resp_, respLen_);
		respLen = respLen_;
		sema.notify();
	});
	if (!success) {
		resp = NULL;
		return -1;
	} else {
		sema.wait();
		return respLen;
	}
}

void Device::handleTransactionResponse(uint8_t *buf, int len) {
	std::unique_lock<std::mutex> lk(transactionMutex);
	if (!currentTransaction) {
		// TODO
		return;
	} else {
		transaction_t *t = currentTransaction;
		if (pendingTransactions.size() > 0) {
			currentTransaction = pendingTransactions.front();
			pendingTransactions.pop();
			assert(write(t->buf, t->len));
		}
		lk.unlock();

		t->cb(buf, len);
		delete[] t->buf;
		delete t;
	}
}

void Device::readHandler(uint8_t *buf, int len) {
	if (((buf[0] & 1) == 1 && buf[0] != ATT_OP_HANDLE_NOTIFY && buf[0] != ATT_OP_HANDLE_IND)
			|| buf[0] == ATT_OP_HANDLE_CNF) {
		handleTransactionResponse(buf, len);
	} else {
		beetle.router->route(buf, len, id);
	}
}

int Device::getHighestHandle() {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	if (handles.size() > 0) {
		return handles.rbegin()->first;
	} else {
		return -1;
	}
}

static std::string discoverDeviceName(Device *d) {
	uint8_t *req = NULL;
	uint8_t *resp = NULL;
	std::string name;
	int reqLen = pack_read_by_type_req_pdu(GATT_CHARAC_DEVICE_NAME, 0x1, 0xFFFF, req);
	int respLen = d->writeTransactionBlocking(req, reqLen, resp);
	if (resp == NULL && resp[0] != ATT_OP_READ_BY_TYPE_RESP) {
		name = "unknown";
	} else {
		char cName[respLen - 4 + 1];
		memcpy(resp + 4, cName, respLen - 4);
		cName[respLen - 4] = '\0';
		name = std::string(cName);
	}
	delete[] req;
	delete[] resp;
	return name;
}

typedef struct {
	uint16_t handle;
	uint16_t endGroup;
	uint8_t *value;
	int len;
} group_t;

static std::vector<group_t> discoverServices(Device *d) {
	uint16_t startHandle = 1;
	uint16_t endHandle = 0xFFFF;

	int reqLen = 7;
	uint8_t req[reqLen];
	req[0] = ATT_OP_READ_BY_GROUP_REQ;
	*(uint16_t *)(req + 3) = htobs(endHandle);
	*(uint16_t *)(req + 5) = htobs(GATT_PRIM_SVC_UUID);

	std::vector<group_t> groups;
	uint16_t currHandle = startHandle;
	while (true) {
		*(uint16_t *)(req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		if (resp == NULL) {
			throw "error on transaction";
		}

		if (resp[0] == ATT_OP_READ_BY_GROUP_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				group_t group;
				group.handle = btohs(*(uint16_t *)(resp + i));
				group.endGroup = btohs(*(uint16_t *)(resp + i + 2));
				group.len = attDataLen - 4;
				group.value = new uint8_t[group.len];
				memcpy(group.value, resp + i + 4, group.len);
				groups.push_back(group);
			}
			currHandle = groups.rbegin()->endGroup + 1;
			delete[] resp;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_GROUP_REQ
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw "unexpected transaction";
		}
	}
	return groups;
}

typedef struct {
	uint16_t handle;
	uint8_t *value;
	int len;
} handle_value_t;

static std::vector<handle_value_t> discoverCharacterisics(Device *d, uint16_t startHandle, uint16_t endHandle) {
	std::vector<handle_value_t> handles;

	int reqLen = 7;
	uint8_t req[reqLen];
	req[0] = ATT_OP_READ_BY_TYPE_REQ;
	*(uint16_t *)(req + 3) = htobs(endHandle);
	*(uint16_t *)(req + 5) = htobs(GATT_CHARAC_UUID);

	uint16_t currHandle = startHandle;
	while (true) {
		*(uint16_t *)(req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		if (resp == NULL) {
			throw "error on transaction";
		}

		if (resp[0] == ATT_OP_READ_BY_TYPE_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				handle_value_t hv;
				hv.handle = btohs(*(uint16_t *)(resp + i));
				hv.len = attDataLen - 2;
				hv.value = new uint8_t[hv.len];
				memcpy(hv.value, resp + i + 2, hv.len);
				handles.push_back(hv);
			}
			currHandle = handles.rbegin()->handle + 1;
			delete[] resp;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_TYPE_RESP
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw "unexpected transaction";
		}
	}
	return handles;
}

typedef struct {
	uint8_t format;
	uint16_t handle;
	uint8_t *value;
	int len;
	UUID uuid;
} handle_info_t;

static std::vector<handle_info_t> discoverHandles(Device *d, uint16_t startGroup, uint16_t endGroup) {
	std::vector<handle_info_t> handles;

	int reqLen = 5;
	uint8_t req[reqLen];
	req[0] = ATT_OP_FIND_INFO_REQ;
	*(uint16_t *)(req + 3) = htobs(endGroup);

	uint16_t currHandle = startGroup;
	while (true) {
		*(uint16_t *)(req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		if (resp == NULL) {
			throw "error on transaction";
		}

		if (resp[0] == ATT_OP_FIND_INFO_RESP) {
			uint8_t format = resp[0];
			int attDataLen = (format == ATT_FIND_INFO_RESP_FMT_16BIT) ? 4 : 18;
			for (int i = 2; i < respLen; i += attDataLen) {
				handle_info_t handleInfo;
				handleInfo.format = format;
				handleInfo.handle = btohs(*(uint16_t *)(resp + i));
				handleInfo.len = attDataLen - 2;
				handleInfo.value = new uint8_t[handleInfo.len];
				memcpy(handleInfo.value, resp + i + 2, handleInfo.len);
				handleInfo.uuid = UUID(handleInfo.value, handleInfo.len);
				handles.push_back(handleInfo);
			}
			currHandle = handles.rbegin()->handle + 1;
			if (currHandle >= endGroup) {
				break;
			}
			delete[] resp;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_TYPE_RESP
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw "unexpected transaction";
		}
	}
	return handles;
}

static std::map<uint16_t, Handle *> discoverAllHandles(Device *d) {
	std::map<uint16_t, Handle *> handles;

	std::vector<group_t> services = discoverServices(d);
	for (group_t &service : services) {
		Handle *serviceHandle = new Handle();
		serviceHandle->setHandle(service.handle);
		serviceHandle->setUuid(UUID(GATT_PRIM_SVC_UUID));
		serviceHandle->setEndGroupHandle(service.endGroup);
		serviceHandle->setCacheInfinite(true);
		// let the handle inherit the pointer
		serviceHandle->getCache().set(service.value, service.len);
		handles[service.handle] = serviceHandle;

		std::vector<handle_value_t> characteristics = discoverCharacterisics(d, serviceHandle->getHandle(), serviceHandle->getEndGroupHandle());
		for (handle_value_t &characteristic : characteristics) {
			Handle *charHandle = new Handle();
			charHandle->setHandle(characteristic.handle);
			charHandle->setUuid(UUID(GATT_CHARAC_UUID));
			charHandle->setServiceHandle(serviceHandle->getHandle());
			charHandle->setCharHandle(btohs(*(uint16_t *)characteristic.value));
			charHandle->setCacheInfinite(true);
			// let the handle inherit the pointer
			charHandle->getCache().set(characteristic.value, characteristic.len);
			handles[characteristic.handle] = charHandle;
		}

		for (int i = 0; i + 1 < characteristics.size(); i++) {
			handle_value_t characteristic = characteristics[i];
			uint16_t startGroup = characteristic.handle;
			uint16_t endGroup = characteristics[i + i].handle;
			handles[characteristic.handle]->setEndGroupHandle(endGroup);

			std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
			for (handle_info_t &handleInfo : handleInfos) {
				Handle *handle = new Handle();
				handle->setHandle(handleInfo.handle);
				handle->setUuid(handleInfo.uuid);
				handle->setCacheInfinite(false);
				handle->setServiceHandle(serviceHandle->getHandle());
				handle->setCharHandle(characteristic.handle);
				handles[handleInfo.handle] = handle;
			}
		}

		if (characteristics.size() > 0) {
			handle_value_t characteristic = characteristics[characteristics.size() - 1];
			uint16_t startGroup = characteristic.handle;
			uint16_t endGroup = serviceHandle->getEndGroupHandle();
			handles[characteristic.handle]->setEndGroupHandle(endGroup);

			std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
			for (handle_info_t &handleInfo : handleInfos) {
				Handle *handle = new Handle();
				handle->setHandle(handleInfo.handle);
				handle->setUuid(handleInfo.uuid);
				handle->setCacheInfinite(false);
				handle->setServiceHandle(serviceHandle->getHandle());
				handle->setCharHandle(characteristic.handle);
				handles[handleInfo.handle] = handle;
			}
		}
	}

	return handles;
}
