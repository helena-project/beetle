/*
 * NetworkDiscovery.cpp
 *
 *  Created on: Apr 21, 2016
 *      Author: James Hong
 */

#include "controller/NetworkDiscoveryClient.h"

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <exception>
#include <iostream>
#include <json/json.hpp>
#include <sstream>

#include "Beetle.h"
#include "Debug.h"
#include "controller/ControllerClient.h"

using json = nlohmann::json;

NetworkDiscoveryClient::NetworkDiscoveryClient(Beetle &beetle, std::shared_ptr<ControllerClient> client_) :
		beetle(beetle) {
	client = client_;
}

NetworkDiscoveryClient::~NetworkDiscoveryClient() {

}

bool NetworkDiscoveryClient::discoverDevices(std::list<discovery_result_t> &ret) {
	std::stringstream resource;
	resource << "network/discover/devices";
	return queryHelper(resource.str(), ret);
}

bool NetworkDiscoveryClient::discoverByUuid(UUID uuid, std::list<discovery_result_t> &ret, bool isService, device_t d) {
	std::stringstream resource;
	resource << "network/discover/" << ((isService) ? "service" : "char") << "/" << uuid.str();
	if (d != -1) {
		resource << "?gateway=" << beetle.name << "&" << "remote_id" << std::fixed << d;
	}
	return queryHelper(resource.str(), ret);
}

bool NetworkDiscoveryClient::findGatewayByName(std::string name, std::string &ip, int &port) {
	std::string url = client->getUrl("network/find/gateway/" + name);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	try {
		auto response = client->getClient()->get(request);
		if (response.status() == 200) {
			std::stringstream ss;
			ss << body(response);
			json j;
			j << ss;
			ip = j["ip"];
			port = j["port"];
			return true;
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "error : " << body(response);
				pwarn(ss.str());
			}
			return false;
		}
	} catch (std::exception &e) {
		pexcept(e);
		return false;
	}
}

bool NetworkDiscoveryClient::queryHelper(std::string resource, std::list<discovery_result_t> &ret) {
	std::string url = client->getUrl(resource);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	try {
		auto response = client->getClient()->get(request);
		if (response.status() == 200) {
			std::stringstream ss;
			ss << body(response);
			json j;
			j << ss;
			for (auto &it : j) {
				discovery_result_t result;
				result.name = it["device"]["name"];
				result.id = it["device"]["id"];
				result.gateway = it["gateway"]["name"];
				result.ip = it["gateway"]["ip"];
				result.port = it["gateway"]["port"];
				ret.push_back(result);
			}
			return true;
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "network discovery failed : " << body(response);
				pwarn(ss.str());
			}
			return false;
		}
	} catch (std::exception &e) {
		pexcept(e);
		return false;
	}
}

void NetworkDiscoveryClient::registerInterestInUuid(device_t id, UUID uuid, bool isService, std::function<void()> cb) {
	std::stringstream resource;
	resource << "network/registerInterest/" << ((isService) ? "service" : "char") << "/" << beetle.name << "/"
			<< std::fixed << id << "/" << uuid.str();

	std::string url = client->getUrl(resource.str());

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	try {
		auto response = client->getClient()->post(request);
		if (response.status() != 200) {
			if (debug_controller) {
				std::stringstream ss;
				ss << "error registering interest: " << body(response);
				pdebug(ss.str());
			}
		}
	} catch (std::exception &e) {
		pexcept(e);
	}

	/*
	 * TODO: might want to wait
	 */
	cb();
}
