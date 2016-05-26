/*
 * Semaphore.h
 *
 *  Created on: Mar 28, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_SYNC_SEMAPHORE_H_
#define INCLUDE_SYNC_SEMAPHORE_H_

#include <condition_variable>
#include <mutex>
#include <chrono>

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
	};

	bool try_wait(int seconds) {
		auto limit = std::chrono::system_clock::now();
		limit += std::chrono::seconds(seconds);

		std::unique_lock<std::mutex> ul(m);
		while (count <= 0) {
			if (cv.wait_until(ul, limit) == std::cv_status::timeout) {
				return false;
			}
		}
		count--;
		return true;
	};

private:
	int count;
	std::condition_variable cv;
	std::mutex m;
};

#endif /* INCLUDE_SYNC_SEMAPHORE_H_ */
