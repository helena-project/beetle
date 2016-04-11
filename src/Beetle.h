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
class BeetleMetaDevice;
class Router;
class HandleAllocationTable;
class TCPDeviceServer;

typedef long device_t;
const device_t BEETLE_RESERVED_DEVICE = 0;
const device_t NULL_RESERVED_DEVICE = -1;

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

	std::map<device_t, Device *> devices;
	boost::shared_mutex devicesMutex;

	/*
	 * Name this Beetle instance
	 */
	std::string name;

	/*
	 * Simulated device for Beetle's own services.
	 */
	BeetleMetaDevice *beetleDevice;

	Router *router;


};

#endif /* BEETLE_H_ */
