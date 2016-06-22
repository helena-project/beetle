/*
 * VirtualDevice.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "device/VirtualDevice.h"

#include <assert.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <time.h>

#include "Beetle.h"
#include "ble/att.h"
#include "ble/beetle.h"
#include "ble/gatt.h"
#include "ble/helper.h"
#include "controller/NetworkDiscoveryClient.h"
#include "Debug.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "device/socket/LEPeripheral.h"
#include "Handle.h"
#include "Router.h"
#include "sync/Semaphore.h"
#include "UUID.h"

static uint64_t getCurrentTimeMillis(void) {
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	return 1000 * spec.tv_sec + round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
}

VirtualDevice::VirtualDevice(Beetle &beetle, bool isEndpoint_, HandleAllocationTable *hat) :
		Device(beetle, hat) {
	isEndpoint = isEndpoint_;
	currentTransaction = NULL;
	mtu = ATT_DEFAULT_LE_MTU;
	unfinishedClientTransactions = 0;
	lastTransactionMillis = 0;
	highestForwardedHandle = -1;
	connectedTime = time(NULL);
}

VirtualDevice::~VirtualDevice() {
	transactionMutex.lock();
	if (currentTransaction != NULL) {
		uint8_t err[ATT_ERROR_PDU_LEN];
		pack_error_pdu(currentTransaction->buf.get()[0], 0, ATT_ECODE_ABORTED, err);
		currentTransaction->cb(err, ATT_ERROR_PDU_LEN);
		currentTransaction.reset();
	}
	while (pendingTransactions.size() > 0) {
		auto t = pendingTransactions.front();
		pendingTransactions.pop();

		uint8_t err[ATT_ERROR_PDU_LEN];
		pack_error_pdu(t->buf.get()[0], 0, ATT_ECODE_ABORTED, err);
		t->cb(err, ATT_ERROR_PDU_LEN);
	}
	transactionMutex.unlock();
}

static std::map<uint16_t, std::shared_ptr<Handle>> discoverAllHandles(VirtualDevice *d);

void VirtualDevice::start(bool discoverHandles) {
	if (debug) {
		pdebug("starting");
	}
	startInternal();
	if (discoverHandles) {
		/* May throw device exception */
		std::map<uint16_t, std::shared_ptr<Handle>> handlesTmp = discoverAllHandles(this);

		handlesMutex.lock();
		handles = handlesTmp;
		handlesMutex.unlock();
	}

	highestForwardedHandle = getHighestHandle();

	if (isEndpoint) {
		setupBeetleService(highestForwardedHandle + 1);
	}

	beetle.updateDevice(getId());
}

std::vector<uint64_t> VirtualDevice::getTransactionLatencies() {
	std::lock_guard<std::mutex> lg(transactionMutex);
	auto ret = transactionLatencies;
	transactionLatencies.clear();
	return ret;
}

void VirtualDevice::writeCommand(uint8_t *buf, int len) {
	assert(buf);
	assert(len > 0);
	assert(!is_att_response(buf[0]) && !is_att_request(buf[0]) && buf[0] != ATT_OP_HANDLE_IND
			&& buf[0] != ATT_OP_HANDLE_CNF);

	write(buf, len);
}

void VirtualDevice::writeResponse(uint8_t *buf, int len) {
	assert(buf);
	assert(len > 0);
	assert(is_att_response(buf[0]) || buf[0] == ATT_OP_HANDLE_CNF || buf[0] == ATT_OP_ERROR);

	write(buf, len);
}

void VirtualDevice::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	assert(buf);
	assert(len > 0);
	assert(is_att_request(buf[0]) || buf[0] == ATT_OP_HANDLE_IND);

	auto t = std::make_shared<transaction_t>();
	t->buf.reset(new uint8_t[len]);
	memcpy(t->buf.get(), buf, len);
	t->len = len;
	t->cb = cb;
	t->time = time(NULL);

	std::lock_guard<std::mutex> lg(transactionMutex);
	if (currentTransaction == NULL) {
		currentTransaction = t;
		lastTransactionMillis = getCurrentTimeMillis();
		if (!write(t->buf.get(), t->len)) {
			cb(NULL, -1);
		}
	} else {
		pendingTransactions.push(t);
	}
}

int VirtualDevice::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	auto sema = std::make_shared<Semaphore>(0);
	auto respLenPtr = std::make_shared<int>();
	auto respPtr = std::make_shared<boost::shared_array<uint8_t>>();
	writeTransaction(buf, len, [sema, respPtr, respLenPtr](uint8_t *resp_, int respLen_) {
		if (resp_ != NULL && respLen_ > 0) {
			respPtr->reset(new uint8_t[respLen_]);
			memcpy(respPtr.get()->get(), resp_, respLen_);
		}
		*respLenPtr = respLen_;
		sema->notify();
	});

	if (sema->try_wait(BLOCKING_TRANSACTION_TIMEOUT)) {
		int respLen = *respLenPtr;
		if (respPtr->get() != NULL && respLen > 0) {
			resp = new uint8_t[respLen];
			memcpy(resp, respPtr->get(),respLen);
			return respLen;
		} else {
			resp = NULL;
			return -1;
		}
	} else {
		if (debug) {
			pdebug("blocking transaction timed out");
		}
		resp = NULL;
		return -1;
	}
}

int VirtualDevice::getMTU() {
	return mtu;
}

int VirtualDevice::getHighestForwardedHandle() {
	return highestForwardedHandle;
}

bool VirtualDevice::isLive() {
	time_t now = time(NULL);

	std::lock_guard<std::mutex> lk(transactionMutex);
	if (currentTransaction) {
		if (difftime(now, currentTransaction->time) > ASYNC_TRANSACTION_TIMEOUT) {
			return false;
		}
	}

	return true;
}

void VirtualDevice::handleTransactionResponse(uint8_t *buf, int len) {
	std::unique_lock<std::mutex> lk(transactionMutex);

	if (debug_performance) {
		uint64_t currentTimeMillis = getCurrentTimeMillis();
		uint64_t elapsed = currentTimeMillis - lastTransactionMillis;
		std::stringstream ss;
		ss << "Transaction" << std::endl;
		ss << " start:\t" << std::fixed << lastTransactionMillis << std::endl;
		ss << " end:\t" << std::fixed << currentTimeMillis << std::endl;
		ss << " len:\t" << std::fixed << elapsed;
		pdebug(ss.str());

		if (transactionLatencies.size() < MAX_TRANSACTION_LATENCIES) {
			transactionLatencies.push_back(elapsed);
		} else {
			pwarn("max latency log capacity reached");
		}
	}

	if (!currentTransaction) {
		pwarn("unexpected transaction response when none existed!");
		return;
	}

	uint8_t reqOpcode = currentTransaction->buf.get()[0];
	uint8_t respOpcode = buf[0];

	if (respOpcode == ATT_OP_ERROR) {
		if (len != ATT_ERROR_PDU_LEN) {
			pwarn("invalid error pdu");
			return;
		}

		if (buf[1] != reqOpcode) {
			pwarn("unmatched error: " + std::to_string(buf[1]));
			return;
		}
	} else if (respOpcode == ATT_OP_HANDLE_CNF && reqOpcode != ATT_OP_HANDLE_IND) {
		pwarn("unmatched confirmation");
		return;
	} else if (respOpcode - 1 != reqOpcode) {
		pwarn("unmatched transaction: " + std::to_string(respOpcode));
		return;
	}

	auto t = currentTransaction;
	if (pendingTransactions.size() > 0) {
		while (pendingTransactions.size() > 0) {
			currentTransaction = pendingTransactions.front();
			currentTransaction->time = time(NULL);
			pendingTransactions.pop();
			if(!write(currentTransaction->buf.get(), currentTransaction->len)) {
				currentTransaction->cb(NULL, -1);
				currentTransaction.reset();
			} else {
				break;
			}
		}
	} else {
		currentTransaction.reset();
	}
	lk.unlock();

	t->cb(buf, len);
}

void VirtualDevice::readHandler(uint8_t *buf, int len) {
	uint8_t opCode = buf[0];
	if (opCode == ATT_OP_MTU_REQ) {
		mtu = btohs(*(uint16_t * )(buf + 1));
		uint8_t resp[3];
		resp[0] = ATT_OP_MTU_RESP;
		*(uint16_t *) (resp + 1) = htobs(ATT_DEFAULT_LE_MTU);
		write(resp, sizeof(resp));
	} else if (is_att_response(opCode) || opCode == ATT_OP_HANDLE_CNF || opCode == ATT_OP_ERROR) {
		handleTransactionResponse(buf, len);
	} else {

		/*
		 * Discover services in the network
		 */
		if (opCode == ATT_OP_FIND_BY_TYPE_REQ && isEndpoint && beetle.discoveryClient) {
			uint16_t startHandle;
			uint16_t endHandle;
			uint16_t attType;
			uint8_t *attValue;
			int attValLen;
			if (!parse_find_by_type_value_request(buf, len, startHandle, endHandle, attType, attValue, attValLen)) {
				uint8_t err[ATT_ERROR_PDU_LEN];
				pack_error_pdu(opCode, 0, ATT_ECODE_INVALID_PDU, err);
				write(err, sizeof(err));
				return;
			}

			if (attType == GATT_PRIM_SVC_UUID) {
				UUID serviceUuid(attValue, attValLen);
				boost::shared_array<uint8_t> bufCpy(new uint8_t[len]);
				memcpy(bufCpy.get(), buf, len);

				beetle.discoveryClient->registerInterestInUuid(getId(), serviceUuid, true, [this, bufCpy, len] {
					beetle.router->route(bufCpy.get(), len, getId());
				});
			}
		} else {
			beetle.router->route(buf, len, getId());
		}
	}
}

void VirtualDevice::setupBeetleService(int handleAlloc) {
	handleAlloc++;

	auto beetleServiceHandle = std::make_shared<PrimaryService>();
	beetleServiceHandle->setHandle(handleAlloc++);
	auto beetleServiceUUID = boost::shared_array<uint8_t>(new uint8_t[2]);
	*(uint16_t *) beetleServiceUUID.get() = btohs(BEETLE_SERVICE_UUID);
	beetleServiceHandle->cache.set(beetleServiceUUID, 2);
	handles[beetleServiceHandle->getHandle()] = beetleServiceHandle;

	uint16_t lastHandle = 0;

	if (type == LE_PERIPHERAL) {
		/*
		 * Bdaddr characteristic
		 */
		LEPeripheral *le = dynamic_cast<LEPeripheral *>(this);
		assert(le);

		auto beetleBdaddrCharHandle = std::make_shared<Characteristic>();
		beetleBdaddrCharHandle->setHandle(handleAlloc++);
		beetleBdaddrCharHandle->setServiceHandle(beetleServiceHandle->getHandle());
		auto beetleBdaddrCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
		beetleBdaddrCharHandleValue[0] = GATT_CHARAC_PROP_READ;
		*(uint16_t *) (beetleBdaddrCharHandleValue.get() + 3) = htobs(BEETLE_CHARAC_BDADDR_UUID);
		beetleBdaddrCharHandle->cache.set(beetleBdaddrCharHandleValue, 5);
		handles[beetleBdaddrCharHandle->getHandle()] = beetleBdaddrCharHandle;

		auto beetleBdaddrAttrHandle = std::make_shared<CharacteristicValue>(true, true);
		beetleBdaddrAttrHandle->setHandle(handleAlloc++);
		beetleBdaddrAttrHandle->setUuid(BEETLE_CHARAC_BDADDR_UUID);
		beetleBdaddrAttrHandle->setServiceHandle(beetleServiceHandle->getHandle());
		beetleBdaddrAttrHandle->setCharHandle(beetleBdaddrCharHandle->getHandle());
		auto beetleBdaddrAttrHandleValue = boost::shared_array<uint8_t>(new uint8_t[sizeof(bdaddr_t)]);
		memcpy(beetleBdaddrAttrHandleValue.get(), le->getBdaddr().b, sizeof(bdaddr_t));
		beetleBdaddrAttrHandle->cache.set(beetleBdaddrAttrHandleValue, sizeof(bdaddr_t));
		handles[beetleBdaddrAttrHandle->getHandle()] = beetleBdaddrAttrHandle;

		// fill in attr handle for characteristic
		beetleBdaddrCharHandle->setCharHandle(beetleBdaddrAttrHandle->getHandle());
		*(uint16_t *) (beetleBdaddrCharHandleValue.get() + 1) = htobs(beetleBdaddrAttrHandle->getHandle());

		auto beetleBdaddrTypeHandle = std::make_shared<Handle>(true, true);
		beetleBdaddrTypeHandle->setHandle(handleAlloc++);
		beetleBdaddrTypeHandle->setServiceHandle(beetleServiceHandle->getHandle());
		beetleBdaddrTypeHandle->setCharHandle(beetleBdaddrCharHandle->getHandle());
		beetleBdaddrTypeHandle->setUuid(UUID(BEETLE_DESC_BDADDR_TYPE_UUID));
		auto beetleBdaddrTypeHandleValue = boost::shared_array<uint8_t>(new uint8_t[1]);
		beetleBdaddrTypeHandleValue[0] = (le->getAddrType() == LEPeripheral::PUBLIC) ?
				LE_PUBLIC_ADDRESS : LE_RANDOM_ADDRESS;
		beetleBdaddrTypeHandle->cache.set(beetleBdaddrTypeHandleValue, 1);
		handles[beetleBdaddrTypeHandle->getHandle()] = beetleBdaddrTypeHandle;

		beetleBdaddrCharHandle->setEndGroupHandle(beetleBdaddrTypeHandle->getHandle());

		lastHandle = beetleBdaddrTypeHandle->getHandle();
	}

	/*
	 * Handle range characteristic
	 */
	{
		auto beetleHandleRangeCharHandle = std::make_shared<Characteristic>();
		beetleHandleRangeCharHandle->setHandle(handleAlloc++);
		beetleHandleRangeCharHandle->setServiceHandle(beetleServiceHandle->getHandle());
		auto beetleHandleRangeCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
		beetleHandleRangeCharHandleValue[0] = GATT_CHARAC_PROP_READ;
		*(uint16_t *) (beetleHandleRangeCharHandleValue.get() + 3) = htobs(BEETLE_CHARAC_HANDLE_RANGE_UUID);
		beetleHandleRangeCharHandle->cache.set(beetleHandleRangeCharHandleValue, 5);
		handles[beetleHandleRangeCharHandle->getHandle()] = beetleHandleRangeCharHandle;

		auto beetleHandleRangeAttrHandle = std::make_shared<CharacteristicValue>(true, true);
		beetleHandleRangeAttrHandle->setHandle(handleAlloc++);
		beetleHandleRangeAttrHandle->setUuid(UUID(BEETLE_CHARAC_HANDLE_RANGE_UUID));
		beetleHandleRangeAttrHandle->setServiceHandle(beetleServiceHandle->getHandle());
		beetleHandleRangeAttrHandle->setCharHandle(beetleHandleRangeCharHandle->getHandle());
		auto beetleHandleRangeAttrHandleValue = boost::shared_array<uint8_t>(new uint8_t[4]);
		memset(beetleHandleRangeAttrHandleValue.get(), 0, 4);
		beetleHandleRangeAttrHandle->cache.set(beetleHandleRangeAttrHandleValue, 4);
		handles[beetleHandleRangeAttrHandle->getHandle()] = beetleHandleRangeAttrHandle;

		// fill in attr handle for characteristic
		beetleHandleRangeCharHandle->setCharHandle(beetleHandleRangeAttrHandle->getHandle());
		*(uint16_t *) (beetleHandleRangeCharHandleValue.get() + 1) = htobs(beetleHandleRangeAttrHandle->getHandle());

		beetleHandleRangeCharHandle->setEndGroupHandle(beetleHandleRangeAttrHandle->getHandle());

		lastHandle = beetleHandleRangeAttrHandle->getHandle();
	}

	/*
	 * Connected time characteristic
	 */
	{
		auto beetleConnTimeCharHandle = std::make_shared<Characteristic>();
		beetleConnTimeCharHandle->setHandle(handleAlloc++);
		beetleConnTimeCharHandle->setServiceHandle(beetleServiceHandle->getHandle());
		auto beetleConnTimeCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
		beetleConnTimeCharHandleValue[0] = GATT_CHARAC_PROP_READ;
		*(uint16_t *) (beetleConnTimeCharHandleValue.get() + 3) = htobs(BEETLE_CHARAC_CONNECTED_TIME_UUID);
		beetleConnTimeCharHandle->cache.set(beetleConnTimeCharHandleValue, 5);
		handles[beetleConnTimeCharHandle->getHandle()] = beetleConnTimeCharHandle;

		auto beetleConnTimeAttrHandle = std::make_shared<CharacteristicValue>(true, true);
		beetleConnTimeAttrHandle->setHandle(handleAlloc++);
		beetleConnTimeAttrHandle->setUuid(UUID(BEETLE_CHARAC_CONNECTED_TIME_UUID));
		beetleConnTimeAttrHandle->setServiceHandle(beetleServiceHandle->getHandle());
		beetleConnTimeAttrHandle->setCharHandle(beetleConnTimeCharHandle->getHandle());
		auto beetleConnTimeAttrHandleValue = boost::shared_array<uint8_t>(new uint8_t[sizeof(int32_t)]);
		*(int32_t *) (beetleConnTimeAttrHandleValue.get()) = htobl(static_cast<int32_t>(connectedTime));
		beetleConnTimeAttrHandle->cache.set(beetleConnTimeAttrHandleValue, sizeof(int32_t));
		handles[beetleConnTimeAttrHandle->getHandle()] = beetleConnTimeAttrHandle;

		// fill in attr handle for characteristic
		beetleConnTimeCharHandle->setCharHandle(beetleConnTimeAttrHandle->getHandle());
		*(uint16_t *) (beetleConnTimeCharHandleValue.get() + 1) = htobs(beetleConnTimeAttrHandle->getHandle());

		beetleConnTimeCharHandle->setEndGroupHandle(beetleConnTimeAttrHandle->getHandle());

		lastHandle = beetleConnTimeAttrHandle->getHandle();
	}

	/*
	 * Gateway name characteristic
	 */
	{
		auto beetleConnGatewayCharHandle = std::make_shared<Characteristic>();
		beetleConnGatewayCharHandle->setHandle(handleAlloc++);
		beetleConnGatewayCharHandle->setServiceHandle(beetleServiceHandle->getHandle());
		auto beetleConnGatewayCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
		beetleConnGatewayCharHandleValue[0] = GATT_CHARAC_PROP_READ;
		*(uint16_t *) (beetleConnGatewayCharHandleValue.get() + 3) = htobs(BEETLE_CHARAC_CONNECTED_GATEWAY_UUID);
		beetleConnGatewayCharHandle->cache.set(beetleConnGatewayCharHandleValue, 5);
		handles[beetleConnGatewayCharHandle->getHandle()] = beetleConnGatewayCharHandle;

		auto beetleConnGatewayAttrHandle = std::make_shared<CharacteristicValue>(true, true);
		beetleConnGatewayAttrHandle->setHandle(handleAlloc++);
		beetleConnGatewayAttrHandle->setUuid(UUID(BEETLE_CHARAC_CONNECTED_GATEWAY_UUID));
		beetleConnGatewayAttrHandle->setServiceHandle(beetleServiceHandle->getHandle());
		beetleConnGatewayAttrHandle->setCharHandle(beetleConnGatewayCharHandle->getHandle());
		int gatewayNameLen = beetle.name.length();
		if (gatewayNameLen >= ATT_DEFAULT_LE_MTU - 3) {
			gatewayNameLen = ATT_DEFAULT_LE_MTU - 3;
		}
		auto beetleConnGatewayAttrHandleValue = boost::shared_array<uint8_t>(new uint8_t[gatewayNameLen]);
		memcpy(beetleConnGatewayAttrHandleValue.get(), beetle.name.c_str(), gatewayNameLen);
		beetleConnGatewayAttrHandle->cache.set(beetleConnGatewayAttrHandleValue, gatewayNameLen);
		handles[beetleConnGatewayAttrHandle->getHandle()] = beetleConnGatewayAttrHandle;

		// fill in attr handle for characteristic
		beetleConnGatewayCharHandle->setCharHandle(beetleConnGatewayAttrHandle->getHandle());
		*(uint16_t *) (beetleConnGatewayCharHandleValue.get() + 1) = htobs(beetleConnGatewayAttrHandle->getHandle());

		beetleConnGatewayCharHandle->setEndGroupHandle(beetleConnGatewayAttrHandle->getHandle());

		lastHandle = beetleConnGatewayAttrHandle->getHandle();
	}

	beetleServiceHandle->setEndGroupHandle(lastHandle);
}

typedef struct {
	uint16_t handle;
	uint16_t endGroup;
	boost::shared_array<uint8_t> value;
	int len;
} group_t;

static std::vector<group_t> discoverServices(VirtualDevice *d) {
	if (debug_discovery) {
		pdebug("discovering services for " + d->getName());
	}

	uint16_t startHandle = 1;
	uint16_t endHandle = 0xFFFF;

	int reqLen = 7;
	uint8_t req[reqLen];
	req[0] = ATT_OP_READ_BY_GROUP_REQ;
	*(uint16_t *) (req + 3) = htobs(endHandle);
	*(uint16_t *) (req + 5) = htobs(GATT_PRIM_SVC_UUID);

	std::vector<group_t> groups;
	uint16_t currHandle = startHandle;
	while (true) {
		*(uint16_t *) (req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		/* Somewhat of a hack */
		boost::shared_array<uint8_t> respOwner(resp);

		if (resp == NULL || respLen < 2) {
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_READ_BY_GROUP_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				if (i + attDataLen > respLen) {
					break;
				}

				uint16_t handle = btohs(*(uint16_t *)(resp + i));
				if (handle < currHandle || handle > endHandle) {
					continue;
				}

				group_t group;
				group.handle = handle;
				group.endGroup = btohs(*(uint16_t *)(resp + i + 2));
				group.len = attDataLen - 4;
				group.value.reset(new uint8_t[group.len]);
				memcpy(group.value.get(), resp + i + 4, group.len);
				groups.push_back(group);
				if (debug_discovery) {
					pdebug("found service at handles " + std::to_string(group.handle) + " - "
							+ std::to_string(group.endGroup));
				}
			}
			currHandle = groups.rbegin()->endGroup + 1;
			if (currHandle == 0) {
				break;
			}
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_GROUP_REQ && resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_GROUP_REQ && resp[4] == ATT_ECODE_REQ_NOT_SUPP) {
			break;
		} else {
			throw DeviceException("unexpected transaction");
		}
	}
	return groups;
}

typedef struct {
	uint16_t handle;
	boost::shared_array<uint8_t> value;
	int len;
} handle_value_t;

static std::vector<handle_value_t> discoverCharacterisics(VirtualDevice *d, uint16_t startHandle, uint16_t endHandle) {
	if (debug_discovery) {
		pdebug("discovering characteristics for " + d->getName());
	}

	std::vector<handle_value_t> handles;

	int reqLen = 7;
	uint8_t req[reqLen];
	req[0] = ATT_OP_READ_BY_TYPE_REQ;
	*(uint16_t *) (req + 3) = htobs(endHandle);
	*(uint16_t *) (req + 5) = htobs(GATT_CHARAC_UUID);

	uint16_t currHandle = startHandle;
	while (true) {
		*(uint16_t *) (req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		/* Somewhat of a hack */
		boost::shared_array<uint8_t> respOwner(resp);

		if (resp == NULL || respLen < 2) {
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_READ_BY_TYPE_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				if (i + attDataLen > respLen) {
					break;
				}

				uint16_t handle = btohs(*(uint16_t *)(resp + i));
				if (handle < currHandle || handle > endHandle) {
					continue;
				}

				handle_value_t handleValue;
				handleValue.handle = handle;
				handleValue.len = attDataLen - 2;
				handleValue.value.reset(new uint8_t[handleValue.len]);
				memcpy(handleValue.value.get(), resp + i + 2, handleValue.len);
				handles.push_back(handleValue);
				if (debug_discovery) {
					pdebug("found characteristic at handle " + std::to_string(handleValue.handle));
				}
			}

			uint16_t nextHandle = handles.rbegin()->handle + 1;
			if (nextHandle <= currHandle || nextHandle >= endHandle) {
				break;
			}
			currHandle = nextHandle;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_TYPE_REQ && resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_READ_BY_TYPE_REQ && resp[4] == ATT_ECODE_REQ_NOT_SUPP) {
			break;
		} else {
			throw DeviceException("unexpected transaction");
		}
	}
	return handles;
}

typedef struct {
	uint8_t format;
	uint16_t handle;
	UUID uuid;
} handle_info_t;

static std::vector<handle_info_t> discoverHandles(VirtualDevice *d, uint16_t startGroup, uint16_t endGroup) {
	if (debug_discovery) {
		pdebug("discovering handles for " + d->getName());
	}

	std::vector<handle_info_t> handles;

	int reqLen = 5;
	uint8_t req[reqLen];
	req[0] = ATT_OP_FIND_INFO_REQ;
	*(uint16_t *) (req + 3) = htobs(endGroup);

	uint16_t currHandle = startGroup;
	while (true) {
		*(uint16_t *) (req + 1) = htobs(currHandle);

		uint8_t *resp;
		int respLen = d->writeTransactionBlocking(req, reqLen, resp);

		/* Somewhat of a hack */
		boost::shared_array<uint8_t> respOwner(resp);

		if (resp == NULL || respLen < 2) {
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_FIND_INFO_RESP) {
			uint8_t format = resp[1];
			int attDataLen = (format == ATT_FIND_INFO_RESP_FMT_16BIT) ? 4 : 18;
			for (int i = 2; i < respLen; i += attDataLen) {
				if (i + attDataLen > respLen) {
					break;
				}

				uint16_t handle = btohs(*(uint16_t *)(resp + i));
				if (handle < currHandle || handle > endGroup) {
					continue;
				}

				handle_info_t handleInfo;
				handleInfo.format = format;
				handleInfo.handle = handle;
				handleInfo.uuid = UUID(resp + i + 2, attDataLen - 2);
				handles.push_back(handleInfo);
				if (debug_discovery) {
					pdebug("found handle at " + std::to_string(handleInfo.handle));
				}
			}

			currHandle = handles.rbegin()->handle + 1;
			if (currHandle == 0 || currHandle >= endGroup) {
				break;
			}
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_FIND_INFO_REQ && resp[4] == ATT_ECODE_ATTR_NOT_FOUND) {
			break;
		} else if (resp[0] == ATT_OP_ERROR && resp[1] == ATT_OP_FIND_INFO_REQ && resp[4] == ATT_ECODE_REQ_NOT_SUPP) {
			break;
		} else {
			throw DeviceException("unexpected transaction");
		}
	}
	return handles;
}

/*
 * TODO robustly validate GATT server packets
 */
static std::map<uint16_t, std::shared_ptr<Handle>> discoverAllHandles(VirtualDevice *d) {
	std::map<uint16_t, std::shared_ptr<Handle>> handles;

	std::shared_ptr<Handle> lastService = NULL;
	std::shared_ptr<Handle> lastCharacteristic = NULL;

	std::vector<group_t> services = discoverServices(d);
	for (group_t &service : services) {
		auto serviceHandle = std::make_shared<PrimaryService>();
		serviceHandle->setHandle(service.handle);
		serviceHandle->setEndGroupHandle(service.endGroup);
		serviceHandle->cache.set(service.value, service.len);
		assert(handles.find(service.handle) == handles.end());
		handles[service.handle] = serviceHandle;

		std::vector<handle_value_t> characteristics = discoverCharacterisics(d, serviceHandle->getHandle(),
				serviceHandle->getEndGroupHandle());
		for (handle_value_t &characteristic : characteristics) {
			auto charHandle = std::make_shared<Characteristic>();
			charHandle->setHandle(characteristic.handle);
			charHandle->setServiceHandle(serviceHandle->getHandle());

			// let the handle inherit the pointer
			charHandle->cache.set(characteristic.value, characteristic.len);
			assert(handles.find(characteristic.handle) == handles.end());
			handles[characteristic.handle] = charHandle;

			// save in case it is the last
			lastCharacteristic = charHandle;
		}

		if (characteristics.size() > 0) {
			for (int i = 0; i < (int) (characteristics.size() - 1); i++) {
				handle_value_t characteristic = characteristics[i];
				uint16_t startGroup = characteristic.handle + 1;
				uint16_t endGroup = characteristics[i + 1].handle - 1;
				auto charHandle = std::dynamic_pointer_cast<Characteristic>(handles[characteristic.handle]);
				charHandle->setEndGroupHandle(endGroup);

				std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
				for (handle_info_t &handleInfo : handleInfos) {
					UUID handleUuid = handleInfo.uuid;
					std::shared_ptr<Handle> handle;
					if (handleUuid.isShort() && handleUuid.getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
						handle = std::make_shared<ClientCharCfg>();
					} else if (handleInfo.handle == charHandle->getAttrHandle()) {
						handle = std::make_shared<CharacteristicValue>(false, false);
						handle->setUuid(handleUuid);
					} else {
						handle = std::make_shared<Handle>();
						handle->setUuid(handleUuid);
					}
					handle->setHandle(handleInfo.handle);
					handle->setServiceHandle(serviceHandle->getHandle());
					handle->setCharHandle(characteristic.handle);
					assert(handles.find(handleInfo.handle) == handles.end());
					handles[handleInfo.handle] = handle;
				}
			}

			handle_value_t characteristic = characteristics[characteristics.size() - 1];
			uint16_t startGroup = characteristic.handle + 1;
			uint16_t endGroup = serviceHandle->getEndGroupHandle();
			auto charHandle = std::dynamic_pointer_cast<Characteristic>(handles[characteristic.handle]);
			charHandle->setEndGroupHandle(endGroup);

			std::vector<handle_info_t> handleInfos = discoverHandles(d, startGroup, endGroup);
			for (handle_info_t &handleInfo : handleInfos) {
				UUID handleUuid = handleInfo.uuid;
				std::shared_ptr<Handle> handle;
				if (handleUuid.isShort() && handleUuid.getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
					handle = std::make_shared<ClientCharCfg>();
				} else if (handleInfo.handle == charHandle->getAttrHandle()) {
					handle = std::make_shared<CharacteristicValue>(false, false);
					handle->setUuid(handleUuid);
				} else {
					handle = std::make_shared<Handle>();
					handle->setUuid(handleUuid);
				}
				handle->setHandle(handleInfo.handle);
				handle->setServiceHandle(serviceHandle->getHandle());
				handle->setCharHandle(characteristic.handle);
				assert(handles.find(handleInfo.handle) == handles.end());
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

	if (debug_discovery) {
		pdebug("done discovering handles for " + d->getName());
	}
	return handles;
}

