/*
 * NetworkReporter.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: James Hong
 */

#include <controller/NetworkStateClient.h>

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/request.hpp>
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
#include <chrono>
#include <string>
#include <thread>

#include "Beetle.h"
#include "controller/ControllerClient.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "Debug.h"
#include "Device.h"
#include "Handle.h"
#include "UUID.h"

using json = nlohmann::json;

NetworkStateClient::NetworkStateClient(Beetle &beetle, std::shared_ptr<ControllerClient> client_, int tcpPort) :
		beetle(beetle) {
	client = client_;

	std::string postParams = "port=" + std::to_string(tcpPort);

	using namespace boost::network;
	http::client::request request(client->getApiUrl("network/connectGateway/" + beetle.name));
	request << header("User-Agent", "linux");
	request << header("Content-Type", "application/x-www-form-urlencoded");
	request << header("Content-Length", std::to_string(postParams.length()));
	request << body(postParams);

	http::client::response response = client->getClient()->post(request);
	if (response.status() != 200) {
		std::stringstream ss;
		ss << "error connecting to beetle controller (" << response.status() << "): " << body(response);
		throw ControllerException(ss.str());
	} else {
		if (debug_controller) {
			pdebug("beetle controller: connected");
		}
		std::stringstream ss;
		ss << body(response);
		client->setSessionToken(ss.str());
	}
}

NetworkStateClient::~NetworkStateClient() {
	using namespace boost::network;
	http::client::request request(client->getApiUrl("network/connectGateway/"));
	request << header("User-Agent", "linux");
	request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());

	try {
		auto response = client->getClient()->delete_(request);
		if (response.status() != 200) {
			if (debug_controller) {
				std::stringstream ss;
				ss << "error disconnecting from controller (" << response.status() << "): " << body(response);
				pwarn(ss.str());
			}
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "beetle controller: " << body(response);
				pdebug(ss.str());
			}
		}
	} catch (std::exception &e) {
		pexcept(e);
	}
}

AddDeviceHandler NetworkStateClient::getAddDeviceHandler() {
	return [this](device_t d) {
		boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
		if (beetle.devices.find(d) == beetle.devices.end()) {
			if (debug_controller) {
				pwarn("tried to add device that does not exist" + std::to_string(d));
			}
			return;
		}
		std::shared_ptr<Device> device = beetle.devices[d];
		switch (device->getType()) {
			case Device::IPC_APPLICATION:
			case Device::LE_PERIPHERAL:
			case Device::TCP_CLIENT: {
				if (debug_controller) {
					pdebug("informing controller of new connection");
				}

				if (device->getName() == "") {
					pwarn("informing controller of unnamed device");
					return;
				}

				try {
					addDeviceHelper(device);
				} catch (std::exception &e) {
					pexcept(e);
				}
				break;
			}
			default:
			if (debug_controller) {
				pdebug("not reporting " + device->getTypeStr());
			}
			return;
		}
	};
}

UpdateDeviceHandler NetworkStateClient::getUpdateDeviceHandler() {
	return [this](device_t d) {
		boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
		if (beetle.devices.find(d) == beetle.devices.end()) {
			if (debug_controller) {
				pwarn("tried to add device that does not exist" + std::to_string(d));
			}
			return;
		}
		std::shared_ptr<Device> device = beetle.devices[d];
		switch (device->getType()) {
			case Device::IPC_APPLICATION:
			case Device::LE_PERIPHERAL:
			case Device::TCP_CLIENT:
			if (debug_controller) {
				pdebug("informing controller of new connection");
			}
			try {
				updateDeviceHelper(device);
			} catch (std::exception &e) {
				pexcept(e);
			}
			break;
			default:
			if (debug_controller) {
				pdebug("not reporting " + device->getTypeStr());
			}
			return;
		}
	};
}

RemoveDeviceHandler NetworkStateClient::getRemoveDeviceHandler() {
	return [this](device_t d) {
		try {
			removeDeviceHelper(d);
		} catch (std::exception &e) {
			pexcept(e);
		}
	};
}

bool NetworkStateClient::lookupNamesAndIds(device_t from, device_t to, std::string &fromGateway, device_t &fromId,
		std::string &toGateway, device_t &toId) {
	boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
	if (beetle.devices.find(from) == beetle.devices.end()) {
		return false;
	}
	auto fromDevice = beetle.devices[from];
	if (beetle.devices.find(to) == beetle.devices.end()) {
		return false;
	}
	auto toDevice = beetle.devices[to];

	switch (fromDevice->getType()) {
	case Device::BEETLE_INTERNAL:
		return false;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CLIENT: {
		fromGateway = beetle.name;
		fromId = fromDevice->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
		return false;
	case Device::TCP_SERVER_PROXY: {
		auto fromCast = std::dynamic_pointer_cast<TCPServerProxy>(fromDevice);
		fromGateway = fromCast->getServerGateway();
		fromId = fromCast->getRemoteDeviceId();
		break;
	}
	case Device::UNKNOWN:
	default:
		return false;
	}

	switch (toDevice->getType()) {
	case Device::BEETLE_INTERNAL:
		return false;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CLIENT: {
		toGateway = beetle.name;
		toId = toDevice->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
	case Device::TCP_SERVER_PROXY:
	case Device::UNKNOWN:
	default:
		return false;
	}

	return true;
}

MapDevicesHandler NetworkStateClient::getMapDevicesHandler() {
	return [this](device_t from, device_t to) {
		device_t fromId;
		device_t toId;
		std::string fromGateway;
		std::string toGateway;
		if (!lookupNamesAndIds(from, to, fromGateway, fromId, toGateway, toId)) {
			return;
		}

		std::stringstream resource;
		resource << "network/map/" << fromGateway << "/" << std::fixed << fromId << "/" << toGateway << "/"
				<< std::fixed << toId;

		std::string url = client->getApiUrl(resource.str());

		using namespace boost::network;
		http::client::request request(url);
		request << header("User-Agent", "linux");
		request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());

		try {
			auto response = client->getClient()->post(request);
			if (response.status() != 200) {
				if (debug_controller) {
					std::stringstream ss;
					ss << "error informing controller of map: " << body(response);
					pdebug(ss.str());
				}
			}
		} catch (std::exception &e) {
			pexcept(e);
		}
	};
}

MapDevicesHandler NetworkStateClient::getUnmapDevicesHandler() {
	return [this](device_t from, device_t to) {
		device_t fromId;
		device_t toId;
		std::string fromGateway;
		std::string toGateway;
		if (!lookupNamesAndIds(from, to, fromGateway, fromId, toGateway, toId)) {
			return;
		}

		std::stringstream resource;
		resource << "network/map/" << fromGateway << "/" << std::fixed << fromId << "/" << toGateway << "/"
				<< std::fixed << toId;

		std::string url = client->getApiUrl(resource.str());

		using namespace boost::network;
		http::client::request request(url);
		request << header("User-Agent", "linux");
		request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());

		try {
			auto response = client->getClient()->delete_(request);
			if (response.status() != 200) {
				if (debug_controller) {
					std::stringstream ss;
					ss << "error informing controller of unmap: " << body(response);
					pdebug(ss.str());
				}
			}
		} catch (std::exception &e) {
			pexcept(e);
		}
	};
}

static std::string serializeHandles(std::shared_ptr<Device> d) {
	std::lock_guard<std::recursive_mutex> lg(d->handlesMutex);

	std::list<json> arr;

	std::string currServiceUuid;
	std::set<std::string> currCharUuids;
	for (auto &h : d->handles) {
		auto pSvc = std::dynamic_pointer_cast<PrimaryService>(h.second);
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
		auto chr = std::dynamic_pointer_cast<Characteristic>(h.second);
		if (chr) {
			currCharUuids.insert(d->handles[chr->getAttrHandle()]->getUuid().str());
		}
	}

	if (currServiceUuid != "") {
		json j;
		j["uuid"] = currServiceUuid;
		j["chars"] = json(currCharUuids);
		arr.push_back(j);
	}

	if (debug_controller) {
		pdebug(json(arr).dump());
	}
	return json(arr).dump();
}

void NetworkStateClient::addDeviceHelper(std::shared_ptr<Device> d) {
	std::string url = client->getApiUrl(
			"network/connectDevice/" + d->getName() + "/" + std::to_string(d->getId()));
	if (debug_controller) {
		pdebug("post: " + url);
	}

	std::string requestBody = serializeHandles(d);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");
	request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());
	request << header("Content-Length", std::to_string(requestBody.length()));
	request << body(requestBody);

	try {
		auto response = client->getClient()->post(request);
		if (response.status() != 200) {
			throw ControllerException("error informing server of connection " + std::to_string(d->getId()));
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "controller responded for " << std::to_string(d->getId()) << ": " << body(response);
				pdebug(ss.str());
			}
		}
	} catch (std::exception &e) {
		pexcept(e);
	}
}

void NetworkStateClient::updateDeviceHelper(std::shared_ptr<Device> d) {
	std::string url = client->getApiUrl("network/updateDevice/" + std::to_string(d->getId()));
	if (debug_controller) {
		pdebug("put: " + url);
	}

	std::string requestBody = serializeHandles(d);

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");
	request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());
	request << header("Content-Length", std::to_string(requestBody.length()));
	request << body(requestBody);

	try {
		auto response = client->getClient()->put(request);
		if (response.status() != 200) {
			throw ControllerException("error informing server of update " + std::to_string(d->getId()));
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "controller responded for " << std::to_string(d->getId()) << ": " << body(response);
				pdebug(ss.str());
			}
		}
	} catch (std::exception &e) {
		pexcept(e);
	}
}

void NetworkStateClient::removeDeviceHelper(device_t d) {
	std::string url = client->getApiUrl("network/updateDevice/" + std::to_string(d));
	if (debug_controller) {
		pdebug("delete: " + url);
	}

	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");
	request << header(ControllerClient::SESSION_HEADER, client->getSessionToken());

	try {
		auto response = client->getClient()->delete_(request);
		if (response.status() != 200) {
			throw ControllerException("error informing server of disconnection " + std::to_string(d));
		} else {
			if (debug_controller) {
				std::stringstream ss;
				ss << "controller responded for " << std::to_string(d) << ": " << body(response);
				pdebug(ss.str());
			}
		}
	} catch (std::exception &e) {
		pexcept(e);
	}
}
