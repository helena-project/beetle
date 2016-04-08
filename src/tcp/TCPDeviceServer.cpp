/*
 * TCPServer.cpp
 *
 *  Created on: Apr 4, 2016
 *      Author: james
 */

#include "TCPDeviceServer.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#include "../device/TCPConnection.h"
#include "../Debug.h"

TCPDeviceServer::TCPDeviceServer(Beetle &beetle, int port, bool discover) : beetle(beetle) {
	running = true;
	t = std::thread(&TCPDeviceServer::serverDaemon, this, port);
}

TCPDeviceServer::~TCPDeviceServer() {
	running = false;
	t.join();
}

void TCPDeviceServer::serverDaemon(int port) {
	if (debug) pdebug("resource serverDaemon started on port " + std::to_string(port));

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		throw ServerException("error creating socket");
	}

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		throw ServerException("error on bind");
	}

	listen(sockfd, 5);
	while (running) {
		struct sockaddr_in client_addr;
		socklen_t clilen = sizeof(client_addr);

		int clifd = accept(sockfd, (struct sockaddr *)&client_addr, &clilen);
		if (clifd < 0) {
			pdebug("error on accept");
			continue;
		}

		startTcpDeviceHelper(clifd);
	}
	close(sockfd);
	if (debug) pdebug("resource serverDaemon exited");
}

void TCPDeviceServer::startTcpDeviceHelper(int clifd) {
	uint32_t paramsLen;
	if (read(clifd, &paramsLen, sizeof(uint32_t)) != sizeof(uint32_t)) {
		if (debug) {
			pdebug("could not read tcp connection parameters length");
		}
		close(clifd);
		return;
	}

	paramsLen = ntohs(paramsLen);
	if (debug) {
		pdebug("expecting " + std::to_string(paramsLen) + " bytes of parameters");
	}

	uint32_t bytesRead = 0;
	while (bytesRead < paramsLen) {
		char buf[64]; // consume the bytes and ignore
		int n = read(clifd, buf, sizeof(buf));
		if (n < 0) {
			if (debug) {
				pdebug("could not finish reading tcp connection parameters");
			}
			close(clifd);
			return;
		}
		bytesRead += n;
	}

	if (debug) {
		pdebug("done reading tcp connection parameters");
	}

	VirtualDevice *device = NULL;
	try {
		/*
		 * Takes over the clifd
		 */
		device = new TCPConnection(beetle, clifd);

		beetle.addDevice(device);

		if (discover) {
			device->startNd();
		} else {
			device->start();
		}

		pdebug("connected to " + device->getName());
		if (debug) {
			pdebug(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (DeviceException& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}
