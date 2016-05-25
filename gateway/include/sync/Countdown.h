/*
 * Countdown.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_SYNC_COUNTDOWN_H_
#define INCLUDE_SYNC_COUNTDOWN_H_

#include <condition_variable>
#include <mutex>
#include <cassert>

class Countdown {
public:
	Countdown(int init = 0) {
		count = init;
		waited = false;
	};

	virtual ~Countdown() {

	};

	void increment() {
		std::lock_guard<std::mutex> lg(m);
		assert(waited == false);
		count++;
	}

	void decrement() {
		std::lock_guard<std::mutex> lg(m);
		count--;
		cv.notify_all();
	};

	void wait() {
		std::unique_lock<std::mutex> ul(m);
		waited = true;
		while (count > 0) {
			cv.wait(ul);
		}
	};
private:
	bool waited;
	int count;
	std::condition_variable cv;
	std::mutex m;
};

#endif /* INCLUDE_SYNC_COUNTDOWN_H_ */
