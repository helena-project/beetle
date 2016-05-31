/*
 * SocketSelect.cpp
 *
 *  Created on: May 30, 2016
 *      Author: James Hong
 */

#include "sync/SocketSelect.h"

#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <set>
#include <sstream>
#include <utility>

#include "Debug.h"

SocketSelect::SocketSelect(unsigned int n) : daemonThread() {
	FD_ZERO(&activeFds);

	if (n > 1) {
		workers = std::make_unique<ThreadPool>(n);
	}

	daemonRunning = true;
	daemonThread = std::thread(&SocketSelect::daemon, this);
}

SocketSelect::~SocketSelect() {
	daemonRunning = false;
	if (daemonThread.joinable()) {
		daemonThread.join();
	}
}

void SocketSelect::add(int fd, std::function<void()> cb) {
	std::lock_guard<std::mutex> lg(activeFdsMutex);
	fdToCallback[fd] = cb;
	FD_SET(fd, &activeFds);
}

void SocketSelect::remove(int fd) {
	std::lock_guard<std::mutex> lg(activeFdsMutex);
	fdToCallback.erase(fd);
	FD_CLR(fd, &activeFds);
}

void SocketSelect::daemon() {
	std::shared_ptr<std::set<int>> fdsInUse;
	std::shared_ptr<std::mutex> fdsInUseMutex;

	if (workers) {
		fdsInUse = std::make_shared<std::set<int>>();
		fdsInUseMutex = std::make_shared<std::mutex>();
	}

	struct timeval defaultTimeout;
	defaultTimeout.tv_sec = 1;
	defaultTimeout.tv_usec = 0;

	while (daemonRunning) {
		activeFdsMutex.lock();
		fd_set readFds = activeFds;
		fd_set exceptFds = activeFds;
		activeFdsMutex.unlock();

		struct timeval timeout = defaultTimeout;
		int result = select(FD_SETSIZE, &readFds, NULL, &exceptFds, &timeout);
		if (result < 0) {
			std::stringstream ss;
			ss << "select failed : " << strerror(errno);
			pdebug(ss.str());
			exit(EXIT_FAILURE);
		} else if (result == 0) {
			continue;
		}

		activeFdsMutex.lock();
		for (auto kv : fdToCallback) {
			int fd = kv.first;
			if (FD_ISSET(fd, &readFds) || FD_ISSET(fd, &exceptFds)) {
				auto cb = kv.second;

				if (workers) {
					fdsInUseMutex->lock();
					if (fdsInUse->find(fd) == fdsInUse->end()) {
						fdsInUse->insert(fd);
						workers->schedule([this, fd, cb, fdsInUse, fdsInUseMutex]{
							try {
								cb();
							} catch (std::exception &e) {
								std::cerr << "socket select caught exception: " << e.what() << std::endl;
							}
							fdsInUseMutex->lock();
							fdsInUse->erase(fd);
							fdsInUseMutex->unlock();
						});
					}
					fdsInUseMutex->unlock();
				} else {
					try {
						cb();
					} catch (std::exception &e) {
						std::cerr << "socket select caught exception: " << e.what() << std::endl;
					}
				}
			}
		}
		activeFdsMutex.unlock();
	}
}
