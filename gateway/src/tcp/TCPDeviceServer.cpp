/*
 * TCPServer.cpp
 *
 *  Created on: Apr 4, 2016
 *      Author: James Hong
 */

#include "tcp/TCPDeviceServer.h"

#include <netinet/in.h>
#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <iostream>
#include <utility>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "Beetle.h"
#include "BeetleTypes.h"
#include "device/socket/tcp/TCPClient.h"
#include "device/socket/tcp/TCPClientProxy.h"
#include "Debug.h"
#include "tcp/TCPConnParams.h"
#include "util/file.h"
#include "util/write.h"

static void startTcpDeviceHelper(Beetle &, SSL *ssl, int clifd, struct sockaddr_in cliaddr);

TCPDeviceServer::TCPDeviceServer(Beetle &beetle, SSLConfig *sslConfig_, int port) :
		beetle(beetle), sslConfig(sslConfig_) {
	if (debug) {
		pdebug("tcp server started on port: " + std::to_string(port));
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		throw ServerException("error creating socket");
	}

	int unused = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &unused, sizeof(int)) < 0) {
	    throw ServerException("error setting socket to reuse");
	}

	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		throw ServerException("error on bind");
	}

	listen(sockfd, 5);

	/*
	 * Add to sockets managed by select
	 */
	int sockfdShared = sockfd;
	auto sslConfigShared = sslConfig;
	beetle.readers.add(sockfd, [&beetle, sockfdShared, sslConfigShared] {
		struct sockaddr_in client_addr;
		socklen_t clilen = sizeof(client_addr);

		int clifd = accept(sockfdShared, (struct sockaddr *) &client_addr, &clilen);
		if (clifd < 0) {
			pwarn("error on accept");
			return;
		}

		SSL *ssl = SSL_new(sslConfigShared->getCtx());
		SSL_set_fd(ssl, clifd);
		if (SSL_accept(ssl) <= 0) {
			pwarn("error on ssl accept");
			ERR_print_errors_fp(stderr);
			SSL_shutdown(ssl);
			shutdown(clifd, SHUT_RDWR);
			SSL_free(ssl);
			close(clifd);
			return;
		}

		beetle.workers.schedule([&beetle, ssl, clifd, client_addr]{
			startTcpDeviceHelper(beetle, ssl, clifd, client_addr);
		});
	});
}

TCPDeviceServer::~TCPDeviceServer() {
	beetle.readers.remove(sockfd);
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	if (debug) {
		pdebug("tcp server stopped");
	}
}

static void startTcpDeviceHelper(Beetle &beetle, SSL *ssl, int clifd, struct sockaddr_in cliaddr) {
	std::map<std::string, std::string> clientParams;
	if (!readParamsHelper(ssl, clifd, clientParams)) {
		pwarn("unable to read parameters");
		SSL_shutdown(ssl);
		shutdown(clifd, SHUT_RDWR);
		SSL_free(ssl);
		close(clifd);
		return;
	}

	if (debug) {
		for (auto &kv : clientParams) {
			pdebug("parsed param (" + kv.first + "," + kv.second + ")");
		}
		pdebug("done reading tcp connection parameters");
	}

	/*
	 * Send params to the client.
	 */
	std::stringstream ss;
	ss << TCP_PARAM_GATEWAY << " " << beetle.name;
	std::string serverParams = ss.str();
	uint32_t serverParamsLen = htonl(serverParams.length());

	if (SSL_write_all(ssl, (uint8_t *) &serverParamsLen, sizeof(serverParamsLen)) == false) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(clifd, SHUT_RDWR);
		SSL_free(ssl);
		close(clifd);
		if (debug) pdebug("could not write server params length");
	}

	if (SSL_write_all(ssl, (uint8_t *) serverParams.c_str(), serverParams.length()) == false) {
		ERR_print_errors_fp(stderr);
		SSL_shutdown(ssl);
		shutdown(clifd, SHUT_RDWR);
		SSL_free(ssl);
		close(clifd);
		if (debug) pdebug("could not write server params");
	}

	/*
	 * Instantiate the virtual device around the client socket.
	 */
	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		/*
		 * Takes over the clifd
		 */
		if (clientParams.find(TCP_PARAM_GATEWAY) == clientParams.end()) {
			device = std::make_shared<TCPClient>(beetle, ssl, clifd, clientParams[TCP_PARAM_CLIENT], cliaddr);
		} else {
			// name of the client gateway
			std::string client = clientParams[TCP_PARAM_GATEWAY];
			// device that the client is requesting
			device_t deviceId = std::stol(clientParams[TCP_PARAM_DEVICE]);
			device = std::make_shared<TCPClientProxy>(beetle, ssl, clifd, client, cliaddr, deviceId);
		}

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);
		device->start(clientParams[TCP_PARAM_SERVER] == "true");

		if (debug) {
			pdebug(device->getName() + " has handle range [0," + std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (std::exception& e) {
		pexcept(e);
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}
