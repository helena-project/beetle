/*
 * Auth.cpp
 *
 *  Created on: Apr 24, 2016
 *      Author: James Hong
 */

#include <controller/access/DynamicAuth.h>

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "controller/ControllerClient.h"
#include "Debug.h"
#include "Device.h"
#include "device/socket/tcp/TCPClient.h"

DynamicAuth::DynamicAuth(rule_t ruleId_) {
	ruleId = ruleId_;
}

static bool isPrivateAddress(uint32_t ip) {
	uint8_t b1, b2;
	b1 = (uint8_t) (ip >> 24);
	b2 = (uint8_t) ((ip >> 16) & 0x0ff);
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

NetworkAuth::NetworkAuth(rule_t r, std::string ip_, bool isPrivate_) :
		DynamicAuth(r) {
	isPrivate = isPrivate_;
	ip = ip_;
}

void NetworkAuth::evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
		std::shared_ptr<Device> to) {
	auto tcpTo = std::dynamic_pointer_cast<TCPClient>(to);
	if (tcpTo) {
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

PasscodeAuth::PasscodeAuth(rule_t r) :
		DynamicAuth(r) {

}

void PasscodeAuth::evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
		std::shared_ptr<Device> to) {
	if (state == SATISFIED) {
		return;
	}

	/*
	 * Must be an endpoint, not a device created by Beetle.
	 */
	switch (to->getType()) {
	case Device::LE_PERIPHERAL:
	case Device::IPC_APPLICATION:
	case Device::TCP_CLIENT: {
		std::stringstream resource;
		resource << "acstate/passcode/isLive/" << std::fixed << ruleId << "/" << cc->getName() << "/" << std::fixed
				<< to->getId();
		std::string url = cc->getUrl(resource.str());

		using namespace boost::network;
		http::client::request request(url);
		request << header("User-Agent", "linux");

		try {
			auto response = cc->getClient()->get(request);
			if (response.status() == 200) {
				std::stringstream ss;
				ss << body(response);
				expire = static_cast<time_t>(std::stod(ss.str()));
				state = SATISFIED;
			} else {
				state = UNATTEMPTED;
			}
		} catch (std::exception &e) {
			std::stringstream ss;
			ss << "caught exception: " << e.what();
			pwarn(ss.str());
			state = DENIED;
		}
		break;
	}
	default:
		state = SATISFIED;
	}

	if (debug_controller) {
		std::stringstream ss;
		ss << "evaluated passcode auth : " << ((state == SATISFIED) ? "satisfied" : "not satisfied");
		pdebug(ss.str());
	}
}

AdminAuth::AdminAuth(rule_t r) :
		DynamicAuth(r) {

}

void AdminAuth::evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
		std::shared_ptr<Device> to) {
	if (state == SATISFIED) {
		return;
	}

	if (state == UNATTEMPTED) {
		switch (to->getType()) {
		case Device::LE_PERIPHERAL:
		case Device::IPC_APPLICATION:
		case Device::TCP_CLIENT: {
			std::stringstream resource;
			resource << "acstate/admin/request/" << std::fixed << ruleId << "/" << cc->getName() << "/" << std::fixed
					<< to->getId();
			std::string url = cc->getUrl(resource.str());

			using namespace boost::network;
			http::client::request request(url);
			request << header("User-Agent", "linux");

			try {
				auto response = cc->getClient()->post(request);
				if (response.status() == 202) {
					std::stringstream ss;
					ss << body(response);
					expire = static_cast<time_t>(std::stod(ss.str()));
					state = SATISFIED;
				} else {
					state = DENIED;
				}
			} catch (std::exception &e) {
				std::stringstream ss;
				ss << "caught exception: " << e.what();
				pwarn(ss.str());
				state = DENIED;
			}
			break;
		}
		default:
			state = SATISFIED;
		}
	}

	if (debug_controller) {
		std::stringstream ss;
		ss << "evaluated admin auth : " << ((state == SATISFIED) ? "satisfied" : "not satisfied");
		pdebug(ss.str());
	}
}

UserAuth::UserAuth(rule_t r) :
		DynamicAuth(r) {

}

void UserAuth::evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
		std::shared_ptr<Device> to) {
	if (state == SATISFIED) {
		return;
	}

	if (state == UNATTEMPTED) {
		switch (to->getType()) {
		case Device::LE_PERIPHERAL:
		case Device::IPC_APPLICATION:
		case Device::TCP_CLIENT: {
			std::stringstream resource;
			resource << "acstate/user/request/" << std::fixed << ruleId << "/" << cc->getName() << "/" << std::fixed
					<< to->getId();
			std::string url = cc->getUrl(resource.str());

			using namespace boost::network;
			http::client::request request(url);
			request << header("User-Agent", "linux");

			try {
				auto response = cc->getClient()->post(request);
				if (response.status() == 202) {
					std::stringstream ss;
					ss << body(response);
					expire = static_cast<time_t>(std::stod(ss.str()));
					state = SATISFIED;
				} else {
					state = DENIED;
				}
			} catch (std::exception &e) {
				std::stringstream ss;
				ss << "caught exception: " << e.what();
				pwarn(ss.str());
				state = DENIED;
			}
			break;
		}
		default:
			state = SATISFIED;
		}
	}

	if (debug_controller) {
		std::stringstream ss;
		ss << "evaluated user auth : " << ((state == SATISFIED) ? "satisfied" : "not satisfied");
		pdebug(ss.str());
	}
}
