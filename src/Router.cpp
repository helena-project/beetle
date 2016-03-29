/*
 * Router.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#include "Router.h"

#include <mutex>
#include <assert.h>
#include <vector>
#include <utility>
#include <boost/thread.hpp>
#include <bluetooth/bluetooth.h>

#include "ble/att.h"
#include "ble/gatt.h"
#include "Beetle.h"

int Router::route(char *buf, int len, device_t src) {
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

bool parseFindInfoRequest(char *buf, int len, uint16_t &startHandle, uint16_t &endHandle) {
	if (len < 5) {
		return false; // invalid length
	}
	startHandle = btohs(*(uint16_t *)(buf + 1));
	endHandle = btohs(*(uint16_t *)(buf + 3));
	return true;
}

int Router::routeFindInfo(char *buf, int len, device_t src) {
	uint16_t startHandle;
	uint16_t endHandle;
	if (!parseFindInfoRequest(buf, len, startHandle, endHandle)) {
		// TODO err
		return -1;
	} else {
		boost::shared_lock<boost::shared_mutex> lk(beetle.devicesMutex);
		std::vector<std::pair<uint16_t, uuid_t>> handles;

		return 0;
	}
}

int Router::routeFindByType(char *buf, int len, device_t src) {
	return 0;
}

int Router::routeReadByType(char *buf, int len, device_t src) {
	return 0;
}
