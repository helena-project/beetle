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

#include "BlockingQueue.h"
#include "Semaphore.h"

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


#endif /* SYNC_THREADPOOL_H_ */
