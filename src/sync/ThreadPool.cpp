/*
 * ThreadPool.cpp
 *
 *  Created on: Apr 15, 2016
 *      Author: james
 */

#include "ThreadPool.h"

#include <cassert>
#include <stddef.h>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <utility>

#include "Semaphore.h"

ThreadPool::ThreadPool(int n) {
	running = true;

	for (int i = 0; i < n; i++) {
		workers.push_back(std::thread(&ThreadPool::workerDaemon, this));
	}
}

ThreadPool::~ThreadPool() {
	running = false;
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
		auto f = queue.pop();
		f();
	}
	auto qInternal = queue.destroy();
	while (qInternal != NULL && qInternal->size() > 0) {
		qInternal->front()();
		qInternal->pop();
	}
	delete qInternal;
}


OrderedThreadPool::OrderedThreadPool(int n) : s(0) {
	running = true;

	for (int i = 0; i < n; i++) {
		workers.push_back(std::thread(&OrderedThreadPool::workerDaemon, this));
	}
}

OrderedThreadPool::~OrderedThreadPool() {
	running = false;
	for (auto &w : workers) {
		w.join();
	}
	for (auto &t : queue) {
		t.f();
	}
}

void OrderedThreadPool::schedule(long id, std::function<void()> task) {
	std::lock_guard<std::mutex> lg(m);
	queue.push_back({id, task});
	if (locked.find(id) == locked.end()) s.notify();
}

void OrderedThreadPool::workerDaemon() {
	while (running) {
		s.wait();
		std::unique_lock<std::mutex> lk(m);
		while (running) {
			long id = -1;
			std::function<void()> f;

			auto it = queue.begin();
			while (it != queue.end()) {
				if (locked.find(it->id) == locked.end()) {
					id = it->id;
					f = it->f;
					queue.erase(it);
					break;
				}
			}
			if (id == -1) {
				break;
			} else {
				locked.insert(id);
				lk.unlock();
				f();
				lk.lock();
				locked.erase(id);
			}
		}
	}
}

