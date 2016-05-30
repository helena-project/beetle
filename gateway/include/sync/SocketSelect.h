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
	SocketSelect(unsigned int n);
	virtual ~SocketSelect();
	
	void add(int fd, std::function<void()> cb);
	void remove(int fd);

private:
	fd_set activeFds;
	std::mutex activeFdsMutex;
	std::map<int, std::function<void()>> fdToCallback;

	bool daemonRunning;
	std::thread daemonThread;
	void daemon();

	std::unique_ptr<ThreadPool> workers;
};

#endif /* SYNC_SOCKETSELECT_H_ */
