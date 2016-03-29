
#include "Beetle.h"

#include <assert.h>

#include "CLI.h"

int main() {
	Beetle btl;
	btl.run();
}

Beetle::Beetle() {
	// TODO Auto-generated constructor stub

}

Beetle::~Beetle() {
	// TODO Auto-generated destructor stub
}

void Beetle::run() {
	CLI cli(this);
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
	std::thread t([](d) { delete d; });
	t.detach();
}

