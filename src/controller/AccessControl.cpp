/*
 * AccessControl.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <controller/AccessControl.h>

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/thread/lock_types.hpp>
#include <exception>
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

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
//	       .openssl_certificate("/tmp/my-cert")
//	       .openssl_verify_path("/tmp/ca-certs")
	       .timeout(10);

	client = new http::client(options);
}

AccessControl::~AccessControl() {
//	delete client;
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

	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, resource.str()));
		request << header("User-Agent", "linux");
	auto response = client->delete_(request);

	switch (response.status()) {
	case 200: {
		if (debug_network) {
			pdebug("controller request ok");
		}
		try {
			std::stringstream ss;
			ss << body(response);
			return handleCanMapResponse(from, to, ss.str());
		} catch (std::exception &e) {
			std::stringstream ss;
			ss << "Error parsing access control server response: " << e.what();
			pwarn(ss.str());
			return false;
		}
	}
	case 500:
		pwarn("internal server error");
		return false;
	default:
		if (debug_network) {
			std::stringstream ss;
			ss << "controller request failed " << response.status();
			pdebug(ss.str());
		}
		return false;
	}
}

/*
 * Unpacks the controller response and returns whether the mapping is allowed.
 */
bool AccessControl::handleCanMapResponse(Device *from, Device *to, std::string body) {
	if (debug_network) {
		pdebug(body);
	}

	json j = json::parse(body);
	bool result = j["result"];

	json access = j["access"];
	json rules = access["rules"];
	json services = access["services"];

	cached_mapping_info_t cacheEntry;
	for (json::iterator it = rules.begin(); it != rules.end(); ++it) {

		std::string key = it.key();
		json value = it.value();

		rule_t ruleId = std::stoi(key);

		Rule rule;
		rule.properties = value["prop"];
		rule.encryption = value["enc"];
		rule.integrity = value["int"];
		std::string lease = value["lease"];
		std::string tStr = value["lease"];
		rule.lease = static_cast<time_t>(std::stod(tStr));

		cacheEntry.rules[ruleId] = rule;
	}

	for (json::iterator it = services.begin(); it != services.end(); ++it) {
		std::string serviceUuid = it.key();
		json chars = it.value();
		if (debug_network) {
			pdebug("service: " + serviceUuid);
		}

		for (json::iterator it2 = chars.begin(); it2 != chars.end(); ++it2) {
			std::string charUuid = it2.key();
			json value = it2.value();
			if (debug_network) {
				pdebug("char: " + charUuid);
			}

			std::set<rule_t> charRulesSet;
			for (json::iterator it = value.begin(); it != value.end(); ++it) {
				rule_t ruleId = *it;
				charRulesSet.insert(ruleId);
			}

			cacheEntry.service_char_rules[serviceUuid][charUuid] = charRulesSet;
		}
	}

	boost::unique_lock<boost::shared_mutex> lk(cacheMutex);
	cache[std::make_pair(from->getId(), to->getId())] = cacheEntry;

	return result;
}
