/*
 * Beetle.h
 *
 *  Created on: Mar 23, 2016
 *      Author: james
 */

#ifndef BEETLE_H_
#define BEETLE_H_

#include <map>
#include <mutex>
#include <boost/thread.hpp>

class Device;
class Router;
class HAT;

typedef long device_t;
const device_t BEETLE_RESERVED_DEVICE = 0;
const device_t NULL_RESERVED_DEVICE = -1;

class Beetle {
public:
	Beetle();
	virtual ~Beetle();

	void run();

	void addDevice(Device *);
	void removeDevice(device_t);

	std::map<device_t, Device *> devices;
	boost::shared_mutex devicesMutex;

	HAT *hat;
	boost::shared_mutex hatMutex;

	Router *router;
};

#endif /* BEETLE_H_ */
