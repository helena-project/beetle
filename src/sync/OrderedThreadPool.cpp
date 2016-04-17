/*
 * OrderedThreadPool.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#include "../../include/sync/OrderedThreadPool.h"

#include <cassert>
#include <map>
#include <utility>


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
	assert(id >= 0);
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
