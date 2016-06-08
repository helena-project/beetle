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

using namespace boost::asio;

// TODO SSL this
// http://www.boost.org/doc/libs/1_45_0/doc/html/boost_asio/example/ssl/client.cpp

static ip::tcp::iostream *connect_controller(ControllerClient *client) {
	auto stream = new ip::tcp::iostream;

	try{
		stream->connect(client->getHost(), std::to_string(client->getCtrlPort()));
	} catch (std::exception &e) {
		stream->close();
		delete stream;
		return NULL;
	}

	if(stream->fail()) {
		stream->close();
		delete stream;
		return NULL;
	}

	*stream << client->getSessionToken();
	sleep(1);
	if (stream->eof()) {
		stream->close();
		delete stream;
		return NULL;
	}

	return stream;
}

ControllerConnection::ControllerConnection(Beetle &beetle, std::shared_ptr<ControllerClient> client_,
		int maxReconnectionAttempts_) :
	beetle(beetle), daemonThread() {
	client = client_;

	maxReconnectionAttempts = maxReconnectionAttempts_;

	stream.reset(connect_controller(client.get()));
	if(!stream) {
		throw ControllerException("Could not connect to command server.");
	}

	daemonRunning = true;
	daemonThread = std::thread(&ControllerConnection::socketDaemon, this);
}

ControllerConnection::~ControllerConnection() {
	if (stream) {
		stream->close();
		stream.reset();
	}
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

bool ControllerConnection::reconnect() {
	int backoff = 1;
	bool success = false;

	for (int i = 0; i < maxReconnectionAttempts; i++) {
		if (debug) {
			pdebug("reconnecting to controller: " + std::to_string(i));
		}
		stream.reset(connect_controller(client.get()));
		if (stream) {
			if (debug) {
				pdebug("reconnected to controller");
			}
			success = true;
			break;
		} else {
			if (debug) {
				backoff *= 2;
				pdebug("failed to reconnect to controller: trying again in " + std::to_string(backoff));
				sleep(backoff);
			}
		}
	}

	return success;
}

void ControllerConnection::socketDaemon() {
	while (daemonRunning) {
		std::vector<std::string> cmd;
		if (!getCommand(cmd)) {
			stream->close();
			stream.reset();
			if (!reconnect()) {
				throw ControllerException("max controller reconnection attempts exceeded");
			}
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
		return false;
	} else {
		if (debug) {
			pdebug("command from controller: " + line);
		}
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

	std::string err;
	if (!beetle.mapDevices(from, to, err)) {
		if (debug) {
			pdebug(err);
		}
	}
}

void ControllerConnection::doUnmapLocal(const std::vector<std::string>& cmd) {
	device_t from = std::stol(cmd[1]);
	device_t to = std::stol(cmd[2]);
	std::string err;
	if (!beetle.unmapDevices(from, to, err)) {
		if (debug) {
			pdebug(err);
		}
	}
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



