/*
 * Auth.cpp
 *
 *  Created on: Apr 24, 2016
 *      Author: james
 */

#include <controller/access/DynamicAuth.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdint>

#include <device/socket/tcp/TCPClientProxy.h>
#include <device/socket/tcp/TCPServerProxy.h>
#include <Debug.h>
#include <sstream>

DynamicAuth::DynamicAuth() {

}

DynamicAuth::~DynamicAuth() {

}

static bool isPrivateAddress(uint32_t ip) {
    uint8_t b1, b2;
    b1 = (uint8_t)(ip >> 24);
    b2 = (uint8_t)((ip >> 16) & 0x0ff);
    if (b1 == 10) {
    	return true;
    }
    if ((b1 == 172) && (b2 >= 16) && (b2 <= 31)) {
    	return true;
    }
    if ((b1 == 192) && (b2 == 168)) {
    	return true;
    }
    return false;
}

void NetworkAuth::evaluate(Device *from, Device *to) {
	TCPConnection *tcpTo = dynamic_cast<TCPConnection *>(to);
	if (tcpTo && !dynamic_cast<TCPClientProxy *>(to) && !dynamic_cast<TCPServerProxy *>(to)) {
		struct sockaddr_in addr = tcpTo->getSockaddr();
		if (isPrivate) {
			state = isPrivateAddress(ntohl(addr.sin_addr.s_addr)) ? SATISFIED : DENIED;
		} else {
			struct in_addr toAddr;
			inet_aton(ip.c_str(), &toAddr);
			state = (addr.sin_addr.s_addr == toAddr.s_addr) ? SATISFIED : DENIED;
		}
	} else {
		state = SATISFIED;
	}
	if (debug_controller) {
		std::stringstream ss;
		ss << "evaluated network auth : " << ((state == SATISFIED) ? "satisfied" : "denied");
		pdebug(ss.str());
	}
}
