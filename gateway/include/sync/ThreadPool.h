/*
 * ThreadPool.h
 *
 *  Created on: Apr 15, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_SYNC_THREADPOOL_H_
#define INCLUDE_SYNC_THREADPOOL_H_

#include <functional>
#include <thread>
#include <vector>

#include "BlockingQueue.h"

/*
 * This thread pool makes no ordering guarantees.
 */
class ThreadPool {
public:
	ThreadPool(int n);
	virtual ~ThreadPool();

	/*
	 * Schedule a new task.
	 */
	void schedule(std::function<void()> task);
private:
	bool running;
	std::vector<std::thread> workers;
	BlockingQueue<std::function<void()>> queue;
	void workerDaemon();
};

#endif /* INCLUDE_SYNC_THREADPOOL_H_ */
