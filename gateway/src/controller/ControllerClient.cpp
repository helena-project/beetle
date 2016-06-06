/*
 * NetworkClient.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: James Hong
 */

#include "controller/ControllerClient.h"

#include <boost/network/protocol/http/client/options.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <cassert>
#include <sstream>
#include <memory>

#include "Beetle.h"
#include "util/file.h"

static const int CLIENT_TIMEOUT = 180;

const std::string ControllerClient::SESSION_HEADER = "Beetle-Gateway-Session";

ControllerClient::ControllerClient(Beetle &beetle, std::string host, int port, bool verifyPeers) :
		beetle(beetle), host(host), port(port) {
	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false).cache_resolved(true).always_verify_peer(verifyPeers)
//	       .openssl_certificate(defaultVerifyFile)
//	       .openssl_verify_path(defaultCertPath)
	.timeout(CLIENT_TIMEOUT);
	// TODO use certs
	client.reset(new http::client(options));
}

ControllerClient::ControllerClient(Beetle &beetle, std::string host, int port, std::string cert, std::string caCerts,
		bool verifyPeers) :
		beetle(beetle), host(host), port(port) {
	assert(file_exists(cert));
	assert(file_exists(caCerts));

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false).cache_resolved(true).always_verify_peer(verifyPeers)
//	       .openssl_certificate(cert)
//	       .openssl_verify_path(caCerts)
	.timeout(CLIENT_TIMEOUT);
	client.reset(new http::client(options));
}

ControllerClient::~ControllerClient() {

}

std::string ControllerClient::getUrl(std::string resource) {
	// TODO more robust escaping
	boost::replace_all(resource, " ", "%20");
	std::stringstream ss;
	ss << "https://" << host << ":" << port << "/" << resource;
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

int ControllerClient::getPort() {
	return port;
}

void ControllerClient::setSessionToken(std::string token) {
	sessionToken = token;
}

std::string ControllerClient::getSessionToken() {
	return sessionToken;
}
