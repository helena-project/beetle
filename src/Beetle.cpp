
#include "Beetle.h"

#include <boost/thread/lock_types.hpp>
#include <cassert>
#include <mutex>
#include <sstream>
#include <utility>

#include "controller/AccessControl.h"
#include "Debug.h"
#include "device/BeetleInternal.h"
#include "hat/HandleAllocationTable.h"
#include "Router.h"

Beetle::Beetle(std::string name_) : workers(2), writers(4) {
	router = new Router(*this);
	beetleDevice = new BeetleInternal(*this, name_);
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
	accessControl = NULL;
	name = name_;
}

Beetle::~Beetle() {

}

void Beetle::addDevice(Device *d) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	device_t id = d->getId();
	devices[id] = d;
	deviceslk.unlock();

	for (auto &h : addHandlers) {
		workers.schedule([h,id] { h(id); });
	}
}

void Beetle::removeDevice(device_t id) {
	if (id == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to remove Beetle!");
		return;
	}
	boost::unique_lock<boost::shared_mutex> devicesLk(devicesMutex);
	if (devices.find(id) == devices.end()) {
		pwarn("removing non existant device!");
		return;
	}

	Device *d = devices[id];
	devices.erase(id);

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

	for (auto &kv : devices) {
		/*
		 * Cancel subscriptions and inform that services have changed
		 */
		assert(kv.first != id);
		if (!kv.second->hat->getDeviceRange(id).isNull()) {
			beetleDevice->informServicesChanged(kv.second->hat->free(id), kv.first);
		}
	}

	workers.schedule([d] { delete d; });

	for (auto &h : removeHandlers) {
		workers.schedule([h,id] { h(id); });
	}
}

void Beetle::mapDevices(device_t from, device_t to) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to map Beetle");
		return;
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		pwarn("not allowed to map null device");
		return;
	} else if (from == to) {
		pwarn("cannot map device to itself");
		return;
	}

	boost::shared_lock<boost::shared_mutex> devicesLk(devicesMutex);
	if (devices.find(from) == devices.end()) {
		pwarn(std::to_string(from) + " does not id a device");
		return;
	} else if (devices.find(to) == devices.end()) {
		pwarn(std::to_string(to) +" does not id a device");
		return;
	}

	Device *fromD = devices[from];
	Device *toD = devices[to];
	if (accessControl->canMap(fromD, toD) == false) {
		pwarn("permission denied");
		return;
	}

	std::lock_guard<std::mutex> hatLg(toD->hatMutex);
	if (!toD->hat->getDeviceRange(from).isNull()) {
		std::stringstream ss;
		ss << from << " is already mapped into " << to << "'s space";
		pwarn(ss.str());
	} else {
		handle_range_t range = toD->hat->reserve(from);
		if (debug) {
			pdebug("reserved " + range.str() + " at device " + std::to_string(to));
		}
	}
}

void Beetle::unmapDevices(device_t from, device_t to) {
	if (from == BEETLE_RESERVED_DEVICE || to == BEETLE_RESERVED_DEVICE) {
		pwarn("not allowed to unmap Beetle");
		return;
	} else if (from == NULL_RESERVED_DEVICE || to == NULL_RESERVED_DEVICE) {
		pwarn("unmapping null is a nop");
		return;
	} else if (from == to) {
		pwarn("cannot unmap self from self");
		return;
	}

	if (devices.find(from) == devices.end()) {
		pwarn(std::to_string(from) + " does not id a device");
		return;
	} else if (devices.find(to) == devices.end()) {
		pwarn(std::to_string(to) +" does not id a device");
		return;
	}

	Device *toD = devices[to];

	std::lock_guard<std::mutex> hatLg(toD->hatMutex);
	handle_range_t range = toD->hat->free(from);
	if (debug) {
		pdebug("freed " + range.str() + " at device " + std::to_string(to));
	}
}

void Beetle::registerAddDeviceHandler(AddDeviceHandler h) {
	addHandlers.push_back(h);
}

void Beetle::registerRemoveDeviceHandler(RemoveDeviceHandler h) {
	removeHandlers.push_back(h);
}

void Beetle::setAccessControl(AccessControl *ac) {
	assert(accessControl == NULL);
	accessControl = ac;
}


