/*
 * NetworkReporter.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#include "controller/ConnectionReporter.h"

#include <boost/network/protocol/http/client.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <iostream>
#include <json/json.hpp>
#include <list>
#include <map>
#include <set>
#include <mutex>
#include <sstream>
#include <utility>

#include "controller/Controller.h"
#include "Debug.h"
#include "Device.h"
#include "Handle.h"
#include "UUID.h"

using json = nlohmann::json;

ConnectionReporter::ConnectionReporter(Beetle &beetle, std::string hostAndPort_)
: beetle(beetle) {
	hostAndPort = hostAndPort_;

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
//	       .openssl_certificate("/tmp/my-cert")
//	       .openssl_verify_path("/tmp/ca-certs")
	       .timeout(10);

	client = new http::client(options);
	http::client::request request(getUrl(hostAndPort, "network/connect/" + beetle.name));
	request << header("User-Agent", "linux");

	http::client::response response = client->post(request);
	if (response.status() != 200) {
		std::stringstream ss;
		ss << "error connecting to controller (" << response.status() << "): "
				<< body(response);
		throw NetworkException(ss.str());
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "connected to controller: " <<  body(response);
			pdebug(ss.str());
		}
	}
}

ConnectionReporter::~ConnectionReporter() {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/connect/" + beetle.name));
		request << header("User-Agent", "linux");
	http::client::response response = client->delete_(request);
	if (response.status() != 200) {
		std::stringstream ss;
				ss << "error disconnecting from controller (" << response.status() << "): "
						<< body(response);
		pwarn(ss.str());
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "disconnected from controller: " << body(response);
			pdebug(ss.str());
		}
	}
//	delete client;
}

AddDeviceHandler ConnectionReporter::getAddDeviceHandler() {
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

RemoveDeviceHandler ConnectionReporter::getRemoveDeviceHandler() {
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
			currServiceUuid = pSvc->getServiceUuid().str();
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

void ConnectionReporter::addDeviceHelper(Device *d) {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort,
			"network/connect/" + beetle.name + "/" + d->getName() + "/" + std::to_string(d->getId())));
	request << header("User-Agent", "linux");

	auto response = client->post(request, serializeHandles(d));
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

void ConnectionReporter::removeDeviceHelper(device_t d) {
	using namespace boost::network;
	http::client::request request(getUrl(hostAndPort, "network/connect/" + beetle.name + "/" + std::to_string(d)));
	request << header("User-Agent", "linux");

	auto response = client->delete_(request);
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