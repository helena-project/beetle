/*
 * TCPServerProxy.cpp
 *
 *  Created on: Apr 10, 2016
 *      Author: James Hong
 */

#include "device/socket/tcp/TCPServerProxy.h"

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string.h>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>

#include <unistd.h>
#include <utility>

#include "Beetle.h"
#include "Debug.h"
#include "Device.h"
#include "hat/SingleAllocator.h"
#include "tcp/TCPConnParams.h"
#include "util/file.h"
#include "util/write.h"

TCPServerProxy::TCPServerProxy(Beetle &beetle, SSL *ssl, int sockfd, std::string serverGateway_,
		struct sockaddr_in serverGatewaySockAddr_, device_t remoteProxyTo_) :
		TCPConnection(beetle, ssl, sockfd, serverGatewaySockAddr_, new SingleAllocator(NULL_RESERVED_DEVICE)) {
	type = TCP_SERVER_PROXY;

	name = "Proxy to " + std::to_string(remoteProxyTo_) + " from " + serverGateway_;
	serverGateway = serverGateway_;
	remoteProxyTo = remoteProxyTo_;
}

TCPServerProxy::~TCPServerProxy() {
	// Nothing to do, handled by superclass
}

device_t TCPServerProxy::getRemoteDeviceId() {
	return remoteProxyTo;
}

std::string TCPServerProxy::getServerGateway() {
	return serverGateway;
}

/*
 * Client SSLConfig
 */
SSLConfig *TCPServerProxy::sslConfig = NULL;

void TCPServerProxy::initSSL(SSLConfig *sslConfig_) {
	assert(sslConfig == NULL);
	assert(sslConfig_ != NULL);
	sslConfig = sslConfig_;
}

TCPServerProxy *TCPServerProxy::connectRemote(Beetle &beetle, std::string host, int port, device_t remoteProxyTo) {
	struct hostent *server;
	server = gethostbyname(host.c_str());
	if (server == NULL) {
		throw DeviceException("could not get host");
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		throw DeviceException("error opening socket");
	}

	struct sockaddr_in serv_addr = { 0 };
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr,
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
	serv_addr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		shutdown(sockfd, SHUT_RDWR);
		close(sockfd);
		throw DeviceException("error connecting");
	}

	SSL *ssl = SSL_new(sslConfig->getCtx());
	SSL_set_fd(ssl, sockfd);
	if (SSL_connect(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(sockfd, SHUT_RDWR);
		SSL_free(ssl);
		close(sockfd);
		throw DeviceException("error on ssl connect");
	}

	/*
	 * Format the plaintext parameters.
	 */
	std::stringstream ss;
	ss << TCP_PARAM_GATEWAY << " " << beetle.name << "\n" << TCP_PARAM_DEVICE << " " << std::to_string(remoteProxyTo);

	std::string clientParams = ss.str();
	uint32_t clientParamsLen = htonl(clientParams.length());
	if (SSL_write_all(ssl, (uint8_t *) &clientParamsLen, sizeof(clientParamsLen)) == false) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(sockfd, SHUT_RDWR);
		SSL_free(ssl);
		close(sockfd);
		throw DeviceException("could not write params length");
	}

	if (SSL_write_all(ssl, (uint8_t *) clientParams.c_str(), clientParams.length()) == false) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(sockfd, SHUT_RDWR);
		SSL_free(ssl);
		close(sockfd);
		throw DeviceException("could not write all the params");
	}

	/*
	 * Read params from the server.
	 */
	std::map<std::string, std::string> serverParams;
	if (!readParamsHelper(ssl, sockfd, serverParams)) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(sockfd, SHUT_RDWR);
		SSL_free(ssl);
		close(sockfd);
		throw DeviceException("unable to read parameters");
	}

	if (debug) {
		for (auto &kv : serverParams) {
			pdebug("parsed param (" + kv.first + "," + kv.second + ")");
		}
		pdebug("done reading tcp connection parameters");
	}

	if (serverParams.find(TCP_PARAM_GATEWAY) == serverParams.end()) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(sockfd, SHUT_RDWR);
		SSL_free(ssl);
		close(sockfd);
		throw DeviceException("server gateway did not respond with name");
	}

	/*
	 * Instantiate the virtual device around the client socket.
	 */
	return new TCPServerProxy(beetle, ssl, sockfd, serverParams[TCP_PARAM_GATEWAY], serv_addr, remoteProxyTo);
}

