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
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <utility>

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
		result = routeHandleNotification(buf, len, src);
		break;
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_SIGNED_WRITE_CMD:
		result = routeReadWrite(buf, len, src);
		break;
	default:
		result = -1;
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
				std::lock_guard<std::recursive_mutex> handlesLg(beetle.devices[dst]->handlesMutex);
				for (auto &mapping : beetle.devices[dst]->handles) {
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
//		bool cutShort = false;
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
				std::lock_guard<std::recursive_mutex> handlesLg(beetle.devices[dst]->handlesMutex);

				for (auto &mapping : beetle.devices[dst]->handles) {
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
						int cmpLen = (attValLen < handle->cache.len) ? attValLen : handle->cache.len;
						if (memcmp(handle->cache.value, attValue, cmpLen) == 0) {
							*(uint16_t *)(resp + respLen) = htobs(offset);
							*(uint16_t *)(resp + respLen + 2) = htobs(handle->getEndGroupHandle());
							respLen += 4;
							respHandleCount++;

							if (respLen + 4 > srcMTU) {
								done = true;
//								cutShort = true;
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

int Router::routeHandleNotification(uint8_t *buf, int len, device_t src) {
	boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
	boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
	std::lock_guard<std::recursive_mutex> handlesLg(beetle.devices[src]->handlesMutex);

	uint16_t handle = btohs(*(uint16_t *)(buf + 1));
	Handle *h = beetle.devices[src]->handles[handle];
	if (h == NULL) {
		return -1; // no such handle
	}

	handle_range_t handleRange = beetle.hat->getDeviceRange(src);
	if (handleRange.start == 0 && handleRange.start == handleRange.end) {
		return -1; // this device has no mapped handles
	} else {
		handle += handleRange.start;
		*(uint16_t *)(buf + 1) = htobs(handle);
		for (device_t dst : h->subscribers) {
			beetle.devices[dst]->writeCommand(buf, len);
		}
	}
	return 0;
}

int Router::routeReadWrite(uint8_t *buf, int len, device_t src) {
	boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
	boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
	uint8_t opCode = buf[0];
	uint16_t handle = btohs(*(uint16_t *)(buf + 1));
	device_t dst = beetle.hat->getDeviceForHandle(src);
	handle_range_t handleRange = beetle.hat->getDeviceRange(src);
	if (dst == BEETLE_RESERVED_DEVICE) {

	} else if (dst == NULL_RESERVED_DEVICE || dst == src) {
		uint8_t *err;
		int len = pack_error_pdu(buf[0], handle, ATT_ECODE_ATTR_NOT_FOUND, err);
		beetle.devices[src]->writeResponse(err, len);
		delete[] err;
	} else {
		std::lock_guard<std::recursive_mutex> handlesLg(beetle.devices[dst]->handlesMutex);
		uint16_t remoteHandle = handle - handleRange.start;
		Handle *proxyH = beetle.devices[dst]->handles[remoteHandle];
		if (proxyH == NULL) {
			uint8_t *err;
			int len = pack_error_pdu(buf[0], handle, ATT_ECODE_ATTR_NOT_FOUND, err);
			beetle.devices[src]->writeResponse(err, len);
			delete[] err;
		} else {
			if (opCode == ATT_OP_WRITE_REQ && proxyH->getUuid().isShort()
					&& proxyH->getUuid().getShort() == GATT_CLIENT_CHARAC_CFG_UUID) {
				uint16_t charHandle = proxyH->getCharHandle();
				Handle *charH = beetle.devices[dst]->handles[charHandle];
				if (buf[3] == 0) {
					charH->subscribers.erase(src);
				} else {
					charH->subscribers.insert(src);
				}
				int numSubscribers = charH->subscribers.size();
				if ((numSubscribers == 0 && buf[3] == 0) || (numSubscribers == 1 && buf[3] == 1)) {
					*(uint16_t *)(buf + 1) = htobs(remoteHandle);
					beetle.devices[dst]->writeTransaction(buf, len, [this, handle, src, dst, opCode](uint8_t *resp, int respLen) -> void {
						boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
						boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
						if (resp == NULL) {
							uint8_t *err;
							int len = pack_error_pdu(opCode, handle, ATT_ECODE_UNLIKELY, err);
							beetle.devices[src]->writeResponse(err, len);
							delete[] err;
						} else {
							beetle.devices[src]->writeResponse(resp, respLen);
						}
					});
				} else {
					uint8_t resp = ATT_OP_WRITE_RESP;
					beetle.devices[src]->writeResponse(&resp, 1);
				}
			} else {
				if (opCode == ATT_OP_READ_REQ && proxyH->cache.value != NULL
						&& proxyH->cache.cachedSet.find(src) == proxyH->cache.cachedSet.end()
						&& proxyH->isCacheInfinite()) {
					proxyH->cache.cachedSet.insert(src);
					int respLen = 1 + proxyH->cache.len;
					uint8_t *resp = new uint8_t[respLen];
					resp[0] = ATT_OP_READ_RESP;
					memcpy(resp + 1, proxyH->cache.value, proxyH->cache.len);
					beetle.devices[src]->writeResponse(resp, respLen);
					delete[] resp;
				} else {
					*(uint16_t *)(buf + 1) = htobs(remoteHandle);
					if (opCode == ATT_OP_WRITE_CMD || opCode == ATT_OP_SIGNED_WRITE_CMD) {
						beetle.devices[dst]->writeCommand(buf, len);
					} else {
						beetle.devices[dst]->writeTransaction(buf, len,
								[this, opCode, src, dst, handle, remoteHandle](uint8_t *resp, int respLen) -> void {
							boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
							boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
							if (resp == NULL) {
								uint8_t *err;
								int len = pack_error_pdu(opCode, handle, ATT_ECODE_UNLIKELY, err);
								beetle.devices[src]->writeResponse(err, len);
								delete[] err;
							} else {
								if (resp[0] == ATT_OP_READ_RESP) {
									std::lock_guard<std::recursive_mutex> handlesLg(beetle.devices[dst]->handlesMutex);
									Handle *proxyH = beetle.devices[dst]->handles[remoteHandle];
									proxyH->cache.cachedSet.clear();
									int tmpLen = respLen - 1;
									uint8_t *tmpVal = new uint8_t[respLen - 1];
									memcpy(proxyH->cache.value, resp, respLen - 1);
									proxyH->cache.set(tmpVal, tmpLen);
								}
								beetle.devices[src]->writeResponse(resp, respLen);
							}
						});
					}
				}

			}
		}
	}
	return 0;
}
