/*
 * VirtualDevice.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "VirtualDevice.h"

#include <assert.h>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../ble/att.h"
#include "../ble/gatt.h"
#include "../ble/helper.h"
#include "../Beetle.h"
#include "../Debug.h"
#include "../Handle.h"
#include "../Router.h"
#include "../sync/Semaphore.h"
#include "../UUID.h"

VirtualDevice::VirtualDevice(Beetle &beetle) : Device(beetle) {
	started = false;
	stopped = false;
	currentTransaction = NULL;
	mtu = ATT_DEFAULT_LE_MTU;
	unfinishedClientTransactions = 0;
}

VirtualDevice::~VirtualDevice() {

}

static std::string discoverDeviceName(VirtualDevice *d);
static std::map<uint16_t, Handle *> discoverAllHandles(VirtualDevice *d);
void VirtualDevice::start() {
	if (debug) pdebug("starting");

	assert(started == false);
	started = true;

	startInternal();

	name = discoverDeviceName(this);
	std::map<uint16_t, Handle *> handlesTmp = discoverAllHandles(this);
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	handles = handlesTmp;
}

void VirtualDevice::startNd() {
	if (debug) pdebug("starting");

	assert(started == false);
	started = true;

	startInternal();
}


void VirtualDevice::stop() {
	if (debug) pdebug("stopping " + getName());

	assert(stopped == false);
	stopped = true;

	transactionMutex.lock();
	if (currentTransaction) {
		uint8_t *err;
		int len = pack_error_pdu(currentTransaction->buf[0], 0, ATT_ECODE_ABORTED, err);
		currentTransaction->cb(err, len);
		delete[] err;
		delete[] currentTransaction->buf;
		delete currentTransaction;
	}
	while (pendingTransactions.size() > 0) {
		transaction_t *t = pendingTransactions.front();
		uint8_t *err;
		int len = pack_error_pdu(t->buf[0], 0, ATT_ECODE_ABORTED, err);
		currentTransaction->cb(err, len);
		delete[] err;
		delete[] t->buf;
		delete t;
		pendingTransactions.pop();
	}
	transactionMutex.unlock();

	handlesMutex.lock();
	handles.clear();
	handlesMutex.unlock();
}

bool VirtualDevice::writeCommand(uint8_t *buf, int len) {
	return isStopped() || write(buf, len);
}

bool VirtualDevice::writeResponse(uint8_t *buf, int len) {
	return isStopped() || write(buf, len);
}

bool VirtualDevice::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
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

int VirtualDevice::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	if (isStopped()) {
		resp = NULL;
		return -1;
	}

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

int VirtualDevice::getMTU() {
	return mtu;
}

void VirtualDevice::handleTransactionResponse(uint8_t *buf, int len) {
	std::unique_lock<std::mutex> lk(transactionMutex);
	if (!currentTransaction) {
		// TODO what to do here?
		if (debug) {
			pdebug("unexpected transaction response when none existed!");
		}
		return;
	} else {
		transaction_t *t = currentTransaction;
		if (pendingTransactions.size() > 0) {
			currentTransaction = pendingTransactions.front();
			pendingTransactions.pop();
			assert(write(t->buf, t->len));
		} else {
			currentTransaction = NULL;
		}
		lk.unlock();

		t->cb(buf, len);
		delete[] t->buf;
		delete t;

	}
}

void VirtualDevice::readHandler(uint8_t *buf, int len) {
	if (buf[0] == ATT_OP_MTU_REQ) {
		mtu = btohs(*(uint16_t *)(buf + 1));
		uint8_t resp[3];
		resp[0] = ATT_OP_MTU_RESP;
		*(uint16_t *)(resp + 1) = htobs(ATT_DEFAULT_LE_MTU);
		write(resp, sizeof(resp));
	} else if (((buf[0] & 1) == 1 && buf[0] != ATT_OP_HANDLE_NOTIFY && buf[0] != ATT_OP_HANDLE_IND)
			|| buf[0] == ATT_OP_HANDLE_CNF) {
		handleTransactionResponse(buf, len);
	} else {
		beetle.router->route(buf, len, getId());
	}
}

static std::string discoverDeviceName(VirtualDevice *d) {
	uint8_t *req = NULL;
	uint8_t *resp = NULL;
	std::string name;
	int reqLen = pack_read_by_type_req_pdu(GATT_CHARAC_DEVICE_NAME, 0x1, 0xFFFF, req);
	int respLen = d->writeTransactionBlocking(req, reqLen, resp);
	if (resp == NULL || resp[0] != ATT_OP_READ_BY_TYPE_RESP) {
		name = "unknown";
	} else {
		char cName[respLen - 4 + 1];
		memcpy(cName, resp + 4, respLen - 4);
		cName[respLen - 4] = '\0';
		name = std::string(cName);
	}

	if (debug) pdebug(name);

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

static std::vector<group_t> discoverServices(VirtualDevice *d) {
	if (debug) {
		pdebug("discovering services for " + d->getName());
	}

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
			throw DeviceException("error on transaction");
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
				if (debug) pdebug("found service at handles " + std::to_string(group.handle) + " - " + std::to_string(group.endGroup));
			}
			delete[] resp;
			currHandle = groups.rbegin()->endGroup + 1;
			if (currHandle == 0) break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_GROUP_REQ
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw DeviceException("unexpected transaction");
		}
	}
	return groups;
}

typedef struct {
	uint16_t handle;
	uint8_t *value;
	int len;
} handle_value_t;

static std::vector<handle_value_t> discoverCharacterisics(VirtualDevice *d, uint16_t startHandle, uint16_t endHandle) {
	if (debug) {
		pdebug("discovering characteristics for " + d->getName());
	}

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
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_READ_BY_TYPE_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				handle_value_t handleValue;
				handleValue.handle = btohs(*(uint16_t *)(resp + i));
				handleValue.len = attDataLen - 2;
				handleValue.value = new uint8_t[handleValue.len];
				memcpy(handleValue.value, resp + i + 2, handleValue.len);
				handles.push_back(handleValue);
				if (debug) pdebug("found characteristic at handle " + std::to_string(handleValue.handle));
			}
			delete[] resp;

			currHandle = handles.rbegin()->handle + 1;
			if (currHandle == 0 || currHandle > endHandle) break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_TYPE_REQ
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw DeviceException("unexpected transaction");
		}
	}
	return handles;
}

typedef struct {
	uint8_t format;
	uint16_t handle;
	uuid_t uuid;
} handle_info_t;

static std::vector<handle_info_t> discoverHandles(VirtualDevice *d, uint16_t startGroup, uint16_t endGroup) {
	if (debug) {
		pdebug("discovering handles for " + d->getName());
	}

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
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_FIND_INFO_RESP) {
			uint8_t format = resp[1];
			int attDataLen = (format == ATT_FIND_INFO_RESP_FMT_16BIT) ? 4 : 18;
			for (int i = 2; i < respLen; i += attDataLen) {
				handle_info_t handleInfo;
				handleInfo.format = format;
				handleInfo.handle = btohs(*(uint16_t *)(resp + i));
				if (format == ATT_FIND_INFO_RESP_FMT_16BIT) {
					memset(handleInfo.uuid.value, 0, 2);
					memcpy(handleInfo.uuid.value + 2, resp + i + 2, attDataLen - 2);
					memcpy(handleInfo.uuid.value + 4, BLUETOOTH_BASE_UUID, 12);
				} else {
					memcpy(handleInfo.uuid.value, resp + i + 2, attDataLen - 2);
				}
				handles.push_back(handleInfo);
				if (debug) pdebug("found handle at " + std::to_string(handleInfo.handle));
			}
			delete[] resp;

			currHandle = handles.rbegin()->handle + 1;
			if (currHandle == 0 || currHandle >= endGroup) break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_FIND_INFO_REQ
				&& resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			delete[] resp;
			break;
		} else {
			delete[] resp;
			throw DeviceException("unexpected transaction");
		}
	}
	return handles;
}

static std::map<uint16_t, Handle *> discoverAllHandles(VirtualDevice *d) {
	std::map<uint16_t, Handle *> handles;

	Handle *lastService = NULL;
	Handle *lastCharacteristic = NULL;

	std::vector<group_t> services = discoverServices(d);
	for (group_t &service : services) {
		Handle *serviceHandle = new PrimaryService();
		serviceHandle->setHandle(service.handle);
		serviceHandle->setEndGroupHandle(service.endGroup);
		serviceHandle->setCacheInfinite(true);
		// let the handle inherit the pointer
		serviceHandle->cache.set(service.value, service.len);
		handles[service.handle] = serviceHandle;

		std::vector<handle_value_t> characteristics = discoverCharacterisics(d,
				serviceHandle->getHandle(),
				serviceHandle->getEndGroupHandle());
		for (handle_value_t &characteristic : characteristics) {
			Handle *charHandle = new Characteristic();
			charHandle->setHandle(characteristic.handle);
			charHandle->setServiceHandle(serviceHandle->getHandle());
			charHandle->setCacheInfinite(true);
			// let the handle inherit the pointer
			charHandle->cache.set(characteristic.value, characteristic.len);
			handles[characteristic.handle] = charHandle;

			// save in case it is the last
			lastCharacteristic = charHandle;
		}

		if (characteristics.size() > 0) {
			for (int i = 0; i < (int)(characteristics.size() - 1); i++) {
				handle_value_t characteristic = characteristics[i];
				uint16_t startGroup = characteristic.handle + 1;
				uint16_t endGroup = characteristics[i + 1].handle - 1;
				handles[characteristic.handle]->setEndGroupHandle(endGroup);

				std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
				for (handle_info_t &handleInfo : handleInfos) {
					UUID handleUuid = UUID(handleInfo.uuid);
					Handle *handle;
					if (handleUuid.isShort() && handleUuid.getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
						handle = new ClientCharCfg();
					} else {
						handle = new Handle();
						handle->setUuid(handleUuid);
					}
					handle->setHandle(handleInfo.handle);
					handle->setCacheInfinite(false);
					handle->setServiceHandle(serviceHandle->getHandle());
					handle->setCharHandle(characteristic.handle);
					handles[handleInfo.handle] = handle;
				}
			}

			handle_value_t characteristic = characteristics[characteristics.size() - 1];
			uint16_t startGroup = characteristic.handle + 1;
			uint16_t endGroup = serviceHandle->getEndGroupHandle();
			handles[characteristic.handle]->setEndGroupHandle(endGroup);

			std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
			for (handle_info_t &handleInfo : handleInfos) {
				UUID handleUuid = UUID(handleInfo.uuid);
				Handle *handle;
				if (handleUuid.isShort() && handleUuid.getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
					handle = new ClientCharCfg();
				} else {
					handle = new Handle();
					handle->setUuid(handleUuid);
				}
				handle->setHandle(handleInfo.handle);
				handle->setCacheInfinite(false);
				handle->setServiceHandle(serviceHandle->getHandle());
				handle->setCharHandle(characteristic.handle);
				handles[handleInfo.handle] = handle;
			}
		}

		// save in case it is the last
		lastService = serviceHandle;
	}

	if (lastService) {
		handles[lastService->getHandle()]->setEndGroupHandle(handles.rbegin()->second->getHandle());
	}
	if (lastCharacteristic) {
		handles[lastCharacteristic->getHandle()]->setEndGroupHandle(handles.rbegin()->second->getHandle());
	}

	if (debug) {
		pdebug("done discovering handles for " + d->getName());
	}
	return handles;
}

