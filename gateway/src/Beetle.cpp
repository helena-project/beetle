#include "Beetle.h"

#include <boost/thread/lock_types.hpp>
#include <cassert>
#include <mutex>
#include <sstream>
#include <utility>

#include "controller/AccessControl.h"
#include "Debug.h"
#include "device/BeetleInternal.h"
#include "device/VirtualDevice.h"
#include "hat/HandleAllocationTable.h"
#include "Router.h"

static const int NUM_WORKERS = 4;
static const int NUM_WRITERS = 1;
static const int NUM_READERS = 1;

Beetle::Beetle(std::string name_) :
		workers(NUM_WORKERS), writers(NUM_WRITERS), readers(NUM_READERS) {
	router = std::make_unique<Router>(*this);
	beetleDevice = std::make_shared<BeetleInternal>(*this, name_);
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
	name = name_;
}

Beetle::~Beetle() {
	boost::unique_lock<boost::shared_mutex> devicesLk(devicesMutex);
	devices.clear();
}

void Beetle::addDevice(std::shared_ptr<Device> d, boost::shared_lock<boost::shared_mutex> &returnLock) {
	devicesMutex.lock();
	device_t id = d->getId();
	devices[id] = d;

	/* Downgrade and return holding shared lock. */
	devicesMutex.unlock_and_lock_shared();
	returnLock = boost::shared_lock<boost::shared_mutex>(devicesMutex, boost::adopt_lock);

	for (auto &h : addHandlers) {
		workers.schedule([h,id] {h(id);});
	}
}

bool Beetle::removeDevice(device_t id, std::string &err) {
	if (id == BEETLE_RESERVED_DEVICE) {
		err = "not allowed to remove Beetle!";
		return false;
	}

	/*
	 * Spawn in a separate thread since caller might be holding the device lock.
	 */
	workers.schedule([this, id] {
		devicesMutex.lock_upgrade();
		if (devices.find(id) == devices.end()) {
			if (debug) {
				pwarn("removing non-existent device!");
			}
			devicesMutex.unlock_upgrade();
			return;
		}

		devicesMutex.unlock_upgrade_and_lock();
		std::shared_ptr<Device> d = devices[id];
		devices.erase(id);
		devicesMutex.unlock_and_lock_shared();

		boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex, boost::adopt_lock);

		d->hatMutex.lock();
		for (device_t server : d->hat->getDevices()) {
			if (devices.find(server) != devices.end()) {
				if (debug) {
					std::stringstream ss;
					ss << "unsubscribing " << id << " from " << server;
					pdebug(ss.str());
				}
				devices[server]->unsubscribeAll(id);
			}
		}
		d->hatMutex.unlock();

		for (auto &kv : devices) {
			/*
			 * Cancel subscriptions and inform that services have changed
			 */
			assert(kv.first != id);
			std::lock_guard<std::mutex> otherHatLg(kv.second->hatMutex);
			if (!kv.second->hat->getDeviceRange(id).isNull()) {
				beetleDevice->informServicesChanged(kv.second->hat->free(id), kv.first);
			}
		}

		for (auto &h : removeHandlers) {
			workers.schedule([h,id] {h(id);});
		}
	});

	return true;
}

void Beetle::updateDevice(device_t id) {
	for (auto &h : updateHandlers) {
		workers.schedule([h,id] {h(id);});
	}
}

bool Beetle::mapDevices(device_t from, device_t to, std::string &err) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		err = "not allowed to map Beetle";
		return false;
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		err = "not allowed to map null device";
		return false;
	} else if (from == to) {
		err = "cannot map device to itself";
		return false;
	}

	boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex);
	if (devices.find(from) == devices.end()) {
		err = std::to_string(from) + " does not id a device";
		return false;
	} else if (devices.find(to) == devices.end()) {
		err = std::to_string(to) + " does not id a device";
		return false;
	}

	std::shared_ptr<Device> fromD = devices[from];
	std::shared_ptr<Device> toD = devices[to];
	if (accessControl && accessControl->canMap(fromD, toD) == false) {
		err = "permission denied";
		return false;
	}

	std::lock_guard<std::mutex> hatLg(toD->hatMutex);
	if (!toD->hat->getDeviceRange(from).isNull()) {
		std::stringstream ss;
		ss << from << " is already mapped into " << to << "'s space";
		err = ss.str();
		return false;
	} else {
		handle_range_t range = toD->hat->reserve(from);
		beetleDevice->informServicesChanged(range, to);
		if (debug) {
			pdebug("reserved " + range.str() + " at device " + std::to_string(to));
		}
	}

	for (auto &h : mapHandlers) {
		workers.schedule([h,from,to] {h(from,to);});
	}

	return true;
}

bool Beetle::unmapDevices(device_t from, device_t to, std::string &err) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		err = "not allowed to unmap Beetle";
		return false;
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		err = "unmapping null is a nop";
		return false;
	} else if (from == to) {
		err = "cannot unmap self from self";
		return false;
	}

	boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex);
	if (devices.find(from) == devices.end()) {
		err = std::to_string(from) + " does not id a device";
		return false;
	} else if (devices.find(to) == devices.end()) {
		err = std::to_string(to) + " does not id a device";
		return false;
	}

	std::shared_ptr<Device> toD = devices[to];

	std::lock_guard<std::mutex> hatLg(toD->hatMutex);
	handle_range_t range = toD->hat->free(from);
	beetleDevice->informServicesChanged(range, to);
	if (debug) {
		pdebug("freed " + range.str() + " at device " + std::to_string(to));
	}

	for (auto &h : unmapHandlers) {
		workers.schedule([h,from,to] {h(from,to);});
	}

	return true;
}

void Beetle::registerAddDeviceHandler(AddDeviceHandler h) {
	addHandlers.push_back(h);
}

void Beetle::registerRemoveDeviceHandler(RemoveDeviceHandler h) {
	removeHandlers.push_back(h);
}

void Beetle::registerUpdateDeviceHandler(UpdateDeviceHandler h) {
	updateHandlers.push_back(h);
}

void Beetle::registerMapDevicesHandler(MapDevicesHandler h) {
	mapHandlers.push_back(h);
}

void Beetle::registerUnmapDevicesHandler(UnmapDevicesHandler h) {
	unmapHandlers.push_back(h);
}

std::function<void()> Beetle::getDaemon() {
	return [this] {
		boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex);

		for (auto &kv : devices) {
			auto vd = std::dynamic_pointer_cast<VirtualDevice>(kv.second);
			if (vd && !vd->isLive()) {
				if (debug) {
					std::stringstream ss;
					ss << "timed out: " << vd->getId();
					pdebug(ss.str());
				}
				removeDevice(vd->getId());
			}
		}
	};
}

void Beetle::setAccessControl(std::shared_ptr<AccessControl> ac) {
	assert(accessControl == NULL && ac != NULL);
	accessControl = ac;
	registerRemoveDeviceHandler(ac->getRemoveDeviceHandler());
}

void Beetle::setDiscoveryClient(std::shared_ptr<NetworkDiscoveryClient> nd) {
	assert(discoveryClient == NULL && nd != NULL);
	discoveryClient = nd;
}

