/*
 * OrderedThreadPool.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef SYNC_ORDEREDTHREADPOOL_H_
#define SYNC_ORDEREDTHREADPOOL_H_

#include <functional>
#include <list>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "Semaphore.h"

/*
 * This thread pool ensures that tasks scheduled with the same id are executed
 * in order.
 */
class OrderedThreadPool {
public:
	OrderedThreadPool(int n);
	virtual ~OrderedThreadPool();
	void schedule(long id, std::function<void()> task);
private:
	bool running;
	std::vector<std::thread> workers;

	struct task {
		long id;
		std::function<void()> f;
	};

	Semaphore s;
	std::mutex m;
	std::set<long> locked;
	std::list<struct task> queue;
	void workerDaemon();
};

#endif /* SYNC_ORDEREDTHREADPOOL_H_ */
