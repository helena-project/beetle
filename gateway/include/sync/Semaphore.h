/*
 * Semaphore.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef INCLUDE_SYNC_SEMAPHORE_H_
#define INCLUDE_SYNC_SEMAPHORE_H_

#include <condition_variable>
#include <mutex>

class Semaphore {
public:
	Semaphore(int init) {
		count = init;
	};

	virtual ~Semaphore() {

	};

	void notify() {
		std::lock_guard<std::mutex> lg(m);
		count++;
		cv.notify_one();
	};

	void wait() {
		std::unique_lock<std::mutex> ul(m);
		while (count <= 0) {
			cv.wait(ul);
		}
		count--;
	};

	bool try_wait() {
		std::lock_guard<std::mutex> lg(m);
		if (count > 0) {
			count--;
			return true;
		} else {
			return false;
		}
	}
private:
	int count;
	std::condition_variable cv;
	std::mutex m;
};

#endif /* INCLUDE_SYNC_SEMAPHORE_H_ */
