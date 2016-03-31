/*
 * Router.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#include "Router.h"

#include <assert.h>
#include <bluetooth/bluetooth.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <cstdbool>
#include <cstdint>
#include <utility>
#include <vector>

#include "ble/att.h"
#include "ble/gatt.h"
#include "ble/helper.h"
#include "Beetle.h"
#include "hat/HAT.h"
#include "Handle.h"
#include "UUID.h"

Router::Router(Beetle &beetle_) : beetle(beetle_) {

}

Router::~Router() {

}

int Router::route(uint8_t *buf, int len, device_t src) {
	assert(len > 0);
	int result;

	switch(buf[0]) {
	case ATT_OP_FIND_INFO_REQ:
		result = routeFindInfo(buf, len, src);
		break;
	case ATT_OP_FIND_BY_TYPE_REQ:
		result = routeFindByTypeValue(buf, len, src);
		break;
	case ATT_OP_READ_BY_TYPE_REQ:
		result = routeReadByType(buf, len, src);
		break;
	case ATT_OP_HANDLE_NOTIFY:
		// TODO
		break;
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_SIGNED_WRITE_CMD:
		// TODO
		break;
	default:
		// TODO unsupported
		break;
	}

	return result;
}

bool parseFindInfoRequest(uint8_t *buf, int len, uint16_t &startHandle, uint16_t &endHandle) {
	if (len < 5) {
		return false; // invalid length
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	return true;
}

int Router::routeFindInfo(uint8_t *buf, int len, device_t src) {
	boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
	uint16_t startHandle;
	uint16_t endHandle;
	if (!parseFindInfoRequest(buf, len, startHandle, endHandle)) {
		uint8_t *err;
		int len = pack_error_pdu(ATT_OP_FIND_INFO_REQ, 0, ATT_ECODE_INVALID_PDU, err);
		beetle.devices[src]->writeResponse(err, len);
		delete[] err;
		return -1;
	} else {
		boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);

		int srcMTU = beetle.devices[src]->getMTU();
		int respLen = 0;
		int respHandleCount = 0;
		uint8_t *resp = new uint8_t[srcMTU];
		resp[0] = ATT_OP_FIND_INFO_RESP;
		resp[1] = 1; // TODO handle non standard uuids (cb4.2 p2177)
		respLen += 2;

		uint16_t currHandle = startHandle;
		bool done = false;
		while (currHandle <= endHandle && !done) {
			device_t dst = beetle.hat->getDeviceForHandle(startHandle);
			handle_range_t handleRange = beetle.hat->getHandleRange(currHandle);
			if (dst == BEETLE_RESERVED_DEVICE) { // 0
				// beetle respond
			} else if (dst == NULL_RESERVED_DEVICE) { // -1
				continue;
			} else if (dst == src) {
				continue;
			} else if (dst >= 0) {
				for (auto &mapping : beetle.devices[dst]->getHandles()) {
					uint16_t offset = mapping.first + handleRange.start;
					if (offset < startHandle) {
						continue;
					} else if (offset > endHandle) {
						done = true;
						break;
					}

					// TODO this allows 16bit handles only
					Handle *handle = mapping.second;
					*(uint16_t *)(resp + respLen) = htobs(offset);
					*(uint16_t *)(resp + respLen + 2) = htobs(handle->getUuid().getShort());
					respLen += 4;
					respHandleCount++;
					if (respLen + 4 > srcMTU) {
						done = true;
						break;
					}
				}
				if (done) {
					break;
				}
			}
			currHandle = handleRange.end + 1;
		}

		if (respHandleCount > 0) {
			beetle.devices[src]->writeResponse(resp, respLen);
		} else {
			uint8_t *err;
			int len = pack_error_pdu(ATT_OP_FIND_INFO_REQ, startHandle, ATT_ECODE_ATTR_NOT_FOUND, err);
			beetle.devices[src]->writeResponse(err, len);
			delete[] err;
		}

		delete[] resp;
		return 0;
	}
}

bool parseFindByTypeValueRequest(uint8_t *buf, int len, uint16_t &startHandle, uint16_t &endHandle, uint16_t &attType, uint8_t *&attValue, int &attValLen) {
	if (len < 7) {
		return false;
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	attType = *(uint16_t *)(buf + 5);
	attValue = buf + 7;
	attValLen = len - 7;
	return true;
}

int Router::routeFindByTypeValue(uint8_t *buf, int len, device_t src) {
	boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
	uint16_t startHandle;
	uint16_t endHandle;
	uint16_t attType;
	uint8_t *attValue;
	int attValLen;
	if (!parseFindByTypeValueRequest(buf, len, startHandle, endHandle, attType, attValue, attValLen)) {
		uint8_t *err;
		int len = pack_error_pdu(ATT_OP_FIND_BY_TYPE_REQ, 0, ATT_ECODE_INVALID_PDU, err);
		beetle.devices[src]->writeResponse(err, len);
		delete[] err;
		return -1;
	} else {
		boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);

		UUID attUUID(attType);

		int srcMTU = beetle.devices[src]->getMTU();
		int respLen = 0;
		int respHandleCount = 0;
		bool cutShort = false;
		uint8_t *resp = new uint8_t[srcMTU];
		resp[0] = ATT_OP_FIND_BY_TYPE_RESP;
		respLen++;

		uint16_t currHandle = startHandle;
		bool done = false;
		while (currHandle <= endHandle && !done) {
			device_t dst = beetle.hat->getDeviceForHandle(startHandle);
			handle_range_t handleRange = beetle.hat->getHandleRange(currHandle);
			if (dst == BEETLE_RESERVED_DEVICE) { // 0
				// beetle respond
			} else if (dst == NULL_RESERVED_DEVICE){ // -1
				continue;
			} else if (dst == src) {
				continue;
			} else if (dst >= 0) {
				for (auto &mapping : beetle.devices[dst]->getHandles()) {
					uint16_t offset = mapping.first + handleRange.start;
					if (offset < startHandle) {
						continue;
					} else if (offset > endHandle) {
						done = true;
						break;
					}

					// TODO handle non standard uuids (cb4.2 p2180)
					Handle *handle = mapping.second;
					if (handle->getUuid().getShort() == attType) {
						CachedHandle cached = handle->getCached();
						int cmpLen = (attValLen < cached.len) ? attValLen : cached.len;
						if (memcmp(cached.value, attValue, cmpLen) == 0) {
							*(uint16_t *)(resp + respLen) = htobs(offset);
							*(uint16_t *)(resp + respLen + 2) = htobs(handle->getEndGroupHandle());
							respLen += 4;
							respHandleCount++;

							if (respLen + 4 > srcMTU) {
								done = true;
								cutShort = true;
								resp[respLen - 1] = 0xFF;
								resp[respLen - 2] = 0xFF;
								break;
							}
						}
					}
				}
				if (done) {
					break;
				}
			}
			currHandle = handleRange.end + 1;
		}

		if (respHandleCount > 0) {
			beetle.devices[src]->writeResponse(resp, respLen);
		} else {
			uint8_t *err;
			int len = pack_error_pdu(ATT_OP_FIND_BY_TYPE_RESP, startHandle, ATT_ECODE_ATTR_NOT_FOUND, err);
			beetle.devices[src]->writeResponse(err, len);
			delete[] err;
		}

		delete[] resp;
		return 0;
	}
}

bool parseReadByTypeValueRequest(uint8_t *buf, int len, uint16_t &startHandle, uint16_t &endHandle, UUID *&uuid) {
	if (len != 7 && len != 21) {
		return false;
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	uuid = new UUID(buf + 5, len - 5);
	return true;
}

//int packReadByTypeRequest(uint *&buf, uint16_t startHandle, uint16_t endHandle, UUID attType) {
//	if (attType.isShort()) {
//		buf = new uint8_t[7];
//	} else {
//		buf = new uint8_t[21];
//	}
//}

int Router::routeReadByType(uint8_t *buf, int len, device_t src) {
	boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
	uint16_t startHandle;
	uint16_t endHandle;
	UUID *attType;
	if (!parseReadByTypeValueRequest(buf, len, startHandle, endHandle, attType)) {
		uint8_t *err;
		int len = pack_error_pdu(ATT_OP_READ_BY_TYPE_REQ, 0, ATT_ECODE_INVALID_PDU, err);
		beetle.devices[src]->writeResponse(err, len);
		delete[] err;
		return -1;
	} else {
		boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
		device_t dst = beetle.hat->getDeviceForHandle(startHandle);
		if (dst == BEETLE_RESERVED_DEVICE) { // 0
			// TODO

		} else if (dst != NULL_RESERVED_DEVICE && dst != src) {
			handle_range_t handleRange = beetle.hat->getHandleRange(startHandle);
			// TODO forward the request to the device where the range falls, this does not work if you want to read across multiple devices
			*(uint16_t *)(buf + 1) = htobs(startHandle - handleRange.start);
			*(uint16_t *)(buf + 3) = htobs(endHandle - handleRange.start);
			beetle.devices[dst]->writeTransaction(buf, len, [this, src, dst, attType, handleRange, startHandle](uint8_t *resp, int respLen) -> void {
				boost::shared_lock<boost::shared_mutex> lkDevices(this->beetle.devicesMutex);
				if (resp == NULL || respLen <= 2) {
					uint8_t *err;
					int len = pack_error_pdu(ATT_OP_READ_BY_TYPE_REQ, startHandle, ATT_ECODE_ABORTED, err);
					beetle.devices[src]->writeResponse(err, len);
					delete[] err;
				} else if (resp[0] == ATT_OP_ERROR) {
					*(uint16_t *)(resp + 2) = htobs(startHandle);
					beetle.devices[src]->writeResponse(resp, respLen);
				} else {
					boost::shared_lock<boost::shared_mutex> lkDevices(this->beetle.hatMutex);
					int segLen = resp[1];
					for (int i = 2; i < respLen; i += segLen) {
						uint16_t handle = btohs(*(uint16_t *)(resp + i));
						handle += handleRange.start;
						*(uint16_t *)(resp + i) = htobs(handle);
						if (attType->isShort() && attType->getShort() == GATT_CHARAC_UUID) { // TODO: endianness is sketchy here
							int j = i + 3;
							uint16_t valueHandle = btohs(*(uint16_t *)(resp + j));
							valueHandle += handleRange.start;
							*(uint16_t *)(resp + j) = htobs(valueHandle);
						}
					}
					this->beetle.devices[src]->writeResponse(resp, respLen);
				}
				delete attType;
			});
		} else {
			uint8_t *err;
			int len = pack_error_pdu(ATT_OP_FIND_BY_TYPE_RESP, startHandle, ATT_ECODE_ATTR_NOT_FOUND, err);
			beetle.devices[src]->writeResponse(err, len);
			delete[] err;
		}
	}
	return 0;
}
