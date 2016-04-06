/*
 * Beetle.h
 *
 *  Created on: Mar 23, 2016
 *      Author: james
 */

#ifndef BEETLE_H_
#define BEETLE_H_

#include <mutex>
#include <boost/thread.hpp>
#include <map>

class AutoConnect;
class Device;
class BeetleDevice;
class Router;
class HAT;
class TCPDeviceServer;

typedef long device_t;
const device_t BEETLE_RESERVED_DEVICE = 0;
const device_t NULL_RESERVED_DEVICE = -1;

class Beetle {
public:
	Beetle();
	virtual ~Beetle();

	/*
	 * Add a device to Beetle's mappings. Threadsafe.
	 * Do not call while holding devices or hat mutexes.
	 */
	void addDevice(Device *, bool allocateHandles = true);

	/*
	 * Removes a device from Beetle's mappings and unsubscribes
	 * the device from all characteristics.
	 */
	void removeDevice(device_t);

	std::map<device_t, Device *> devices;
	boost::shared_mutex devicesMutex;

	HAT *hat;
	boost::shared_mutex hatMutex;

	/*
	 * Simulated device for Beetle's own services.
	 */
	BeetleDevice *beetleDevice;

	Router *router;
};

#endif /* BEETLE_H_ */
