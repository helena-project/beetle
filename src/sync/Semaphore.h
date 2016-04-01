/*
 * Semaphore.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef SYNC_SEMAPHORE_H_
#define SYNC_SEMAPHORE_H_

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
		while (count < 0) {
			cv.wait(ul);
		}
		count--;
	};
private:
	int count;
	std::condition_variable cv;
	std::mutex m;
};

#endif /* SYNC_SEMAPHORE_H_ */
