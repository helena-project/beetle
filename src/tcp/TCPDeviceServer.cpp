/*
 * TCPServer.cpp
 *
 *  Created on: Apr 4, 2016
 *      Author: james
 */

#include "tcp/TCPDeviceServer.h"

#include <netinet/in.h>
#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <iostream>
#include <utility>

#include "device/socket/tcp/TCPClientProxy.h"
#include "Debug.h"
#include "tcp/ConnParams.h"

TCPDeviceServer::TCPDeviceServer(Beetle &beetle, int port) : beetle(beetle) {
	running = true;
	sockfd = -1;
	t = std::thread(&TCPDeviceServer::serverDaemon, this, port);
}

TCPDeviceServer::~TCPDeviceServer() {
	running = false;
	shutdown(sockfd, SHUT_RDWR);
	t.join();
}

void TCPDeviceServer::serverDaemon(int port) {
	if (debug) pdebug("tcp serverDaemon started on port: " + std::to_string(port));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

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
			if (!running) {
				break;
			}
			pwarn("error on accept");
			continue;
		}

		startTcpDeviceHelper(clifd, client_addr);
	}
	close(sockfd);
	if (debug) pdebug("tcp serverDaemon exited");
}

bool TCPDeviceServer::readParamsHelper(int clifd, int paramsLen,
		std::map<std::string, std::string> &params) {
	int bytesRead = 0;
	char lineBuffer[2048];
	int lineIndex = 0;
	while (bytesRead < paramsLen) {
		char ch;
		int n = read(clifd, &ch, 1);
		if (n < 0) {
			if (debug) {
				pdebug("could not finish reading tcp connection parameters");
			}
			return false;
		} else if (n == 0) {
			continue;
		}

		/* advance */
		bytesRead += n;

		if (ch == '\n') {
			lineBuffer[lineIndex] = '\0';
			lineIndex = 0;
			std::string line = std::string(lineBuffer);
			if (line.length() == 0) {
				continue;
			}
			size_t splitIdx = line.find(' ');
			if (splitIdx == std::string::npos) {
				if (debug) {
					pdebug("unable to parse parameters: " + line);
				}
				return false;
			}
			std::string paramName = line.substr(0, splitIdx);
			std::string paramValue = line.substr(splitIdx + 1, line.length());
			params[paramName] = paramValue;
		} else if (ch != '\0') {
			lineBuffer[lineIndex] = ch;
			lineIndex++;
		}
	}

	if (lineIndex != 0) {
		lineBuffer[lineIndex] = '\0';
		std::string line = std::string(lineBuffer);
		size_t splitIdx = line.find(' ');
		if (splitIdx == std::string::npos) {
			if (debug) {
				pdebug("unable to parse parameters: " + line);
			}
			return false;
		}
		std::string paramName = line.substr(0, splitIdx);
		std::string paramValue = line.substr(splitIdx + 1, line.length());
		params[paramName] = paramValue;
	}
	return true;
}

void TCPDeviceServer::startTcpDeviceHelper(int clifd, struct sockaddr_in cliaddr) {
	uint32_t paramsLen;
	if (read(clifd, &paramsLen, sizeof(uint32_t)) != sizeof(uint32_t)) {
		if (debug) {
			pdebug("could not read tcp connection parameters length");
		}
		close(clifd);
		return;
	}

	paramsLen = ntohl(paramsLen);
	if (debug) {
		pdebug("expecting " + std::to_string(paramsLen) + " bytes of parameters");
	}

	std::map<std::string, std::string> params;
	if (!readParamsHelper(clifd, paramsLen, params)) {
		pwarn("unable to read parameters");
		close(clifd);
		return;
	}

	if (debug) {
		for (auto &kv : params) {
			pdebug("parsed param (" + kv.first + "," + kv.second + ")");
		}
		pdebug("done reading tcp connection parameters");
	}

	VirtualDevice *device = NULL;
	try {
		/*
		 * Takes over the clifd
		 */
		if (params.find(TCP_PARAM_GATEWAY) == params.end()) {
			device = new TCPConnection(beetle, clifd, params[TCP_PARAM_CLIENT], cliaddr);
		} else {
			// name of the client gateway
			std::string client = params[TCP_PARAM_GATEWAY];
			// device that the client is requesting
			device_t deviceId = std::stol(params[TCP_PARAM_DEVICE]);
			device = new TCPClientProxy(beetle, clifd, client, cliaddr, deviceId);
		}

		if (params[TCP_PARAM_SERVER] == "true") {
			device->start();
		} else {
			device->startNd();
		}

		beetle.addDevice(device);

		pdebug("connected to " + device->getName());
		if (debug) {
			pdebug(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (std::exception& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}
