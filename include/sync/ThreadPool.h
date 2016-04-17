/*
 * ThreadPool.h
 *
 *  Created on: Apr 15, 2016
 *      Author: james
 */

#ifndef SYNC_THREADPOOL_H_
#define SYNC_THREADPOOL_H_

#include <functional>
#include <list>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "../../include/sync/BlockingQueue.h"
#include "../../include/sync/Semaphore.h"

/*
 * This thread pool makes no ordering guarantees.
 */
class ThreadPool {
public:
	ThreadPool(int n);
	virtual ~ThreadPool();
	void schedule(std::function<void()> task);
private:
	bool running;
	std::vector<std::thread> workers;
	BlockingQueue<std::function<void()>> queue;
	void workerDaemon();
};

#endif /* SYNC_THREADPOOL_H_ */
