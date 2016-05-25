/*
 * Beetle.h
 *
 *  Created on: Mar 23, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_BEETLE_H_
#define INCLUDE_BEETLE_H_

#include <boost/thread/pthread/shared_mutex.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BeetleTypes.h"
#include "sync/OrderedThreadPool.h"
#include "sync/ThreadPool.h"
#include "HCI.h"

/* Forward declarations */
class AccessControl;
class BeetleInternal;
class NetworkDiscoveryClient;
class Router;

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
	 * Inform Beetle that the device has been updated, for instance
	 * the handles have changed.
	 */
	void updateDevice(device_t);

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

	/*
	 * Register handler to be called after a device has been updated.
	 */
	void registerUpdateDeviceHandler(UpdateDeviceHandler h);
	std::vector<UpdateDeviceHandler> updateHandlers;

	/*
	 * Global map of all devices at this instance.
	 */
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
	 * Router object to route ATT packets
	 */
	Router *router;

	/*
	 * HCI socket
	 */
	HCI hci;

	/*
	 * Set the access control mechanism.
	 */
	void setAccessControl(std::shared_ptr<AccessControl> ac);
	std::shared_ptr<AccessControl> accessControl = NULL;

	/*
	 * Set the discovery client.
	 */
	void setDiscoveryClient(std::shared_ptr<NetworkDiscoveryClient> nd);
	std::shared_ptr<NetworkDiscoveryClient> discoveryClient = NULL;

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
