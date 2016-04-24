/*
 * NetworkDiscovery.cpp
 *
 *  Created on: Apr 21, 2016
 *      Author: james
 */

#include <controller/NetworkDiscovery.h>

#include <boost/network/message/directives.hpp>
#include <boost/network/message/wrappers/body.hpp>
#include <boost/network/message.hpp>
#include <boost/network/message/directives/header.hpp>
#include <boost/network/protocol/http/client/facade.hpp>
#include <boost/network/protocol/http/client/options.hpp>
#include <json/json.hpp>
#include <sstream>

#include <Debug.h>

using json = nlohmann::json;

NetworkDiscovery::NetworkDiscovery(ControllerClient &client) : client(client) {

}

NetworkDiscovery::~NetworkDiscovery() {

}

bool NetworkDiscovery::discoverDevices(std::list<discovery_result_t> &ret) {
	std::stringstream resource;
	resource << "network/discover/entities";
	return queryHelper(resource.str(), ret);
}

bool NetworkDiscovery::discoverByUuid(UUID uuid, std::list<discovery_result_t> &ret, bool isService) {
	std::stringstream resource;
	resource << "network/discover/" << ((isService) ? "service" : "char") << "/" << uuid.str();
	return queryHelper(resource.str(), ret);
}

bool NetworkDiscovery::findGatewayByName(std::string name, std::string &ip, int &port) {
	std::string url = client.getUrl("network/find/gateway/" + name);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");
	auto response = client.getClient()->get(request);
	if (response.status() == 200) {
		std::stringstream ss;
		ss << body(response);
		json j;
		j << ss;
		ip = j["ip"];
		port = j["port"];
		return true;
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "error : " << body(response);
			pwarn(ss.str());
		}
		return false;
	}
}

bool NetworkDiscovery::queryHelper(std::string resource, std::list<discovery_result_t> &ret) {
	std::string url = client.getUrl(resource);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	auto response = client.getClient()->get(request);
	if (response.status() == 200) {
		std::stringstream ss;
		ss << body(response);
		json j;
		j << ss;
		for (auto &it : j) {
			discovery_result_t result;
			result.name = it["entity"]["name"];
			result.id = it["entity"]["id"];
			result.gateway = it["gateway"]["name"];
			result.ip = it["gateway"]["ip"];
			result.port = it["gateway"]["port"];
			ret.push_back(result);
		}
		return true;
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "network discovery failed : " << body(response);
			pwarn(ss.str());
		}
		return false;
	}
}
