/*
 * NetworkReporter.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#include "controller/ConnectionReporter.h"

#include <cpr/cpr.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <iostream>
#include <json/json.hpp>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
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

	auto response = cpr::Post(
			cpr::Url{getUrl(hostAndPort, "network/connect/" + beetle.name)},
			cpr::Header{{"User-Agent", "linux"}});
	if (response.status_code != 200) {
		std::stringstream ss;
		ss << "error connecting to controller (" << response.status_code << "): "
				<< response.text;
		throw NetworkException(ss.str());
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "connected to controller: " << response.text;
			pdebug(ss.str());
		}
	}
}

ConnectionReporter::~ConnectionReporter() {
	auto response = cpr::Post(
				cpr::Url{getUrl(hostAndPort, "network/disconnect/" + beetle.name)},
				cpr::Header{{"User-Agent", "linux"}});
	if (response.status_code != 200) {
		throw NetworkException("error disconnecting from controller");
	} else {
		if (debug_network) {
			pdebug("disconnected from controller");
			std::cerr <<response.text << std::endl;
		}
	}
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
	auto response = cpr::Post(
			cpr::Url{getUrl(hostAndPort, "network/connect/" + beetle.name + "/" + d->getName() + "/" + std::to_string(d->getId()))},
			cpr::Header{{"User-Agent", "linux"}},
			cpr::Body{serializeHandles(d)});
	if (response.status_code != 200) {
		throw NetworkException("error informing server of connection " + std::to_string(d->getId()));
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "controller responded for " << std::to_string(d->getId()) << ": " << response.text;
			pdebug(ss.str());
		}
	}
}

void ConnectionReporter::removeDeviceHelper(device_t d) {
	auto response = cpr::Post(
				cpr::Url{getUrl(hostAndPort, "network/disconnect/" + beetle.name + "/" + std::to_string(d))},
				cpr::Header{{"User-Agent", "linux"}});
	if (response.status_code != 200) {
		throw NetworkException("error informing server of disconnection " + std::to_string(d));
	} else {
		if (debug_network) {
			std::stringstream ss;
			ss << "controller responded for " << std::to_string(d) << ": " << response.text;
			pdebug(ss.str());
		}
	}
}
