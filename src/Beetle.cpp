
#include "Beetle.h"

#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <thread>

#include "CLI.h"
#include "device/Device.h"
#include "hat/BlockAllocator.h"
#include "Router.h"

int main() {
	Beetle btl;
	btl.run();
}

Beetle::Beetle() {

}

Beetle::~Beetle() {

}

void Beetle::run() {
	hat = new BlockAllocator(128);
	router = new Router(*this);
	CLI cli(*this);
	cli.join();
}

void Beetle::addDevice(Device *d) {
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
	devices[d->getId()] = d;
}

void Beetle::removeDevice(device_t id) {
	boost::unique_lock<boost::shared_mutex> lk(devicesMutex);
	Device *d = devices[id];
	devices.erase(id);
	std::thread t([d]() { delete d; });
	t.detach();
}

