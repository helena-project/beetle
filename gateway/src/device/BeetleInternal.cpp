/*
 * BeetleDevice.cpp
 *
 *  Created on: Apr 3, 2016
 *      Author: James Hong
 */

#include <ble/utils.h>
#include "device/BeetleInternal.h"

#include <bluetooth/bluetooth.h>
#include <cstring>
#include <map>
#include <mutex>
#include <set>

#include "Beetle.h"
#include "ble/gatt.h"
#include "Debug.h"
#include "hat/SingleAllocator.h"
#include "Handle.h"
#include "UUID.h"

BeetleInternal::BeetleInternal(Beetle &beetle, std::string name_) :
		Device(beetle, BEETLE_RESERVED_DEVICE, new SingleAllocator(NULL_RESERVED_DEVICE)) {
	name = name_;
	type = BEETLE_INTERNAL;
	init();
}

BeetleInternal::~BeetleInternal() {

}

/*
 * Should never get called. All reads and writes are serviced by the cache.
 */
void BeetleInternal::writeResponse(uint8_t *buf, int len) {
	throw DeviceException(getName() + " does not make requests");
}

/*
 * Should never get called. All reads and writes are serviced by the cache.
 */
void BeetleInternal::writeCommand(uint8_t *buf, int len) {
	if (debug) {
		pwarn("Beetle received an unanticipated command");
	}
}

/*
 * Should never get called. All reads and writes are serviced by the cache.
 */
void BeetleInternal::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	uint8_t *resp;
	int respLen = writeTransactionBlocking(buf, len, resp);
	cb(resp, respLen);
	delete[] resp;
}

/*
 * Should never get called. All reads and writes are serviced by the cache.
 */
int BeetleInternal::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	if (debug) {
		pwarn("Beetle received an unanticipated request");
	}
	resp = new uint8_t[ATT_ERROR_PDU_LEN];
	pack_error_pdu(buf[0], 0, ATT_ECODE_UNLIKELY, resp);
	return ATT_ERROR_PDU_LEN;
}

int BeetleInternal::getMTU() {
	return ATT_DEFAULT_LE_MTU;
}

void BeetleInternal::informServicesChanged(handle_range_t range, device_t dst) {
	if (debug) {
		pdebug("informing " + std::to_string(dst) + " of service change " + range.str());
	}

	std::lock_guard<std::recursive_mutex> handlesLg(handlesMutex);
	if (serviceChangedAttr->subscribersNotify.size() == 0) {
		return;
	}
	uint8_t cmd[7];
	cmd[0] = ATT_OP_HANDLE_NOTIFY;
	*(uint16_t *) (cmd + 1) = htobs(serviceChangedAttr->getHandle());
	*(uint16_t *) (cmd + 3) = htobs(range.start);
	*(uint16_t *) (cmd + 5) = htobs(range.end);

	if (serviceChangedAttr->subscribersNotify.find(dst) != serviceChangedAttr->subscribersNotify.end()) {
		if (beetle.devices.find(dst) != beetle.devices.end()) {
			beetle.devices[dst]->writeCommand(cmd, sizeof(cmd));
		} else {
			pwarn("cannot inform " + std::to_string(dst) + ", does not name a device");
		}
	} else {
		if (debug) {
			pdebug(std::to_string(dst) + " is not subscribed to service changed characteristic");
		}
	}
}

void BeetleInternal::init() {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	uint16_t handleAlloc = 1; // 0 is special

	/*
	 * Setup GAP service with name characteristic
	 */
	auto gapServiceHandle = std::make_shared<PrimaryService>();
	gapServiceHandle->setHandle(handleAlloc++);
	auto gapHandleUUID = boost::shared_array<uint8_t>(new uint8_t[2]);
	*(uint16_t *) gapHandleUUID.get() = btohs(GATT_GAP_SERVICE_UUID);
	gapServiceHandle->cache.set(gapHandleUUID, 2);
	handles[gapServiceHandle->getHandle()] = gapServiceHandle;

	auto gapDeviceNameCharHandle = std::make_shared<Characteristic>();
	gapDeviceNameCharHandle->setHandle(handleAlloc++);
	gapDeviceNameCharHandle->setServiceHandle(gapServiceHandle->getHandle());
	auto gapDeviceNameCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
	gapDeviceNameCharHandleValue[0] = GATT_CHARAC_PROP_READ;
	*(uint16_t *) (gapDeviceNameCharHandleValue.get() + 3) = htobs(GATT_GAP_CHARAC_DEVICE_NAME_UUID);
	gapDeviceNameCharHandle->cache.set(gapDeviceNameCharHandleValue, 5);
	handles[gapDeviceNameCharHandle->getHandle()] = gapDeviceNameCharHandle;

	auto gapDeviceNameAttrHandle = std::make_shared<CharacteristicValue>(true, true);
	gapDeviceNameAttrHandle->setHandle(handleAlloc++);
	gapDeviceNameAttrHandle->setUuid(UUID(GATT_GAP_CHARAC_DEVICE_NAME_UUID));
	gapDeviceNameAttrHandle->setServiceHandle(gapServiceHandle->getHandle());
	gapDeviceNameAttrHandle->setCharHandle(gapDeviceNameCharHandle->getHandle());
	auto gapDeviceNameAttrHandleValue = boost::shared_array<uint8_t>(new uint8_t[name.length()]);
	memcpy(gapDeviceNameAttrHandleValue.get(), name.c_str(), name.length());
	gapDeviceNameAttrHandle->cache.set(gapDeviceNameAttrHandleValue, name.length());
	handles[gapDeviceNameAttrHandle->getHandle()] = gapDeviceNameAttrHandle;

	// fill in attr handle for characteristic
	gapDeviceNameCharHandle->setCharHandle(gapDeviceNameAttrHandle->getHandle());
	*(uint16_t *) (gapDeviceNameCharHandleValue.get() + 1) = htobs(gapDeviceNameAttrHandle->getHandle());

	// end the characteristic
	gapDeviceNameCharHandle->setEndGroupHandle(gapDeviceNameAttrHandle->getHandle());

	// end the service
	gapServiceHandle->setEndGroupHandle(gapDeviceNameAttrHandle->getHandle());

	/*
	 * Setup GATT service with service changed characteristic
	 */
	auto gattServiceHandle = std::make_shared<PrimaryService>();
	gattServiceHandle->setHandle(handleAlloc++);
	auto gattHandleUUID = boost::shared_array<uint8_t>(new uint8_t[2]);
	*(uint16_t *) gattHandleUUID.get() = btohs(GATT_GATT_SERVICE_UUID);
	gattServiceHandle->cache.set(gattHandleUUID, 2);
	handles[gattServiceHandle->getHandle()] = gattServiceHandle;

	auto gattServiceChangedCharHandle = std::make_shared<Characteristic>();
	gattServiceChangedCharHandle->setHandle(handleAlloc++);
	gattServiceChangedCharHandle->setServiceHandle(gattServiceHandle->getHandle());
	auto gattServiceChangedCharHandleValue = boost::shared_array<uint8_t>(new uint8_t[5]);
	gattServiceChangedCharHandleValue[0] = GATT_CHARAC_PROP_NOTIFY;
	*(uint16_t *) (gattServiceChangedCharHandleValue.get() + 3) = htobs(GATT_GATT_CHARAC_SERVICE_CHANGED_UUID);
	gattServiceChangedCharHandle->cache.set(gattServiceChangedCharHandleValue, 5);
	handles[gattServiceChangedCharHandle->getHandle()] = gattServiceChangedCharHandle;

	auto gattServiceChangedAttrHandle = std::make_shared<CharacteristicValue>(false, false);
	gattServiceChangedAttrHandle->setHandle(handleAlloc++);
	gattServiceChangedAttrHandle->setUuid(UUID(GATT_GATT_CHARAC_SERVICE_CHANGED_UUID));
	gattServiceChangedAttrHandle->setServiceHandle(gattServiceHandle->getHandle());
	gattServiceChangedAttrHandle->setCharHandle(gattServiceChangedCharHandle->getHandle());
	gattServiceChangedAttrHandle->cache.clear();
	handles[gattServiceChangedAttrHandle->getHandle()] = gattServiceChangedAttrHandle;

	// fill in attr handle for characteristic
	gattServiceChangedCharHandle->setCharHandle(gattServiceChangedAttrHandle->getHandle());
	*(uint16_t *) (gattServiceChangedCharHandleValue.get() + 1) = htobs(gattServiceChangedAttrHandle->getHandle());

	auto gattServiceChangedCfgHandle = std::make_shared<ClientCharCfg>();
	gattServiceChangedCfgHandle->setHandle(handleAlloc++);
	gattServiceChangedCfgHandle->setServiceHandle(gattServiceHandle->getHandle());
	gattServiceChangedCfgHandle->setCharHandle(gattServiceChangedCharHandle->getHandle());
	gattServiceChangedCfgHandle->cache.clear();
	handles[gattServiceChangedCfgHandle->getHandle()] = gattServiceChangedCfgHandle;

	// end the characteristic
	gattServiceChangedCharHandle->setEndGroupHandle(gattServiceChangedCfgHandle->getHandle());
	// end the service
	gattServiceHandle->setEndGroupHandle(gattServiceChangedCfgHandle->getHandle());

	// save the service changed attr handle
	serviceChangedAttr = gattServiceChangedAttrHandle;
}

