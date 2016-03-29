/*
 * Router.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#include "Router.h"

#include <assert.h>
#include <bluetooth/bluetooth.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <cstdbool>
#include <cstdint>
#include <utility>
#include <vector>

#include "ble/att.h"
#include "Beetle.h"
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
		result = routeFindByType(buf, len, src);
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

	return 0;
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
	uint16_t startHandle;
	uint16_t endHandle;
	if (!parseFindInfoRequest(buf, len, startHandle, endHandle)) {
		// TODO err
		return -1;
	} else {
		boost::shared_lock<boost::shared_mutex> lkDevices(beetle.devicesMutex);
		boost::shared_lock<boost::shared_mutex> lkHat(beetle.hatMutex);
		std::vector<std::pair<uint16_t, uuid_t>> handles;

		return 0;
	}
}

int Router::routeFindByType(unsigned char *buf, int len, device_t src) {
	return 0;
}

int Router::routeReadByType(unsigned char *buf, int len, device_t src) {
	return 0;
}
