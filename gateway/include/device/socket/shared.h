/*
 * shared.h
 *
 *  Created on: May 28, 2016
 *      Author: james
 */

#ifndef DEVICE_SOCKET_TCP_SHARED_H_
#define DEVICE_SOCKET_TCP_SHARED_H_

#include <boost/smart_ptr/shared_array.hpp>
#include <device/VirtualDevice.h>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>
#include <sstream>

#include "ble/gatt.h"
#include "ble/helper.h"
#include "Debug.h"
#include "util/write.h"

typedef struct {
	boost::shared_array<uint8_t> buf;
	int len;
} delayed_packet_t;

inline bool request_device_name(int fd, std::string &name, std::list<delayed_packet_t> &other) {
	uint8_t *req = NULL;
	int reqLen = pack_read_by_type_req_pdu(GATT_GAP_CHARAC_DEVICE_NAME_UUID, 0x1, 0xFFFF, req);

	if (write_all(fd, req, reqLen) != reqLen) {
		return false;
	}

	int respLen;
	uint8_t resp[256];
	while (true) {
		int n = read(fd, resp, sizeof(resp));
		if (debug_socket) {
			pdebug("read " + std::to_string(n) + " bytes");
		}
		if (n <= 0) {
			if (debug_socket) {
				std::stringstream ss;
				ss << "socket errno: " << strerror(errno);
			}
			break;
		} else {
			if (debug_socket) {
				pdebug(resp, n);
			}
			if (resp[0] == ATT_OP_READ_BY_TYPE_RESP) {
				respLen = n;
				break;
			} else if (resp[0] == ATT_OP_ERROR) {
				return false;
			} else {
				delayed_packet_t packet;
				packet.buf = boost::shared_array<uint8_t>(new uint8_t(n));
				packet.len = n;
				memcpy(packet.buf.get(), resp, n);
				other.push_back(packet);
			}
		}
	}

	if (respLen < 4) {
		return false;
	} else {
		resp[respLen] = '\0';
		name = std::string((char *)(resp + 4));
		return true;
	}
}

#endif /* DEVICE_SOCKET_TCP_SHARED_H_ */
