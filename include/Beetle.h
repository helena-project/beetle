/*
 * Beetle.h
 *
 *  Created on: Mar 23, 2016
 *      Author: james
 */

#ifndef INCLUDE_BEETLE_H_
#define INCLUDE_BEETLE_H_

#include <mutex>
#include <boost/thread.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "sync/OrderedThreadPool.h"
#include "sync/ThreadPool.h"

class AutoConnect;
class Device;
class BeetleInternal;
class Router;

typedef long device_t;
const device_t BEETLE_RESERVED_DEVICE = 0;
const device_t NULL_RESERVED_DEVICE = -1;

typedef std::function<void(device_t d)> AddDeviceHandler;
typedef std::function<void(device_t d)> RemoveDeviceHandler;

class Beetle {
public:
	Beetle(std::string name);
	virtual ~Beetle();

	/*
	 * Add a device to Beetle's mappings. Threadsafe.
	 * Do not call while holding devices or hat mutexes.
	 */
	void addDevice(Device *);

	/*
	 * Removes a device from Beetle's mappings and unsubscribes
	 * the device from all characteristics.
	 */
	void removeDevice(device_t);

	/*
	 * Maps handles from device 1 to device 2.
	 */
	void mapDevices(device_t from, device_t to);

	/*
	 * Unmaps handles from device 1 to device 2.
	 */
	void unmapDevices(device_t from, device_t to);

	/*
	 * Register handler to be called after a device is added.
	 */
	void registerAddDeviceHandler(AddDeviceHandler h);
	std::vector<AddDeviceHandler> addHandlers;

	/*
	 * Register handler to be called after a device is removed.
	 */
	void registerRemoveDeviceHandler(RemoveDeviceHandler h);
	std::vector<RemoveDeviceHandler> removeHandlers;

	std::map<device_t, Device *> devices;
	boost::shared_mutex devicesMutex;

	/*
	 * Name this Beetle instance
	 */
	std::string name;

	/*
	 * Simulated device for Beetle's own services.
	 */
	BeetleInternal *beetleDevice;

	/*
	 *
	 */
	Router *router;

	/*
	 * Workers for callbacks.
	 */
	ThreadPool workers;

	/*
	 * Threads used for writing.
	 */
	OrderedThreadPool writers;
};

#endif /* INCLUDE_BEETLE_H_ */
