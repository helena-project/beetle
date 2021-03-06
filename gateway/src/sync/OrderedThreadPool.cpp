/*
 * OrderedThreadPool.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: James Hong
 */

#include "sync/OrderedThreadPool.h"

#include <cassert>
#include <map>
#include <utility>
#include <iostream>

OrderedThreadPool::OrderedThreadPool(int n) :
		s(0) {
	running = true;

	for (int i = 0; i < n; i++) {
		workers.push_back(std::thread(&OrderedThreadPool::workerDaemon, this));
	}
}

OrderedThreadPool::~OrderedThreadPool() {
	running = false;
	std::unique_lock<std::mutex> lk(m);
	queue.clear();
	lk.unlock();
	for (int i = 0; i < (int) workers.size(); i++) {
		s.notify();
	}
	for (auto &w : workers) {
		w.join();
	}
}

void OrderedThreadPool::schedule(long id, std::function<void()> task) {
	assert(id >= 0);
	std::lock_guard<std::mutex> lg(m);
	queue.push_back( { id, task });
	if (locked.find(id) == locked.end()) {
		s.notify();
	}
}

void OrderedThreadPool::workerDaemon() {
	while (running) {
		s.wait();
		std::unique_lock<std::mutex> lk(m);
		while (running) {
			long id = -1;
			std::function<void()> f;

			/*
			 * Find a task id that is not locked.
			 */
			auto it = queue.begin();
			while (it != queue.end()) {
				if (locked.find(it->id) == locked.end()) {
					id = it->id;
					f = it->f;
					queue.erase(it);
					break;
				} else {
					it++;
				}
			}
			if (id == -1) {
				/*
				 * Cannot make progress. Another thread must be working. All task ids are locked.
				 */
				break;
			} else {
				locked.insert(id);
				lk.unlock();
				try {
					f();
				} catch (std::exception &e) {
					std::cerr << "worker caught exception: " << e.what() << std::endl;
				}
				lk.lock();
				locked.erase(id);
			}
		}
	}
}
