/*
 * NetworkDiscovery.cpp
 *
 *  Created on: Apr 21, 2016
 *      Author: james
 */

#include <controller/NetworkDiscovery.h>

#include <controller/Controller.h>
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

NetworkDiscovery::NetworkDiscovery(std::string hostAndPort_) {
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

NetworkDiscovery::~NetworkDiscovery() {
	delete client; // teardown race
}

std::list<discovery_result_t> NetworkDiscovery::discoverDevices() {
	std::stringstream resource;
	resource << "network/discover/entities";
	return queryHelper(resource.str());
}

std::list<discovery_result_t> NetworkDiscovery::discoverByUuid(UUID uuid, bool isService) {
	std::stringstream resource;
	resource << "network/discover/" << ((isService) ? "service" : "char") << "/" << uuid.str();
	return queryHelper(resource.str());
}

std::list<discovery_result_t> NetworkDiscovery::queryHelper(std::string resource) {
	std::string url = getUrl(hostAndPort, resource);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	std::list<discovery_result_t> ret;

	auto response = client->get(request);
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
	} else {
		std::stringstream ss;
		ss << "network discovery failed : " << body(response);
		pwarn(ss.str());
	}
	return ret;
}
