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
#include "sync/SocketSelect.h"
#include "HCI.h"

/* Forward declarations */
class AccessControl;
class BeetleInternal;
class NetworkDiscoveryClient;
class Router;

/* Defaults parallelism */
const int DEFAULT_NUM_WORKERS = 8;
const int DEFAULT_NUM_WRITERS = 4;
const int DEFAULT_NUM_READERS = 4;

class Beetle {
public:
	Beetle(std::string name, int numWorkers = DEFAULT_NUM_WORKERS, int numWriters = DEFAULT_NUM_WRITERS,
			int numReaders = DEFAULT_NUM_READERS);
	virtual ~Beetle();

	/*
	 * Add a device to Beetle's mappings. Threadsafe. Returns holding a shared lock on devices.
	 */
	void addDevice(std::shared_ptr<Device> device, boost::shared_lock<boost::shared_mutex> &returnLock);
	inline void addDevice(std::shared_ptr<Device> device) {
		boost::shared_lock<boost::shared_mutex> unused;
		addDevice(device, unused);
	};

	/*
	 * Inform Beetle that the device has been updated; for instance, if the handles have changed.
	 */
	void updateDevice(device_t device);

	/*
	 * Removes a device from Beetle's mappings and unsubscribes the device from all characteristics.
	 */
	bool removeDevice(device_t device, std::string &err);
	inline void removeDevice(device_t device) {
		std::string unused;
		removeDevice(device, unused);
	}

	/*
	 * Maps handles from device 1 to device 2.
	 */
	bool mapDevices(device_t from, device_t to, std::string &err);
	inline void mapDevices(device_t from, device_t to) {
		std::string unused;
		mapDevices(from, to, unused);
	};

	/*
	 * Unmaps handles from device 1 to device 2.
	 */
	bool unmapDevices(device_t from, device_t to, std::string &err);
	inline void unmapDevices(device_t from, device_t to) {
		std::string unused;
		unmapDevices(from, to, unused);
	}

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
	 * Register handler to be called after mapping two devices.
	 */
	void registerMapDevicesHandler(MapDevicesHandler h);
	std::vector<MapDevicesHandler> mapHandlers;

	/*
	 * Register handler to be called after unmapping two devices.
	 */
	void registerUnmapDevicesHandler(UnmapDevicesHandler h);
	std::vector<UnmapDevicesHandler> unmapHandlers;

	/*
	 * Return a daemon to timeout devices.
	 */
	std::function<void()> getDaemon();

	/*
	 * Global map of all devices at this instance.
	 */
	std::map<device_t, std::shared_ptr<Device>> devices;
	boost::shared_mutex devicesMutex;

	/*
	 * Name this Beetle instance
	 */
	std::string name;

	/*
	 * Simulated device for Beetle's own services.
	 */
	std::shared_ptr<BeetleInternal> beetleDevice;

	/*
	 * Router object to route ATT packets
	 */
	std::unique_ptr<Router> router;

	/*
	 * HCI socket
	 */
	HCI hci;

	/*
	 * Set the access control mechanism.
	 */
	void setAccessControl(std::shared_ptr<AccessControl> ac);
	std::shared_ptr<AccessControl> accessControl;

	/*
	 * Set the discovery client.
	 */
	void setDiscoveryClient(std::shared_ptr<NetworkDiscoveryClient> nd);
	std::shared_ptr<NetworkDiscoveryClient> discoveryClient;

	/*
	 * Workers for callbacks.
	 */
	ThreadPool workers;

	/*
	 * Threads used for writing.
	 */
	OrderedThreadPool writers;

	/*
	 * Threads used for reading.
	 */
	SocketSelect readers;
};

#endif /* INCLUDE_BEETLE_H_ */
