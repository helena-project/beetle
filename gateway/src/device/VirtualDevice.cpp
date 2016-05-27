/*
 * VirtualDevice.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#include "device/VirtualDevice.h"

#include <assert.h>
#include <bluetooth/bluetooth.h>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <time.h>

#include "Beetle.h"
#include "ble/att.h"
#include "ble/gatt.h"
#include "ble/helper.h"
#include "controller/NetworkDiscoveryClient.h"
#include "Debug.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "Handle.h"
#include "Router.h"
#include "sync/Semaphore.h"
#include "UUID.h"

static const int maxTransactionLatencies = 50;

static int64_t getCurrentTimeMillis(void) {
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

static std::string discoverDeviceName(VirtualDevice *d);
static std::map<uint16_t, std::shared_ptr<Handle>> discoverAllHandles(VirtualDevice *d);

void VirtualDevice::start(bool discoverHandles) {
	if (debug) {
		pdebug("starting");
	}

	if (discoverHandles) {
		startInternal();

		if (name == "") {
			name = discoverDeviceName(this);
			if (name == "") {
				throw DeviceException("device must have a name");
			}
		}

		/* May throw device exception */
		std::map<uint16_t, std::shared_ptr<Handle>> handlesTmp = discoverAllHandles(this);

		handlesMutex.lock();
		handles = handlesTmp;
		handlesMutex.unlock();

		beetle.updateDevice(getId());
	} else {
		startInternal();

		if (name == "") {
			throw DeviceException("device must have a name");
		}
	}
}

std::vector<uint64_t> VirtualDevice::getTransactionLatencies() {
	std::lock_guard<std::mutex> lg(transactionMutex);
	auto ret = transactionLatencies;
	transactionLatencies.clear();
	return ret;
}

void VirtualDevice::writeCommand(uint8_t *buf, int len) {
	write(buf, len);
}

void VirtualDevice::writeResponse(uint8_t *buf, int len) {
	write(buf, len);
}

void VirtualDevice::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	transaction_t *t = new transaction_t;
	t->buf.reset(new uint8_t[len]);
	memcpy(t->buf.get(), buf, len);
	t->len = len;
	t->cb = cb;

	std::lock_guard<std::mutex> lg(transactionMutex);
	if (currentTransaction == NULL) {
		currentTransaction.reset(t);
		lastTransactionMillis = getCurrentTimeMillis();
		if (!write(t->buf.get(), t->len)) {
			cb(NULL, -1);
		}
	} else {
		pendingTransactions.push(std::shared_ptr<transaction_t>(t));
	}
}

int VirtualDevice::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	std::shared_ptr<Semaphore> sema(new Semaphore(0));
	std::shared_ptr<int> respLenPtr(new int);
	std::shared_ptr<std::unique_ptr<uint8_t>> respPtr(new std::unique_ptr<uint8_t>());
	writeTransaction(buf, len, [sema, respPtr, respLenPtr](uint8_t *resp_, int respLen_) {
		if (resp_ != NULL && respLen_ > 0) {
			respPtr->reset(new uint8_t[respLen_]);
			memcpy(respPtr->get(), resp_, respLen_);
		}
		*respLenPtr = respLen_;
		sema->notify();
	});

	if (sema->try_wait(10)) {
		resp = respPtr->release();
		return *respLenPtr;
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

void VirtualDevice::handleTransactionResponse(uint8_t *buf, int len) {
	std::unique_lock<std::mutex> lk(transactionMutex);

	if (debug_performance) {
		int64_t currentTimeMillis = getCurrentTimeMillis();
		int64_t elapsed = currentTimeMillis - lastTransactionMillis;
		std::stringstream ss;
		ss << "Transaction start: " << std::fixed << lastTransactionMillis << "\n" << "Transaction end: " << std::fixed
				<< currentTimeMillis << "\n" << "Transaction length: " << std::fixed << elapsed;
		pdebug(ss.str());

		if (transactionLatencies.size() < maxTransactionLatencies) {
			transactionLatencies.push_back(elapsed);
		} else {
			pwarn("max latency log capacity reached");
		}
	}

	if (!currentTransaction) {
		pwarn("unexpected transaction response when none existed!");
		return;
	} else {
		auto t = currentTransaction;
		if (pendingTransactions.size() > 0) {
			while (pendingTransactions.size() > 0) {
				currentTransaction = pendingTransactions.front();
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
}

void VirtualDevice::readHandler(uint8_t *buf, int len) {
	uint8_t opCode = buf[0];
	if (opCode == ATT_OP_MTU_REQ) {
		mtu = btohs(*(uint16_t * )(buf + 1));
		uint8_t resp[3];
		resp[0] = ATT_OP_MTU_RESP;
		*(uint16_t *) (resp + 1) = htobs(ATT_DEFAULT_LE_MTU);
		write(resp, sizeof(resp));
	} else if (((opCode & 1) == 1 && opCode != ATT_OP_HANDLE_NOTIFY && opCode != ATT_OP_HANDLE_IND)
			|| opCode == ATT_OP_HANDLE_CNF) {
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
				discoverNetworkServices(serviceUuid);
			}
		}

		beetle.router->route(buf, len, getId());
	}
}

void VirtualDevice::discoverNetworkServices(UUID serviceUuid) {
	std::list<discovery_result_t> discovered;
	beetle.discoveryClient->discoverByUuid(serviceUuid, discovered, true, getId());
	for (discovery_result_t &d : discovered) {
		if (d.gateway == beetle.name) {
			/*
			 * Device is local.
			 */
			if (d.id == getId()) {
				continue;
			}
			beetle.mapDevices(d.id, getId());
		} else {
			/*
			 * Device is remote.
			 */
			device_t localId = NULL_RESERVED_DEVICE;

			/*
			 * Check if the remote device is already mapped locally.
			 */
			beetle.devicesMutex.lock_shared();
			for (auto &kv : beetle.devices) {
				auto tcpSp = std::dynamic_pointer_cast<TCPServerProxy>(kv.second);
				if (tcpSp && tcpSp->getServerGateway() == d.gateway && tcpSp->getRemoteDeviceId() == d.id) {
					localId = tcpSp->getId();
				}
			}
			beetle.devicesMutex.unlock_shared();

			if (localId == NULL_RESERVED_DEVICE) {
				Semaphore sync(0);
				std::thread t([this, &sync, &localId, d] {
					/*
					 * Try to connect and map from remote.
					 */
					std::shared_ptr<VirtualDevice> device = NULL;
					try {
						device.reset(TCPServerProxy::connectRemote(beetle, d.ip, d.port, d.id));
						localId = device->getId();
						sync.notify();

						boost::shared_lock<boost::shared_mutex> devicesLk;
						beetle.addDevice(device, devicesLk);

						device->start();

						if (debug_controller) {
							pdebug("connected to remote " + std::to_string(device->getId())
									+ " : " + device->getName());
						}
					} catch (DeviceException &e) {
						pexcept(e);
						if (device) {
							beetle.removeDevice(device->getId());
						}
						sync.notify();
					}
				});
				sync.wait();
			}

			if (localId != NULL_RESERVED_DEVICE) {
				beetle.mapDevices(localId, getId());
			}
		}
	}
}

static std::string discoverDeviceName(VirtualDevice *d) {
	if (debug_discovery) {
		pdebug("discovering name");
	}

	uint8_t *req = NULL;
	uint8_t *resp = NULL;

	std::string name;
	int reqLen = pack_read_by_type_req_pdu(GATT_GAP_CHARAC_DEVICE_NAME_UUID, 0x1, 0xFFFF, req);
	int respLen = d->writeTransactionBlocking(req, reqLen, resp);
	if (resp == NULL || respLen < 4 || resp[0] != ATT_OP_READ_BY_TYPE_RESP) {
		// nothing
	} else {
		char cName[respLen - 4 + 1];
		memcpy(cName, resp + 4, respLen - 4);
		cName[respLen - 4] = '\0';
		name = std::string(cName);
	}

	if (debug_discovery) {
		pdebug(name);
	}

	delete[] req;
	delete[] resp;
	return name;
}

typedef struct {
	uint16_t handle;
	uint16_t endGroup;
	std::shared_ptr<uint8_t> value;
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
		std::unique_ptr<uint8_t> respOwner(resp);

		if (resp == NULL || respLen < 2) {
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_READ_BY_GROUP_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				if (i + attDataLen > respLen) {
					break;
				}
				group_t group;
				group.handle = btohs(*(uint16_t *)(resp + i));
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
	std::shared_ptr<uint8_t> value;
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
		std::unique_ptr<uint8_t> respOwner(resp);

		if (resp == NULL || respLen < 2) {
			throw DeviceException("error on transaction");
		}

		if (resp[0] == ATT_OP_READ_BY_TYPE_RESP) {
			int attDataLen = resp[1];
			for (int i = 2; i < respLen; i += attDataLen) {
				if (i + attDataLen > respLen) {
					break;
				}
				handle_value_t handleValue;
				handleValue.handle = btohs(*(uint16_t *)(resp + i));
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
		std::unique_ptr<uint8_t> respOwner(resp);

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
				handle_info_t handleInfo;
				handleInfo.format = format;
				handleInfo.handle = btohs(*(uint16_t * )(resp + i));
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

static std::map<uint16_t, std::shared_ptr<Handle>> discoverAllHandles(VirtualDevice *d) {
	std::map<uint16_t, std::shared_ptr<Handle>> handles;

	std::shared_ptr<Handle> lastService = NULL;
	std::shared_ptr<Handle> lastCharacteristic = NULL;

	std::vector<group_t> services = discoverServices(d);
	for (group_t &service : services) {
		auto serviceHandle = std::make_shared<PrimaryService>();
		serviceHandle->setHandle(service.handle);
		serviceHandle->setEndGroupHandle(service.endGroup);
		serviceHandle->setCacheInfinite(true);
		// let the handle inherit the pointer
		serviceHandle->cache.set(service.value.get(), service.len);
		assert(handles.find(service.handle) == handles.end());
		handles[service.handle] = serviceHandle;

		std::vector<handle_value_t> characteristics = discoverCharacterisics(d, serviceHandle->getHandle(),
				serviceHandle->getEndGroupHandle());
		for (handle_value_t &characteristic : characteristics) {
			auto charHandle = std::make_shared<Characteristic>();
			charHandle->setHandle(characteristic.handle);
			charHandle->setServiceHandle(serviceHandle->getHandle());
			charHandle->setCacheInfinite(true);

			// let the handle inherit the pointer
			charHandle->cache.set(characteristic.value.get(), characteristic.len);
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
						handle = std::make_shared<CharacteristicValue>();
						handle->setUuid(handleUuid);
					} else {
						handle = std::make_shared<Handle>();
						handle->setUuid(handleUuid);
					}
					handle->setHandle(handleInfo.handle);
					handle->setCacheInfinite(false);
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
					handle = std::make_shared<CharacteristicValue>();
					handle->setUuid(handleUuid);
				} else {
					handle = std::make_shared<Handle>();
					handle->setUuid(handleUuid);
				}
				handle->setHandle(handleInfo.handle);
				handle->setCacheInfinite(false);
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

