/*
 * TimedDaemon.h
 *
 *  Created on: May 28, 2016
 *      Author: james
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
	typedef struct {
		std::function<void()> f;
		int interval;
	} scheduled_t;

	TimedDaemon() : t() {
		isRunning.test_and_set();
		t = std::thread([this] {
			while (isRunning.test_and_set()) {
				int tick = 0;
				tick++;
				sleep(1);

				m.lock();
				for (auto daemon : daemons) {
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

	void repeat(std::function<void()> f, int interval) {
		std::lock_guard<std::mutex> lg(m);
		daemons.push_back(scheduled_t{f, interval});
	}

	std::atomic_flag isRunning;
	std::vector<scheduled_t> daemons;
	std::mutex m;
	std::thread t;
};

#endif /* SYNC_TIMEDDAEMON_H_ */
