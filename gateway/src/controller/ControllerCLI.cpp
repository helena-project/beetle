/*
 * ControllerCLI.cpp
 *
 *  Created on: Jun 5, 2016
 *      Author: james
 */

#include "controller/ControllerCLI.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/basic_socket_iostream.hpp>
#include <iostream>

#include "Beetle.h"
#include "BeetleConfig.h"
#include "CLI.h"
#include "controller/ControllerClient.h"

ControllerCLI::ControllerCLI(Beetle &beetle, std::string host, int port, BeetleConfig beetleConfig,
		std::shared_ptr<NetworkDiscoveryClient> discovery, bool useDaemon) {
	auto stream = new boost::asio::ip::tcp::iostream;

	// TODO SSL this
	// http://www.boost.org/doc/libs/1_45_0/doc/html/boost_asio/example/ssl/client.cpp

	stream->connect(host, std::to_string(port));
	if(stream->fail()) {
		throw ControllerException("Could not connect to server.");
	}

	cli = std::make_unique<CLI>(beetle, beetleConfig, discovery, stream, useDaemon);
}

ControllerCLI::~ControllerCLI() {

}

void ControllerCLI::join() {
	cli->join();
}

