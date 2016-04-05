
#include "Beetle.h"

#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <cassert>
#include <thread>
#include <utility>

#include "CLI.h"
#include "device/BeetleDevice.h"
#include "Debug.h"
#include "Device.h"
#include "hat/BlockAllocator.h"
#include "hat/HAT.h"
#include "Router.h"
#include "tcp/TCPDeviceServer.h"

/* Global debug variable */
bool debug = true;

int main(int argc, char *argv[]) {
	Beetle btl;
	TCPDeviceServer(btl, 5000);

	CLI cli(btl);
	cli.join();
}

Beetle::Beetle() {
	hat = new BlockAllocator(256);
	router = new Router(*this);
	beetleDevice = new BeetleDevice(*this, "Beetle");
	devices[BEETLE_RESERVED_DEVICE] = beetleDevice;
}

Beetle::~Beetle() {

}

void Beetle::addDevice(Device *d, bool allocateHandles) {
	boost::unique_lock<boost::shared_mutex> deviceslk(devicesMutex);
	boost::unique_lock<boost::shared_mutex> hatLk(hatMutex);
	devices[d->getId()] = d;
	if (allocateHandles) {
		handle_range_t range = hat->reserve(d->getId());
		assert(!HAT::isNullRange(range));
	}
}

void Beetle::removeDevice(device_t id) {
	if (id == BEETLE_RESERVED_DEVICE) {
		pdebug("not allowed to remove Beetle!");
		return;
	}
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
	Device *d = devices[id];
	devices.erase(id);
	for (auto &kv : devices) {
		kv.second->unsubscribeAll(id);
	}
	std::thread t([d]() { delete d; });
	t.detach();
}

