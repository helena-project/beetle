/*
 * AccessControl.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <controller/AccessControl.h>

#include <cpr/cpr.h>
#include <iostream>
#include <json/json.hpp>
#include <sstream>

#include <controller/Controller.h>
#include <Debug.h>
#include <device/socket/tcp/TCPServerProxy.h>
#include <Device.h>


using json = nlohmann::json;

AccessControl::AccessControl(Beetle &beetle, std::string hostAndPort_) : beetle(beetle) {
	hostAndPort = hostAndPort_;
}

AccessControl::~AccessControl() {
}

bool AccessControl::canMap(Device *from, Device *to) {

	std::string fromGateway;
	device_t fromId;
	switch (from->getType()) {
	case Device::BEETLE_INTERNAL:
		return true;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CONNECTION: {
		fromGateway = beetle.name;
		fromId = from->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
		return false;
	case Device::TCP_SERVER_PROXY: {
		TCPServerProxy *fromCast = dynamic_cast<TCPServerProxy *>(from);
		fromGateway = fromCast->getServerGateway();
		fromId = fromCast->getId();
		break;
	}
	case Device::UNKNOWN:
	default:
		return false;
	}

	std::string toGateway;
	device_t toId;
	switch (from->getType()) {
	case Device::BEETLE_INTERNAL:
		return false;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CONNECTION: {
		toGateway = beetle.name;
		toId = to->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
	case Device::TCP_SERVER_PROXY:
	case Device::UNKNOWN:
	default:
		return false;
	}

	std::stringstream resource;
	resource << "access/canMap/" << fromGateway << "/" << std::fixed << fromId
			<< "/" << toGateway << "/" << std::fixed << toId;

	auto response = cpr::Post(
			cpr::Url{getUrl(hostAndPort, resource.str())},
			cpr::Header{{"User-Agent", "linux"}});

	switch (response.status_code) {
	case 200: {
		if (debug_network) {
			pdebug("controller request ok");
		}
		return handleCanMapResponse(from, to, response.text);
	}
	case 500:
		pwarn("internal server error");
		return false;
	default:
		if (debug_network) {
			pdebug("controller request failed " + std::to_string(response.status_code));
		}
		return false;
	}
}

/*
 * Unpacks the controller response and returns whether the mapping is allowed.
 */
bool AccessControl::handleCanMapResponse(Device *from, Device *to, std::string body) {
	json j = json::parse(body);
	bool result = j["result"];

	std::cout << j << std::endl;

	return result;
}
