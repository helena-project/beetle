/*
 * AccessControl.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <controller/AccessControl.h>

#include <boost/network/protocol/http/client/facade.hpp>
#include <boost/network/protocol/http/message/async_message.hpp>
#include <boost/network/protocol/http/message/wrappers/body.hpp>
#include <iostream>
#include <json/json.hpp>
#include <sstream>

#include <controller/shared.h>
#include <Debug.h>
#include <device/socket/tcp/TCPServerProxy.h>
#include <Device.h>


using json = nlohmann::json;

AccessControl::AccessControl(Beetle &beetle, std::string hostAndPort_) : beetle(beetle) {
	hostAndPort = hostAndPort_;


}

AccessControl::~AccessControl() {
	// TODO Auto-generated destructor stub
}

bool AccessControl::canMap(Device *from, Device *to) {
	using namespace boost::network;

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
	resource << "policy/canMap/" << fromGateway << "/" << std::fixed << fromId
			<< "/" << toGateway << "/" << std::fixed << toId;

	http::client::request request(getUrl(hostAndPort, resource.str()));
	http::client::response response = client.get(request);

	switch (response.status()) {
	case 200: {
		if (debug_network) {
			pdebug("controller request ok");
		}
		std::stringstream ss;
		ss << body(response);
		return handleCanMapResponse(from, to, ss.str());
	}
	case 500:
		pwarn("internal server error");
		return false;
	default:
		if (debug_network) {
			pdebug("controller request failed " + std::to_string(response.status()));
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
	/*
	 * TODO cache rest of the response
	 */
	return result;
}
