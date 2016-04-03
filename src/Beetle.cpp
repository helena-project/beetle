
#include "Beetle.h"

#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <stddef.h>
#include <thread>

#include "CLI.h"
#include "device/BeetleDevice.h"
#include "device/Device.h"
#include "hat/BlockAllocator.h"
#include "Router.h"

bool debug = true;

int main() {
	Beetle btl;
	btl.run();
}

Beetle::Beetle() {
	router = NULL;
	hat = NULL;
}

Beetle::~Beetle() {

}

void Beetle::run() {
	hat = new BlockAllocator(256);
	router = new Router(*this);

	devices[BEETLE_RESERVED_DEVICE] = new BeetleDevice(*this);

	CLI cli(*this);
	cli.join();
}

void Beetle::addDevice(Device *d) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	boost::unique_lock<boost::shared_mutex> hatLk(hatMutex);
	devices[d->getId()] = d;
	hat->reserve(d->getId());
}

void Beetle::removeDevice(device_t id) {
	assert(id != BEETLE_RESERVED_DEVICE);
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
	Device *d = devices[id];
	devices.erase(id);
	for (auto &kv : devices) {
		kv.second->unsubscribeAll(id);
	}
	std::thread t([d]() { delete d; });
	t.detach();
}

