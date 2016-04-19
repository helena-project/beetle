/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include <string>

#include "Beetle.h"

/*
 * Reports devices connected and disconnected.
 */
class ConnectionReporter {
public:
	ConnectionReporter(Beetle &beetle, std::string hostAndPort);
	virtual ~ConnectionReporter();

	AddDeviceHandler getAddDeviceHandler();
	RemoveDeviceHandler getRemoveDeviceHandler();
private:
	Beetle &beetle;

	std::string hostAndPort;

	void addDeviceHelper(Device *device);
	void removeDeviceHelper(device_t d);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
