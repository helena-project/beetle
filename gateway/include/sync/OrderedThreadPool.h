/*
 * OrderedThreadPool.h
 *
 *  Created on: Apr 16, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_SYNC_ORDEREDTHREADPOOL_H_
#define INCLUDE_SYNC_ORDEREDTHREADPOOL_H_

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

	/*
	 * Schedule a new task with id.
	 */
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

#endif /* INCLUDE_SYNC_ORDEREDTHREADPOOL_H_ */
