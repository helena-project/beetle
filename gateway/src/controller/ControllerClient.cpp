/*
 * NetworkClient.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: James Hong
 */

#include "controller/ControllerClient.h"

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client/options.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <cassert>
#include <sstream>
#include <memory>

#include "Beetle.h"
#include "util/file.h"

static const int CLIENT_TIMEOUT = 180;

/* Custom header to identify gateway */
const std::string ControllerClient::SESSION_HEADER = "Beetle-Gateway-Session";

ControllerClient::ControllerClient(Beetle &beetle, std::string host, int apiPort, int ctrlPort,
		bool verifyPeers, std::string clientCert, std::string clientKey, std::string caCert) :
		beetle(beetle), host(host), apiPort(apiPort), ctrlPort(ctrlPort) {
	assert(file_exists(clientCert));
	assert(file_exists(clientKey));
	assert(file_exists(caCert));

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
		.cache_resolved(true)
		.always_verify_peer(verifyPeers)
		.timeout(CLIENT_TIMEOUT)
		.openssl_certificate(caCert)
		.openssl_certificate_file(clientCert)
		.openssl_private_key_file(clientKey);
	client.reset(new http::client(options));
}

ControllerClient::~ControllerClient() {

}

std::string ControllerClient::getApiUrl(std::string resource) {
	// TODO more robust escaping
	boost::replace_all(resource, " ", "%20");
	std::stringstream ss;
	ss << "https://" << host << ":" << apiPort << "/" << resource;
	return ss.str();
}

std::shared_ptr<boost::network::http::client> ControllerClient::getClient() {
	return client;
}

std::string ControllerClient::getName() {
	return beetle.name;
}

std::string ControllerClient::getHost() {
	return host;
}

int ControllerClient::getApiPort() {
	return apiPort;
}

int ControllerClient::getCtrlPort() {
	return ctrlPort;
}

void ControllerClient::setSessionToken(std::string token) {
	sessionToken = token;
}

std::string ControllerClient::getSessionToken() {
	return sessionToken;
}
