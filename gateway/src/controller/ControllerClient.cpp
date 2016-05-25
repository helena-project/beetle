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

ControllerClient::ControllerClient(Beetle &beetle, std::string hostAndPort, bool verifyPeers)
: beetle(beetle), hostAndPort(hostAndPort) {
	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
		   .always_verify_peer(verifyPeers)
//	       .openssl_certificate(defaultVerifyFile)
//	       .openssl_verify_path(defaultCertPath)
	       .timeout(CLIENT_TIMEOUT);
	// TODO use certs
	client.reset(new http::client(options));
}

ControllerClient::ControllerClient(Beetle &beetle, std::string hostAndPort, std::string cert,
		std::string caCerts, bool verifyPeers) : beetle(beetle), hostAndPort(hostAndPort) {
	assert(file_exists(cert));
	assert(file_exists(caCerts));

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
		   .always_verify_peer(verifyPeers)
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
	ss << "https://" << hostAndPort << "/" << resource;
	return ss.str();
}

std::shared_ptr<boost::network::http::client> ControllerClient::getClient() {
	return client;
}

std::string ControllerClient::getName() {
	return beetle.name;
}
