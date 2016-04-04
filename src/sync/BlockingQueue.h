/*
 * BlockingQueue.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef SYNC_BLOCKINGQUEUE_H_
#define SYNC_BLOCKINGQUEUE_H_

#include <stddef.h>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>

struct QueueDestroyedException : public std::exception {
  const char *what() const throw() {
    return "the queue no longer exists";
  }
};

template <typename T>
class BlockingQueue {
public:
	BlockingQueue() {
	    q = new std::queue<T>();
	};

	~BlockingQueue() {};

	/*
	 * Enqueues an element at the end.
	 */
	void push(T v) {
	    std::lock_guard<std::mutex> lg(m);
	    if (q == NULL) {
	        throw QueueDestroyedException();
	    }
	    q->push(v);
	    cv.notify_one();
	};

	/*
	 * Dequeues an element from the front, or blocks until one is
	 * available. If destroy() is called while waiting, an
	 * exception is thrown.
	 */
	T pop() {
	    std::unique_lock<std::mutex> ul(m);
	    if (q == NULL) {
	        throw QueueDestroyedException();
	    }

	    while (q->size() == 0) {
	        cv.wait(ul);
	        if (q == NULL) {
	            throw QueueDestroyedException();
	        }
	    }

	    T ret = q->front();
	    q->pop();
	    return ret;
	};

	/*
	 * Destroys the queue, making it unable to accept new input,
	 * waking all threads that are waiting, and returning a pointer
	 * to the internal, unscynchronized queue, which the caller is
	 * responsible for freeing.
	 */
	std::queue<T> *destroy() {
	    std::lock_guard<std::mutex> lg(m);
	    auto tmp = q;
	    q = NULL;
	    cv.notify_all();
	    return tmp;
	};
private:
	std::mutex m;
	std::condition_variable cv;
	std::queue<T> *q;
};

#endif /* SYNC_BLOCKINGQUEUE_H_ */
