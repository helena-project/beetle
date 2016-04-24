/*
 * NetworkClient.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: james
 */

#include <controller/ControllerClient.h>

#include <boost/network/protocol/http/client/options.hpp>
#include <boost/optional/optional.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <unistd.h>
#include <cassert>
#include <sstream>

inline bool fileExists(const std::string& name) {
    return (access(name.c_str(), F_OK) != -1);
}

ControllerClient::ControllerClient(std::string hostAndPort) : hostAndPort(hostAndPort) {
	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
		   .always_verify_peer(false)
//	       .openssl_certificate(defaultVerifyFile)
//	       .openssl_verify_path(defaultCertPath)
	       .timeout(10);
	// TODO use certs
	client = new http::client(options);
}

ControllerClient::ControllerClient(std::string hostAndPort, std::string cert,
		std::string caCerts) : hostAndPort(hostAndPort) {
	assert(fileExists(cert));
	assert(fileExists(caCerts));

	using namespace boost::network;
	http::client::options options;
	options.follow_redirects(false)
	       .cache_resolved(true)
		   .always_verify_peer(false)
//	       .openssl_certificate(cert)
//	       .openssl_verify_path(caCerts)
	       .timeout(10);
	client = new http::client(options);
}

ControllerClient::~ControllerClient() {
	delete client;
}

std::string ControllerClient::getUrl(std::string resource) {
	// TODO more robust escaping
	boost::replace_all(resource, " ", "%20");
	std::stringstream ss;
	ss << "https://" << hostAndPort << "/" << resource;
	return ss.str();
}

boost::network::http::client *ControllerClient::getClient() {
	return client;
}

