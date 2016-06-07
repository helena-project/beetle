/*
 * ControllerConnection.cpp
 *
 *  Created on: Jun 6, 2016
 *      Author: james
 */

#include "controller/ControllerConnection.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/basic_socket_iostream.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "Beetle.h"
#include "controller/ControllerClient.h"
#include "device/socket/tcp/TCPServerProxy.h"
#include "Debug.h"
#include "Device.h"

ControllerConnection::ControllerConnection(Beetle &beetle, std::shared_ptr<ControllerClient> client_) :
	beetle(beetle), daemonThread() {
	client = client_;

	stream = new boost::asio::ip::tcp::iostream;

	// TODO SSL this
	// http://www.boost.org/doc/libs/1_45_0/doc/html/boost_asio/example/ssl/client.cpp

	stream->connect(client->getHost(), std::to_string(client->getCtrlPort()));
	if(stream->fail()) {
		throw ControllerException("Could not connect to command server.");
	}

	/*
	 * Send initial token.
	 */
	*stream << client->getSessionToken();

	daemonRunning = true;
	daemonThread = std::thread(&ControllerConnection::socketDaemon, this);
}

ControllerConnection::~ControllerConnection() {
	stream->close();
	daemonRunning = false;
	if (daemonThread.joinable()) {
		daemonThread.join();
	}
}

void ControllerConnection::join() {
	if (daemonThread.joinable()) {
		daemonThread.join();
	}
}

void ControllerConnection::socketDaemon() {
	while (daemonRunning) {
		std::vector<std::string> cmd;
		if (!getCommand(cmd)) {
			break;
		}
		if (cmd.size() == 0) {
			continue;
		}

		std::string c1 = cmd[0];
		if (c1 == "map-local") {
			doMapLocal(cmd);
		} else if (c1 == "unmap-local") {
				doUnmapLocal(cmd);
		} else if (c1 == "map-remote") {
			doMapRemote(cmd);
		} else if (c1 == "unmap-remote") {
			doUnmapRemote(cmd);
		} else {
			if (debug_controller) {
				pdebug("unrecognized command : " + c1);
			}
		}
	}
}

bool ControllerConnection::getCommand(std::vector<std::string> &ret) {
	assert(ret.empty());
	std::string line;
	getline(*stream, line);
	transform(line.begin(), line.end(), line.begin(), ::tolower);
	if ((*stream).eof()) {
		return false;
	} else if ((*stream).bad()) {
		throw std::runtime_error("IO exception");
	} else {
		boost::char_separator<char> sep { " " };
		boost::tokenizer<boost::char_separator<char>> tokenizer { line, sep };
		for (const auto &t : tokenizer) {
			ret.push_back(t);
		}
		return true;
	}
}

void ControllerConnection::doMapLocal(const std::vector<std::string>& cmd) {
	device_t from = std::stol(cmd[1]);
	device_t to = std::stol(cmd[2]);
	beetle.mapDevices(from, to);
}

void ControllerConnection::doUnmapLocal(const std::vector<std::string>& cmd) {
	device_t from = std::stol(cmd[1]);
	device_t to = std::stol(cmd[2]);
	beetle.unmapDevices(from, to);
}

void ControllerConnection::doMapRemote(const std::vector<std::string>& cmd) {
	std::string gateway = cmd[1];
	std::string addr = cmd[2];
	int port = std::stoi(cmd[3]);
	device_t remote = std::stol(cmd[4]);
	device_t to = std::stol(cmd[5]);

	device_t from = NULL_RESERVED_DEVICE;
	beetle.devicesMutex.lock_shared();
	for (auto &kv : beetle.devices) {
		if (kv.second->getType() == Device::TCP_SERVER_PROXY) {
			auto proxy = std::dynamic_pointer_cast<TCPServerProxy>(kv.second);
			if (proxy->getServerGateway() == gateway && proxy->getRemoteDeviceId() == remote) {
				from = proxy->getId();
			}
		}
	}
	beetle.devicesMutex.unlock_shared();

	if (from == NULL_RESERVED_DEVICE) {
		std::shared_ptr<VirtualDevice> device = NULL;
		try {
			device.reset(TCPServerProxy::connectRemote(beetle, addr, port, remote));

			boost::shared_lock<boost::shared_mutex> devicesLk;
			beetle.addDevice(device, devicesLk);

			device->start();

			if (debug) {
				pdebug("connected to remote " + std::to_string(device->getId()) + " : " + device->getName());
			}

			from = device->getId();
		} catch (DeviceException& e) {
			pexcept(e);
			if (debug) {
				pdebug("failed to complete controller command");
			}
			if (device) {
				beetle.removeDevice(device->getId());
			}
		}
	}

	if (from != NULL_RESERVED_DEVICE) {
		beetle.unmapDevices(from, to);
	}
}

void ControllerConnection::doUnmapRemote(const std::vector<std::string>& cmd) {
	std::string gateway = cmd[1];
	device_t remote = std::stol(cmd[2]);
	device_t to = std::stol(cmd[3]);

	device_t from = NULL_RESERVED_DEVICE;
	boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
	for (auto &kv : beetle.devices) {
		if (kv.second->getType() == Device::TCP_SERVER_PROXY) {
			auto proxy = std::dynamic_pointer_cast<TCPServerProxy>(kv.second);
			if (proxy->getServerGateway() == gateway && proxy->getRemoteDeviceId() == remote) {
				from = proxy->getId();
			}
		}
	}
	devicesLk.unlock();

	if (from != NULL_RESERVED_DEVICE) {
		beetle.unmapDevices(from, to);
	}
}



