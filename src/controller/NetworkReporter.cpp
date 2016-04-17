/*
 * NetworkReporter.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#include "controller/NetworkReporter.h"

#include <boost/network/message/directives/header.hpp>
#include <boost/network/message/directives.hpp>
#include <boost/network/protocol/http/client/facade.hpp>
#include <boost/network/protocol/http/message/async_message.hpp>
#include <boost/network/protocol/http/message/wrappers/body.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <iostream>
#include <json.hpp>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>

#include "Debug.h"
#include "Device.h"
#include "Handle.h"

using json = nlohmann::json;

static std::string getUrl(std::string hostAndPort, std::string resource) {
	std::stringstream ss;
	ss << "http://" << hostAndPort << "/" << resource;
	return ss.str();
}

NetworkReporter::NetworkReporter(Beetle &beetle, std::string hostAndPort_) : beetle(beetle) {
	hostAndPort = hostAndPort_;

//	client::options options;
//	options.follow_redirects(true)
//	       .cache_resolved(true)
//	       .io_service(boost::make_shared<boost::asio::io_service>())
//	       .openssl_certificate("/tmp/my-cert")
//	       .openssl_verify_path("/tmp/ca-certs");
//	client(options);

	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/connect/" + beetle.name));
	http::client::response response = client.post(request);
	if (response.status() != 200) {
		throw NetworkException("error connecting to controller");
	} else {
		if (debug_network) {
			pdebug("connected to controller");
			std::cerr << body(response) << std::endl;
		}
	}
}

NetworkReporter::~NetworkReporter() {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/disconnect/" + beetle.name));
	http::client::response response = client.post(request);
	if (response.status() != 200) {
		throw NetworkException("error disconnecting from controller");
	} else {
		if (debug_network) {
			pdebug("disconnected from controller");
			std::cerr << body(response) << std::endl;
		}
	}
}

AddDeviceHandler NetworkReporter::getAddDeviceHandler() {
	return [this](device_t d) {
		boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
		if (beetle.devices.find(d) == beetle.devices.end()) {
			if (debug_network) {
				pwarn("tried to add device that does not exist" + std::to_string(d));
			}
			return;
		}
		Device *device = beetle.devices[d];
		switch (device->getType()) {
		case Device::IPC_APPLICATION:
		case Device::LE_PERIPHERAL:
		case Device::TCP_CONNECTION:
			if (debug_network) {
				pdebug("informing controller of new connection");
			}
			try {
				addDeviceHelper(device);
			} catch (std::exception &e) {
				std::cerr << "caught exception: " << e.what() << std::endl;
			}
			break;
		default:
			if (debug_network) {
				pdebug("not reporting " + device->getTypeStr());
			}
			return;
		}
	};
}

RemoveDeviceHandler NetworkReporter::getRemoveDeviceHandler() {
	return [this](device_t d){
		try {
			removeDeviceHelper(d);
		} catch (std::exception &e) {
			std::cerr << "caught exception: " << e.what() << std::endl;
		}
	};
}

static std::string serializeHandles(Device *d) {
	std::lock_guard<std::recursive_mutex> lg(d->handlesMutex);

	std::list<json> arr;

	std::string currServiceUuid;
	std::set<std::string> currCharUuids;
	for (auto &h : d->handles) {
		PrimaryService *pSvc = dynamic_cast<PrimaryService *>(h.second);
		if (pSvc) {
			if (currServiceUuid != "") {
				json j;
				j["uuid"] = currServiceUuid;
				j["chars"] = json(currCharUuids);
				arr.push_back(j);
			}
			currCharUuids.clear();
			currServiceUuid = pSvc->getUuid().str();
		}
		Characteristic *chr = dynamic_cast<Characteristic *>(h.second);
		if (chr) {
			currCharUuids.insert(d->handles[chr->getCharHandle()]->getUuid().str());
		}
	}

	if (currServiceUuid != "") {
		json j;
		j["uuid"] = currServiceUuid;
		j["chars"] = json(currCharUuids);
		arr.push_back(j);
	}

	if (debug_network) {
		pdebug(json(arr).dump());
	}
	return json(arr).dump();
}

void NetworkReporter::addDeviceHelper(Device *d) {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/connect/" + beetle.name
			+ "/" + d->getName() + "/" + std::to_string(d->getId())));
	request << header("Content-Type", "application/json");
	http::client::response response = client.post(request, serializeHandles(d));
	if (response.status() != 200) {
		throw NetworkException("error informing server of connection " + std::to_string(d->getId()));
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "controller responded for " << std::to_string(d->getId()) << ": " << body(response);
			pdebug(ss.str());
		}
	}
}

void NetworkReporter::removeDeviceHelper(device_t d) {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/disconnect/" + beetle.name
			+ "/" + std::to_string(d)));
	http::client::response response = client.post(request);
	if (response.status() != 200) {
		throw NetworkException("error informing server of disconnection " + std::to_string(d));
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "controller responded for " << std::to_string(d) << ": " << body(response);
			pdebug(ss.str());
		}
	}
}
