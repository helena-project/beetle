/*
 * SocketSelect.h
 *
 *  Created on: May 30, 2016
 *      Author: James Hong
 */

#ifndef SYNC_SOCKETSELECT_H_
#define SYNC_SOCKETSELECT_H_

#include <sys/select.h>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

#include "sync/ThreadPool.h"

class SocketSelect {
public:
	/*
	 * Create SocketSelect object with n worker threads.
	 */
	SocketSelect(unsigned int n);
	virtual ~SocketSelect();
	
	/*
	 * Register a file descriptor and callback.
	 */
	void add(int fd, std::function<void()> cb);

	/*
	 * Unregister a file descriptor.
	 */
	void remove(int fd);

private:
	fd_set activeFds;
	std::map<int, std::function<void()>> fdToCallback;
	std::mutex activeFdsMutex;

	bool daemonRunning;
	std::thread daemonThread;
	void daemon();

	std::unique_ptr<ThreadPool> workers;
};

#endif /* SYNC_SOCKETSELECT_H_ */
