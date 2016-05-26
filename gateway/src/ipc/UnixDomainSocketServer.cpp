/*
 * UnixDomainSocketServer.cpp
 *
 *  Created on: Apr 15, 2016
 *      Author: James Hong
 */

#include "ipc/UnixDomainSocketServer.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <exception>
#include <iostream>
#include <thread>

#include "Beetle.h"
#include "device/socket/IPCApplication.h"
#include "Debug.h"

UnixDomainSocketServer::UnixDomainSocketServer(Beetle &beetle, std::string path) :
		beetle(beetle), daemonThread() {
	daemonRunning = true;
	fd = -1;
	daemonThread = std::thread(&UnixDomainSocketServer::serverDaemon, this, path);
}

UnixDomainSocketServer::~UnixDomainSocketServer() {
	daemonRunning = false;
	shutdown(fd, SHUT_RDWR);
	daemonThread.join();
}

void UnixDomainSocketServer::serverDaemon(std::string path) {
	if (debug) pdebug("ipc serverDaemon started at path: " + path);

	unlink(path.c_str());

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd < 0) {
		if (debug) pdebug("failed to get ipc socket");
		return;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		if (debug) pdebug("failed to bind ipc socket");
		return;
	}

	listen(fd, 5);
	while (daemonRunning) {
		struct sockaddr_un cliaddr;
		socklen_t clilen = sizeof(cliaddr);

		int clifd = accept(fd, (struct sockaddr *) &cliaddr, &clilen);
		if (clifd < 0) {
			if (!daemonRunning) {
				break;
			}
			pwarn("error on accept");
			continue;
		}

		struct ucred clicred;
		unsigned int clicredLen = sizeof(struct ucred);
		if (getsockopt(clifd, SOL_SOCKET, SO_PEERCRED, &clicred, &clicredLen) < 0) {
			pwarn("error on getting client info");
			close(clifd);
			continue;
		}

		startIPCDeviceHelper(clifd, cliaddr, clicred);
	}

	close(fd);
	if (debug) pdebug("ipc serverDaemon exited");
}

void UnixDomainSocketServer::startIPCDeviceHelper(int clifd, struct sockaddr_un cliaddr, struct ucred clicred) {
	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		/*
		 * Takes over the clifd
		 */
		device.reset(new IPCApplication(beetle, clifd, "PID-" + std::to_string(clicred.pid), cliaddr, clicred));

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);
		device->start();

		pdebug("connected to " + device->getName());
		if (debug) {
			pdebug(device->getName() + " has handle range [0," + std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (std::exception& e) {
		std::cout << "caught exception: " << e.what() << std::endl;
		if (device) {
			beetle.removeDevice(device->getId());
		}
	}
}
