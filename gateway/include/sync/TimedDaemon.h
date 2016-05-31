/*
 * TimedDaemon.h
 *
 *  Created on: May 28, 2016
 *      Author: James Hong
 */

#ifndef SYNC_TIMEDDAEMON_H_
#define SYNC_TIMEDDAEMON_H_

#include <unistd.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class TimedDaemon {
public:
	TimedDaemon() : t() {
		isRunning.test_and_set();
		t = std::thread([this] {
			int tick = 0;
			while (isRunning.test_and_set()) {
				tick++;
				sleep(1);

				m.lock();
				for (auto &daemon : daemons) {
					if (tick % daemon.interval == 0) {
						daemon.f();
					}
				}
				m.unlock();
			}
		});
	}

	virtual ~TimedDaemon() {
		isRunning.clear();
		if (t.joinable()) {
			t.join();
		}
	}

	/*
	 * Call the function repeatedly.
	 */
	void repeat(std::function<void()> f, int seconds) {
		std::lock_guard<std::mutex> lg(m);
		scheduled_t s;
		s.f = f;
		s.interval = seconds;
		daemons.push_back(s);
	}

private:

	typedef struct {
		std::function<void()> f;
		int interval;
	} scheduled_t;

	std::atomic_flag isRunning;
	std::vector<scheduled_t> daemons;
	std::mutex m;
	std::thread t;
};

#endif /* SYNC_TIMEDDAEMON_H_ */
