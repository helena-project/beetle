/*
 * ThreadPool.cpp
 *
 *  Created on: Apr 15, 2016
 *      Author: James Hong
 */

#include "sync/ThreadPool.h"

#include <stddef.h>
#include <cassert>
#include <queue>
#include <iostream>

ThreadPool::ThreadPool(int n) {
	running = true;

	for (int i = 0; i < n; i++) {
		workers.push_back(std::thread(&ThreadPool::workerDaemon, this));
	}
}

ThreadPool::~ThreadPool() {
	running = false;
	auto qInternal = queue.destroy();
	while (qInternal != NULL && qInternal->size() > 0) {
		qInternal->front()();
		qInternal->pop();
	}
	delete qInternal;
	for (auto &w : workers) {
		w.join();
	}
}

void ThreadPool::schedule(std::function<void()> task) {
	assert(task != NULL);
	queue.push(task);
}

void ThreadPool::workerDaemon() {
	while (running) {
		try {
			auto f = queue.pop();
			try {
				f();
			} catch (std::exception &e) {
				std::cerr << "worker caught exception: " << e.what() << std::endl;
			}
		} catch (QueueDestroyedException &e) {
			break;
		}
	}
}

