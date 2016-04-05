/*
 * BeetleDevice.cpp
 *
 *  Created on: Apr 3, 2016
 *      Author: root
 */

#include "BeetleDevice.h"

#include <bluetooth/bluetooth.h>
#include <mutex>

#include "../ble/gatt.h"
#include "../Handle.h"

BeetleDevice::BeetleDevice(Beetle &beetle, std::string name_) : Device(beetle, BEETLE_RESERVED_DEVICE) {
	name = name_;
	type = "Beetle";
	init();
}

BeetleDevice::~BeetleDevice() {

}

bool BeetleDevice::writeResponse(uint8_t *buf, int len) {
	throw DeviceException(getName() + " does not make requests");
}

bool BeetleDevice::writeCommand(uint8_t *buf, int len) {
	// TODO implement beetle local write
	return true;
}

bool BeetleDevice::writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) {
	uint8_t *resp;
	int respLen = writeTransactionBlocking(buf, len, resp);
	cb(resp, respLen);
	return true;
}

int BeetleDevice::writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) {
	// TODO implement beetle local transaction
	return 0;
}

void BeetleDevice::init() {
	std::lock_guard<std::recursive_mutex> lg(handlesMutex);
	uint16_t handleAlloc = 1; // 0 is special

	/*
	 * Setup GAP service
	 */
	Handle *gapServiceHandle = new PrimaryService();
	gapServiceHandle->setHandle(handleAlloc++);
	uint16_t *gapHandleUUID = new uint16_t;
	*gapHandleUUID = btohs(GATT_GAP_SERVICE_UUID);
	gapServiceHandle->cache.set((uint8_t *)gapHandleUUID, sizeof(uint16_t));
	gapServiceHandle->setCacheInfinite(true);
	handles[gapServiceHandle->getHandle()] = gapServiceHandle;

	Handle *gapDeviceNameCharHandle = new Characteristic();
	gapDeviceNameCharHandle->setHandle(handleAlloc++);
	gapDeviceNameCharHandle->setServiceHandle(gapServiceHandle->getHandle());
	uint8_t *gapDeviceNameCharHandleValue = new uint8_t[5];
	gapDeviceNameCharHandleValue[0] = GATT_CHARAC_PROP_READ;
	*(uint16_t *)(gapDeviceNameCharHandleValue + 3) = htobs(GATT_GAP_CHARAC_DEVICE_NAME_UUID);
	gapDeviceNameCharHandle->cache.set(gapDeviceNameCharHandleValue, 5);
	gapDeviceNameCharHandle->setCacheInfinite(true);
	handles[gapDeviceNameCharHandle->getHandle()] = gapDeviceNameCharHandle;

	Handle *gapDeviceNameAttrHandle = new Handle();
	gapDeviceNameAttrHandle->setHandle(handleAlloc++);
	gapDeviceNameAttrHandle->setUuid(UUID(GATT_GAP_CHARAC_DEVICE_NAME_UUID));
	gapDeviceNameAttrHandle->setServiceHandle(gapServiceHandle->getHandle());
	gapDeviceNameAttrHandle->setCharHandle(gapDeviceNameCharHandle->getHandle());
	uint8_t *gapDeviceNameAttrHandleValue = new uint8_t[name.length()];
	memcpy(gapDeviceNameAttrHandleValue, name.c_str(), name.length());
	gapDeviceNameAttrHandle->cache.set(gapDeviceNameAttrHandleValue, name.length());
	gapDeviceNameAttrHandle->setCacheInfinite(true);
	handles[gapDeviceNameAttrHandle->getHandle()] = gapDeviceNameAttrHandle;

	// end the characteristic
	*(uint16_t *)(gapDeviceNameCharHandleValue + 1) = htobs(gapDeviceNameAttrHandle->getHandle());
	gapDeviceNameCharHandle->setEndGroupHandle(gapDeviceNameAttrHandle->getHandle());

	// end the service
	gapServiceHandle->setEndGroupHandle(gapDeviceNameAttrHandle->getHandle());

	/*
	 * Setup GATT service
	 */
	Handle *gattServiceHandle = new PrimaryService();
	gattServiceHandle->setHandle(handleAlloc++);
	uint16_t *gattHandleUUID = new uint16_t;
	*gattHandleUUID = btohs(GATT_GATT_SERVICE_UUID);
	gattServiceHandle->cache.set((uint8_t *)gattHandleUUID, sizeof(uint16_t));
	gattServiceHandle->setCacheInfinite(true);
	handles[gattServiceHandle->getHandle()] = gattServiceHandle;

	Handle *gattServiceChangedCharHandle = new Characteristic();
	gattServiceChangedCharHandle->setHandle(handleAlloc++);
	gattServiceChangedCharHandle->setServiceHandle(gattServiceHandle->getHandle());
	uint8_t *gattServiceChangedCharHandleValue = new uint8_t[5];
	gattServiceChangedCharHandleValue[0] = GATT_CHARAC_PROP_NOTIFY;
	*(uint16_t *)(gattServiceChangedCharHandleValue + 3) = htobs(GATT_GATT_CHARAC_SERVICE_CHANGED_UUID);
	gattServiceChangedCharHandle->cache.set(gattServiceChangedCharHandleValue, 5);
	gattServiceChangedCharHandle->setCacheInfinite(true);
	handles[gattServiceChangedCharHandle->getHandle()] = gattServiceChangedCharHandle;

	Handle *gattServiceChangedAttrHandle = new Handle();
	gattServiceChangedAttrHandle->setHandle(handleAlloc++);
	gattServiceChangedAttrHandle->setUuid(UUID(GATT_GATT_CHARAC_SERVICE_CHANGED_UUID));
	gattServiceChangedAttrHandle->setServiceHandle(gattServiceHandle->getHandle());
	gattServiceChangedAttrHandle->setCharHandle(gattServiceChangedCharHandle->getHandle());
	gattServiceChangedAttrHandle->cache.set(NULL, 0);
	gattServiceChangedAttrHandle->setCacheInfinite(false);
	handles[gattServiceChangedAttrHandle->getHandle()] = gattServiceChangedAttrHandle;

	// fill in attr handle for characteristic
	*(uint16_t *)(gattServiceChangedCharHandleValue + 1) = htobs(gattServiceChangedAttrHandle->getHandle());

	Handle *gattServiceChangedCfgHandle = new ClientCharCfg();
	gattServiceChangedCfgHandle->setHandle(handleAlloc++);
	gattServiceChangedCfgHandle->setServiceHandle(gattServiceHandle->getHandle());
	gattServiceChangedCfgHandle->setCharHandle(gattServiceChangedCharHandle->getHandle());
	gattServiceChangedCfgHandle->cache.set(NULL, 0);
	gattServiceChangedCfgHandle->setCacheInfinite(false);
	handles[gattServiceChangedCfgHandle->getHandle()] = gattServiceChangedCfgHandle;

	// end the characteristic
	gattServiceChangedCharHandle->setEndGroupHandle(gattServiceChangedCfgHandle->getHandle());
	// end the service
	gattServiceHandle->setEndGroupHandle(gattServiceChangedCfgHandle->getHandle());
}

